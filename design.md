# The Sacrificed of Verdant

## Story

You had always heard of the stories of dragons swooping down and burning people,
you know that it *happened* but you never really thought it would happen to you.

Well, one day, one entirely normal and unremarkable day, while you were 
traveling between towns you get jumped. These looked to be normal bandits, they
probably just wanted some of your money and would gladly take some in exchange
for your life, but as you were preparing to surrender you see your vision start
to haze, and then the dreaded purple star like shapes come in, crap, you're
being put to sleep.

The next thing you know you're in the back of a carriage and are being moved
to [GOD_NAME]'s knows where. Eventually, after a shockingly short voyage, you 
see out of the back of the carriage that you're entering the base of a mountain.
At this your chest goes cold. There's only one mountain nearby and it's rumored 
to be the spot of the largest dragon hoard in the known world.

After a few minutes you're thrown out of the back of the carriage and the dragon
that was half in myth was suddenly right in front of you, frothing.

"Oh great [DRAGON_NAME] we've come with this offering of your greatness, in
exchange would you please improve our magical abilities"

'SHIT, they're exchanging me!' 

The dragon stops and stares for a few seconds before looking right at you and
a golden hazy beam emits from between it's eyeballs. You brace for impact,
however at the last moment it sails right over you and you presume it hit one
of your kidnappers.

After that you hear them excitedly scurry away, and the dragon looks at you,
alone. It speaks in your language with a hauntingly beautiful voice: "You're 
different, you're useful..., oh! no matter!" #CUT_TO_BLACK

[Groggy Intermissions, you're floating in a tank of some type]

Eventually, you start to notice that the other groggy tanks near you are opening
and shattering one by one. Your tank is the last to shatter and you're ejected
onto the floor. Wet, slimy, hungry, thirsty, and cold. You don't know what
happened, the last thing you remember was the carriage ride, but that was so
long ago, it's been centuries.

Now here you are, in a world your attachment to was ripped away. You're enraged
and hungry for vengeance.

Your mission: Track down the thing that has wronged you, destroyed your life
and destroy it in kind. Destroy the dragon, destroy the bandits (not bandits)
anymore...

[Control to user]
In the room is a single dead body, it lacks wounds, wearing a simple red tunic,
a gem key, and a wand with a few charges (3 paralize & antiparalize amulet)





## Gameplay

Have you ever played a game like
[Barony](https://store.steampowered.com/app/371970/Barony/) before? It's a lot
of fun however it's extremely narrow in scope and not fun at all to play on your
own. [Verdigris], from a gameplay perspective, could be best described as an
adventure game that takes place in a world that is utterly indifferent to your
plight. It draws from many inspirations, mainly older style modded minecraft.
Namely: 
[Roguelike Dungeons](https://www.curseforge.com/minecraft/modpacks/roguelike-adventures-and-dungeons-2),
[The Twilight Forest](https://www.curseforge.com/minecraft/mc-mods/the-twilight-forest),
and [Biomes O' Plenty](https://www.curseforge.com/minecraft/mc-mods/biomes-o-plenty)

The general gameplay loop consists of the following elements, done as desired
- Manage your resources and your base of operations
- Venture through the world to find information towards your main quest
- Participate in massive sidequests for rewards and to learn more about the world
- Participate in the overarching story
- Looting and plundering dungeons
- Exploring natural landscapes to stumble upon previous structures of people before.
- Upgrade your base, weapons, tools, and magic
- Participate in mini games to learn new magical abilities



## Things To Figure out
- Come up with a magic system that just isn't ripped from dnd
- How old is the world, why hasn't technology progressed 
    - I like the idea that it's stayed stagnant for 100k+ years
    - this is happening because an AI is silently preventing major progression
    - this means that ancient magic things can be useful
- How to balance other classes and things that aren't so heavily focused on magic. 

Gameplay:

## Background

The world of verdigris is actually very similar to our own.


It takes place on an exoplanet somewhere in the milky that was propped up by 
transplanting lifeforms from earth
along with massive help from the AI running everything

it;s goal? create a utopia that encourages as much human triving as is possible


As a result it decided to recreate the stories that we tell ourselves of magic systems
as that, is enough to satiate us, and new disoveries can still happen within it and new developments
yet its all constrained and nothing like building a rocket will ever be feasable
be cause why?

all of the magical enerry radiates from the planet and it dies
so even if you made a magical rocket motor it would just fizzle out when you leave

actually our own, long in the future




## Random Miscellaneous Ideas
- Traveling Traders
- Discovery of complexes that actually had a purpose (you have to find an old cantina)
- How puzzles... & player world interaction
- Harden out the nanomachine magic idea
- Keys based off of UUIDs
- Underground procedural dungeons
- Floating island
- Puzzle Escapes
- Screen drawing to cast (like jgh shotgun reload)
- Loquavian Hellship
- Echoes of the sky dragon
- Monsters that can actually scare the player
- Spirits and little ghostly things that just watch you lakitu style
- Entire complexes have a reason for existing
    - Rooms for bread storage, food storage, growing mushrooms, research,
    - water purification, alcohol, kitchens, armories, defense, sleeping
    - magical power storage


## Random TODOs
- Dynamic reallocing and shrinking of gpu allocations
- Dense Brick optimization
- Renderer rewrite
- Migrate to sync2 & other extensions
- Real Time Rendering Graphical Effects
- Dedicated Transfer Queues
- GraphicalVoxel and MaterialVoxel (Stone{0, 1, 2, 3, 4, 5, 6, ...} vs Stone)
    - This could also just be accomplished by doing different pallets per chunk
- Add proper readback support (esp for raytracing of what the player is looking at)
- Floating origin / figure that whole mess out
- Command Line Arguments & dynamically enableable asserts
- Colored output for stdout 
- Doing lighting work in the vertex shader (multiple attachments could make this easier)
- An actually good event system (string based?) 
    - make the debug menu not special
- Inherent entity marker component (reflection!)
- Validation layer detection 
    - Print Debug, Optimized, Validation Enabled in the startup sequence
    - seperate verdigris and vulkan validation
- Bad Apple Tech Demo for gpu remeshing
- Ensure player movement isnt interrupted when ticking is stalled
- threadpooling util::runAsync(...) -> std::future
- Central 
- Imgui tracing profiler
- Placing and breaking stuff in the world
    - Aligned to 2^n
    - shapes (cube, sphere, line, equation in form of z = floor(f(x, y)), etc)
- Voxel Groups
    - Dynamic Voxel Volume
    - Voxel Transactions (sharding locks per chunk)
- Physics
- LODs
    - GpuMap<ChunkCorner, Chunk {Lod, BrickmapPtr}>
    - fn(WorldPosition, LOD) -> ChunkCorner
    - keep ascending LOD levels untill you find your containing chunk.
- Just a better chunk management scheme, it sucks rn
    - Gpu stuff, cpu read/write, LODs, and sync between it all.
- An actually good UI
- CPU / GPU bottleneck detector
Explore, fight, and try to decyper the strange world that you have found yourself
in


## MVP stuff


While simple, this gameplay loop should be sufficiently complex enough for an MVP and ripe for expansion with some simple possibilities listed below
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
