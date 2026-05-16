<img align="center" padding="2" width="75%" src="Adria/Resources/Icons/adria_logo_wide2.png"/>

Modern cross-platform graphics engine written in C++.

## Rendering Backends

| Backend | Platforms | Status |
|---|---|---|
| Direct3D 12 | Windows | Stable |
| Metal | macOS | Stable |
| Metal 4 | macOS | Planned |
| Vulkan | Windows | Experimental |
| Vulkan (MoltenVK) | macOS | Experimental |
| Vulkan | Linux | Planned |

## Features

* Render graph
    - Automatic barriers and resource state tracking
    - Transient resource pooling and reuse
    - Automatic bind-flag and initial-state deduction
    - Async compute scheduling

* GPU-driven renderer
    - GPU frustum culling and two-phase GPU occlusion culling
    - Indirect rendering
    - Ultimate bindless resource model

* Global illumination and ray tracing
    - DDGI, ReSTIR DI (WIP), reference path tracer
    - Ray-traced shadows, reflections, and ambient occlusion 

* Deferred rendering pipeline
    - Tiled and clustered lighting
    - PCF shadow maps for directional, spot, and point lights
    - Cascaded shadow maps for directional lights
    - SSR, SSAO, HBAO
    - Volumetric raymarching and fog volumes

* Atmosphere and environment rendering
    - Volumetric clouds
    - Hosek-Wilkie sky
    - FFT ocean
    - Rain

* Upscaling and image reconstruction
    - DLSS 3.5, XeSS2, FSR2, FSR3, MetalFX

* Post-processing and image effects
    - TAA, FXAA, bloom, automatic exposure
    - Variable rate shading, CAS, depth of field
    - Film grain, vignette, chromatic aberration, lens distortion, CRT filter
    - Texture-based and procedural lens flare

* Editor and visualization tools
    - Entity picking with selection silhouettes and transform gizmos
    - Debug renderer
    - Debug outputs: diffuse, normal, depth, roughness, metallic, emissive, AO, GI, custom, shading extension, mipmap view, triangle overdraw, material ID, meshlet ID, motion vectors

* Tooling and debugging
    - Shader hot reload
    - GPU printf and GPU assert
    - Render graph Graphviz visualization
    - PIX and RenderDoc programmatic APIs
    - Nsight Aftermath SDK, Nsight Perf SDK
    - Custom profiler and Tracy integration

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

### Editor
![](Adria/Saved/Screenshots/editor2.png "Editor") 

### Nsight Perf HUD
![](Adria/Saved/Screenshots/nsightperf.png "Nsight Perf HUD") 

### Render Graph Visualization
![](Adria/Saved/RenderGraph/rendergraph.svg "Render graph visualization") 




