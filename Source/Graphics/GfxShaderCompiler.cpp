#include "dxcapi.h"
#if defined(ADRIA_PLATFORM_MACOS)
#include "metal_irconverter/metal_irconverter.h"
#include "Metal/MetalShaderReflection.h"
#endif
#include "GfxShaderCompiler.h"
#include "GfxDevice.h"
#include "GfxDefines.h"
#include "Core/Paths.h"
#include "Core/FatalAssert.h"
#include "Utilities/StringConversions.h"
#include "Utilities/PathHelpers.h"
#include "Utilities/Hash.h"
#include "Utilities/Ref.h"
#include "Utilities/DynamicLibrary.h"
#include "Utilities/BinarySerializer.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(ShaderCompiler);

	using DxcCreateInstanceT = decltype(DxcCreateInstance);
	static DxcCreateInstanceT* PFN_DxcCreateInstance = nullptr;

	namespace
	{
		Ref<IDxcLibrary> library = nullptr;
		Ref<IDxcCompiler3> compiler = nullptr;
		Ref<IDxcUtils> utils = nullptr;
		Ref<IDxcIncludeHandler> include_handler = nullptr;
		DynamicLibrary dxcompiler;
		GfxBackend current_backend = GfxBackend::Unknown;

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

	namespace
	{
		enum SpvOp : Uint32
		{
			SpvOpName                         = 5,
			SpvOpTypeImage                    = 25,
			SpvOpTypeRuntimeArray             = 29,
			SpvOpTypePointer                  = 32,
			SpvOpVariable                     = 59,
			SpvOpDecorate                     = 71,
			SpvOpMemberDecorate               = 72,
			SpvOpTypeAccelerationStructureKHR = 5341,
		};
		enum : Uint32
		{
			SpvDecorationNonWritable  = 24,
			SpvDecorationBinding      = 33,
			SpvStorageClassUniformConstant = 0,
			SpvStorageClassStorageBuffer   = 12,
		};

		void PatchSpirvBindlessBindings(void* spirv_data, Uint64 spirv_size)
		{
			Uint32* words = (Uint32*)spirv_data;
			Uint32 word_count = (Uint32)(spirv_size / 4);
			if (word_count < 5) return;

			std::unordered_map<Uint32, std::string> names;
			std::unordered_map<Uint32, Uint32> var_result_type;
			std::unordered_map<Uint32, Uint32> var_storage_class;
			std::unordered_map<Uint32, Uint32> ptr_pointee;
			std::unordered_map<Uint32, Uint32> rta_element;
			std::unordered_map<Uint32, Uint32> image_sampled;
			std::unordered_map<Uint32, Uint32> binding_word_offset;
			std::unordered_set<Uint32> nonwritable_types;
			std::unordered_set<Uint32> accel_struct_types;

			Uint32 i = 5;
			while (i < word_count)
			{
				Uint32 inst = words[i];
				Uint32 len = inst >> 16;
				Uint32 op  = inst & 0xFFFF;
				if (len == 0) break;

				switch (op)
				{
				case SpvOpName:
				{
					Uint32 target = words[i + 1];
					names[target] = (char const*)&words[i + 2];
					break;
				}
				case SpvOpTypeImage:
				{
					Uint32 result_id = words[i + 1];
					if (len >= 9) image_sampled[result_id] = words[i + 7];
					break;
				}
				case SpvOpTypeRuntimeArray:
				{
					rta_element[words[i + 1]] = words[i + 2];
					break;
				}
				case SpvOpTypeAccelerationStructureKHR:
				{
					accel_struct_types.insert(words[i + 1]);
					break;
				}
				case SpvOpTypePointer:
				{
					ptr_pointee[words[i + 1]] = words[i + 3];
					break;
				}
				case SpvOpVariable:
				{
					var_result_type[words[i + 2]] = words[i + 1];
					var_storage_class[words[i + 2]] = words[i + 3];
					break;
				}
				case SpvOpDecorate:
				{
					Uint32 target = words[i + 1];
					Uint32 dec    = words[i + 2];
					if (dec == SpvDecorationBinding && len >= 4)
						binding_word_offset[target] = i + 3;
					break;
				}
				case SpvOpMemberDecorate:
				{
					Uint32 target = words[i + 1];
					Uint32 dec    = words[i + 3];
					if (dec == SpvDecorationNonWritable)
						nonwritable_types.insert(target);
					break;
				}
				}
				i += len;
			}

			for (auto& [var_id, type_id] : var_result_type)
			{
				auto name_it = names.find(var_id);
				if (name_it == names.end()) continue;
				if (name_it->second.find("ResourceDescriptorHeap") == std::string::npos) continue;

				auto boff = binding_word_offset.find(var_id);
				if (boff == binding_word_offset.end()) continue;

				Uint32 sc = var_storage_class[var_id];
				Uint32 new_binding = 0;

				if (sc == SpvStorageClassStorageBuffer)
				{
					auto pp = ptr_pointee.find(type_id);
					Uint32 rta_id = pp != ptr_pointee.end() ? pp->second : 0;
					auto re = rta_element.find(rta_id);
					Uint32 struct_id = re != rta_element.end() ? re->second : 0;
					new_binding = nonwritable_types.count(struct_id) ? 2 : 3;
				}
				else if (sc == SpvStorageClassUniformConstant)
				{
					auto pp = ptr_pointee.find(type_id);
					Uint32 rta_id = pp != ptr_pointee.end() ? pp->second : 0;
					auto re = rta_element.find(rta_id);
					Uint32 elem_id = re != rta_element.end() ? re->second : 0;
					if (accel_struct_types.contains(elem_id))
					{
						new_binding = 4; // VK_BINDLESS_BINDING_AS
					}
					else
					{
						auto si = image_sampled.find(elem_id);
						new_binding = (si != image_sampled.end() && si->second == 1) ? 0 : 1;
					}
				}

				words[boff->second] = new_binding;
			}
		}
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

			BinaryFileReader metadata_reader(cache_metadata);
			if (!metadata_reader.IsValid()) 
			{
				return false;
			}

			Uint64 binary_size = 0;
			metadata_reader.Read(output.shader_hash);
			metadata_reader.Read(output.includes);
			metadata_reader.Read(binary_size);

#if defined(ADRIA_PLATFORM_MACOS)
			Uint64 reflection_size = 0;
			metadata_reader.Read(reflection_size);
#endif

			for (std::string const& include : output.includes)
			{
				if (!FileExists(include))
				{
					return false;
				}
				if (GetFileLastWriteTime(cache_binary) < GetFileLastWriteTime(include))
				{
					return false;
				}
			}

			BinaryFileReader binary_reader(cache_binary);
			if (!binary_reader.IsValid()) 
			{
				return false;
			}

			std::unique_ptr<Char[]> binary_data(new Char[binary_size]);
			binary_reader.ReadBlob(binary_data.get(), binary_size);
			output.shader.SetShaderData(binary_data.get(), binary_size);
			output.shader.SetDesc(input);

#if defined(ADRIA_PLATFORM_MACOS)
			if (reflection_size > 0)
			{
				std::unique_ptr<Char[]> reflection_data(new Char[reflection_size]);
				binary_reader.ReadBlob(reflection_data.get(), reflection_size);
				output.shader.SetReflectionData(reflection_data.get(), reflection_size);
			}
#endif

			return true;
		}
		static Bool SaveToCache(Char const* cache_path, GfxShaderCompileOutput const& output)
		{
			std::string cache_metadata(cache_path); cache_metadata += ".meta";
			BinaryFileWriter metadata_writer(cache_metadata);
			if (!metadata_writer.IsValid()) 
			{
				return false;
			}

			metadata_writer.Write(output.shader_hash);
			metadata_writer.Write(output.includes);
			metadata_writer.Write(output.shader.GetSize());

#if defined(ADRIA_PLATFORM_MACOS)
			Uint64 reflection_size = output.shader.GetReflectionSize();
			metadata_writer.Write(reflection_size);
#endif

			std::string cache_binary(cache_path); cache_binary += ".bin";
			BinaryFileWriter binary_writer(cache_binary);
			if (!binary_writer.IsValid()) 
			{
				return false;
			}

			binary_writer.WriteBlob(output.shader.GetData(), output.shader.GetSize());

#if defined(ADRIA_PLATFORM_MACOS)
			if (reflection_size > 0)
			{
				binary_writer.WriteBlob(output.shader.GetReflectionData(), reflection_size);
			}
#endif

			return true;
		}

		void Initialize(GfxDevice* gfx)
		{
			ADRIA_ASSERT(gfx != nullptr);
			current_backend = gfx->GetBackend();

#if defined(ADRIA_PLATFORM_WINDOWS)
			std::string dxcompiler_path = "dxcompiler.dll";
#elif defined(ADRIA_PLATFORM_MACOS)
			std::string dxcompiler_path = "@executable_path/dxcompiler.dylib";
#elif defined(ADRIA_PLATFORM_LINUX)
			std::string dxcompiler_path = "libdxcompiler.so";
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
			IRRootParameter1 root_parameters[4] = {};
			root_parameters[0].ParameterType = IRRootParameterTypeCBV;
			root_parameters[0].ShaderVisibility = IRShaderVisibilityAll;
			root_parameters[0].Descriptor.ShaderRegister = 0;

			root_parameters[1].ParameterType = IRRootParameterType32BitConstants;
			root_parameters[1].ShaderVisibility = IRShaderVisibilityAll;
			root_parameters[1].Constants.ShaderRegister = 1;
			root_parameters[1].Constants.Num32BitValues = 8;

			root_parameters[2].ParameterType = IRRootParameterTypeCBV;
			root_parameters[2].ShaderVisibility = IRShaderVisibilityAll;
			root_parameters[2].Descriptor.ShaderRegister = 2;

			root_parameters[3].ParameterType = IRRootParameterTypeCBV;
			root_parameters[3].ShaderVisibility = IRShaderVisibilityAll;
			root_parameters[3].Descriptor.ShaderRegister = 3;

			IRStaticSamplerDescriptor static_samplers[10] = {};
			static_samplers[0].Filter = IRFilterMinMagMipLinear;
			static_samplers[0].AddressU = IRTextureAddressModeWrap;
			static_samplers[0].AddressV = IRTextureAddressModeWrap;
			static_samplers[0].AddressW = IRTextureAddressModeWrap;
			static_samplers[0].MipLODBias = 0.0f;
			static_samplers[0].MaxAnisotropy = 1;
			static_samplers[0].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[0].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[0].MinLOD = 0.0f;
			static_samplers[0].MaxLOD = 3.402823466e+38f; // FLT_MAX
			static_samplers[0].ShaderRegister = 0;
			static_samplers[0].RegisterSpace = 0;
			static_samplers[0].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[1].Filter = IRFilterMinMagMipLinear;
			static_samplers[1].AddressU = IRTextureAddressModeClamp;
			static_samplers[1].AddressV = IRTextureAddressModeClamp;
			static_samplers[1].AddressW = IRTextureAddressModeClamp;
			static_samplers[1].MipLODBias = 0.0f;
			static_samplers[1].MaxAnisotropy = 1;
			static_samplers[1].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[1].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[1].MinLOD = 0.0f;
			static_samplers[1].MaxLOD = 3.402823466e+38f;
			static_samplers[1].ShaderRegister = 1;
			static_samplers[1].RegisterSpace = 0;
			static_samplers[1].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[2].Filter = IRFilterMinMagMipLinear;
			static_samplers[2].AddressU = IRTextureAddressModeBorder;
			static_samplers[2].AddressV = IRTextureAddressModeBorder;
			static_samplers[2].AddressW = IRTextureAddressModeBorder;
			static_samplers[2].MipLODBias = 0.0f;
			static_samplers[2].MaxAnisotropy = 1;
			static_samplers[2].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[2].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[2].MinLOD = 0.0f;
			static_samplers[2].MaxLOD = 3.402823466e+38f;
			static_samplers[2].ShaderRegister = 2;
			static_samplers[2].RegisterSpace = 0;
			static_samplers[2].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[3].Filter = IRFilterMinMagMipPoint;
			static_samplers[3].AddressU = IRTextureAddressModeWrap;
			static_samplers[3].AddressV = IRTextureAddressModeWrap;
			static_samplers[3].AddressW = IRTextureAddressModeWrap;
			static_samplers[3].MipLODBias = 0.0f;
			static_samplers[3].MaxAnisotropy = 1;
			static_samplers[3].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[3].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[3].MinLOD = 0.0f;
			static_samplers[3].MaxLOD = 3.402823466e+38f;
			static_samplers[3].ShaderRegister = 3;
			static_samplers[3].RegisterSpace = 0;
			static_samplers[3].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[4].Filter = IRFilterMinMagMipPoint;
			static_samplers[4].AddressU = IRTextureAddressModeClamp;
			static_samplers[4].AddressV = IRTextureAddressModeClamp;
			static_samplers[4].AddressW = IRTextureAddressModeClamp;
			static_samplers[4].MipLODBias = 0.0f;
			static_samplers[4].MaxAnisotropy = 1;
			static_samplers[4].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[4].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[4].MinLOD = 0.0f;
			static_samplers[4].MaxLOD = 3.402823466e+38f;
			static_samplers[4].ShaderRegister = 4;
			static_samplers[4].RegisterSpace = 0;
			static_samplers[4].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[5].Filter = IRFilterMinMagMipPoint;
			static_samplers[5].AddressU = IRTextureAddressModeBorder;
			static_samplers[5].AddressV = IRTextureAddressModeBorder;
			static_samplers[5].AddressW = IRTextureAddressModeBorder;
			static_samplers[5].MipLODBias = 0.0f;
			static_samplers[5].MaxAnisotropy = 1;
			static_samplers[5].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[5].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[5].MinLOD = 0.0f;
			static_samplers[5].MaxLOD = 3.402823466e+38f;
			static_samplers[5].ShaderRegister = 5;
			static_samplers[5].RegisterSpace = 0;
			static_samplers[5].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[6].Filter = IRFilterComparisonMinMagMipLinear;
			static_samplers[6].AddressU = IRTextureAddressModeClamp;
			static_samplers[6].AddressV = IRTextureAddressModeClamp;
			static_samplers[6].AddressW = IRTextureAddressModeClamp;
			static_samplers[6].MipLODBias = 0.0f;
			static_samplers[6].MaxAnisotropy = 16;
			static_samplers[6].ComparisonFunc = IRComparisonFunctionLessEqual;
			static_samplers[6].BorderColor = IRStaticBorderColorOpaqueWhite;
			static_samplers[6].MinLOD = 0.0f;
			static_samplers[6].MaxLOD = 3.402823466e+38f;
			static_samplers[6].ShaderRegister = 6;
			static_samplers[6].RegisterSpace = 0;
			static_samplers[6].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[7].Filter = IRFilterComparisonMinMagMipLinear;
			static_samplers[7].AddressU = IRTextureAddressModeWrap;
			static_samplers[7].AddressV = IRTextureAddressModeWrap;
			static_samplers[7].AddressW = IRTextureAddressModeWrap;
			static_samplers[7].MipLODBias = 0.0f;
			static_samplers[7].MaxAnisotropy = 16;
			static_samplers[7].ComparisonFunc = IRComparisonFunctionLessEqual;
			static_samplers[7].BorderColor = IRStaticBorderColorOpaqueWhite;
			static_samplers[7].MinLOD = 0.0f;
			static_samplers[7].MaxLOD = 3.402823466e+38f;
			static_samplers[7].ShaderRegister = 7;
			static_samplers[7].RegisterSpace = 0;
			static_samplers[7].ShaderVisibility = IRShaderVisibilityAll;

			static_samplers[8].Filter = IRFilterMinMagMipLinear;
			static_samplers[8].AddressU = IRTextureAddressModeMirror;
			static_samplers[8].AddressV = IRTextureAddressModeMirror;
			static_samplers[8].AddressW = IRTextureAddressModeWrap;
			static_samplers[8].MipLODBias = 0.0f;
			static_samplers[8].MaxAnisotropy = 1;
			static_samplers[8].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[8].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[8].MinLOD = 0.0f;
			static_samplers[8].MaxLOD = 3.402823466e+38f;
			static_samplers[8].ShaderRegister = 8;
			static_samplers[8].RegisterSpace = 0;
			static_samplers[8].ShaderVisibility = IRShaderVisibilityAll;
			static_samplers[9].Filter = IRFilterMinMagMipPoint;
			static_samplers[9].AddressU = IRTextureAddressModeMirror;
			static_samplers[9].AddressV = IRTextureAddressModeMirror;
			static_samplers[9].AddressW = IRTextureAddressModeWrap;
			static_samplers[9].MipLODBias = 0.0f;
			static_samplers[9].MaxAnisotropy = 1;
			static_samplers[9].ComparisonFunc = IRComparisonFunctionNever;
			static_samplers[9].BorderColor = IRStaticBorderColorOpaqueBlack;
			static_samplers[9].MinLOD = 0.0f;
			static_samplers[9].MaxLOD = 3.402823466e+38f;
			static_samplers[9].ShaderRegister = 9;
			static_samplers[9].RegisterSpace = 0;
			static_samplers[9].ShaderVisibility = IRShaderVisibilityAll;

			IRVersionedRootSignatureDescriptor desc = {};
			desc.version = IRRootSignatureVersion_1_1;
			desc.desc_1_1.NumParameters = 4;
			desc.desc_1_1.pParameters = root_parameters;
			desc.desc_1_1.NumStaticSamplers = 10;
			desc.desc_1_1.pStaticSamplers = static_samplers;
			desc.desc_1_1.Flags = IRRootSignatureFlags(IRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed);

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
			static bool destroyed = false;
			if (destroyed) 
			{
				return;
			}
			destroyed = true;

#if defined(ADRIA_PLATFORM_MACOS)
			if (metal_root_signature)
			{
				IRRootSignatureDestroy(metal_root_signature);
				metal_root_signature = nullptr;
			}
			if (metal_ir_compiler)
			{
				IRCompilerDestroy(metal_ir_compiler);
				metal_ir_compiler = nullptr;
			}
#endif
			if (include_handler) include_handler.Reset();
			if (utils) utils.Reset();
			if (compiler) compiler.Reset();
			if (library) library.Reset();
		}

		Bool CompileShader(GfxShaderCompileInput const& input, GfxShaderCompileOutput& output)
		{
			std::string define_key = input.file;
			for (GfxShaderDefine const& define : input.defines)
			{
				define_key += define.name;
				define_key += define.value;
			}
			Uint64 define_hash = crc64(define_key.c_str(), define_key.size());
			std::string build_string = input.flags & GfxShaderCompilerFlag_Debug ? "debug" : "release";
			Char const* backend_suffix = "";
			switch (current_backend)
			{
			case GfxBackend::Vulkan: backend_suffix = "_vk";   break;
			case GfxBackend::Metal:  backend_suffix = "_mtl";  break;
			case GfxBackend::D3D12:  backend_suffix = "_dx12"; break;
			default: break;
			}
			Char cache_path[256];
			snprintf(cache_path, sizeof(cache_path), "%s%s_%s_%llx_%s%s", paths::ShaderCacheDir.c_str(), GetFilenameWithoutExtension(input.file).c_str(),
												     input.entry_point.c_str(), define_hash, build_string.c_str(), backend_suffix);

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
#if defined(ADRIA_PLATFORM_MACOS)
				compile_args.push_back(L"-Qembed_debug");
#endif
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
			defines.reserve(input.defines.size() + 1);

			if (current_backend == GfxBackend::Metal)
			{
				compile_args.push_back(L"-D");
				defines.push_back(L"GFX_METAL=1");
				compile_args.push_back(defines.back().c_str());
			}
			else if (current_backend == GfxBackend::D3D12)
			{
				compile_args.push_back(L"-D");
				defines.push_back(L"GFX_D3D12=1");
				compile_args.push_back(defines.back().c_str());
			}
			else if (current_backend == GfxBackend::Vulkan)
			{
				compile_args.push_back(L"-D");
				defines.push_back(L"GFX_VULKAN=1");
				compile_args.push_back(defines.back().c_str());
				compile_args.push_back(L"-spirv");
				compile_args.push_back(L"-fspv-target-env=vulkan1.3");
				compile_args.push_back(L"-fvk-use-dx-layout");
				compile_args.push_back(L"-fvk-use-scalar-layout");
				compile_args.push_back(L"-Vd");
				compile_args.push_back(L"-fspv-extension=SPV_KHR_ray_tracing");
				compile_args.push_back(L"-fspv-extension=SPV_KHR_ray_query");
				compile_args.push_back(L"-fspv-extension=SPV_EXT_descriptor_indexing");
				compile_args.push_back(L"-fspv-extension=SPV_EXT_mesh_shader");
				compile_args.push_back(L"-fspv-extension=SPV_KHR_physical_storage_buffer");
				compile_args.push_back(L"-fvk-s-shift");
				compile_args.push_back(L"0");
				compile_args.push_back(L"1");
			}
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
			
#if !defined(ADRIA_PLATFORM_MACOS)
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
#endif

			Ref<IDxcBlob> hash;
			if (SUCCEEDED(result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(hash.GetAddressOf()), nullptr)))
			{
				DxcShaderHash* hash_buf = (DxcShaderHash*)hash->GetBufferPointer();
				memcpy(output.shader_hash, hash_buf->HashDigest, sizeof(Uint64) * 2);
			}

#if defined(ADRIA_PLATFORM_MACOS)
			if (current_backend == GfxBackend::Vulkan)
			{
				std::vector<Uint8> spirv(blob->GetBufferSize());
				memcpy(spirv.data(), blob->GetBufferPointer(), spirv.size());
				PatchSpirvBindlessBindings(spirv.data(), spirv.size());
				output.shader.SetDesc(input);
				output.shader.SetShaderData(spirv.data(), spirv.size());
			}
			else
			{
			ADRIA_ASSERT(metal_ir_compiler && metal_root_signature);
			IRCompilerSetGlobalRootSignature(metal_ir_compiler, metal_root_signature);
			IRCompilerSetMinimumGPUFamily(metal_ir_compiler, IRGPUFamilyApple7);
			IRCompilerSetMinimumDeploymentTarget(metal_ir_compiler, IROperatingSystem_macOS, "15.0.0");

			if (input.flags & GfxShaderCompilerFlag_Debug)
			{
				IRCompilerIgnoreDebugInformation(metal_ir_compiler, false);
			}

			if (input.stage != GfxShaderStage::LIB)
			{
				IRCompilerSetEntryPointName(metal_ir_compiler, input.entry_point.empty() ? "main" : input.entry_point.c_str());
			}
			else
			{
				IRCompilerSetEntryPointName(metal_ir_compiler, "");
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
				ADRIA_LOG_SYNC(ERROR, "Failed to extract Metal library binary for shader");
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
				ADRIA_LOG_SYNC(ERROR, "Metal library binary has zero size");
				IRMetalLibBinaryDestroy(metallib);
				IRObjectDestroy(metal_ir_obj);
				if (rt_config)
				{
					IRRayTracingPipelineConfigurationDestroy(rt_config);
				}
				return false;
			}

			MetalShaderReflection reflection{};
			Bool has_threadgroup_info = false;
			IRShaderReflection* ir_reflection = IRShaderReflectionCreate();
			if (ir_reflection && IRObjectGetReflection(metal_ir_obj, ir_stage, ir_reflection))
			{
				if (ir_stage == IRShaderStageCompute)
				{
					IRVersionedCSInfo cs_info;
					if (IRShaderReflectionCopyComputeInfo(ir_reflection, IRReflectionVersion_1_0, &cs_info))
					{
						reflection.threadsPerThreadgroup[0] = cs_info.info_1_0.tg_size[0];
						reflection.threadsPerThreadgroup[1] = cs_info.info_1_0.tg_size[1];
						reflection.threadsPerThreadgroup[2] = cs_info.info_1_0.tg_size[2];
						IRShaderReflectionReleaseComputeInfo(&cs_info);
						has_threadgroup_info = true;
						ADRIA_LOG(INFO, "Compute shader threadgroup: [%u, %u, %u]",
							reflection.threadsPerThreadgroup[0],
							reflection.threadsPerThreadgroup[1],
							reflection.threadsPerThreadgroup[2]);
					}
				}
				else if (ir_stage == IRShaderStageMesh)
				{
					IRVersionedMSInfo ms_info;
					if (IRShaderReflectionCopyMeshInfo(ir_reflection, IRReflectionVersion_1_0, &ms_info))
					{
						reflection.threadsPerThreadgroup[0] = ms_info.info_1_0.num_threads[0];
						reflection.threadsPerThreadgroup[1] = ms_info.info_1_0.num_threads[1];
						reflection.threadsPerThreadgroup[2] = ms_info.info_1_0.num_threads[2];
						IRShaderReflectionReleaseMeshInfo(&ms_info);
						has_threadgroup_info = true;
					}
				}
				else if (ir_stage == IRShaderStageAmplification)
				{
					IRVersionedASInfo as_info;
					if (IRShaderReflectionCopyAmplificationInfo(ir_reflection, IRReflectionVersion_1_0, &as_info))
					{
						reflection.threadsPerThreadgroup[0] = as_info.info_1_0.num_threads[0];
						reflection.threadsPerThreadgroup[1] = as_info.info_1_0.num_threads[1];
						reflection.threadsPerThreadgroup[2] = as_info.info_1_0.num_threads[2];
						IRShaderReflectionReleaseAmplificationInfo(&as_info);
						has_threadgroup_info = true;
					}
				}

				IRShaderReflectionDestroy(ir_reflection);
			}

			std::vector<Uint8> shader_data;
			shader_data.resize(metallib_size);
			IRMetalLibGetBytecode(metallib, shader_data.data());

			IRMetalLibBinaryDestroy(metallib);
			IRObjectDestroy(metal_ir_obj);

			if (rt_config)
			{
				IRRayTracingPipelineConfigurationDestroy(rt_config);
			}

			output.shader.SetDesc(input);
			output.shader.SetShaderData(shader_data.data(), shader_data.size());
			output.shader.SetReflectionData(&reflection, sizeof(MetalShaderReflection));
			ADRIA_LOG(INFO, "Successfully converted DXIL to Metal IR for shader: %s", input.file.c_str());
			}
#else
			output.shader.SetDesc(input);
			if (current_backend == GfxBackend::Vulkan)
			{
				std::vector<Uint8> spirv(blob->GetBufferSize());
				memcpy(spirv.data(), blob->GetBufferPointer(), spirv.size());
				PatchSpirvBindlessBindings(spirv.data(), spirv.size());
				output.shader.SetShaderData(spirv.data(), spirv.size());
			}
			else
			{
				output.shader.SetShaderData(blob->GetBufferPointer(), blob->GetBufferSize());
			}
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

