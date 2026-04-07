<img align="center" padding="2" src="Adria/Resources/Icons/adria_logo_wide2.png"/>

Graphics engine written in C++ with DirectX12 and Metal backends.

## Backends
* **DirectX12** (Windows) 
* **Metal** (macOS) 

## Features
* Render graph
    - Automatic resource barriers
    - Resource reuse using resource pool
    - Automatic resource bind flags and initial state deduction
    - Async Compute
* Ultimate Bindless resource binding
* DDGI
* GPU-Driven Rendering : GPU frustum culling + 2 phase GPU occlusion culling
* Reference path tracer
* Upscalers : FSR2, FSR3, XeSS2, DLSS3.5, MetalFX
* Volumetric lighting: Raymarching, Fog volumes
* Tiled/Clustered deferred rendering 
* ReSTIR DI (wip)
* Shadows
    - PCF shadow maps for directional, spot and point lights
	- Cascade shadow maps for directional lights
    - Ray traced shadows (DXR)
* Volumetric clouds, Hosek-Wilkie sky, Rain
* FFT Ocean
* Automatic exposure
* Bloom
* Ambient occlusion: SSAO, HBAO, RTAO (DXR)
* Reflections: SSR, RTR (DXR)
* Antialiasing: FXAA, TAA
* FFX: Variable Rate Shading, Contrast Adaptive Sharpening, Depth of Field 
* Entity picking with selection silhouettes and transform gizmos
* Film effects: Lens distortion, Chromatic aberration, Vignette, Film grain, CRT filter
* Lens flare: texture-based and procedural
* Profiler: custom and tracy profiler
* Debug tools
    - Debug renderer
    - Shader hot reloading
    - Render graph graphviz visualization
    - GPU printf, GPU assert
    - PIX and RenderDoc programmatic APIs
    - Nsight Aftermath SDK, Nsight Perf SDK
    - Debug Outputs: Diffuse, Normal, Depth, Roughness, Metallic, Emissive, AO, GI, \
      Custom, Shading Extension, View Mipmaps, Triangle Overdraw, Material and Meshlet ID, Motion Vectors

## Screenshots

### DDGI

| Disabled |  Enabled |
|---|---|
|  ![](Adria/Saved/Screenshots/noddgi.png) | ![](Adria/Saved/Screenshots/ddgi.png) |

| Probe Visualization |
|---|
|  ![](Adria/Saved/Screenshots/ddgi_probes1.png) |

### Volumetric Clouds
![](Adria/Saved/Screenshots/clouds.png "Volumetric clouds") 

### San Miguel
![](Adria/Saved/Screenshots/sanmiguel.png "San Miguel") 
![](Adria/Saved/Screenshots/sanmiguel2.png "San Miguel") 

### Bistro
![](Adria/Saved/Screenshots/bistro_rain.png "Rainy Bistro") 

### New Sponza
![](Adria/Saved/Screenshots/newsponza.png "New Sponza") 

### Sun Temple
![](Adria/Saved/Screenshots/suntemple.png "Sun Temple") 

### Brutalism Hall
![](Adria/Saved/Screenshots/brutalism.png "Brutalism Hall") 

### Ocean
![](Adria/Saved/Screenshots/ocean.png "Ocean") 

### Path Tracer
![](Adria/Saved/Screenshots/pathtracing1.png "Path traced Sponza") 
![](Adria/Saved/Screenshots/arcade.png "Path traced Arcade") 

### Ray Tracing Features

| Cascaded Shadow Maps |  Hard Ray Traced Shadows |
|---|---|
|  ![](Adria/Saved/Screenshots/cascades.png) | ![](Adria/Saved/Screenshots/raytraced.png) |

| Screen Space Reflections |  Ray Traced Reflections |
|---|---|
|  ![](Adria/Saved/Screenshots/ssr.png) | ![](Adria/Saved/Screenshots/rtr.png) |

| SSAO | RTAO |
|---|---|
|  ![](Adria/Saved/Screenshots/ssao.png) | ![](Adria/Saved/Screenshots/rtao.png) |

### Triangle Overdraw
![](Adria/Saved/Screenshots/bistrooverdraw.png "Bistro Triangle Overdraw") 

### Transparent Objects
![](Adria/Saved/Screenshots/transparent.png "Transparent Water") 

### Editor
![](Adria/Saved/Screenshots/editor2.png "Editor") 

### Nsight Perf HUD
![](Adria/Saved/Screenshots/nsightperf.png "Nsight Perf HUD") 

### Render Graph Visualization
![](Adria/Saved/RenderGraph/rendergraph.svg "Render graph visualization") 




