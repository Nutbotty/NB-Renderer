# NBPT

A PBR GPU compute shader path tracing application written in C++ and Slang.

This project is a practical application of the modern offline rendering framework outlined by the PBRT textbook.
It is written in  c++ and Slang, and supports OpenGL, Vulkan, and DirectX12


## Features

- Real-time path traced scene editor
- Progressive sample accumulation
- Physically based metallic-roughness materials
- GGX microfacet reflection
- GGX VNDF importance sampling
- Texture support
- Sphere, triangle, and quad primitives
- glTF model loading
- Meshes and mesh instancing
- Editable material assignments
- Custom BVH with TLAS and BLAS acceleration structures
- Iterative GPU BVH traversal
- HDR environment lighting
- Depth of field
- SSAA
- Offline/final rendering at configurable resolution, sample count, and bounce depth

The openGL implementation exists as an exercise to understand and work directly with bvh acceleration structures.
The vulkan implementation will make use of the KHR extensions.

## TODO

- Add Vulkan rendering path using Vulkan acceleration structures
- Change from median split bvh to an SAH BVH construction
- Normal maps
- Next event estimation]
- Clean up main
- Restructure shader pipeline to support spectral rendering
- Add more debug features/performance metrics
- Implement DirectX 12 renderer

## References
### Texbooks and Implementation Resources

- [Physically Based Rendering: From Theory to Implementation](https://pbr-book.org/)
- [Ray Tracing in One Weekend Series](https://raytracing.github.io/books/RayTracingInOneWeekend.html) — Peter Shirley, Trevor David Black, Steve Hollasch
- [LearnOpenGL](https://learnopengl.com/)
- [Cycles Renderer Documentation](https://developer.blender.org/docs/features/cycles/) — Blender Foundation

### Papers
- [Ray Tracing Deformable Scenes using
  Dynamic Bounding Volume Hierarchies](https://www.sci.utah.edu/~wald/Publications/2007/BVH/download/togbvh.pdf)
-[Enterprise PBR Shading Model](https://dassaultsystemes-technology.github.io/EnterprisePBRShadingModel/spec-2025x.md.html#components)

### Models

- [Skull 3D Model](https://free3d.com/nl/3d-model/skull-v3--785914.html?dd_referrer=https%3A%2F%2Fthreejs3d.com%2F)


