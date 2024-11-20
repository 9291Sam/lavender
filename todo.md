
async chunk meshing  thread chunk remeshing https://x.com/FindingFortune1/status/1784419629827248131
voxel transactrions
animations
Add dense brick optimization
allow for enabling of assertions at startup (assert and debug seperate)
auto creation of chunks in the world
voxel groups
entity system
physics
LODs
    Map<ChunkCorner, Chunk {Lod, BrickmapPtr}>
    to find a chunk get a world position and then if its chunk doesnt exist
    keep going up in LOD levels untill you find a chunk
three tiered abstraction for voxel writes
voxel::World
voxel::***** // does synchronization, takes in lists in world space and then divvies it up by chunk
voxel::ChunkManager // takes in vertex buffers and just organizes them and calls the commands
immutable components
InherentEntityComponent

timings of whether or not we're CPU or GPU bottlenecked

profiling on imgui via tracing
render thread & worker threads

asperta
aperita
asperita
(it literally came to me...)
