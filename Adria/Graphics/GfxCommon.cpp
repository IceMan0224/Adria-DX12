#include "GfxCommon.h"
#include "GfxDevice.h"
#include "GfxTexture.h"
#include "D3D12/D3D12DescriptorAllocator.h"
#include "D3D12/D3D12Conversions.h"
#include "D3D12/D3D12Device.h"

namespace adria
{
	namespace gfxcommon
	{
		using enum GfxCommonTextureType;
		namespace
		{
			Bool initialized = false;
			std::array<std::unique_ptr<GfxTexture>, (Uint64)Count>	common_textures;
			std::unique_ptr<D3D12DescriptorHeap> common_views_heap;

			void CreateCommonTextures(GfxDevice* gfx)
			{
				GfxTextureDesc desc{};
				desc.width = 1;
				desc.height = 1;
				desc.format = GfxFormat::R8G8B8A8_UNORM;
				desc.bind_flags = GfxBindFlag::ShaderResource;
				desc.initial_state = GfxResourceState::PixelSRV;

				GfxTextureSubData init_data{};
				GfxTextureData data{};
				data.sub_data = &init_data;
				data.sub_count = 1;

				Uint8 white[] = { 0xff, 0xff, 0xff, 0xff };
				init_data.data = white;
				init_data.row_pitch = sizeof(white);
				init_data.slice_pitch = 0;
				common_textures[(Uint64)WhiteTexture2D] = gfx->CreateTexture(desc, data);
				
				Uint8 black[] = { 0x00, 0x00, 0x00, 0xff };
				init_data.data = black;
				init_data.row_pitch = sizeof(black);
				common_textures[(Uint64)BlackTexture2D] = gfx->CreateTexture(desc, data);

				GfxTextureDesc default_normal_desc{};
				default_normal_desc.width = 1;
				default_normal_desc.height = 1;
				default_normal_desc.format = GfxFormat::R8G8B8A8_UNORM;
				default_normal_desc.bind_flags = GfxBindFlag::ShaderResource;
				default_normal_desc.initial_state = GfxResourceState::PixelSRV;

				Uint8 default_normal[] = { 0x7f, 0x7f, 0xff, 0xff };
				init_data.data = default_normal;
				init_data.row_pitch = sizeof(default_normal);
				init_data.slice_pitch = 0;
				common_textures[(Uint64)DefaultNormal2D] = gfx->CreateTexture(default_normal_desc, data);

				GfxTextureDesc metallic_roughness_desc{};
				metallic_roughness_desc.width = 1;
				metallic_roughness_desc.height = 1;
				metallic_roughness_desc.format = GfxFormat::R8G8B8A8_UNORM;
				metallic_roughness_desc.bind_flags = GfxBindFlag::ShaderResource;
				metallic_roughness_desc.initial_state = GfxResourceState::PixelSRV;

				Uint8 metallic_roughness[] = { 0xff, 0x7f, 0x00, 0xff };
				init_data.data = metallic_roughness;
				init_data.row_pitch = sizeof(metallic_roughness);
				init_data.slice_pitch = 0;
				common_textures[(Uint64)MetallicRoughness2D] = gfx->CreateTexture(metallic_roughness_desc, data);
			}

			void CreateCommonViews(GfxDevice* gfx)
			{
				using enum GfxCommonViewType;

				if (gfx->GetBackend() == GfxBackend::D3D12)
				{
					D3D12Device* device = (D3D12Device*)gfx;
					ID3D12Device* d3d12_device = device->GetD3D12Device();

					D3D12DescriptorHeapDesc desc{};
					desc.type = GfxDescriptorType::CBV_SRV_UAV;
					desc.shader_visible = false;
					desc.descriptor_count = (Uint64)Count;

					common_views_heap = device->CreateDescriptorHeap(desc);

					D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
					null_srv_desc.Texture2D.MostDetailedMip = 0;
					null_srv_desc.Texture2D.MipLevels = -1;
					null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

					null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
					null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

					d3d12_device->CreateShaderResourceView(nullptr, &null_srv_desc, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)NullTexture2D_SRV)));
					null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					d3d12_device->CreateShaderResourceView(nullptr, &null_srv_desc, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)NullTextureCube_SRV)));
					null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					d3d12_device->CreateShaderResourceView(nullptr, &null_srv_desc, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)NullTexture2DArray_SRV)));

					D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
					null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
					d3d12_device->CreateUnorderedAccessView(nullptr, nullptr, &null_uav_desc, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)NullTexture2D_UAV)));

					GfxDescriptor white_srv = gfx->CreateTextureSRV(common_textures[(Uint64)WhiteTexture2D].get());
					GfxDescriptor black_srv = gfx->CreateTextureSRV(common_textures[(Uint64)BlackTexture2D].get());
					GfxDescriptor default_normal_srv = gfx->CreateTextureSRV(common_textures[(Uint64)DefaultNormal2D].get());
					GfxDescriptor metallic_roughness_srv = gfx->CreateTextureSRV(common_textures[(Uint64)MetallicRoughness2D].get());

					d3d12_device->CopyDescriptorsSimple(1, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)WhiteTexture2D_SRV)), DecodeToD3D12CPUHandle(white_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					d3d12_device->CopyDescriptorsSimple(1, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)BlackTexture2D_SRV)), DecodeToD3D12CPUHandle(black_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					d3d12_device->CopyDescriptorsSimple(1, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)DefaultNormal2D_SRV)), DecodeToD3D12CPUHandle(default_normal_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					d3d12_device->CopyDescriptorsSimple(1, ToD3D12CPUHandle(common_views_heap->GetDescriptor((Uint64)MetallicRoughness2D_SRV)), DecodeToD3D12CPUHandle(metallic_roughness_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
			}
		}

		void Initialize(GfxDevice* gfx)
		{
			if (initialized)
			{
				return;
			}
			CreateCommonTextures(gfx);
			CreateCommonViews(gfx);
			initialized = true;
		}

		void Destroy()
		{
			common_views_heap.reset();
			for (auto& texture : common_textures) texture.reset();
		}

		GfxTexture* GetCommonTexture(GfxCommonTextureType type)
		{
			return common_textures[(Uint64)type].get();
		}

		GfxDescriptor GetCommonView(GfxCommonViewType type)
		{
			return EncodeFromD3D12Descriptor(common_views_heap->GetDescriptor((Uint64)type));
		}

	}
}

