# CPSC427 game project: Verloren - A Grimm Tale

Verloren is a top-down, isometric, roguelike, RPG, shooter game made with C++ and OpenGL-GLSL using an Entity-Component-System framework.
The Project was made by a group of 6 full-time UBC students in the course CPSC 427.
It was completed over the course of 3 months from mid January to mid April 2022.
The original repository (along with commit history) is private as per course guidelines.

Note: Due to the fact the game is built with OpenGL-GLSL, it may be difficult or impossible to run on Mac computers.

***My noteworthy contributions:
* Major part in deciding the theme, story, style, genre, and features of the game
* Helped teammates with various ideas and implementation strategies
* Keeping project code organized, optimized, and scalable
* The engine of the game: _Includes..._ (Further explained in later points below)
  * 2.5-D Physics System optimized with grid-based spatial parititoning and efficient collision detection
  * 2.5-D Render System optimized with SSBO batch rendering, texture atlases, and GPU accelerated transformation calculations
  * Player input handling allowing the player to move, shoot, strike and switch characters
  * Debug mode allowing teammates to effectively troubleshoot engine related problems
* Spatial Grid Partition: _Includes..._
  * 2-D grid with adjustable size and amount of square cells each holding an array of contained entities 
    * API for adding/removing entities from cells
    * API for the rastering of lines and polygons (ex. for adding obstacles like lakes to the spatial grid)
    * API for querying the grid with rectangles, circles, or rays, returning a list of potentially intersecting entities
  * API for physics: Collection of 2-D collision detection functions (ex. box-box, circle-circle, circle-polygon edge, line-line)
* Physics System: _Includes..._
  * All sprites are culled if outside the rectangular view frustum
  * Player and enemy sprites steer/accelerate realistically
  * Moving sprites know their cell position and index in the spatial grid and can thus remove themselves from it
  * The spatial grid partition is used for all collision calculations
  * Different entity types represented by a type mask to efficiently determine what types are interacting
  * Collision detection and resolution are handled differently between different entity types:
    * Players, enemies, and obstacles use circle-circle
    * Player or enemy collision with ComplexPolygons uses circle-polygon edge
  * Check for overlapping sprites using spatial grid partition
    * Fading in/out transparency of obstacle sprites when character sprites are behind them
  * Various debug functions that can be toggled at runtime for solving physics related bugs
* Render System: _Includes..._
  * Various code to set up shaders for drawing entities
  * Large InstanceData array that hold entity batch data, mapping to a GPU SSBO
  * Texture array sent to GPU so batches can use many different textures
  * Texture atlases with associated .json file of stylized pixel flora sprites found on OpenGameArt.org and packed into atlas using Free Texture Packer
  * Batched rendering that flushes when InstanceData array or texture array fill completely - massively cuts down on draw calls 
  * 2.5-D orthographic player tracking camera allowing changing of vertical angle
  * Shadows cast by directional light (simple method of duplicating sprite, making it black and semi-transparent, and transforming it)
  * .obj importer using given parser function that creates ComplexPolygon component object from a 2-D mesh
    * Algorithm to get only the outer edges of a 2-D mesh
    * Create ComplexPolygon using edges which can then be used by physics system (ex. to prevent walking on a lake)
    * Add vertex 'normal' data to mesh data to allow vertex extrusion along normal
* Lighting System: _Includes..._
  * 3-D directional light with day-night cycle using spherical coordinates
  * Lerp sequences allowing changing directional light colour throughout day-night cycle (i.e. warmer, colder)
  * Multiple 3-D point lights with light culling using the grid-based spatial partition
* Textured Sprite Vertex and Fragment Shaders: _Includes..._
  * GPU accelerated sprite transformation calculations
  * Adjustable wind effect on flora sprites
  * 2 SSBOs for holding batched instance data and point light data
  * Array of textures that batched instanced entities can index into for vastly improved batching
  * Batched entities can have different texture locations for use of texture atlases and scrolling textures
  * Blinn-Phong reflection model using lights and normal mapped sprites
  * Shadow shading with varying shadow alpha based on vertical sun angle and point light proximity
  * Dithered transparency when sprites overlap for an unobstructed view of the player and enemies
  * Option for 2 blended normal maps per sprite
  * Option for mask texture (ex. for creating differently shaped terrain patches)
  * Multiplicative and additive blending allowing for differently coloured sprites at runtime (i.e. trees and flowers)
  * Red vignette that fades in/out when player takes damages
  * Full screen fade in/out for starting/ending the game
* Animated Lakes/Rivers in Animation System: _Includes..._
  * Bodies of water with 2 scrolling normal maps to simulate waves
  * Oscillate extrude size of mesh to simulate waves crashing along coast
* Projectiles: _Projectiles are..._
  * Affected by gravity and collide with the ground
  * Skip/bounce along the ground realistically
  * Destroyed when colliding with obstacles or a water body, resulting in exploding particle effect
* Particle system: _Particles are..._
  * Created by ParticleGenerators with various customizability (ex. speed, rate, lifetime)
  * Affected by gravity with a multiplier to allow for floating/rising particles like smoke
  * Taken from a particle pool upon creation to reduce instantiation overhead
  * Created with conically random directional velocities
* Props: _Props are..._
  * Small decorative game elements such as bushes, flowers, logs, lily pads
  * Randomly chosen from a .json texture atlas based on predefined weightings
  * Procedurally placed evenly across the map based on density factor
    * Not placed near existing entities like obstacles (i.e. trees, rocks) using spatial grid queries
    * Only water props like lily pads are placed on water bodies (checking if position is in water body ComplexPolygon)
  * Randomly coloured differently using multiplicative blending on grayscale pixels
