#include "Heightmap.h"
#include "Image.h"
#include "Cpp/FastNoiseLite.h"

namespace adria
{
	constexpr FastNoiseLite::NoiseType GetNoiseType(NoiseType type)
	{
		switch (type)
		{
		case NoiseType::OpenSimplex2:
			return FastNoiseLite::NoiseType_OpenSimplex2;
		case NoiseType::OpenSimplex2S:
			return FastNoiseLite::NoiseType_OpenSimplex2S;
		case NoiseType::Cellular:
			return FastNoiseLite::NoiseType_Cellular;
		case NoiseType::ValueCubic:
			return FastNoiseLite::NoiseType_ValueCubic;
		case NoiseType::Value:
			return FastNoiseLite::NoiseType_Value;
		case NoiseType::Perlin:
		default:
			return FastNoiseLite::NoiseType_Perlin;
		}

		return FastNoiseLite::NoiseType_Perlin;
	}
	constexpr FastNoiseLite::FractalType GetFractalType(FractalType type)
	{
		switch (type)
		{
		case FractalType::FBM:
			return FastNoiseLite::FractalType_FBm;
		case FractalType::Ridged:
			return FastNoiseLite::FractalType_Ridged;
		case FractalType::PingPong:
			return FastNoiseLite::FractalType_PingPong;
		case FractalType::None:
		default:
			return FastNoiseLite::FractalType_None;
		}

		return FastNoiseLite::FractalType_None;
	}

	Heightmap::Heightmap(HeightmapDesc const& desc)
	{
		FastNoiseLite noise{};
		noise.SetFractalType(GetFractalType(desc.fractal_type));
		noise.SetSeed(desc.seed);
		noise.SetNoiseType(GetNoiseType(desc.noise_type));
		noise.SetFractalOctaves(desc.octaves);
		noise.SetFractalLacunarity(desc.lacunarity);
		noise.SetFractalGain(desc.persistence);
		noise.SetFrequency(0.1f);
		heightmap.resize(desc.depth);

		for (Uint32 z = 0; z < desc.depth; z++)
		{
			heightmap[z].resize(desc.width);
			for (Uint32 x = 0; x < desc.width; x++)
			{

				Float xf = x * desc.noise_scale / desc.width; 
				Float zf = z * desc.noise_scale / desc.depth; 

				Float total = noise.GetNoise(xf, zf);

				heightmap[z][x] = total * desc.max_height;
			}
		}
	}
	Heightmap::Heightmap(std::string_view heightmap_path)
	{
		Image img(heightmap_path);
		Uint32 w = img.Width();
		Uint32 h = img.Height();

		heightmap.resize(h);
		Uint8 const* pixels = img.Data<Uint8>();
		Bool is_hdr = img.IsHDR();

		for (Uint32 z = 0; z < h; z++)
		{
			heightmap[z].resize(w);
			for (Uint32 x = 0; x < w; x++)
			{
				if (is_hdr)
				{
					Float const* fp = reinterpret_cast<Float const*>(pixels);
					heightmap[z][x] = fp[(z * w + x) * 4];
				}
				else
				{
					Uint32 idx = (z * w + x) * 4;
					Float value = pixels[idx] / 255.0f;
					if (img.Format() == GfxFormat::R16_UNORM || img.Format() == GfxFormat::R16_FLOAT)
					{
						Uint16 const* p16 = reinterpret_cast<Uint16 const*>(pixels);
						value = p16[z * w + x] / 65535.0f;
					}
					heightmap[z][x] = value;
				}
			}
		}
	}
	Float Heightmap::HeightAt(Uint64 x, Uint64 z) const
	{
		return heightmap[z][x];
	}
	Uint64 Heightmap::Width() const
	{
		return heightmap[0].size();
	}
	Uint64 Heightmap::Depth() const
	{
		return heightmap.size();
	}
}