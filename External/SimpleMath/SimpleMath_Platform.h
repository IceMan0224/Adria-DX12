//-------------------------------------------------------------------------------------
// SimpleMath_Platform.h -- Platform compatibility definitions for SimpleMath
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#pragma once

// Define Windows types for non-Windows platforms
#if !defined(_WIN32) && !defined(WINAPI_FAMILY)

#include <cstdint>

// Windows RECT structure
typedef struct tagRECT {
    long left;
    long top;
    long right;
    long bottom;
} RECT;

// Windows UINT type
typedef unsigned int UINT;
typedef long LONG;

// DXGI types that may be referenced but not actually used on non-Windows platforms
#ifndef DXGI_SCALING
typedef enum DXGI_SCALING {
    DXGI_SCALING_STRETCH = 0,
    DXGI_SCALING_NONE = 1,
    DXGI_SCALING_ASPECT_RATIO_STRETCH = 2
} DXGI_SCALING;
#endif

#endif // !_WIN32 && !WINAPI_FAMILY
