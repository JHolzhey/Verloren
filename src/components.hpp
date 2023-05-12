#pragma once
#include <unordered_map>
#include "common.hpp"
#include "../ext/stb_image/stb_image.h"
#include "../ext/rapidjson/document.h"
#include "../ext/pathfinder/AStar.hpp"



struct range
{
	float min;
	float max;
};

struct rangeInt
{
	int min;
	int max;
};

enum class PLAYER_CHARACTER {
	HANSEL = 0,
	GRETEL = 1,
	CHICKEN = 2,
};

// Player component. If invuln_timer > 0, player does not take damage from collisions
struct Player
{
	Entity line1;
	Entity line2;

	float invulnerable_timer = 0;
	bool torchlight_enabled = false;
	float torchlight_time_max = 1000;
	float torchlight_timer = 0;
	PLAYER_CHARACTER character;

	Player(PLAYER_CHARACTER chr) : character(chr) {}
};

struct Healthy
{
	float max_hp;
	float current_hp;

	Healthy(float max, float curr) : max_hp(max), current_hp(curr) {}
	Healthy(float base_hp) : max_hp(base_hp), current_hp(base_hp) {}
};

// two entities, primary and secondary health bars use this component
// each has a max health, current health
struct HealthBar
{
	// maximum health of a character
	int max_hp;
	// current health of a character
	int current_hp;
	// there are two health bars, primary and secondary
	bool is_primary;
};

struct Navigator {
	
};

struct Witch {
	float light_time_remaining_ms;
	// hellfire
	// the witch calls on hellfire and rains hellfire from above
	// hellfire is generated randomly with a pattern or without a pattern
	// before hellfire comes, there are warning circles generated to let player know that something's coming
	// deals projectile damage
	float hellfire_radius = 600.f;
	int   max_hellfire_allowed = 18;
	int   hellfire_accuracy = 60;
	float hellfire_counter = 0.f;
	float hellfire_duration = 3000.f;
	float hellfire_damage = 5.f;
	////////////////////////////////////
	// swipe
	// the witch swipes in the area around herself
	// deals melee damage
	float swipe_radius;
	float swipe_duration = 1000.f;
	float swipe_damage;
	////////////////////////////////////
	// fireball
	// the witch throws "max_fireball_allowed" # of fireballs
	// deals projectile damage
	//float max_fireball_allowed;
	float fireball_duration = 3000.f;
	float fireball_damage;
	////////////////////////////////////
	// dash
	// the witch dashes in the direction of the player
	// deals collision damage
	//vec2 pos_before_dash;
	float dash_duration = 3000.f;
	float dash_damage;
	////////////////////////////////////
	// utility
	// a count for the number of spells the witch can cast
	int num_of_spells = 4;
	// keep track of the index of last casted spell
	// if spell is still casting before the next decision, continue casting
	int last_spell_casted = -1;
	// a vector that keep tracks of all spells duration
	// extract this using the index from "last_spell_casted"
	std::vector<float> spell_durations = { hellfire_duration,
										   swipe_duration,
										   fireball_duration,
										   dash_duration };
	// a vector that keep tracks of all spells duration
	// extract this using the index from "last_spell_casted"
	// define w.r.t to "spell_durations" order
	std::vector<float> spell_counters = {  0.f, 0.f, 0.f, 0.f };
	// the interval between each different consecutive spells
	// if casting the same spell before the next decision, this interval is 0
	float cast_spells_interval = 1000;
};

struct Attack
{
	int damage = 1;
	float knockback = 0;
	// Could add an 'owner' field here like I did in Projectile for the TODO in world_system - colliding with own attack
};

enum class TEMP_EFFECT_TYPE : int {
	PLAYER_MELEE_ATTACK,
	PLAYER_PUSH_ATTACK,
	HEALING,
	CHEST_OPENING,
	ATTACK_WARNING,
	BEING_DEATH, // Consider changing to FADE_OUT as well
	PARTICLE_GENERATOR,
	FADE_OUT,
	TEXT,
	TEMP_EFFECT_COUNT,
};
struct TemporaryEffect {
	TEMP_EFFECT_TYPE type;
	float time_remaining_ms;
	//float total_time_ms;
};

struct Chest 
{
};

enum class ENEMY_TYPE : int {
	SPIDER = 0,
	WORM,
	SQUIRREL,
	RAT,
	BEAR,
	BOAR,
	DEER,
	FOX,
	RABBIT,
	ALPHAWOLF,
	WOLF,
	SNAIL,
	WITCH,
	SKIP, // = No enemy
};

enum class SOUND_TYPE : int {
	SQUIRREL = 0,
	RAT,
	BEAR,
	BOAR,
	DEER,
	FOX,
	RABBIT,
	ALPHAWOLF,
	WOLF,
	SKIP, // = No enemy
};

struct Enemy
{
	ENEMY_TYPE type;
	// immunities
	bool knock_back_immune = false;
	bool collision_immune = false;
	bool invulnerable = false;
	bool buff_immune = false;
	bool debuff_immune = false;
	// special enemy abilities
	int curr_num_of_companions = 0;
	int max_num_of_companions = 3;
	bool howled = false;
	bool is_transformed = false;

	float damaged_color_timer = 0;
	int collision_damage = 1;

	// Time until decision tree is run next
	float next_decision_ms;
	// Time until pathfinding is run again
	float next_pathfinding_ms;

	// Coordinates of target
	std::optional<Entity> target = std::nullopt;
	Enemy(ENEMY_TYPE et) : type(et) {
		// Minor offset until first decision is made, preventing simultaneously 
		// spawned enemies from all being processed in lockstep
		this->next_decision_ms = (float) (rand() % 600);
		this->next_pathfinding_ms = 0;		
	}
};

struct EnemySpawner
{
	std::vector<ENEMY_TYPE> enemies;
};

// ui elements
struct UI_Element {
	// if an icon is enable
	bool icon_enabled = true;
	// Whether an icon should be visible in the pause screen only
	bool always_visible = false;
	// Whether the icon should be shown or not
	bool visible = false;
	// Whether the icon responds to clicks and/or hovers
	bool interactive = true;
	// Whether the element has support for various hover effects
	bool hover_glow_enabled = true;
	bool hover_text_enabled = true;

	vec2 icon_scale = { 0.2, -0.2 };
	vec2 health_bar_frame_scale = { 0.5, -0.25 };
	vec2 menu_option_scale = { 0.5,-0.5 };

	// simple switch of texture on icons, does not allow advance animation
	// i.e. sprite sheet num > 2
	int sprite_sheet_num = 1;
	// Used to draw elements on top of each other. Lower depth => drawn earlier
	int depth = 0;

	// UI Icons may have a colored "aura" around (underneath) them.
	// Auras are a separate entity from the ui_element on top of them.
	// Auras are circluar - the opacity of a pixel is proportional to its proximity (euclidian) from the center
	bool is_glow = false;
	vec2 glow_offset = {0, 0};
	float glow_scale_multiplier = 1;

	// text that appears on hover
	// NOTE: In some places the ui_elem entity = hover_text entity. This kinda fucky-wucky and may cause problems. But it will also help prevent memory leaks/blowups
	std::optional<Entity> hover_text = std::nullopt;
};

struct BezierCurve {
	std::vector<vec2> points;
	mat2x4 basis;
	int num_curves;
	int current_curve = 1; // the current curve. in [1, num_curves]
	float t = 0;
	float curve_duration_ms = 2000;
	vec2 default_sprite_offset = {0.f, 0.f};

	BezierCurve(std::vector<vec2> ps) : points(ps) {
		// Must have a multiple of 2 points. If not, last point will be ignored
		assert(ps.size() >= 4 && ps.size() % 2 == 0);
		mat2x4 G = {
			{ps[0].x, ps[1].x, ps[2].x, ps[3].x},
			{ps[0].y, ps[1].y, ps[2].y, ps[3].y}
		};
		this->basis = create_basis(G);

		this->num_curves = (int) ((ps.size() - 2) / 2);
	}

	bool has_more_joints() {
		return this->points.size() >= 6;
	}

	void joint_next_point() {
		assert(this->has_more_joints());
		auto& ps = this->points;

		// Find control points for new curve, starting at final control point of previous curve
		vec2 p1 = ps[3],
			 p2 = ps[3] + (ps[3] - ps[2]),
			 p3 = ps[4],
			 p4 = ps[5];
		
		// Create control points matrix and calculate basis functions matrix
		mat2x4 G = {
			{p1.x, p2.x, p3.x, p4.x},
			{p1.y, p2.y, p3.y, p4.y},
		};
		this->basis = create_basis(G);
		
		ps.erase(ps.begin());
		ps.erase(ps.begin());
		ps[0] = p1;
		ps[1] = p2;
		ps[2] = p3;
		ps[3] = p4;

		t -= 1.f;
		current_curve++;
	}

	static mat2x4 create_basis(mat2x4 points) {
		mat4x4 bezier_basis_matrix = {
			-1, 3, -3, 1,
			3, -6, 3, 0,
			-3, 3, 0, 0,
			1, 0, 0, 0,	
		};
		return bezier_basis_matrix * points;
	}
};

// All data relevant to the shape and motion of entities
struct Motion {	
	uint32 type_mask = 0; // For optimizing collision tests. Initiliazed to UNCOLLIDABLE_MASK (see world_init.hpp for different types)
	bool moving = false;
	bool is_culled = false;
	vec2 move_direction = { 0,0 };
	vec2 look_direction = { 1,0 };
	vec2 position = { 0,0 };
	vec2 velocity = { 0,0 };
	float angular_velocity = 0.f; // In radians/second. Positive spins clockwise, negtive spins ccw
	float radius = 40.f; // Used for calculating circle-circle collisions between entities
	bool angled = false;
	float angle = 0.f;
	float accel_rate = 600.f; // Positive constant
	float friction = 0.7f; // Speed Dampening factor. 0 -> 100% dampening, 1 -> no dampening (frictionless)
	float max_speed = 50.f;
	float mass = 100.f;
	float current_speed = 0;
	vec2 scale = { 1.f,1.f };		// For sprite_offset, we could just have it be the z value of a vec3 position
	vec2 sprite_offset = { 0,0 }; 	// Moves sprite upwards by a certain amount so the circle collider is at the sprite's feet
	vec3 sprite_normal = { 0,cos(0.2),sin(0.2) }; // Used for screen tilt and lighting. {0,1,0} faces screen down and {0,0,1} faces up
									// x and y represent regular ground plane (with {0,0} in top left). z is up/down
									// normal = {0, 1/sqrt(2), 1/sqrt(2)} faces angled up by 45deg
	float sprite_offset_velocity = 0.f; // 1 dimensional vector, positive means sprite_offset.y will increase (projectiles and particles)
	float gravity_multiplier = 1.f; // 0 == not affected by gravity, 1 == fully affected, 1 < over affected
	vec3 forces = { 0,0,0 }; // TODO: Finish

	ivec2 cell_coords = { INT_MAX, INT_MAX };
	int cell_index = INT_MAX; // Allows entity to remove itself from it's current Cell
	bool checked_collisions = false;

	// Other motion components that should move with/follow this object
	std::vector<Entity> children = {};
	std::optional<Entity> parent = std::nullopt;

	void add_child(Entity child) {
		this->children.push_back(child);
	}

	// Note: This does not reset the child's parent field. This will have to be done manually
	void remove_child(Entity child) {
		for (auto it = this->children.begin(); it != this->children.end(); it++) {			
			if (*it == child) {
				this->children.erase(it);
				return;
			}
		}
		// Some motion has this as its parent, but this does not have that motion in its child list
		// This should never happen.
		assert(false);
	}
	
	// Use reference equality
	friend bool operator==(const Motion& lhs, const Motion& rhs) {
		return &lhs == &rhs;
	}
	friend bool operator!=(const Motion& lhs, const Motion& rhs) {
		return &lhs != &rhs;
	}
};

struct Water // For appling effects to entities with water (happens in animation_system)
{
	float time_ms = 0.f; // Used to create an in and out wave effect
};

struct Fire // For appling effects to entities with fire
{
};

enum class OBSTACLE_TYPE : int { // Used to determine what type of effect to use when hit
	TREE,						 // Note: to determine if an obstacle is a polygon, just check if it's a WATER etc.
	FURNITURE,					 // or check if the polygon registry contains it
	CAMP_FIRE,
	ROCK,
	WATER,
	PLATFORM,
	OBSTACLE_COUNT
};
struct Obstacle
{
	OBSTACLE_TYPE type = OBSTACLE_TYPE::OBSTACLE_COUNT;
};

enum class PROP_TYPE : int { // Used to determine what type of effect to use when hit
	FOLIAGE,
	PROP_COUNT
};
struct Prop
{
	PROP_TYPE type = PROP_TYPE::PROP_COUNT;
	// sub_type
};

enum class PICKUP_ITEM : int {
	FOOD,
	UPGRADE,
	SHINY,
	SHINY_HEAP,
	PICKUP_COUNT
};
struct Pickupable
{
	PICKUP_ITEM type = PICKUP_ITEM::PICKUP_COUNT;
	std::function<void(void)> on_pickup_callback = [](){0;};
};

struct EnemyAttractor 
{
	int hits_left = 3;
};

struct Projectile
{
	float time_alive_ms = 0;
	float lifetime_ms = 4000;
	// The number of times the projectile can hit a target
	int hits_left = 1;
	//bool heatSeeking = false; // Not implemented
	// Whether the projectile bounces off obstacles
	bool bouncy = false;
	Entity owner; // Needed so that when fired, the projectile doesn't just immediately damage the firer
	Projectile(Entity owner) : owner(owner) {}

	// Prevent a projectile from hitting the same enemy multiple times in very quick succession
	std::optional<Entity> last_hit_entity = std::nullopt;
	// Reset last_hit_entity if timer runs out, so same entity may be hit twice, but not rapidly
	float last_enemy_hit_timer = 250.f;
};

struct RangedEnemy {
	float shooting_interval = 750.f;
	float time_since_last_shot_ms = 0;
	float stop_chasing_range = 400.f;
	bool can_shoot = false;
};

// Stucture to store collision information
struct Collision
{
	// Note: the first object is stored in the ECS container.entities
	Entity other; // The second object involved in the collision
	uint32 combined_type_mask = 0; // This is used to quickly determine the types of the 2 colliding entities
	Collision(Entity other, uint32 mask) : other(other), combined_type_mask(mask) {}
};

// Data structure for toggling debug mode
struct Debug {
	bool in_debug_mode = 0;
	bool in_freeze_mode = 0;
};
extern Debug debugging;

// Sets the brightness of the screen
struct ScreenState
{
	int window_height_px = 1;
	int window_width_px = 1;
	float darken_screen_factor = 0;
};

template <typename T>
struct KeyFrame // Not in registry, just used in the below
{
	T value;
	float time_frac = 0.f; // Represents time of key_frame, must be [0, 1]
	KeyFrame(T value, float time_frac) : value(value), time_frac(time_frac) {} // Constructor necessary?
};

template <typename T>
struct LerpSequence
{
	std::vector<KeyFrame<T>> key_frames; // must have time_fracs in ascending order with 0.f at start and 1.f at end (see example below)
	float cycle_ms = 1000.f;	   // how long sequence will take (to loop if enabled) in ms
	bool loops = true;			   // does this sequence loop
	float elapsed_time_frac = 0.f; // [0, 1] time fraction of entire sequence (when reaches 1, will loop if enabled)
	int key_index = 0;			   // current key index into key_frames vector (prev = key_frames[index], next = key_frames[index+1])
	bool enabled = true;
	LerpSequence(std::vector<KeyFrame<T>> key_frames, float cycle_ms) : key_frames(key_frames), cycle_ms(cycle_ms) {}
};

//const std::vector<KeyFrame<vec3>> dir_light_color_key_frames = { {vec3(1.f), 0.f }, // Moon peak midnight
//														{ vec3(1.f), 0.25f },
//														{ vec3(1.f), 0.5f } , // Sun high noon
//														{ vec3(1.f), 0.75f },
//														{ vec3(1.f), 1.f } }; // Moon peak midnight

//const std::vector<KeyFrame<vec3>> dir_light_color_key_frames = { {vec3(1.00f,0.00f,0.00f), 0.f }, // Moon peak midnight
//														{ vec3(0.00f,0.00f,1.00f), 0.25f },
//														{ vec3(0.00f,1.00f,0.00f), 0.5f } , // Sun high noon
//														{ vec3(0.00f,0.00f,1.00f), 0.75f },
//														{ vec3(1.00f,0.00f,0.00f), 1.f } }; // Moon peak midnight

const std::vector<KeyFrame<vec3>> dir_light_color_key_frames = {{ (vec3(64, 156, 255)/255.f)*0.6f, 0.f }, // Moon peak midnight
																{ vec3(255, 147, 41)/255.f, 0.25f },
																{ vec3(255, 255, 255)/255.f, 0.5f } , // Sun high noon
																{ vec3(255, 147, 41)/255.f, 0.75f },
																{ (vec3(64, 156, 255)/255.f)*0.6f, 1.f } }; // Moon peak midnight
// 100, 100, 455 // 255, 197, 143

struct PointLight // Spherical radius of light held by an entity with a Motion. It's world position = (motion.position, 0) + offset
{
	vec3 offset_position = vec3(0.f, 3.f, 0.f); int pad = 0;
	vec3 position = vec3(0.f, 0.f, 0.f); int pad2 = 0;

	float entity_id = 0;
	float constant = 1.f;
	float linear = 0.007f;
	float quadratic = 0.0002f;

	vec3 ambient = vec3(0.f); int pad3 = 0;
	vec3 diffuse = vec3(255.f, 30.f, 0.f) / 255.f; int pad4 = 0;
	vec3 specular = vec3(0.2f);	int pad5 = 0;

	float radius = 1.0f;
	float flicker_radius = 1000.f; // Set to 0.f if light doesn't flicker at all
	float max_radius = 1.f; // Represents it's max reach == radius + flicker_radius
	float intensity = 1.f; // Currently unused

	PointLight() {}
	PointLight(float radius, float flicker_radius, float entity_id, vec3 diffuse_colour = vec3(255.f, 30.f, 0.f) / 255.f) {
		this->set_radius(radius, flicker_radius, entity_id);
		diffuse = diffuse_colour;
	}
	void set_radius(float _radius, float _flicker_radius, int _entity_id) { // Unfinished, requires tweaking
		this->constant = 4000.f; // 1 / intensity; // Maybe will be constant so move out of here
		this->linear = 0.02f;
		this->quadratic = 1.f;

		this->radius = _radius;
		this->flicker_radius = _flicker_radius;
		this->max_radius = _radius + _flicker_radius;
		this->entity_id = _entity_id;
	}
};

struct DirLight // Directional
{
	vec3 direction = vec3(0.f); // normalized direction vector (from origin to light) of the light source
	vec3 ambient = vec3(0.0f); // Overwritten in lighting_system
	vec3 diffuse = vec3(1.f);  // Overwritten in lighting_system
	vec3 specular = vec3(1.f); // Overwritten in lighting_system

	float strength = 0.f; // [0,1]. Not to be passed to shader
};

struct WorldLighting // TODO: Maybe turn this into WorldEnvironment?
{
	Entity dir_light;
	//Entity point_lights[10] = {}; // Unfinished
	//int num_point_lights = 0; // Unfinished
	//bool is_render_updated = false; // Unfinished
	int num_important_point_lights = 0;

	float day_cycle_ms = 0.f;// how long is a day in ms
	float latitude = -1.f;  // [0, pi]
	float theta_offset = 0.f; // M_PI / 4.f;
	float time_of_day = 10.f; // in hours [0, 24] including decimal values

	bool is_new = true;
	bool is_time_changing = true; // AKA; is_day / night_cycle_enabled
	bool is_day = true;
	float theta = 0.f; // angle of rotation [0, 2pi] (although it's fine if it's less or greater since sin/cos take care of it)
	float phi = M_PI / 4.f; // angle off of the ground's normal [0, pi]. 0 = directly above, pi/2 = sunset/rise, pi = below

	float latitude_change = 0; // Only for debugging
	float time_of_day_change = 0; // Only for debugging

	float theta_change = 0;
	float phi_change = 0;
	WorldLighting(Entity dir_light, float latitude, float cycle_ms) : dir_light(dir_light), latitude(latitude), day_cycle_ms(cycle_ms) {}
	void set_time_and_latitude(float _time_of_day, bool _is_time_changing = true, float _latitude = M_PI / 4.f)
	{
		this->time_of_day = _time_of_day;
		this->is_time_changing = _is_time_changing;
		this->latitude = _latitude;
	}
};

struct Camera
{
	float phi = M_PI / 4.f; // angle off of the ground's normal [0,pi]. 0 = directly above, pi/2 = horizontal, pi/4 = 45deg
	float phi_change = 0;
	float zoom = 3.f;
	vec2 position = { window_width_px / 2.f, window_height_px / 2.f };
	vec3 direction = { 0.f,0.f,0.f }; // normalized direction vector (from origin to camera) of the camera
	vec2 scale_factor = { 1.f, cos(phi) }; // Represents (vertical) scaling of world based on phi

	vec2 frustum_size = vec2(16, 9.2) * 85.f; // 80.f is best
	BBox view_frustum;
};

struct GroundPiece
{
};

// A struct to refer to debugging graphics in the ECS
struct DebugComponent
{
};

struct ColliderDebug // A struct held by an entity that refers to its debug components
{
	Entity circle;
	Entity box;
	Entity look_line;
	Entity cell;
	ColliderDebug(Entity circle, Entity box, Entity look_line, Entity cell) : 
		circle(circle), box(box), look_line(look_line), cell(cell) {}
};

// A timer that will be associated to dying player
struct DeathTimer
{
	float counter_ms = 3000;
};

// Single Vertex Buffer element for non-textured meshes (coloured.vs.glsl & chicken.vs.glsl)
struct ColoredVertex
{
	vec3 position;
	vec3 color;
};

// Single Vertex Buffer element for textured sprites (textured.vs.glsl)
struct TexturedVertex
{
	vec3 position;
	vec2 texcoord;
	vec2 normal;
};

struct Edge // Not in registry, just used in the below
{
	vec2 vertex1;
	vec2 vertex2;
	bool is_collidable = true;
	friend bool operator==(const Edge& lhs, const Edge& rhs) {
		return (lhs.vertex1 == rhs.vertex1 && lhs.vertex2 == rhs.vertex2) || (lhs.vertex1 == rhs.vertex2 && lhs.vertex2 == rhs.vertex1);
	}
};

struct ComplexPolygon // Counter-clockwise winding order
{
	bool is_only_edge = false;
	bool is_platform = false; // When player is on a platform, they ignore collisions with other non-platform polygons
	vec2 position = { 0.f, 0.f };
	std::vector<Edge> world_edges; // In world coordinates as opposed to mesh coordinates
	ComplexPolygon(std::vector<Edge> world_edges, vec2 position, bool is_only_edge, bool is_platform) : 
		world_edges(world_edges), position(position), is_only_edge(is_only_edge), is_platform(is_platform) {}
	ComplexPolygon(std::vector<vec2> world_vertices) { // Untested
		for (uint i = 0; i < world_vertices.size(); i++) {
			vec2 vertex1 = world_vertices[i];
			vec2 vertex2 = world_vertices[(i + 1) % world_vertices.size()];
			Edge new_edge = { vertex1, vertex2 };
			this->world_edges.push_back(new_edge);
		}
	}
};

// Mesh datastructure for storing vertex and index buffers
struct Mesh
{
	static std::tuple<std::vector<vec3>, std::vector<vec2>, std::vector<vec3>, std::vector<uint>, std::vector<uint>, std::vector<uint>> ParseObj(const std::string& path);
	static bool loadFromOBJFile(std::string obj_path, std::vector<ColoredVertex>& out_vertices, std::vector<uint16_t>& out_vertex_indices, vec2& out_size);
	vec2 original_size = {1,1};
	std::vector<ColoredVertex> vertices; // TODO: Remove?
	std::vector<uint16_t> vertex_indices;
	std::vector<Edge> edges;
};

struct Room
{
	static rapidjson::Document loadFromJSONFile(std::string room_file_name);
	AStar::Generator generator;

	ivec2 grid_size = { 1,1 };
	vec2 player_spawn = { 0,0 };
	int num_waves = 0;
	int current_wave = 0;

	bool is_render_updated = false;
};

struct RoomExit {
	bool enabled = false;
	int next_room_ind;
};

/**
 * The following enumerators represent global identifiers refering to graphic
 * assets. For example DIFFUSE_ID are the identifiers of each diffuse texture
 * and NORMAL_ID are the identifiers of each normal texture currently
 * supported by the system.
 *
 * So, instead of referring to a game asset directly, the game logic just
 * uses these enumerators and the RenderRequest struct to inform the renderer
 * how to structure the next draw command.
 *
 * There are 2 reasons for this:
 *
 * First, game assets such as textures and meshes are large and should not be
 * copied around as this wastes memory and runtime. Thus separating the data
 * from its representation makes the system faster.
 *
 * Second, it is good practice to decouple the game logic from the render logic.
 * Imagine, for example, changing from OpenGL to Vulkan, if the game logic
 * depends on OpenGL semantics it will be much harder to do the switch than if
 * the renderer encapsulates all asset data and the game logic is agnostic to it.
 *
 * The final value in each enumeration is both a way to keep track of how many
 * enums there are, and as a default value to represent uninitialized fields.
 */

enum class FOLIAGE_ID { // UNUSED
	BUSH1,
	BUSH2,
	BUSH3,
	BUSH4,
	BUSH5,
	BUSH6,
	BUSH_BARE,
	FOLIAGE_COUNT,
};
const int foliage_count = (int)FOLIAGE_ID::FOLIAGE_COUNT;

enum class TEXTURE_TYPE {
	DIFFUSE = 0,
	NORMAL = DIFFUSE + 1,
	TEXTURE_TYPE_COUNT = NORMAL + 1,
};
const int texture_type_count = (int)TEXTURE_TYPE::TEXTURE_TYPE_COUNT;

enum class AUDIO_ID {};

enum class DIFFUSE_ID {
	// Obstacles:
	FIR_TREE = 0,
	SPRUCE_TREE = FIR_TREE + 1,
	CHESTNUT_TREE = SPRUCE_TREE + 1,
	PINE_TREE1 = CHESTNUT_TREE + 1,
	PINE_TREE2 = PINE_TREE1 + 1,
	PINE_TREE1_BARE = PINE_TREE2 + 1,
	PINE_TREE2_BARE = PINE_TREE1_BARE + 1,
	CHESTNUT_TREE_BARE = PINE_TREE2_BARE + 1,
	CAMP_FIRE = CHESTNUT_TREE_BARE + 1,
	// Complex Obstacles:
	LAKE = CAMP_FIRE + 1,
	// Beings:
	HANSEL = LAKE + 1,
	GRETEL = HANSEL + 1,
	RAT = GRETEL + 1,
	SQUIRREL = RAT + 1,
	SPIDER = SQUIRREL + 1,
	WORM = SPIDER + 1,
	BEAR = WORM + 1,
	BOAR = BEAR + 1,
	DEER = BOAR + 1,
	FOX = DEER + 1,
	RABBIT = FOX + 1,
	WOLF = RABBIT + 1,
	ALPHA_WOLF = WOLF + 1,
	ANGRY_SNAIL = ALPHA_WOLF + 1,
	WITCH = ANGRY_SNAIL + 1,
	// Projectiles:
	ROCK = WITCH + 1,
	ACORN = ROCK + 1,
	FIREBALL = ACORN + 1,
	CHESTNUT = FIREBALL + 1,
	BEE = CHESTNUT + 1,
	BREADCRUMBS = BEE + 1,

	// Upgrades:
	BAT = BREADCRUMBS + 1,
	BEEHIVE = BAT + 1,
	BUTTER = BEEHIVE + 1,
	FAST_BULLETS = BUTTER + 1,
	HEART = FAST_BULLETS + 1,
	LONGER_BAGUETTE = HEART + 1,
	QUESTION_MARK = LONGER_BAGUETTE + 1,
	REGEN = QUESTION_MARK + 1,
	SNAIL_SHELL = REGEN + 1,
	STRENGTH_UP = SNAIL_SHELL + 1,
	// Shinies:
	SHINY_BUTTON = STRENGTH_UP + 1,
	SHINY_COIN = SHINY_BUTTON + 1,
	SHINY_COIN_PILE = SHINY_COIN + 1,
	SHINY_GEM = SHINY_COIN_PILE + 1,
	SHINY_HOOK = SHINY_GEM + 1,
	SHINY_MARBLE = SHINY_HOOK + 1,
	SHINY_NAIL = SHINY_MARBLE + 1,
	SHINY_SPOON = SHINY_NAIL + 1,
	// Props:
	FOLIAGE_ATLAS = SHINY_SPOON + 1,
	CHEST = FOLIAGE_ATLAS + 1,
	CHEST_OPENING = CHEST + 1,
	GRASS_TUFT = CHEST_OPENING + 1,
	BUSH2 = GRASS_TUFT + 1,
	// Particles:
	LEAF = BUSH2 + 1,
	SMOKE = LEAF + 1,
	BUTTERFLY = SMOKE + 1,
	DRAGONFLY = BUTTERFLY + 1,
	// Temporary Effects:
	ATTACK = DRAGONFLY + 1,
	HEALING_EFFECT = ATTACK + 1,
	SPARKLE_EFFECT = HEALING_EFFECT + 1,
	PUSH_EFFECT = SPARKLE_EFFECT + 1,
	FLYING_BREADCRUMBS = PUSH_EFFECT + 1,
	// Doors:
	ROOM_EXIT = FLYING_BREADCRUMBS + 1,
	ROOM_EXIT_DISABLED = ROOM_EXIT + 1,
	// Groud Layers:
	GRASS = ROOM_EXIT_DISABLED + 1,
	DIRT = GRASS + 1,
	PLATFORM = DIRT + 1,
	GROUND_MASK = PLATFORM + 1,
	MENU = GROUND_MASK + 1,
	// Pickupables:
	MEAT = MENU + 1,
	// Tutorial:
	ARROW_KEY_TUTORIAL = MEAT + 1,
	CHEST_TUTORIAL = ARROW_KEY_TUTORIAL + 1,
	CLEARING_ROOMS_TUTORIAL = CHEST_TUTORIAL + 1,
	CURRENCY_SHOP_TUTORIAL = CLEARING_ROOMS_TUTORIAL + 1,
	GRETEL_SPECIAL_TUTORIAL = CURRENCY_SHOP_TUTORIAL + 1,
	HANSEL_SPECIAL_TUTORIAL = GRETEL_SPECIAL_TUTORIAL + 1,
	MOUSE_ATTACK_TUTORIAL = HANSEL_SPECIAL_TUTORIAL + 1,
	SWITCH_CHARACTER_TUTORIAL = MOUSE_ATTACK_TUTORIAL + 1,
	// UI:
	HEALTH_BAR_FRAME = SWITCH_CHARACTER_TUTORIAL + 1,
	UI_EXIT = HEALTH_BAR_FRAME + 1,
	UI_HELP = UI_EXIT + 1,
	UI_MUSIC = UI_HELP + 1,
	UI_SFX = UI_MUSIC + 1,
 	UI_PAUSE = UI_SFX + 1,
  	// menu:
	MENUUI_PLAY = UI_PAUSE + 1,
	MENUUI_HELP = MENUUI_PLAY + 1,
	MENUUI_TITLE = MENUUI_HELP + 1,
	MENUUI_PIANO = MENUUI_TITLE + 1,
	MENUUI_PIANO_WHITE = MENUUI_PIANO + 1,
	MENUUI_LOAD = MENUUI_PIANO_WHITE + 1,
	// Screens:
	SHOP_SCREEN = MENUUI_LOAD + 1,
	SHOP_SCREEN_SPEECH_1 = SHOP_SCREEN + 1,
	SHOP_SCREEN_SPEECH_2 = SHOP_SCREEN_SPEECH_1 + 1,
	SHOP_SCREEN_SPEECH_3 = SHOP_SCREEN_SPEECH_2 + 1,
	SHOP_SCREEN_SPEECH_4 = SHOP_SCREEN_SPEECH_3 + 1,
	SHOP_SCREEN_SPEECH_5 = SHOP_SCREEN_SPEECH_4 + 1,
	SHOP_SCREEN_SPEECH_6 = SHOP_SCREEN_SPEECH_5 + 1,
	NEXT_SIGN = SHOP_SCREEN_SPEECH_6 + 1,
	// Debug:
	WHITE = NEXT_SIGN + 1,
	BLACK = WHITE + 1,
	CIRCLE = BLACK + 1,
	WARNING_CIRCLE = CIRCLE + 1,
	BOX = WARNING_CIRCLE + 1,
	GRID = BOX + 1,
	// Enemy navigator
	NAVIGATOR = GRID + 1,
	// TEXT
	FONT_SYLFAEN = NAVIGATOR + 1,
	FONT_YAHEI_THICC = FONT_SYLFAEN + 1,
	FONT_SMALL_FONTS = FONT_YAHEI_THICC + 1,
	FONT_VERDANA = FONT_SMALL_FONTS + 1,
	FONT_VERDANA_BOLD = FONT_VERDANA + 1,
	
	DIFFUSE_COUNT = FONT_VERDANA_BOLD + 1,
};
const int diffuse_count = (int)DIFFUSE_ID::DIFFUSE_COUNT;
const DIFFUSE_ID shiny_diffuses[7] = {
	DIFFUSE_ID::SHINY_BUTTON,
	DIFFUSE_ID::SHINY_COIN,
	DIFFUSE_ID::SHINY_GEM,
	DIFFUSE_ID::SHINY_HOOK,
	DIFFUSE_ID::SHINY_MARBLE,
	DIFFUSE_ID::SHINY_NAIL,
	DIFFUSE_ID::SHINY_SPOON,
};

enum class NORMAL_ID {
	// Misc:
	FLAT = 0,
	TEST = FLAT + 1,
	SPHERE = TEST + 1,
	ROUNDED = SPHERE + 1,
	// Obstacles:
	MAPLE_TREE = ROUNDED + 1,
	SPRUCE_TREE = MAPLE_TREE + 1,
	FIR_TREE = SPRUCE_TREE + 1,
	PINE_TREE1 = FIR_TREE + 1,
	PINE_TREE2 = PINE_TREE1 + 1,
	CHESTNUT_TREE = PINE_TREE2 + 1,
	CHESTNUT_TREE_ADD = CHESTNUT_TREE + 1,
	PINE_TREE1_BARE = CHESTNUT_TREE_ADD + 1,
	PINE_TREE2_BARE = PINE_TREE1_BARE + 1,
	CHESTNUT_TREE_BARE = PINE_TREE2_BARE + 1,
	CHESTNUT_TREE_BARE_ADD = CHESTNUT_TREE_BARE + 1,
	CAMP_FIRE = CHESTNUT_TREE_BARE_ADD + 1,
	// Props:
	FOLIAGE_ATLAS = CAMP_FIRE + 1,
	GRASS_TUFT = FOLIAGE_ATLAS + 1,
	BUSH2 = GRASS_TUFT + 1,
	// Ground Layers:
	GRASS = BUSH2 + 1,
	DIRT = GRASS + 1,
	PLATFORM = DIRT + 1,
	WATER1 = PLATFORM + 1,
	WATER2 = WATER1 + 1,
	HILL1 = WATER2 + 1,
	HILL2 = HILL1 + 1,
	MOUNTAIN = HILL2 + 1,
	NORMAL_COUNT = MOUNTAIN + 1,
};

enum class ROOM_ID {
	//MENU
	MENU_ROOM = 0,

	//TUTORIAL
	MOVEMENT_TUTORIAL = MENU_ROOM + 1,
	ATTACK_TUTORIAL = MOVEMENT_TUTORIAL + 1,
	SWITCH_CHARACTER_TUTORIAL = ATTACK_TUTORIAL + 1,
	CLEAR_ROOM_TUTORIAL = SWITCH_CHARACTER_TUTORIAL + 1,

	//GAME
	WIDE_ROOM = CLEAR_ROOM_TUTORIAL + 1 ,
	SQUIRREL_ROOM = WIDE_ROOM + 1,
	SQUARE_ROOM = SQUIRREL_ROOM + 1,

	ROOM_COUNT = SQUARE_ROOM + 1,
};

const int normal_count = (int)NORMAL_ID::NORMAL_COUNT;

const int room_count = (int)ROOM_ID::ROOM_COUNT;

const int texture_count = std::max(diffuse_count, normal_count);

enum class FONT : int {
	SYLFAEN = 0,
	YAHEI_THICC,
	SMALL_FONTS,
	VERDANA,
	VERDANA_BOLD,

	
	FONT_COUNT
};

struct Text 
{
	std::string content;
	vec2 start_position;
	bool is_visible = false;
	
	// Member dictating text appearance
	FONT font = FONT::SYLFAEN;
	std::vector<int> newlines; // Indices at which a newline will be added when rendering
	float size_multiplier = 6.f;
	float newline_height = 0.8f; // vertical offset caused by newline, as a percentage of the char height
	// Text color
	// Each pair contains start and stop index of characters and the color the characters within that range should have
	// std::vector<std::pair<ivec2, vec3>> color_ranges;
	vec4 text_color = {1, 1, 1, 1}; // Set alpha to 0 to use text color present in font map
	std::function<void(Text&, float)> update_func = nullptr;
	
	// Members dictating the text's background box's appearance
	bool is_box_enabled = true;
	BBox text_bounds = {}; // Bounds of the text itself. Takes into account the extremes of the text and chars.
	vec2 box_margin = {0.02f, 0.03f}; // Margins (in screen space) added to the text's box
	vec4 box_color = {0, 0, 0, 0.6};

	// Whether render data should be recomputed. Should be set true when changes are made to other attribs
	// Updating can probably be optimized a lot: Usually (always?), only transforms need updating, while currently we also recomputes the charinfo.
	bool update_render_data = true;
	bool create_new_buffers = true;
	bool perform_repositioning = true;


	// The VBOs used in rendering the text. 
	// The first is for the texture sheet, the second for transforms
	GLuint charinfoVBO = 0, transformVBO = 0;

	Text(std::string str, vec2 position) 
		: start_position(position)
	{
		// Remove newlines
		content.reserve(str.size());
		for (int i = 0; i < str.size(); i++) {
			const char c = str[i];
			if (c == '\n') {
				newlines.push_back(i - (int) newlines.size());
			} else {
				content.push_back(c);
			}
		}

		// Assign valid identifiers to the VBO members
		glGenBuffers(1, &charinfoVBO);
		glGenBuffers(1, &transformVBO);
	}
};

enum class EFFECT_ID {
	HEALTH_BAR = 0,
	UI_ELEMENTS = HEALTH_BAR + 1,
	TEXT = UI_ELEMENTS + 1,
	COLOR = TEXT + 1,
	TEXTURED = COLOR + 1,
	SCREEN = TEXTURED + 1,
	EFFECT_COUNT = SCREEN + 1
};
const int effect_count = (int)EFFECT_ID::EFFECT_COUNT;

enum class GEOMETRY_ID {
	// Custom meshes:
	LAKE = 0,
	PLANE = LAKE + 1,
	SUBDIVIDED_SQUARE = PLANE + 1,
	// Defined in render_system_init:
	SPRITE = SUBDIVIDED_SQUARE + 1,
	EGG = SPRITE + 1,
	DEBUG_LINE = EGG + 1,
	HEALTH_BAR = DEBUG_LINE + 1,
	SCREEN_TRIANGLE = HEALTH_BAR + 1,
	GEOMETRY_COUNT = SCREEN_TRIANGLE + 1
};
const int geometry_count = (int)GEOMETRY_ID::GEOMETRY_COUNT;


struct TextureAtlas // Not the best, I did this quickly
{
	static rapidjson::Document loadFromJSONFile(std::string room_file_name);
	DIFFUSE_ID diffuse_id;
	NORMAL_ID normal_id;
	vec2 size_px = { -100.f, -100.f };
	int last_picked_type = 0;
	std::vector<vec4> frames;
	std::vector<rangeInt> type_ranges; // List of range of frames that belong to a specific type
	std::vector<int> type_weights; // Not actually weights, just a list of indices into type_ranges with duplicates to represent weight
								   // e.g. { 0,0,0,1,1,2 } - type 0 has 50% chance of being picked, type 1 has 33%, and type 2 has 16%
	vec4 pick_frame(int type_index = -1) {
		if (type_index == -1) {
			int rand_weight_index = rand() % type_weights.size();
			type_index = type_weights[rand_weight_index];
		}
		last_picked_type = type_index;
		rangeInt range = type_ranges[type_index];
		int rand_frame_index = (range.min == range.max) ? (range.min) : (rand() % ((range.max + 1) - range.min) + range.min);
		return frames[rand_frame_index];
	}
	TextureAtlas(DIFFUSE_ID diffuse_id, NORMAL_ID normal_id, std::vector<rangeInt> type_ranges, std::vector<int> type_weights) :
		diffuse_id(diffuse_id), normal_id(normal_id), type_ranges(type_ranges), type_weights(type_weights) {}
};

struct RenderRequest {
	DIFFUSE_ID diffuse_id = DIFFUSE_ID::DIFFUSE_COUNT;
	NORMAL_ID normal_id = NORMAL_ID::FLAT;

	EFFECT_ID effect_id = EFFECT_ID::TEXTURED;
	GEOMETRY_ID geometry_id = GEOMETRY_ID::SPRITE;

	NORMAL_ID normal_add_id = NORMAL_ID::NORMAL_COUNT; // Optional
	DIFFUSE_ID mask_id = DIFFUSE_ID::DIFFUSE_COUNT; // Optional

	vec4 diffuse_coord_loc = { 0.f, 0.f, 1.f, 1.f }; // x,y is relative offset and z,w is the scale factor
	vec4 normal_coord_loc = { 0.f, 0.f, 1.f, 1.f };  // repeate_texture is now within these values, since adjusting z,w scales the texture
	vec4 normal_add_coord_loc = { 0.f, 0.f, 1.f, 1.f };
	vec4 mask_coord_loc = { 0.f, 0.f, 1.f, 1.f };
	bool is_normal_sprite_sheet = false; // Set this to true if there is a normal sprite sheet that matches diffuse sprite sheet

	bool casts_shadow = true;
	float wind_affected = 0.f;
	bool is_ground_piece = false;
	bool is_debug_component = false;
	
	float extrude_size = 0.f;

	// Uniforms
	int num_lights_affecting = 0;
	int lights_affecting[16] = {}; // Needs to be same size as MAX_POINT_LIGHTS in render_system.hpp

	bool flip_texture = false;
	float transparency = 0.f; // Dithered for vertical sprites, alpha blending subtract factor for ground pieces. 0 means fully opaque
	float transparency_offset = -100.f; // Vertical offset for vertical transparency sigmoid curve (i.e. base of trees stay less transparent)

	vec3 add_color = vec3(0.f);
	vec3 multiply_color = vec3(1.f);
	vec3 ignore_color = vec3(-1.f);

	vec3 specular = vec3(0.5f);
	float shininess = 50.f; // Higher means sharper/tighter specular bright spot

	bool is_TBN_updated = false;
	mat4 TBN; // Caching for optimization
};

//struct Material { // For reference
//	vec3 ambient;
//	vec3 diffuse;
//	vec3 specular;
//	float shininess;
//};

struct SpriteSheetAnimation {
	int num_sprites;
	
	// List of lists. Each sublist is a list of indexes in the spritesheet which make up an animation
	std::vector<std::vector<int>> tracks;
	// The interval between each frame in a track. Unique to each track
	std::vector<float> track_intervals;
	// Current track ("track" = set of frames that make up an animation)
	int track_index = 0;
	// current sprite in current track
	int sprite_index = 0;
	// Time elapsed displaying current frame
	float frame_elapsed_time = 0.f;

    // Whether the animation should loop or not
	bool loop = true;

	// Does the entity use this animation or is it just for reference. i.e. particle generators only hold a reference
	// for particles to use. Necessary to prevent animation_system.cpp trying to update the particle generator's animations
	bool is_reference = false; // Hacky but it's good enough I think
	bool is_updated = false; // Hacky as well but a good optimization

	SpriteSheetAnimation(
		int n_sprites,
		std::vector<std::vector<int>> track_list,
		std::vector<float> track_interval_list
	)
		: num_sprites(n_sprites)
		, tracks(track_list)
		, track_intervals(track_interval_list)
	{
		// Make sure that each track has a defined "frame rate"
		assert(track_list.size() == track_interval_list.size());
	}

	// Pass force=true to reset track stats, even for same animation track
	void set_track(int new_track, bool force_reset=false) {
		if (new_track == this->track_index && !force_reset) return;
		
		// Prevent going out of bounds. 
		// This won't immediately lead to crash, so assert here for easier debugging
		assert(new_track < tracks.size());
		
		this->sprite_index = 0;
		this->frame_elapsed_time = 0.f;
		this->track_index = new_track;
	}
};

struct ParticleGenerator
{
	bool enabled = true;

	vec3 position = { 0,0,0 };
	vec3 direction = { 0,0,0 };
	float gravity_multiplier = 1; // How much does gravity affect the particles

	vec2 scale = vec2(30.f);
	float scale_spread = 10.f;
	float angle = -10.f;

	float frequency = 2.f; // How many to spawn per second
	float time_since_last = 0.f; // Time since last particle spawned in seconds used by particle_system. Not to be set

	range particle_lifetime_ms = { 5000.f, 5000.f }; // Not to be confused with generator lifetime given in the create function
	range max_angular_spread = { 0.f, M_PI / 4.f }; // In radians. Cone with inner and outer radius (keep first = 0.f for most)
	range speed = { 100.f, 100.f };
	range angular_speed = { 0.f, M_PI / 4.f }; // Angular speed in a random direction. If min == max it will always spin at that

	vec3 max_rect_spread = { 0.f, 0.f, 0.f };
	//vec3 random_rect_spread = { 100.f, 100.f, 100.f }; // Increase this to make particles spawn in a (uniform rect) random position

	DIFFUSE_ID diffuse_id = DIFFUSE_ID::DIFFUSE_COUNT;
	vec3 multiply_color = { 0,0,0 };
	vec3 ignore_color = { -1,-1,-1 };
	bool casts_shadow = false;
	float transparency = 0.f;

	bool is_random_color = false;
	//SpriteSheetAnimation* sprite_sheet = nullptr;
	//vec3 max_multiply_color_offset = {0.1f, 0.f, 0.f};
};

struct Particle
{
	float life_remaining_ms = 1000.f;
	vec3 forces = { 0,0,0 };
};
