Game roadmap:
- Add basic vehicle
- Add basic spherical world
- Import vehicle model and setup physics (ignore vehicle shape for now)

Engine roadmap:
- [partly done] optimize tiled lighting or implement clustered
- [partly done] FBX mesh export
- Proper deletion of entities loaded from resource
- export default materials from maya (with texture paths)
- Add support for singleton components (store in EntityManager by pointer)
- Proper alignment of ECS components, remove packing
- physics rendering interpolation
- Fix logging, log from multiple threads
- Fix and migrate lighting, decals, shadowmaps to ECS
- HLSL includes must be included in hash
- Image based lighting support
- Handles for all engine resources
- New skinning animation system
- Use pipelines directly
- Shader reloading
- dds loading
- Multithreaded rendering
- GUI library integration

Check:
- Shader compiles / loads every time?
- postprocess, changed HLSL texture binding but C++ side didn't see it changed

-- Completed -----------------------------------

+ Material resource
+ Multithreading, job system
+ Async resource loading
+ Fix release build settings in cmake
+ lambda API for ECS
+ PhysX integration 
+ Rendering of MultiMesh
+ Extract assets into a private repo
+ FBX physics mesh export
+ GameObject file format