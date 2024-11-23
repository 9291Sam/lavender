Have you ever played a game like [Barony](https://store.steampowered.com/app/371970/Barony/) before? It's a lot of fun however it's extremely narrow in scope and not fun at all to play on your own. Verdigris, from a gameplay perspective, could be best described as a combination of the magic system of Barony with dynamic gameplay experience of modded Minecraft. More specifically the mods [Roguelike Dungeons](https://www.curseforge.com/minecraft/modpacks/roguelike-adventures-and-dungeons-2),  [The Twilight Forest](https://www.curseforge.com/minecraft/mc-mods/the-twilight-forest), and [Biomes O' Plenty](https://www.curseforge.com/minecraft/mc-mods/biomes-o-plenty)

Verdigris' initial gameplay loop is generally very simple:
1. Spawn into the world
2. Explore through the world to find a dungeon to loot (~30 / 45m)
3. Loot a dungeon (~1hr / 8hrs)
4. Return back to your homebase where you collect your loot together and make upgraded weapons and tools


While simple, this gameplay look should be sufficiently complex enough for an MVP and ripe for expansion with some simple possibilities listed below
- Creation of new Magic Items
- Exploring the world for resources
- Traveling to towns and cities to enhance magical abilities and/or sell items
- More dimensions / planes of existence

All gameplay will take place in a voxelized world that extends out to the horizon with minimal visual artifacts. Some type of global illumination must be an option for users with a high end graphics card, however the entire game should be runnable at minimum graphical settings at 30 FPS without global illumination on integrated graphics. 


MVP tasks:
1. Rendering of a single 512^3 chunk at >60 fps with direct and indirect lighting
2. Optimizing that single chunk so that it can be used in larger and larger LODs (for now just use Perlin or simplex noise) Target a visible area of 12k ^ 3 to start
3. Modify the player so that they can place / remove any voxel at any point in the world in a few different manners. Single, Sphere, Cube
4. ---- Graphics Checkpoint  ---- 
	- At this point you should have a fully dynamic world in which and voxels can be placed in and removed from in any order
5. Create a simple physics system which interacts with the player, the world, free voxels, and entities
6. Begin the creation of a simple magic system. Implement something like fireball and something that causes and explosion at a distance (Focus on visual, collision and damage support)   
7. Create simple enemies that fire these randomly into the world and optimize them so that you can spawn about ~1000 of them before any noticeable lag occurs
8. ---- Gameplay Checkpoint 1 ----
	- At this point it should be possible to walk around in the world, and shoot some simple spells at enemies, have them shoot some back at you, you can melee attack them and vice versa.
9. Begin working on changing world generation to be more interesting, add about ~10 simple biomes
10. Work on player interaction with this world, add tools and the ability to interact with the world with them
11. Add droppable voxels and an inventory system (bag of holding?)
12. ---- Gameplay Checkpoint 2 ----
	- At this point it should be possible to walk around and explore an open world, build anything you want, interact with some simple monsters
13. Add more monsters and 3 initial dungeons to explore
	1. Add a [Hydra's Lair](https://ftbwiki.org/File:Hydra_in_its_lair.png)
		- It's an open half dome, fireball is required to singe the heads
	2. Add a Wvyern's mountain
		- It's a mountain with a wyvern atop it. If you get too close it swoops down and tries to attack you, there is a loot hoard at the base
	3. Add a Catacombs 
		- It's underground, with a small entrance
1. Add simple towns and simple merchants, able to sell items you've made / looted and buy items
2. Add more biomes with a goal of around ~35 different ones
3. ---- MVP Checkpoint ---- 
	- At this point you should have a game. Show it off, ask others for their opinions. 



wandering trader


https://discord.com/channels/331718482485837825/851121440425639956/1257849161160724552


Tasks:
    Better Culling (douglas...)
    Editable World
    Make A UI / Spell
    




Game ui
breaking and placing of blocks in the world
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


Voxel ray tracing

combined images

make Player blessed to continue moving when game ticking is stalled

make a buffer upload abstraction to test how much data can be uploaded per frame consistently

Add views of cpu and gpu memory usage in debug menu

Add toggles for validation layers

Show logged messages to stderr, and log them to a file

event system (set player position)
command line arguments test and loggigng

seperate raster and rt gpu threads?

seperate toggles for verdigris validation and vulkan validation

bad apple as a tech demo

proper argument parser (console commands and command line args)\

track where the player is moving and if theyre going to need to completely reallocate where the octree is centered do that 
at the same time this also allows for you to deal with a floating origin just ifne 

add etst in the raycasting shader to cehck if that pixel is the ceter of teh screen if so then you can put that in a buffer that the cpu reads tosee were the center of the world is 




cam you suet eh dedicated transfer hardware on the gpu i.e a queue with only Transfer

to upload the brick informatio nso that you dotn have to store it on the cpu side at all? 

i.e you can return some type of astynchronous future request for it that gets populated at the end of each command buffer submission (follow the barrier) 

fix the cmake script lmfao



add returning of values from util::Mutex lambdas!!! next thing!


traversal of 512x512 volume

entire world try different types
each chunk as a drawcalls, octree, etc...


take a break from memory stuff and work on graphics effects Real Time Rendering

migrate to sync2 and many of the other 2 functions

merge raster and rt image

hack brickmap together

add dynamic reallocing

shader + compute renderer rewrite

refactor pipeline to be a similar dual inheritance thing

move menu to be not special

central predeclaration file