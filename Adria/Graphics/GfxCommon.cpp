#include "GfxCommon.h"
#include "GfxDevice.h"
#include "GfxTexture.h"

namespace adria
{
	namespace GfxCommon
	{
		using enum GfxCommonTextureType;
		namespace
		{
			Bool initialized = false;
			GfxDevice* gfx = nullptr;
			std::array<std::unique_ptr<GfxTexture>, (Uint64)GfxCommonTextureType::Count>	common_textures;
			std::array<GfxDescriptor, (Uint64)GfxCommonViewType::Count>	common_views;

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

				Uint8 black[] = { 0x00, 0x00, 0x00, 0xff };
				init_data.data = black;
				init_data.row_pitch = sizeof(black);
				common_textures[(Uint64)BlackTexture2D] = gfx->CreateTexture(desc, data);

				Uint8 white[] = { 0xff, 0xff, 0xff, 0xff };
				init_data.data = white;
				init_data.row_pitch = sizeof(white);
				common_textures[(Uint64)WhiteTexture2D] = gfx->CreateTexture(desc, data);

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
				common_views[(Uint64)BlackTexture2D_SRV] = gfx->CreateTextureSRV(common_textures[(Uint64)BlackTexture2D].get());
				common_views[(Uint64)WhiteTexture2D_SRV] = gfx->CreateTextureSRV(common_textures[(Uint64)WhiteTexture2D].get());
				common_views[(Uint64)DefaultNormal2D_SRV] = gfx->CreateTextureSRV(common_textures[(Uint64)DefaultNormal2D].get());
				common_views[(Uint64)MetallicRoughness2D_SRV] = gfx->CreateTextureSRV(common_textures[(Uint64)MetallicRoughness2D].get());
			}
		}

		void Initialize(GfxDevice* _gfx)
		{
			if (initialized)
			{
				return;
			}

			gfx = _gfx;
			CreateCommonTextures(gfx);
			CreateCommonViews(gfx);
			initialized = true;
		}

		void Destroy()
		{
			for (auto& texture : common_textures)
			{
				texture.reset();
			}
		}

		GfxTexture* GetCommonTexture(GfxCommonTextureType type)
		{
			return common_textures[(Uint64)type].get();
		}

		GfxDescriptor GetCommonView(GfxCommonViewType type)
		{
			return common_views[(Uint64)type];
		}

		Uint32 GetCommonViewBindlessIndex(GfxCommonViewType type)
		{
			GfxDescriptor common_view_descriptor = GetCommonView(type);
			return gfx->GetBindlessDescriptorIndex(common_view_descriptor);
		}

	}
}

