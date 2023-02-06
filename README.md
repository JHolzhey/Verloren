# CPSC427 game project: Verloren - A Grimm Tale

Video game project made with C++ and OpenGl-GLSL using an ECS framework, for the UBC course CPSC 427. Project was made in a group of 6 full-time students over the course of 3.5 months from beginning of January to mid April 2022.
Note: Due to the fact the game is built with OpenGl-GLSL, it may be difficult to run on Mac computers

My noteworthy contributions: 
* The engine of the game:
  * 2.5-D physics system optimized with grid-based spatial parititoning and efficient collision detection
  * 2.5-D render system optimized with SSBO batch rendering and GPU accelerated transformation calculations
  * Player input system allowing the player to move, shoot, and melee
  * Debug mode allowing teammates to effectively troubleshoot problems
* Stylized pixel graphics:
  * Dithered transparency for obstacles for an unobstructed view of the player and enemies
  * Lights; 3-D directional and multiple point lights
  * Normal maps shaded by lights
  * Shadows cast by directional light
* Projectile system: (Projectiles are...)
  * Affected by gravity
  * Skip/bounce along the ground realistically
  * Destroyed when colliding with obstacles or a water body
* Particle system: (Particles are...)
  * Affected by gravity with a multiplier to allow for floating particles like smoke
  * Taken from a particle pool upon creation to reduce instantiation overhead
  * Created with conically random directional velocities



# Entry Points
* Text: Efficient text rendering with background boxes. There should be lots of comments explaining how this feature works in detail in the relevant code.
  * Text component itself is in `components.hpp`
  * Font and character information structs are found in `render_system.hpp`
  * Font information and character information is initialized and calculated in `render_system_init.cpp`
  * The text rendering itself is done in `drawText` in `render_system.cpp`. 
  * The text VBO updating/creation is done in `updateText` in `render_system.cpp`. Text rendering is instanced, so each text component (string can be arbitrarily long) has its entire text information inside a single (two, really) VBO.
* Navigation system: done in `world_system.cpp`. It clearifies where enemies are roughly located.
  * a new component `Navigator` for each navigator entity
  * one-to-one relation between navigators and enemimes
  * only visible when enemies are certain distance away from the player
  * when enemies are all cleared, point to the portal instead
* Witch (Boss): 
  * a new component `Witch` for the witch
  * `Witch` has all the spells information, i.e. cooldown, duration, ...
  * Logic:
    * Each spell has a cast duration and unique effect
    * Witch rests between each consecutive and different spells
* Particles:
  * New particle generator and particle components found in 'components.hpp'
  * Their initilization is in 'world_init'
* Props:
  * Also a new component and initialzation in 'world_init'
* Batch Renderer:
  * Found in 'render_system', makes rendering particles and props much more efficient
* Audio:
  * New audios loaded in 'WorldSystem'
  * Credits:
    * https://freesound.org/people/nezuai/sounds/577048/
    * https://freesound.org/people/unfa/sounds/221539/
    * https://freesound.org/people/micahlg/sounds/413182/
    * https://freesound.org/people/Reitanna/sounds/344033/
    * https://freesound.org/people/plasterbrain/sounds/608431/
    * https://freesound.org/people/watsnick/sounds/423479/
    * https://freesound.org/people/jhyland/sounds/539678/
    * https://freesound.org/people/ssierra1202/sounds/391947/
    * https://freesound.org/people/mrgreaper/sounds/223486/
    * https://www.youtube.com/watch?v=dwQTDJO7MHQ
    * https://www.zapsplat.com/music/elk-moose-mating-call-fake-human-imitating/
    * https://freesound.org/people/kiddpark/sounds/201159/
    * https://freesound.org/people/egomassive/sounds/536753/
    * https://freesound.org/people/cmusounddesign/sounds/119814/
    * https://www.youtube.com/watch?v=vDOfrjA9CG0
    * https://freesound.org/people/rvinyard/sounds/117250/ 
    * https://www.youtube.com/watch?v=pz6eZbESlU8
     
# User Experience Feedback:
## Anonymous A:
1.	Confused about what the enemy “snail” does
2.	Does not know what right click does for each character
3.	Gretel right click knocks enemy too far
## Anonymous B:
4.	Isn’t aware of how to pause the game
5.	Squirrel room is difficult to clear
6.	Not sure what the objective for the game is
## In response to above:
1.	Since we have text rendering, we have the description for the item that causes the snail to appear
2.	There are new tutorials for right-click and more
3.	Gretel right click now doesn’t knock as far as before
4.	Fix during polish before final demo
5.	None. Some rooms are meant to be challenging
6.	Navigation system points to the objectives. After enemies are all eliminated, there is another navigation to portals.
