#include "dxcapi.h"
#if defined(ADRIA_PLATFORM_MACOS)
#include "metal_irconverter/metal_irconverter.h"
#endif
#include "GfxShaderCompiler.h"
#include "GfxDefines.h"
#include "Core/Paths.h"
#include "Core/FatalAssert.h"
#include "Utilities/StringConversions.h"
#include "Utilities/PathHelpers.h"
#include "Utilities/Hash.h"
#include "Utilities/Ref.h"
#include "Utilities/DynamicLibrary.h"
#include "cereal/archives/binary.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"

namespace adria
{
	ADRIA_LOG_CHANNEL(ShaderCompiler);

	using DxcCreateInstanceT = decltype(DxcCreateInstance);
	extern DxcCreateInstanceT* PFN_DxcCreateInstance = nullptr;

	namespace
	{
		Ref<IDxcLibrary> library = nullptr;
		Ref<IDxcCompiler3> compiler = nullptr;
		Ref<IDxcUtils> utils = nullptr;
		Ref<IDxcIncludeHandler> include_handler = nullptr;
		DynamicLibrary dxcompiler;

#if defined(ADRIA_PLATFORM_MACOS)
		IRCompiler* metal_ir_compiler = nullptr;
		IRRootSignature* metal_root_signature = nullptr;
#endif
	}

	class GfxIncludeHandler : public IDxcIncludeHandler
	{
	public:
		GfxIncludeHandler() {}

		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
		{
			Ref<IDxcBlobEncoding> encoding;
			std::string include_file = NormalizePath(ToString(pFilename));
			if (!FileExists(include_file))
			{
				*ppIncludeSource = nullptr;
				return E_FAIL;
			}

			Bool already_included = false;
			for (std::string const& included_file : include_files)
			{
				if (include_file == included_file)
				{
					already_included = true;
					break;
				}
			}

			if (already_included)
			{
				static const Char nullStr[] = " ";
				utils->CreateBlob(nullStr, ARRAYSIZE(nullStr), CP_UTF8, encoding.GetAddressOf());
				*ppIncludeSource = encoding.Detach();
				return S_OK;
			}

			std::wstring winclude_file = ToWideString(include_file);
			HRESULT hr = utils->LoadFile(winclude_file.c_str(), nullptr, encoding.GetAddressOf());
			if (SUCCEEDED(hr))
			{
				include_files.push_back(include_file);
				*ppIncludeSource = encoding.Detach();
				return S_OK;
			}
			else *ppIncludeSource = nullptr;
			return E_FAIL;
		}
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
		{
			return include_handler->QueryInterface(riid, ppvObject);
		}

		ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
		ULONG STDMETHODCALLTYPE Release(void) override { return 1; }

		std::vector<std::string> include_files;
	};

	class GfxShaderCompilerBlob : public IDxcBlob
	{
	public:
		GfxShaderCompilerBlob(void const* pShaderBytecode, Uint64 byteLength) : bytecode_size{ byteLength }
		{
			bytecode = const_cast<void*>(pShaderBytecode);
		}
		virtual ~GfxShaderCompilerBlob() { /*non owning blob -> empty destructor*/ }
		virtual LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override { return bytecode; }
		virtual SIZE_T STDMETHODCALLTYPE GetBufferSize(void) override { return bytecode_size; }
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppv) override
		{
			if (ppv == NULL)
			{
				return E_POINTER;
			}
			if (riid == __uuidof(IDxcBlob))
			{
				*ppv = static_cast<IDxcBlob*>(this);
			}
			else if (riid == __uuidof(IUnknown))
			{
				*ppv = static_cast<IUnknown*>(this);
			}
			else
			{
				*ppv = NULL;
				return E_NOINTERFACE;
			}
			reinterpret_cast<IUnknown*>(*ppv)->AddRef();
			return S_OK;
		}
		virtual ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
		virtual ULONG STDMETHODCALLTYPE Release(void) override { return 1; }

	private:
		LPVOID bytecode = nullptr;
		SIZE_T bytecode_size = 0;
	};

	
	inline constexpr std::wstring GetTarget(GfxShaderStage stage, GfxShaderModel model)
	{
		std::wstring target = L"";
		switch (stage)
		{
		case GfxShaderStage::VS:
			target += L"vs_";
			break;
		case GfxShaderStage::PS:
			target += L"ps_";
			break;
		case GfxShaderStage::CS:
			target += L"cs_";
			break;
		case GfxShaderStage::GS:
			target += L"gs_";
			break;
		case GfxShaderStage::HS:
			target += L"hs_";
			break;
		case GfxShaderStage::DS:
			target += L"ds_";
			break;
		case GfxShaderStage::LIB:
			target += L"lib_";
			break;
		case GfxShaderStage::MS:
			target += L"ms_";
			break;
		case GfxShaderStage::AS:
			target += L"as_";
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Shader Stage");
		}
		switch (model)
		{
		case SM_6_0:
			target += L"6_0";
			break;
		case SM_6_1:
			target += L"6_1";
			break;
		case SM_6_2:
			target += L"6_2";
			break;
		case SM_6_3:
			target += L"6_3";
			break;
		case SM_6_4:
			target += L"6_4";
			break;
		case SM_6_5:
			target += L"6_5";
			break;
		case SM_6_6:
			target += L"6_6";
			break;
		case SM_6_7:
			target += L"6_7";
			break;
		case SM_6_8:
			target += L"6_8";
			break;
		default:
			break;
		}
		return target;
	}

	namespace GfxShaderCompiler
	{
		static Bool CheckCache(Char const* cache_path, GfxShaderCompileInput const& input, GfxShaderCompileOutput& output)
		{
			std::string cache_binary(cache_path); cache_binary += ".bin";
			std::string cache_metadata(cache_path); cache_metadata += ".meta";

			if (!FileExists(cache_binary) || !FileExists(cache_metadata))
			{
				return false;
			}
			if (GetFileLastWriteTime(cache_binary) < GetFileLastWriteTime(input.file))
			{
				return false;
			}

			std::ifstream is(cache_metadata, std::ios::binary);
			cereal::BinaryInputArchive metadata_archive(is);

			metadata_archive(output.shader_hash);
			metadata_archive(output.includes);
			Uint64 binary_size = 0;
			metadata_archive(binary_size);

			//check if the one of the includes was modified after the cache binary was generated
			for (std::string const& include : output.includes)
			{
				if (GetFileLastWriteTime(cache_binary) < GetFileLastWriteTime(include))
				{
					return false;
				}
			}

			std::ifstream is2(cache_binary, std::ios::binary);
			cereal::BinaryInputArchive binary_archive(is2);

			std::unique_ptr<Char[]> binary_data(new Char[binary_size]);
			binary_archive.loadBinary(binary_data.get(), binary_size);
			output.shader.SetShaderData(binary_data.get(), binary_size);
			output.shader.SetDesc(input);

			return true;
		}
		static Bool SaveToCache(Char const* cache_path, GfxShaderCompileOutput const& output)
		{
			std::string cache_metadata(cache_path); cache_metadata += ".meta";
			std::ofstream os(cache_metadata, std::ios::binary);
			cereal::BinaryOutputArchive metadata_archive(os);
			metadata_archive(output.shader_hash);
			metadata_archive(output.includes);
			metadata_archive(output.shader.GetSize());

			std::string cache_binary(cache_path); cache_binary += ".bin";
			std::ofstream os2(cache_binary, std::ios::binary);
			cereal::BinaryOutputArchive binary_archive(os2);
			binary_archive.saveBinary(output.shader.GetData(), output.shader.GetSize());

			return true;
		}

		void Initialize()
		{
#if defined(ADRIA_PLATFORM_WINDOWS)
			std::string dxcompiler_path = paths::MainDir + "dxcompiler.dll";
#elif defined(ADRIA_PLATFORM_MACOS)
			std::string dxcompiler_path = paths::MainDir + "dxcompiler.dylib";
#endif
			dxcompiler.Open(dxcompiler_path.c_str());
			ADRIA_FATAL_ASSERT(dxcompiler.IsOpen(), "Couldn't open dxcompiler!");

			Bool const success = dxcompiler.GetSymbol("DxcCreateInstance", &PFN_DxcCreateInstance);
			ADRIA_FATAL_ASSERT(success && PFN_DxcCreateInstance != nullptr, "Couldn't get DxcCreateInstance symbol from dxcompiler!");

			HRESULT hr = S_OK;
			hr = PFN_DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
			ADRIA_ASSERT(SUCCEEDED(hr));
			hr = PFN_DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));
			ADRIA_ASSERT(SUCCEEDED(hr));
			hr = library->CreateIncludeHandler(include_handler.GetAddressOf());
			ADRIA_ASSERT(SUCCEEDED(hr));
			hr = PFN_DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
			ADRIA_ASSERT(SUCCEEDED(hr));

			std::filesystem::create_directory(paths::ShaderPDBDir);

#if defined(ADRIA_PLATFORM_MACOS)
			metal_ir_compiler = IRCompilerCreate();

			// Match D3D12 common root signature: CB0, 8 constants at register 1, CB2, CB3
			IRRootParameter1 rootParameters[4] = {};

			// Root parameter 0: CBV at register 0
			rootParameters[0].ParameterType = IRRootParameterTypeCBV;
			rootParameters[0].ShaderVisibility = IRShaderVisibilityAll;
			rootParameters[0].Descriptor.ShaderRegister = 0;

			// Root parameter 1: 8 root constants at register 1
			rootParameters[1].ParameterType = IRRootParameterType32BitConstants;
			rootParameters[1].ShaderVisibility = IRShaderVisibilityAll;
			rootParameters[1].Constants.ShaderRegister = 1;
			rootParameters[1].Constants.Num32BitValues = 8;

			// Root parameter 2: CBV at register 2
			rootParameters[2].ParameterType = IRRootParameterTypeCBV;
			rootParameters[2].ShaderVisibility = IRShaderVisibilityAll;
			rootParameters[2].Descriptor.ShaderRegister = 2;

			// Root parameter 3: CBV at register 3
			rootParameters[3].ParameterType = IRRootParameterTypeCBV;
			rootParameters[3].ShaderVisibility = IRShaderVisibilityAll;
			rootParameters[3].Descriptor.ShaderRegister = 3;

			// Static samplers matching D3D12 (registers 0-9)
			IRStaticSamplerDescriptor staticSamplers[10] = {};

			// Sampler 0: Linear, Wrap
			staticSamplers[0].Filter = IRFilterMinMagMipLinear;
			staticSamplers[0].AddressU = IRTextureAddressModeWrap;
			staticSamplers[0].AddressV = IRTextureAddressModeWrap;
			staticSamplers[0].AddressW = IRTextureAddressModeWrap;
			staticSamplers[0].MipLODBias = 0.0f;
			staticSamplers[0].MaxAnisotropy = 1;
			staticSamplers[0].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[0].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[0].MinLOD = 0.0f;
			staticSamplers[0].MaxLOD = 3.402823466e+38f; // FLT_MAX
			staticSamplers[0].ShaderRegister = 0;
			staticSamplers[0].RegisterSpace = 0;
			staticSamplers[0].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 1: Linear, Clamp
			staticSamplers[1].Filter = IRFilterMinMagMipLinear;
			staticSamplers[1].AddressU = IRTextureAddressModeClamp;
			staticSamplers[1].AddressV = IRTextureAddressModeClamp;
			staticSamplers[1].AddressW = IRTextureAddressModeClamp;
			staticSamplers[1].MipLODBias = 0.0f;
			staticSamplers[1].MaxAnisotropy = 1;
			staticSamplers[1].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[1].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[1].MinLOD = 0.0f;
			staticSamplers[1].MaxLOD = 3.402823466e+38f;
			staticSamplers[1].ShaderRegister = 1;
			staticSamplers[1].RegisterSpace = 0;
			staticSamplers[1].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 2: Linear, Border (black)
			staticSamplers[2].Filter = IRFilterMinMagMipLinear;
			staticSamplers[2].AddressU = IRTextureAddressModeBorder;
			staticSamplers[2].AddressV = IRTextureAddressModeBorder;
			staticSamplers[2].AddressW = IRTextureAddressModeBorder;
			staticSamplers[2].MipLODBias = 0.0f;
			staticSamplers[2].MaxAnisotropy = 1;
			staticSamplers[2].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[2].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[2].MinLOD = 0.0f;
			staticSamplers[2].MaxLOD = 3.402823466e+38f;
			staticSamplers[2].ShaderRegister = 2;
			staticSamplers[2].RegisterSpace = 0;
			staticSamplers[2].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 3: Point, Wrap
			staticSamplers[3].Filter = IRFilterMinMagMipPoint;
			staticSamplers[3].AddressU = IRTextureAddressModeWrap;
			staticSamplers[3].AddressV = IRTextureAddressModeWrap;
			staticSamplers[3].AddressW = IRTextureAddressModeWrap;
			staticSamplers[3].MipLODBias = 0.0f;
			staticSamplers[3].MaxAnisotropy = 1;
			staticSamplers[3].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[3].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[3].MinLOD = 0.0f;
			staticSamplers[3].MaxLOD = 3.402823466e+38f;
			staticSamplers[3].ShaderRegister = 3;
			staticSamplers[3].RegisterSpace = 0;
			staticSamplers[3].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 4: Point, Clamp
			staticSamplers[4].Filter = IRFilterMinMagMipPoint;
			staticSamplers[4].AddressU = IRTextureAddressModeClamp;
			staticSamplers[4].AddressV = IRTextureAddressModeClamp;
			staticSamplers[4].AddressW = IRTextureAddressModeClamp;
			staticSamplers[4].MipLODBias = 0.0f;
			staticSamplers[4].MaxAnisotropy = 1;
			staticSamplers[4].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[4].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[4].MinLOD = 0.0f;
			staticSamplers[4].MaxLOD = 3.402823466e+38f;
			staticSamplers[4].ShaderRegister = 4;
			staticSamplers[4].RegisterSpace = 0;
			staticSamplers[4].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 5: Point, Border (black)
			staticSamplers[5].Filter = IRFilterMinMagMipPoint;
			staticSamplers[5].AddressU = IRTextureAddressModeBorder;
			staticSamplers[5].AddressV = IRTextureAddressModeBorder;
			staticSamplers[5].AddressW = IRTextureAddressModeBorder;
			staticSamplers[5].MipLODBias = 0.0f;
			staticSamplers[5].MaxAnisotropy = 1;
			staticSamplers[5].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[5].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[5].MinLOD = 0.0f;
			staticSamplers[5].MaxLOD = 3.402823466e+38f;
			staticSamplers[5].ShaderRegister = 5;
			staticSamplers[5].RegisterSpace = 0;
			staticSamplers[5].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 6: Comparison Linear, Clamp
			staticSamplers[6].Filter = IRFilterComparisonMinMagMipLinear;
			staticSamplers[6].AddressU = IRTextureAddressModeClamp;
			staticSamplers[6].AddressV = IRTextureAddressModeClamp;
			staticSamplers[6].AddressW = IRTextureAddressModeClamp;
			staticSamplers[6].MipLODBias = 0.0f;
			staticSamplers[6].MaxAnisotropy = 16;
			staticSamplers[6].ComparisonFunc = IRComparisonFunctionLessEqual;
			staticSamplers[6].BorderColor = IRStaticBorderColorOpaqueWhite;
			staticSamplers[6].MinLOD = 0.0f;
			staticSamplers[6].MaxLOD = 3.402823466e+38f;
			staticSamplers[6].ShaderRegister = 6;
			staticSamplers[6].RegisterSpace = 0;
			staticSamplers[6].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 7: Comparison Linear, Wrap
			staticSamplers[7].Filter = IRFilterComparisonMinMagMipLinear;
			staticSamplers[7].AddressU = IRTextureAddressModeWrap;
			staticSamplers[7].AddressV = IRTextureAddressModeWrap;
			staticSamplers[7].AddressW = IRTextureAddressModeWrap;
			staticSamplers[7].MipLODBias = 0.0f;
			staticSamplers[7].MaxAnisotropy = 16;
			staticSamplers[7].ComparisonFunc = IRComparisonFunctionLessEqual;
			staticSamplers[7].BorderColor = IRStaticBorderColorOpaqueWhite;
			staticSamplers[7].MinLOD = 0.0f;
			staticSamplers[7].MaxLOD = 3.402823466e+38f;
			staticSamplers[7].ShaderRegister = 7;
			staticSamplers[7].RegisterSpace = 0;
			staticSamplers[7].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 8: Linear, Mirror
			staticSamplers[8].Filter = IRFilterMinMagMipLinear;
			staticSamplers[8].AddressU = IRTextureAddressModeMirror;
			staticSamplers[8].AddressV = IRTextureAddressModeMirror;
			staticSamplers[8].AddressW = IRTextureAddressModeWrap;
			staticSamplers[8].MipLODBias = 0.0f;
			staticSamplers[8].MaxAnisotropy = 1;
			staticSamplers[8].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[8].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[8].MinLOD = 0.0f;
			staticSamplers[8].MaxLOD = 3.402823466e+38f;
			staticSamplers[8].ShaderRegister = 8;
			staticSamplers[8].RegisterSpace = 0;
			staticSamplers[8].ShaderVisibility = IRShaderVisibilityAll;

			// Sampler 9: Point, Mirror
			staticSamplers[9].Filter = IRFilterMinMagMipPoint;
			staticSamplers[9].AddressU = IRTextureAddressModeMirror;
			staticSamplers[9].AddressV = IRTextureAddressModeMirror;
			staticSamplers[9].AddressW = IRTextureAddressModeWrap;
			staticSamplers[9].MipLODBias = 0.0f;
			staticSamplers[9].MaxAnisotropy = 1;
			staticSamplers[9].ComparisonFunc = IRComparisonFunctionNever;
			staticSamplers[9].BorderColor = IRStaticBorderColorOpaqueBlack;
			staticSamplers[9].MinLOD = 0.0f;
			staticSamplers[9].MaxLOD = 3.402823466e+38f;
			staticSamplers[9].ShaderRegister = 9;
			staticSamplers[9].RegisterSpace = 0;
			staticSamplers[9].ShaderVisibility = IRShaderVisibilityAll;

			IRVersionedRootSignatureDescriptor desc = {};
			desc.version = IRRootSignatureVersion_1_1;
			desc.desc_1_1.NumParameters = 4;
			desc.desc_1_1.pParameters = rootParameters;
			desc.desc_1_1.NumStaticSamplers = 10;
			desc.desc_1_1.pStaticSamplers = staticSamplers;
			desc.desc_1_1.Flags = IRRootSignatureFlags(
				IRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed |
				IRRootSignatureFlagSamplerHeapDirectlyIndexed);

			IRError* error = nullptr;
			metal_root_signature = IRRootSignatureCreateFromDescriptor(&desc, &error);
			if (!metal_root_signature)
			{
				ADRIA_LOG(ERROR, "Failed to create Metal root signature");
				if (error)
				{
					IRErrorDestroy(error);
				}
			}

			ADRIA_LOG(INFO, "Metal IR Converter initialized");
#endif
		}
		void Destroy()
		{
#if defined(ADRIA_PLATFORM_MACOS)
			if (metal_root_signature)
			{
				IRRootSignatureDestroy(metal_root_signature);
			}
			if (metal_ir_compiler)
			{
				IRCompilerDestroy(metal_ir_compiler);
			}
#endif
			include_handler.Reset();
			compiler.Reset();
			library.Reset();
			utils.Reset();
		}
		Bool CompileShader(GfxShaderCompileInput const& input, GfxShaderCompileOutput& output)
		{
			std::string define_key;
			for (GfxShaderDefine const& define : input.defines)
			{
				define_key += define.name;
				define_key += define.value;
			}
			Uint64 define_hash = crc64(define_key.c_str(), define_key.size());
			std::string build_string = input.flags & GfxShaderCompilerFlag_Debug ? "debug" : "release";
			Char cache_path[256];
			snprintf(cache_path, sizeof(cache_path), "%s%s_%s_%llx_%s", paths::ShaderCacheDir.c_str(), GetFilenameWithoutExtension(input.file).c_str(),
												     input.entry_point.c_str(), define_hash, build_string.c_str());

			if (CheckCache(cache_path, input, output))
			{
				return true;
			}
			ADRIA_LOG(INFO, "Shader '%s.%s' not found in cache. Compiling...", input.file.c_str(), input.entry_point.c_str());

			compile:
			Uint32 code_page = CP_UTF8;
			Ref<IDxcBlobEncoding> source_blob;

			std::wstring shader_source = ToWideString(input.file);
			HRESULT hr = library->CreateBlobFromFile(shader_source.data(), &code_page, source_blob.GetAddressOf());
			if (FAILED(hr))
			{
				ADRIA_LOG(ERROR, "Failed to load shader file: %s", input.file.c_str());
				return false;
			}

			std::wstring name = ToWideString(GetFilenameWithoutExtension(input.file));
			std::wstring dir  = ToWideString(paths::ShaderDir);
			std::wstring path = ToWideString(GetParentPath(input.file));

			std::wstring target = GetTarget(input.stage, input.model);
			std::wstring entry_point = ToWideString(input.entry_point);
			if (entry_point.empty())
			{
				entry_point = L"main";
			}

			std::vector<Wchar const*> compile_args{};
			compile_args.push_back(name.c_str());
			if (input.flags & GfxShaderCompilerFlag_Debug)
			{
				compile_args.push_back(DXC_ARG_DEBUG);
			}

			if (input.flags & GfxShaderCompilerFlag_DisableOptimization)
			{
				compile_args.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
			}
			else
			{
				compile_args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
			}
			compile_args.push_back(L"-HV 2021");

			compile_args.push_back(L"-E");
			compile_args.push_back(entry_point.c_str());
			compile_args.push_back(L"-T");
			compile_args.push_back(target.c_str());

			compile_args.push_back(L"-I");
			compile_args.push_back(dir.c_str());
			compile_args.push_back(L"-I");
			compile_args.push_back(path.c_str());

			std::vector<std::wstring> defines;
			defines.reserve(input.defines.size());
			for (GfxShaderDefine const& define : input.defines)
			{
				std::wstring define_name = ToWideString(define.name);
				std::wstring define_value = ToWideString(define.value);
				compile_args.push_back(L"-D");
				if (define.value.empty())
				{
					defines.push_back(define_name + L"=1");
				}
				else
				{
					defines.push_back(define_name + L"=" + define_value);
				}
				compile_args.push_back(defines.back().c_str());
			}

			DxcBuffer source_buffer{};
			source_buffer.Ptr = source_blob->GetBufferPointer();
			source_buffer.Size = source_blob->GetBufferSize();
			source_buffer.Encoding = DXC_CP_ACP;
			GfxIncludeHandler custom_include_handler{};

			Ref<IDxcResult> result;
			hr = compiler->Compile(
				&source_buffer,
				compile_args.data(), (Uint32)compile_args.size(),
				&custom_include_handler,
				IID_PPV_ARGS(result.GetAddressOf()));

			Ref<IDxcBlobUtf8> errors;
			if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr)))
			{
				if (errors && errors->GetStringLength() > 0)
				{
					Char const* err_msg = errors->GetStringPointer();
					ADRIA_LOG(ERROR, "%s", err_msg);
#if defined(ADRIA_PLATFORM_WINDOWS)
					std::string msg = "Click OK after you fixed the following errors: \n";
					msg += err_msg;
					Int32 result = MessageBoxA(NULL, msg.c_str(), NULL, MB_OKCANCEL);
					if (result == IDOK)
					{
						goto compile;
					}
					else if (result == IDCANCEL)
					{
						return false;
					}
#else
					return false;
#endif
				}
			}

			Ref<IDxcBlob> blob;
			hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(blob.GetAddressOf()), nullptr);
			if (FAILED(hr))
			{
				ADRIA_LOG(ERROR, "Failed to get shader bytecode");
				return false;
			}
			
			if (input.flags & GfxShaderCompilerFlag_Debug)
			{
				Ref<IDxcBlob> pdb_blob;
				Ref<IDxcBlobWide> pdb_path_utf16;
				if (SUCCEEDED(result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pdb_blob.GetAddressOf()), pdb_path_utf16.GetAddressOf())))
				{
					Ref<IDxcBlobUtf8> pdb_path_utf8;
					if (SUCCEEDED(utils->GetBlobAsUtf8(pdb_path_utf16.Get(), pdb_path_utf8.GetAddressOf())))
					{
						Char pdb_path[256];
						snprintf(pdb_path, sizeof(pdb_path), "%s%s", paths::ShaderPDBDir.c_str(), pdb_path_utf8->GetStringPointer());
						FILE* pdb_file = fopen(pdb_path, "wb");
						if (pdb_file)
						{
							fwrite(pdb_blob->GetBufferPointer(), pdb_blob->GetBufferSize(), 1, pdb_file);
							fclose(pdb_file);
						}
					}
				}
			}
			Ref<IDxcBlob> hash;
			if (SUCCEEDED(result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(hash.GetAddressOf()), nullptr)))
			{
				DxcShaderHash* hash_buf = (DxcShaderHash*)hash->GetBufferPointer();
				memcpy(output.shader_hash, hash_buf->HashDigest, sizeof(Uint64) * 2);
			}

#if defined(ADRIA_PLATFORM_MACOS)
			ADRIA_ASSERT(metal_ir_compiler && metal_root_signature);
			IRCompilerSetGlobalRootSignature(metal_ir_compiler, metal_root_signature);
			IRCompilerSetMinimumGPUFamily(metal_ir_compiler, IRGPUFamilyApple7);
			IRCompilerSetMinimumDeploymentTarget(metal_ir_compiler, IROperatingSystem_macOS, "15.0.0");

			// Don't set entry point name for library shaders, they have multiple entry points
			if (input.stage != GfxShaderStage::LIB)
			{
				IRCompilerSetEntryPointName(metal_ir_compiler, input.entry_point.empty() ? "main" : input.entry_point.c_str());
			}

			IRRayTracingPipelineConfiguration* rt_config = nullptr;
			if (input.stage == GfxShaderStage::LIB)
			{
				ADRIA_LOG_SYNC(INFO, "Configuring raytracing pipeline for library shader");
				rt_config = IRRayTracingPipelineConfigurationCreate();
				IRRayTracingPipelineConfigurationSetMaxAttributeSizeInBytes(rt_config, 8);
				IRRayTracingPipelineConfigurationSetMaxRecursiveDepth(rt_config, 1);
				IRRayTracingPipelineConfigurationSetRayGenerationCompilationMode(rt_config, IRRayGenerationCompilationKernel);
				IRCompilerSetRayTracingPipelineConfiguration(metal_ir_compiler, rt_config);
			}

			IRObject* dxil_obj = IRObjectCreateFromDXIL((uint8_t const*)blob->GetBufferPointer(), blob->GetBufferSize(), IRBytecodeOwnershipNone);
			if (!dxil_obj)
			{
				return false;
			}

			IRError* ir_error = nullptr;
			IRObject* metal_ir_obj = IRCompilerAllocCompileAndLink(metal_ir_compiler, nullptr, dxil_obj, &ir_error);
			IRObjectDestroy(dxil_obj);

			if (!metal_ir_obj)
			{
				if (ir_error)
				{
					uint32_t error_code = IRErrorGetCode(ir_error);
					const char* error_payload = (const char*)IRErrorGetPayload(ir_error);
					if (error_payload)
					{
						ADRIA_LOG_SYNC(ERROR, "Failed to convert DXIL to Metal IR: %s", error_payload);
					}
					else
					{
						ADRIA_LOG_SYNC(ERROR, "Failed to convert DXIL to Metal IR: error code %d", error_code);
					}
					IRErrorDestroy(ir_error);
				}
				return false;
			}

			IRMetalLibBinary* metallib = IRMetalLibBinaryCreate();
			IRShaderStage ir_stage = IRObjectGetMetalIRShaderStage(metal_ir_obj);
			Bool extraction_success = IRObjectGetMetalLibBinary(metal_ir_obj, ir_stage, metallib);

			if (!extraction_success)
			{
				ADRIA_LOG_SYNC(ERROR, "Failed to extract Metal library binary for shader stage %d", ir_stage);
				IRMetalLibBinaryDestroy(metallib);
				IRObjectDestroy(metal_ir_obj);
				if (rt_config)
				{
					IRRayTracingPipelineConfigurationDestroy(rt_config);
				}
				return false;
			}

			Usize metallib_size = IRMetalLibGetBytecodeSize(metallib);
			if (metallib_size == 0)
			{
				ADRIA_LOG_SYNC(ERROR, "Metal library binary has zero size for shader stage %d", ir_stage);
				IRMetalLibBinaryDestroy(metallib);
				IRObjectDestroy(metal_ir_obj);
				if (rt_config)
				{
					IRRayTracingPipelineConfigurationDestroy(rt_config);
				}
				return false;
			}

			std::vector<Uint8> metallib_data(metallib_size);
			IRMetalLibGetBytecode(metallib, metallib_data.data());

			IRMetalLibBinaryDestroy(metallib);
			IRObjectDestroy(metal_ir_obj);

			if (rt_config)
			{
				IRRayTracingPipelineConfigurationDestroy(rt_config);
			}

			output.shader.SetDesc(input);
			output.shader.SetShaderData(metallib_data.data(), metallib_data.size());
			ADRIA_LOG(INFO, "Successfully converted DXIL to Metal IR for shader: %s", input.file.c_str());
#else
			output.shader.SetDesc(input);
			output.shader.SetShaderData(blob->GetBufferPointer(), blob->GetBufferSize());
#endif
			output.includes = std::move(custom_include_handler.include_files);
			output.includes.push_back(input.file);
			SaveToCache(cache_path, output);
			return true;
		}
		void ReadBlobFromFile(std::string const& filename, GfxShaderBlob& blob)
		{
			std::wstring wide_filename = ToWideString(filename);
			Uint32 code_page = CP_UTF8;
			Ref<IDxcBlobEncoding> source_blob;
			HRESULT hr = library->CreateBlobFromFile(wide_filename.data(), &code_page, source_blob.GetAddressOf());
			if (FAILED(hr))
			{
				ADRIA_LOG(ERROR, "Failed to read blob from file: %s", filename.c_str());
				return;
			}
			blob.resize(source_blob->GetBufferSize());
			memcpy(blob.data(), source_blob->GetBufferPointer(), source_blob->GetBufferSize());
		}
	}
}

