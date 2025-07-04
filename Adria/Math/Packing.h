#pragma once

namespace adria
{
	Uint32 PackToUint(Float r, Float g, Float b, Float a = 1.0f);
	Uint32 PackToUint(Float(&arr)[3]);

	Uint32 PackTwoFloatsToUint32(Float x, Float y);
	Uint64 PackFourFloatsToUint64(Float x, Float y, Float z, Float w);

	Uint32 PackTwoUint16ToUint32(Uint16 value1, Uint16 value2);
}