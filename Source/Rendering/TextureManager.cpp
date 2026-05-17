#include "TextureManager.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Utilities/Image.h"
#include "Utilities/PathHelpers.h"


namespace adria
{

    TextureManager::TextureManager() {}
    TextureManager::~TextureManager() = default;

	void TextureManager::Initialize(GfxDevice* _gfx)
	{
        gfx = _gfx;
	}

	void TextureManager::Clear()
	{
		handle = TEXTURE_MANAGER_START_HANDLE;
		texture_srv_map.clear();
		texture_map.clear();
		loaded_textures.clear();
		is_scene_initialized = false;
	}

	void TextureManager::Shutdown()
	{
		Clear();
		gfx = nullptr;
	}

    TextureHandle TextureManager::LoadTexture(std::string_view path, Bool srgb)
    {
        std::string texture_name = NormalizePath(path);
        if (auto it = loaded_textures.find(texture_name); it == loaded_textures.end())
        {
			TextureHandle texture_handle = handle++;
            loaded_textures.insert({ texture_name, texture_handle });
            Image img(texture_name);

			GfxTextureDesc desc{};
			desc.type = img.Depth() > 1 ? GfxTextureType_3D : GfxTextureType_2D;
			desc.misc_flags = GfxTextureMiscFlag::None;
			desc.width = img.Width();
			desc.height = img.Height();
			desc.array_size = img.IsCubemap() ? 6 : 1;
			desc.depth = img.Depth();
			desc.bind_flags = GfxBindFlag::ShaderResource;
			desc.format = img.Format();
			desc.initial_state = GfxResourceState::AllSRV; 
			desc.heap_type = GfxResourceUsage::Default;
			desc.mip_levels = img.MipLevels();
            desc.misc_flags = img.IsCubemap() ? GfxTextureMiscFlag::TextureCube : GfxTextureMiscFlag::None;
			if (srgb)
			{
				desc.misc_flags |= GfxTextureMiscFlag::SRGB;
			}

            std::vector<GfxTextureSubData> tex_data;
			Image const* curr_img = &img;
			while (curr_img)
			{
				for (Uint32 i = 0; i < desc.mip_levels; ++i)
				{
                    GfxTextureSubData& data = tex_data.emplace_back();
					data.data = curr_img->MipData(i);
					data.row_pitch = GetRowPitch(curr_img->Format(), desc.width, i);
					data.slice_pitch = GetSlicePitch(img.Format(), desc.width, desc.height, i);
				}
                curr_img = curr_img->NextImage();
			}

			GfxTextureData init_data{};
			init_data.sub_data = tex_data.data();
			init_data.sub_count = (Uint32)tex_data.size();
            std::unique_ptr<GfxTexture> tex = gfx->CreateTexture(desc, init_data);

            texture_map[texture_handle] = std::move(tex);
			CreateViewForTexture(texture_handle);
			return texture_handle;
        }
	    else return it->second;
    }

	TextureHandle TextureManager::LoadCubemap(std::array<std::string, 6> const& cubemap_textures)
	{
		GfxTextureDesc desc{};
		desc.type = GfxTextureType_2D;
		desc.mip_levels = 1;
		desc.misc_flags = GfxTextureMiscFlag::TextureCube;
		desc.array_size = 6;
		desc.bind_flags = GfxBindFlag::ShaderResource;

		std::vector<Image> images{};
		std::vector<GfxTextureSubData> subresources;
		for (Uint32 i = 0; i < cubemap_textures.size(); ++i)
		{
			std::string normalized_path = NormalizePath(cubemap_textures[i]);
			images.emplace_back(normalized_path);
			GfxTextureSubData subresource_data{};
			subresource_data.data = images.back().Data<void>();
			subresource_data.row_pitch = GetRowPitch(images.back().Format(), desc.width, 0);
			subresources.push_back(subresource_data);
		}
		desc.width  = images[0].Width();
		desc.height = images[0].Height();
		desc.format = images[0].IsHDR() ? GfxFormat::R32G32B32A32_FLOAT : GfxFormat::R8G8B8A8_UNORM;

		GfxTextureData init_data{};
		init_data.sub_data = subresources.data();
		std::unique_ptr<GfxTexture> cubemap = gfx->CreateTexture(desc, init_data);

		TextureHandle texture_handle = handle++;
		texture_map.insert({ texture_handle, std::move(cubemap) });
		CreateViewForTexture(texture_handle);
		return texture_handle;
	}

	Uint32 TextureManager::GetBindlessIndex(TextureHandle handle) const
	{
		if (handle == INVALID_TEXTURE_HANDLE)
		{
			return Uint32(-1);
		}
		if (handle < TEXTURE_MANAGER_START_HANDLE)
		{
			return GfxCommon::GetCommonViewBindlessIndex(static_cast<GfxCommonViewType>(handle));
		}
		GfxDescriptor descriptor = GetDescriptor(handle);
		return descriptor.IsValid() ? gfx->GetBindlessDescriptorIndex(descriptor) : Uint32(-1);
	}

	GfxDescriptor TextureManager::GetDescriptor(TextureHandle tex_handle) const
	{
		if (handle == INVALID_TEXTURE_HANDLE)
		{
			return GfxDescriptor{};
		}
		if (auto it = texture_srv_map.find(tex_handle); it != texture_srv_map.end())
		{
			return it->second;
		}
		return GfxDescriptor{};
	}

	GfxTexture* TextureManager::GetTexture(TextureHandle handle) const
	{
		if (handle == INVALID_TEXTURE_HANDLE)
		{
			return nullptr;
		}
		else if (auto it = texture_map.find(handle); it != texture_map.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	void TextureManager::EnableMipMaps(Bool mips)
    {
        enable_mipmaps = mips;
    }

	void TextureManager::OnSceneInitialized()
	{
		for (Uint64 i = TEXTURE_MANAGER_START_HANDLE; i <= handle; ++i)
        {
            GfxTexture* texture = texture_map[TextureHandle(i)].get();
            if (texture)
            {
                CreateViewForTexture(TextureHandle(i), true);
            }
        }
        is_scene_initialized = true;
	}

	void TextureManager::CreateViewForTexture(TextureHandle handle, Bool flag)
	{
		if (!is_scene_initialized && !flag)
		{
			return;
		}

		GfxTexture* texture = texture_map[handle].get();
		ADRIA_ASSERT(texture);
        texture_srv_map[handle] = gfx->CreateTextureSRV(texture);
 	}
}
