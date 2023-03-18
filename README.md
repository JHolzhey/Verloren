# CPSC427 game project: Verloren - A Grimm Tale

Verloren is a top-down, isometric, roguelike, RPG, shooter game made with C++ and OpenGl-GLSL using an Entity-Component-System framework.
The Project was made by a group of 6 full-time UBC students in the course CPSC 427.
It was completed over the course of 3 months from mid January to mid April 2022.
The original repository (along with commit history) is private as per course guidelines.
Note: Due to the fact the game is built with OpenGl-GLSL, it may be difficult to run on Mac computers

My noteworthy contributions:
* Major part in deciding the theme, story, style, genre, and features of the game
* The engine of the game: _Includes..._
  * 2.5-D Physics System optimized with grid-based spatial parititoning and efficient collision detection
  * 2.5-D Render System optimized with SSBO batch rendering, texture atlases, and GPU accelerated transformation calculations
  * Player input handling allowing the player to move, shoot, strike and switch characters
  * Debug mode allowing teammates to effectively troubleshoot engine related problems
* Physics System: _Includes..._

  * Fading in/out transparency of obstacle sprites when character sprites are behind them
* Render System: _Includes..._
  * Texture atlases of stylized pixel flora sprites found on OpenGameArt.org
  * 2.5-D player tracking camera allowing changing vertical angle
  * Shadows cast by directional light
  
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
  * 2 SSBOs for holding batch rendering instance data and point light data
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
  * Affected by gravity with a multiplier to allow for floating/rising particles like smoke
  * Taken from a particle pool upon creation to reduce instantiation overhead
  * Created with conically random directional velocities
