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

			IRRootParameter1 rootParameters[1] = {};
			rootParameters[0].ParameterType = IRRootParameterTypeCBV;
			rootParameters[0].ShaderVisibility = IRShaderVisibilityAll;
			rootParameters[0].Descriptor.ShaderRegister = 0;

			IRVersionedRootSignatureDescriptor desc = {};
			desc.version = IRRootSignatureVersion_1_1;
			desc.desc_1_1.NumParameters = 1;
			desc.desc_1_1.pParameters = rootParameters;
			desc.desc_1_1.Flags = IRRootSignatureFlags(
				IRRootSignatureFlagDenyHullShaderRootAccess |
				IRRootSignatureFlagDenyDomainShaderRootAccess |
				IRRootSignatureFlagDenyGeometryShaderRootAccess |
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
			IRCompilerSetMinimumDeploymentTarget(metal_ir_compiler, IROperatingSystem_macOS, "14.0.0");
			IRCompilerSetEntryPointName(metal_ir_compiler, input.entry_point.empty() ? "main" : input.entry_point.c_str());

			IRObject* dxil_obj = IRObjectCreateFromDXIL((uint8_t const*)blob->GetBufferPointer(), blob->GetBufferSize(), IRBytecodeOwnershipNone);
			if (!dxil_obj)
			{
				ADRIA_LOG(ERROR, "Failed to create IR object from DXIL");
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
						ADRIA_LOG(ERROR, "Failed to convert DXIL to Metal IR: %s", error_payload);
					}
					else
					{
						ADRIA_LOG(ERROR, "Failed to convert DXIL to Metal IR: error code %d", error_code);
					}
					IRErrorDestroy(ir_error);
				}
				return false;
			}

			IRMetalLibBinary* metallib = IRMetalLibBinaryCreate();
			IRShaderStage ir_stage = IRShaderStageVertex; 
			switch (input.stage)
			{
			case GfxShaderStage::VS: ir_stage = IRShaderStageVertex; break;
			case GfxShaderStage::PS: ir_stage = IRShaderStageFragment; break;
			case GfxShaderStage::CS: ir_stage = IRShaderStageCompute; break;
			case GfxShaderStage::MS: ir_stage = IRShaderStageMesh; break;
			case GfxShaderStage::AS: ir_stage = IRShaderStageAmplification; break;
			case GfxShaderStage::GS: ir_stage = IRShaderStageGeometry; break;
			case GfxShaderStage::DS: ir_stage = IRShaderStageDomain; break;
			case GfxShaderStage::HS: ir_stage = IRShaderStageHull; break;
			//case GfxShaderStage::LIB: ir_stage = IRShaderStageRayGeneration; break;
			default: break;
			}

			IRObjectGetMetalLibBinary(metal_ir_obj, ir_stage, metallib);
			Usize metallib_size = IRMetalLibGetBytecodeSize(metallib);
			std::vector<Uint8> metallib_data(metallib_size);
			IRMetalLibGetBytecode(metallib, metallib_data.data());

			IRMetalLibBinaryDestroy(metallib);
			IRObjectDestroy(metal_ir_obj);

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

