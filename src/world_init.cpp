#include "world_init.hpp"
#include "tiny_ecs_registry.hpp"
#include "../ext/rapidjson/document.h"
#include "../ext/pathfinder/AStar.hpp"
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/random.hpp>
#include "world_system.hpp"


std::array<std::array<vec4, foliage_count>, texture_type_count> foliage_atlas_locs;

const std::array<std::string, 100> foliage_names = {
	// Flowers:
	"flower1.png",
	"flower2.png",
	"flower3.png",
	"flower4.png",
	"flower5.png",
	"flower6.png",
	"flower7.png",
	"flower8.png",
	"flower9.png",
	"flower10.png",
	"flower11.png",
	"flower12.png",
	"flower13.png",
	"flower14.png",
	"flower15.png",
	"flower16.png",
	"flower17.png",
	"flower18.png",
	"flower19.png",
	"flower20.png",
	"flower21.png",
	"flower22.png",
	"flower23.png",
	"flower24.png",
	"flower25.png",
	"flower26.png", // 25
	// Foliage:
	"bush1.png",
	"bush2.png",
	"bush3.png",
	"bush4.png",
	"bush5.png",
	"bush6.png", // 31
	// Bush Bare:
	"bush_bare.png", // 32
	// Logs:
	"log1.png",
	"log2.png", // 34
	// Rocks:
	"rock1.png",
	"rock2.png",
	"rock4.png",
	"rock5.png",
	"rock6.png",
	"rock7.png",
	"rock8.png",
	"rock9.png", // 42
	// Grass Tufts:
	"grass_tuft1.png",
	"grass_tuft2.png",
	"grass_tuft3.png",
	"grass_tuft4.png",
	"grass_tuft5.png",
	"grass_tuft6.png",
	"grass_tuft7.png",
	"leaves.png", // 50
	// Water Plants:
	"water_plant1.png",
	"water_plant2.png",
	"water_plant3.png",
	"lily_pad1.png",
	"lily_pad2.png",
	"lily_pad3.png", // 56
	// Mushrooms:
	"mushroom1.png",
	"mushroom2.png",
	"mushroom3.png",
	"mushroom4.png",
	"mushroom5.png",
	"mushroom6.png",
	"mushroom7.png",
	"mushroom8.png",
	"mushroom9.png", // 65
	// Pumpkin & weird flowers:
	"pumpkin1.png",
	"weird_flower1.png",
	"weird_flower2.png", // 68
};

std::vector<rangeInt> type_ranges = {
	{ 0, 25 }, // Flowers - 0 // Sunflowers: 19, 
	{ 26, 31 }, // Foliage - 1
	{ 33, 34 }, // Logs - 2
	{ 35, 42 }, // Rocks - 3
	{ 43, 50 }, // Grass tufts - 4
	{ 51, 56 }, // Water plants - 5
	{ 57, 65 }, // Mushrooms - 6
	{ 66, 68 }, // Pumpkin & weird flowers - 7
	{ 32, 32 }, // Bush bare - 8
};

int water_type_index = 5; // Don't add to type_weights below

//std::vector<int> type_weights = { 0, }; 
//std::vector<int> type_weights = { 0,0,8,1,1,1,1,1,1,1,1,1,1,1,3,6 };

std::vector<std::vector<int>> type_weights_list = {
	{ 2,3,4,4,6,7,8 },
	{ 0,0,8,1,1,1,1,1,1,1,1,1,1,1,3,6 },
	{ 0 },
	{ 1 },
	{ 3 },
	{ 4 },
	{ 6 },
};

Motion& createMotion(Entity e, uint32 type, vec2 pos, vec2 scale, float max_speed) {
	Motion& motion = registry.motions.emplace(e);
	motion.position = pos;
	motion.max_speed = max_speed;
	motion.moving = (max_speed == 0.f) ? false : true;
	motion.scale = scale;
	motion.sprite_offset = (type & (BEING_MASK | OBSTACLE_MASK | PROP_MASK)) ? vec2(0, -scale.y / 2.f) : vec2(0, 0);
	motion.radius = abs(scale.x) / 4.f;
	motion.type_mask = type;
	// but give them cell coords?
	// Spatial grid contains BEINGS, OBSTACLES, PROJECTILES, PICKUPABLES, DOORS, and POLYGONS (in createComplexPolygon)
	if (type != UNCOLLIDABLE_MASK && type != MELEE_ATTACK_MASK && type != POLYGON_MASK && type != PARTICLE_MASK && type != PROP_MASK) {
		SpatialGrid& spatial_grid = SpatialGrid::getInstance();
		motion.cell_coords = spatial_grid.get_grid_cell_coords(motion.position);
		if (!spatial_grid.are_cell_coords_out_of_bounds(motion.cell_coords)) {
			motion.cell_index = spatial_grid.add_entity_to_cell(motion.cell_coords, e);
			if (type == OBSTACLE_MASK) {
				Room& room = registry.rooms.components[0];
				room.generator.addCollision({ motion.cell_coords.x, motion.cell_coords.y });
			}
		} else { // Testing not even putting them in the grid at all:
			//motion.cell_index = spatial_grid.add_entity_to_cell({0,0}, e); // Hacky fix for when spawned entities are outside of grid
			//motion.cell_coords = {0,0};
		}
	}
	return motion;
}

// TODO: Finish this for better optimization
RenderRequest& createRenderRequest(Entity e, Motion& motion, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id, bool casts_shadow) {
	RenderRequest& render_request = registry.renderRequests.insert(e, { diffuse_id, normal_id });
	return render_request;
}

Entity createPlayer(Entity entity, vec2 pos, CharacterState char_state) {
	Motion& motion = createMotion(entity, PLAYER_MASK | BEING_MASK, pos, vec2(150.f), 500.f);
	motion.accel_rate = 3000.f;

	auto [max_hp, current_hp, character] = char_state;
	motion.scale = (character == PLAYER_CHARACTER::GRETEL) ? vec2(130.f) * vec2(1.f, 1.05f) : vec2(150.f) * vec2(1.f, 1.05f);
	motion.sprite_offset = vec2(0, -motion.scale.y / 2.f);
	motion.sprite_normal = { 0,cos(0.25),sin(0.25) };

	// Set player hit points
	registry.healthies.emplace(entity, max_hp, current_hp);
	registry.players.emplace(entity, character);

	auto diffuse_id = (character == PLAYER_CHARACTER::GRETEL) ? DIFFUSE_ID::GRETEL : DIFFUSE_ID::HANSEL;
	RenderRequest& render_request = registry.renderRequests.insert(entity, { diffuse_id, NORMAL_ID::ROUNDED });
	render_request.specular = vec3(0.f);
	render_request.shininess = 1.f;
	//render_request.normal_add_id = NORMAL_ID::TEST;
	// Could be a specific Hansel/Gretel normal map in future

	// Setup animations
	std::vector<int> walk_track = { 0,1,2,3 };
	float walk_interval = 250.f;
	std::vector<int> idle_track = { 0 };
	float idle_interval = 10000.f;

	auto all_tracks = { walk_track, idle_track };
	std::vector<float> track_intervals = { walk_interval, idle_interval };
	registry.spriteSheets.emplace(entity, 4, all_tracks, track_intervals);

	return entity;
}

Entity createParticleGenerator(vec3 position, vec3 direction, vec2 scale, float frequency, DIFFUSE_ID diffuse_id, float transparency,
	vec3 multiply_color, float generator_lifetime_ms, float gravity_multiplier, range particle_lifetime_ms, range max_angular_spread,
	range speed, range angular_speed, vec3 max_rect_spread, float scale_spread, vec3 ignore_color)
{
	auto entity = Entity();

	ParticleGenerator& generator = registry.particleGenerators.emplace(entity);
	generator.position = position;
	generator.direction = normalize(direction);
	generator.scale = scale;
	generator.frequency = frequency;
	generator.diffuse_id = diffuse_id;
	generator.transparency = transparency;
	generator.multiply_color = multiply_color;
	generator.ignore_color = ignore_color;

	if (generator_lifetime_ms > 0.f) {
		registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::PARTICLE_GENERATOR, generator_lifetime_ms });
	}
	generator.gravity_multiplier = gravity_multiplier;
	generator.particle_lifetime_ms = particle_lifetime_ms;
	generator.max_angular_spread = max_angular_spread;
	generator.speed = speed;
	generator.angular_speed = angular_speed;
	generator.max_rect_spread = max_rect_spread;
	generator.scale_spread = scale_spread;

	return entity;
}

float random_float() {
	return (rand() % 10000) / 10000.f - 0.5f; // returns [-0.5, 0.5]
}

float random_sign() {
	return (rand() % 2) * 2.f - 1.f; // returns -1 or 1
}

float random_range(range range) { // Probably doesn't work well if not using a float
	return range.min + (rand() % 10000) / 10000.f * (range.max - range.min);
}

Entity createParticle(Entity generator_entity, ParticleGenerator& generator) {
	auto entity = Entity();

	// Old for reference:
	//mat4 rot_spread = glm::rotate(mat4(1.0), random_range(generator.max_angular_spread), vec3(0.0, 1.0, 0.0)); // Cone spread angle using range
	//mat4 rot_cone = glm::rotate(mat4(1.0), random_float() * (2.f * M_PI), vec3(0.0, 0.0, 1.0)); // Perfectly conical randomness
	//vec3 velocity = (rot_cone * rot_spread * vec4(generator.direction, 0.0)) * random_range(generator.speed);

	mat4 rot1 = glm::rotate(mat4(1.0), random_range(generator.max_angular_spread) * random_sign(), vec3(1.0, 0.0, 0.0));
	mat4 rot2 = glm::rotate(mat4(1.0), random_range(generator.max_angular_spread) * random_sign(), vec3(0.0, 1.0, 0.0));
	mat4 rot3 = glm::rotate(mat4(1.0), random_range(generator.max_angular_spread) * random_sign(), vec3(0.0, 0.0, 1.0));

	vec3 velocity = (rot3 * rot2 * rot1 * vec4(generator.direction, 0.0)) * random_range(generator.speed);

	float angular_velocity = random_range(generator.angular_speed) * random_sign();

	vec3 random_offset = generator.max_rect_spread * vec3(random_float(), random_float(), random_float());
	vec3 position = generator.position + random_offset;
	vec2 scale = generator.scale + vec2(random_float() * generator.scale_spread);
	
	Motion& motion = createMotion(entity, PARTICLE_MASK, vec2(position), scale, 1.f);
	motion.sprite_offset = { 0.f, -position.z };
	motion.sprite_offset_velocity = velocity.z;
	motion.gravity_multiplier = generator.gravity_multiplier;

	if (registry.bezierCurves.has(generator_entity)) {
		BezierCurve& curve = insertBezierCurve(entity, position, normalize(velocity));
		curve.curve_duration_ms = 5000.f;
	} else {
		motion.velocity = vec2(velocity);
		motion.mass = 2.f;
		motion.forces = { -100.f, 0.f, 0.f };
		motion.friction = 1.f;
		motion.angle = (generator.angle == -10.f) ? (random_float() * M_PI) : generator.angle;
		motion.angular_velocity = angular_velocity;
	}

	Particle& particle = registry.particles.insert(entity, { random_range(generator.particle_lifetime_ms) });

	if (registry.spriteSheets.has(generator_entity)) {
		SpriteSheetAnimation sprite_sheet = registry.spriteSheets.get(generator_entity); // No ampersand so that it copies instead
		sprite_sheet.is_reference = false;
		registry.spriteSheets.insert(entity, sprite_sheet);
	}

	vec3 multiply_color = generator.multiply_color;

	if (generator.is_random_color) {
		int r = rand() % 100;
		if (r < 33) {
			multiply_color = vec3(1.f, 0.84f, 0.f);
		} else if (r < 66) {
			multiply_color = vec3(1.f, 0.f, 1.f);
		} else if (r < 100) {
			multiply_color = vec3(0.f, 0.f, 1.f);
		}
	}
	RenderRequest& render_request = registry.renderRequests.insert(entity, { generator.diffuse_id });
	render_request.multiply_color = multiply_color;
	render_request.ignore_color = generator.ignore_color;
	render_request.casts_shadow = generator.casts_shadow;
	render_request.transparency = generator.transparency;

	return entity;
}

Entity createFood(vec2 pos) { // removed renderer->getMesh as it is unnecessary for sprites, only for geometry
	auto entity = Entity();
	
	Motion& motion = createMotion(entity, PICKUPABLE_MASK, pos, vec2(40.f), 0.f); // Passing speed = 0 sets moving = false
	motion.sprite_normal = { 0, 1 / sqrt(2), 1 / sqrt(2) };

	registry.pickupables.insert(entity, {PICKUP_ITEM::FOOD});

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::MEAT });
	render_request.casts_shadow = false;
	render_request.ignore_color = vec3(-10.f);

	createSparkleEffect(pos, entity, (vec3(250, 128, 114) / 255.f));

	return entity;
}

Entity createUpgradePickupable(vec2 pos, Upgrades::PlayerUpgrade* upgrade) {
	auto entity = Entity();
	
	Motion& motion = createMotion(entity, PICKUPABLE_MASK, pos, vec2(40.f), 0.f);
	Pickupable& pickup = registry.pickupables.insert(entity, { PICKUP_ITEM::UPGRADE });
	pickup.on_pickup_callback = [upgrade](){ 
		Upgrades::apply_upgrade(upgrade); 
		createUI_UPGRADE(upgrade->id);
	};
	
	RenderRequest& render_request = registry.renderRequests.insert(entity, { upgrade->icon_diffuse });
	render_request.casts_shadow = false;
	render_request.ignore_color = vec3(-10.f);

	createSparkleEffect(pos, entity, (vec3(204, 164, 61) / 255.f) * 1.2f);

	return entity;
}

Entity createShiny(vec2 pos) {
	auto entity = Entity();
	
	Motion& motion = createMotion(entity, PICKUPABLE_MASK, pos, vec2(28.f), 0.f);
	motion.angle = (rand() % 10 - 5) / 10.f;
	
	registry.pickupables.insert(entity, {PICKUP_ITEM::SHINY});
	
	// Size of shiny_array: https://stackoverflow.com/questions/4108313/how-do-i-find-the-length-of-an-array
	int num_shinies = sizeof(shiny_diffuses)/sizeof(*shiny_diffuses);
	auto diffuse = shiny_diffuses[rand() % num_shinies];
	RenderRequest& render_request = registry.renderRequests.insert(entity, {diffuse});
	render_request.casts_shadow = false;
	render_request.ignore_color = vec3(-10.f);

	createSparkleEffect(pos, entity, (vec3(70, 130, 180) / 255.f));

	return entity;
}

Entity createShinyHeap(vec2 pos) {
	auto entity = Entity();
	
	createMotion(entity, PICKUPABLE_MASK, pos, vec2(40.f), 0.f);
	registry.pickupables.insert(entity, {PICKUP_ITEM::SHINY_HEAP});
	registry.renderRequests.insert(entity, {DIFFUSE_ID::SHINY_COIN_PILE});

	return entity;
}

Entity createChest(vec2 pos) {
	auto entity = Entity();
	
	Motion& motion = createMotion(entity, OBSTACLE_MASK, pos, vec2(80.f), 0.f);
	motion.radius = motion.scale.x / 2.f;
	registry.healthies.emplace(entity, 1);
    registry.chests.emplace(entity);

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::FURNITURE });
	
	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::CHEST });
	render_request.ignore_color = vec3(-10.f);
	render_request.add_color = -vec3(0.2f);

	createSparkleEffect(pos, entity, (vec3(204, 164, 61) / 255.f) * 1.2f);

	return entity;
}

Entity createChestOpening(vec2 pos) {
	float anim_length = 2000.f;
	float effect_duration = anim_length + 500;
	
	auto entity = Entity();
	
	Motion& motion = createMotion(entity, OBSTACLE_MASK, pos, vec2(80.f), 0.f);
	motion.radius = motion.scale.x / 2.f;
	registry.healthies.emplace(entity, 1);

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::FURNITURE });
	
	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::CHEST_OPENING });
	render_request.ignore_color = vec3(-10.f);
	render_request.add_color = -vec3(0.2f);

		// Setup animations
	std::vector<int> anim_frames = {0, 1, 2, 3, 4, 5, 6, 7};
	float anim_interval = anim_length / anim_frames.size();
	auto all_anim_tracks = {anim_frames};
	std::vector<float> all_anim_intervals = {anim_interval};
	auto& animation = registry.spriteSheets.emplace(entity, 8, all_anim_tracks, all_anim_intervals);
	animation.loop = false;

	// Register as effect
	registry.tempEffects.insert(entity, {TEMP_EFFECT_TYPE::CHEST_OPENING, effect_duration});

	return entity;
}

Entity createBreadcrumbEatingEffect(vec2 position) {
	auto entity = Entity();
	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position, {150.f, 150.f}, 0.f);
	assert(motion.parent == std::nullopt);

	// setup animation
	float anim_length = 500.f;
	std::vector<int> frames = {0, 1, 2, 3, 4, 5, 6, 7};
	float anim_interval = anim_length / frames.size();
	auto all_anim_tracks = {frames};
	std::vector<float> all_anim_intervals = {anim_interval};
	registry.spriteSheets.emplace(entity, 8, all_anim_tracks, all_anim_intervals);

	registry.renderRequests.insert(entity, { DIFFUSE_ID::FLYING_BREADCRUMBS });

	registry.tempEffects.insert(entity, {TEMP_EFFECT_TYPE::TEMP_EFFECT_COUNT, anim_length});

	return entity;
}

Entity createPush(vec2 position, Entity player) {
	int damage = Upgrades::get_gretel_m2_damage();
	float knockback = Upgrades::get_gretel_m2_knockback();
	float anim_length = 300.f;

	auto entity = Entity();
	Motion& motion = createMotion(entity, MELEE_ATTACK_MASK, position, Upgrades::get_gretel_m2_area(), 0.f);
	registry.motions.get(player).add_child(entity);
	motion.parent = player;
	motion.sprite_normal = {0,0,1};
	motion.sprite_offset.y = registry.motions.get(player).sprite_offset.y;

	registry.attacks.insert(entity, {damage, knockback});

	// Setup animations
	std::vector<int> attack_anim_frames = {0, 1, 2, 3};
	float attack_anim_interval = anim_length / attack_anim_frames.size();
	auto all_anim_tracks = {attack_anim_frames};
	std::vector<float> all_anim_intervals = {attack_anim_interval};
	registry.spriteSheets.emplace(entity, 4, all_anim_tracks, all_anim_intervals);

	// Register as effect
	registry.tempEffects.insert(entity, {TEMP_EFFECT_TYPE::PLAYER_PUSH_ATTACK, anim_length});

	registry.renderRequests.insert(entity, { DIFFUSE_ID::PUSH_EFFECT });

	return entity;
}

Entity createWitchPush(vec2 position, Entity owner) {
	auto entity = Entity();
	Motion& motion = createMotion(entity, MELEE_ATTACK_MASK, position, vec2(500.f), 0.f);
	registry.motions.get(owner).add_child(entity);
	motion.parent = owner;
	motion.sprite_normal = { 0,0,1 };
	motion.sprite_offset.y = registry.motions.get(owner).sprite_offset.y;

	registry.attacks.insert(entity, { 4, 2000.f });

	// Setup animations
	float anim_length = 300.f;
	std::vector<int> attack_anim_frames = { 0, 1, 2, 3 };
	float attack_anim_interval = anim_length / attack_anim_frames.size();
	auto all_anim_tracks = { attack_anim_frames };
	std::vector<float> all_anim_intervals = { attack_anim_interval };
	registry.spriteSheets.emplace(entity, 4, all_anim_tracks, all_anim_intervals);

	// Register as effect
	registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::PLAYER_PUSH_ATTACK, anim_length });

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::PUSH_EFFECT });
	render_request.multiply_color = vec3(0.f, 0.f, 1.f);
	render_request.ignore_color = vec3(0.f, 0.f, 1.f);

	return entity;
}

Entity createAttack(vec2 position, vec2 orientation, Entity player) {
	float anim_length = Upgrades::get_gretel_m1_cd();
	int damage = Upgrades::get_gretel_m1_damage();
	float knockback = Upgrades::get_gretel_m1_knockback();
	
	auto entity = Entity();

	// Setting initial motion values
	Motion& motion = createMotion(entity, MELEE_ATTACK_MASK, position, Upgrades::get_gretel_m1_area(), 0.f);
	// Testing:
	//motion.scale = vec2(40.f);

	// Make child of player motion. Will cause attack sprite to follow player
	registry.motions.get(player).add_child(entity);
	motion.parent = player;
	motion.sprite_normal = {0,0,1}; // You can change it back, but I think it's better for it to be horizontal with the ground
									// Or maybe we can make it 45deg
	motion.sprite_offset = registry.motions.get(player).sprite_offset;

	// Set damage
	registry.attacks.insert(entity, {damage, knockback});

	// Setup animations
	std::vector<int> attack_anim_frames = {0, 1, 2, 3};
	float attack_anim_interval = anim_length / attack_anim_frames.size();
	auto all_anim_tracks = {attack_anim_frames};
	std::vector<float> all_anim_intervals = {attack_anim_interval};
	registry.spriteSheets.emplace(entity, 4, all_anim_tracks, all_anim_intervals);
	
	// Register as effect
	registry.tempEffects.insert(entity, {TEMP_EFFECT_TYPE::PLAYER_MELEE_ATTACK, anim_length});

	auto& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::ATTACK }); // ATTACK // ROCK
	
	// if mouse is pointing up
	if (orientation.y < 0.f) {
		motion.angle = -acos(orientation.x);
	}
	// else mouse is pointing down
	else {
		motion.angle = acos(orientation.x);
	}
	return entity;
}

Entity createHealingEffect(vec2 position, Entity parent) {
	auto entity = Entity();

	// Setting initial motion values
	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position + vec2(0.f,5.f), vec2(80.f, 90.f), 0.f);
	motion.sprite_offset.y += -100.f;

	// Make child of player motion. Will cause attack sprite to follow player
	registry.motions.get(parent).add_child(entity);
	motion.parent = parent;

	// Setup animations
	float animation_length = 1000.f;
	std::vector<int> attack_anim_frames = { 0, 1, 2, 3, 4 };
	float attack_anim_interval = animation_length / attack_anim_frames.size();
	auto all_anim_tracks = {attack_anim_frames};
	std::vector<float> all_anim_intervals = {attack_anim_interval};
	registry.spriteSheets.emplace(entity, 5, all_anim_tracks, all_anim_intervals);

	// Register as effect
	registry.tempEffects.insert(entity, {TEMP_EFFECT_TYPE::HEALING, animation_length});

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::HEALING_EFFECT });
	render_request.ignore_color = vec3(-10.f);
	//render_request.casts_shadow = false;

	return entity;
}

Entity createSparkleEffect(vec2 position, Entity parent, vec3 multiply_color) {
	auto entity = Entity();

	// Setting initial motion values
	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position + vec2(0.f, 5.f), vec2(60.f, 70.f), 0.f);
	motion.sprite_offset.y += -30.f;

	// Make child of player motion. Will cause attack sprite to follow player
	registry.motions.get(parent).add_child(entity);
	motion.parent = parent;

	// Setup animations
	float animation_length = 1000.f;
	std::vector<int> attack_anim_frames = { 0, 1, 2, 3, 4 };
	float attack_anim_interval = animation_length / attack_anim_frames.size();
	auto all_anim_tracks = { attack_anim_frames };
	std::vector<float> all_anim_intervals = { attack_anim_interval };
	registry.spriteSheets.emplace(entity, 5, all_anim_tracks, all_anim_intervals);

	// Register as effect
	//registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::HEALING, animation_length });

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::SPARKLE_EFFECT });
	render_request.ignore_color = vec3(-10.f);
	render_request.multiply_color = multiply_color;

	return entity;
}

Entity createFurniture(vec2 position, vec2 scale, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id)
{
	auto entity = Entity();

	Motion& motion = createMotion(entity, OBSTACLE_MASK, position, scale, 0.f);
	motion.radius = scale.x / 3.f;
	motion.mass = 50.f;

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::FURNITURE });
	
	registry.renderRequests.insert(entity, { diffuse_id, normal_id });

	return entity;
}

Entity createTree(vec2 position, bool is_force_bare)
{
	auto entity = Entity();

	DIFFUSE_ID diffuse_id = DIFFUSE_ID::DIFFUSE_COUNT;
	NORMAL_ID normal_id = NORMAL_ID::FLAT;
	NORMAL_ID normal_add_id = NORMAL_ID::NORMAL_COUNT;
	vec2 tree_scale = SPRUCE_TREE_SCALE;
	vec3 multiply_color = vec3(0.f, 0.7f, 0.f);
	float multiply_color_multiplier = 1.f + random_float()/2.f;
	bool is_bare = false;
	float bare_mult = is_force_bare;

	int r = (rand() % 100) / (9*bare_mult + 1) + (90*bare_mult);
	//r = 88;
	if (r < 25) {
		diffuse_id = DIFFUSE_ID::SPRUCE_TREE; normal_id = NORMAL_ID::SPRUCE_TREE;
		tree_scale = SPRUCE_TREE_SCALE;
	} else if (r < 50) {
		diffuse_id = DIFFUSE_ID::FIR_TREE; normal_id = NORMAL_ID::FIR_TREE;
		tree_scale = FIR_TREE_SCALE;
	} else if (r < 65) {
		diffuse_id = DIFFUSE_ID::PINE_TREE1; normal_id = NORMAL_ID::PINE_TREE1;
		tree_scale = PINE_TREE1_SCALE;
	} else if (r < 80) {
		diffuse_id = DIFFUSE_ID::PINE_TREE2; normal_id = NORMAL_ID::PINE_TREE2;
		tree_scale = PINE_TREE2_SCALE;
	} else if (r < 90) {
		diffuse_id = DIFFUSE_ID::CHESTNUT_TREE; normal_id = NORMAL_ID::CHESTNUT_TREE; //normal_add_id = NORMAL_ID::CHESTNUT_TREE_ADD;
		tree_scale = CHESTNUT_TREE_SCALE/1.7f;
	} else if (r < 94) {
		diffuse_id = DIFFUSE_ID::PINE_TREE1_BARE; normal_id = NORMAL_ID::PINE_TREE1_BARE;
		tree_scale = PINE_TREE1_SCALE;
		is_bare = true;
	} else if (r < 98) {
		diffuse_id = DIFFUSE_ID::PINE_TREE2_BARE; normal_id = NORMAL_ID::PINE_TREE2_BARE;
		tree_scale = PINE_TREE2_SCALE;
		is_bare = true;
	} else if (r < 100) {
		diffuse_id = DIFFUSE_ID::CHESTNUT_TREE_BARE; normal_id = NORMAL_ID::CHESTNUT_TREE_BARE; //normal_add_id = NORMAL_ID::CHESTNUT_TREE_BARE_ADD;
		tree_scale = CHESTNUT_TREE_BARE_SCALE/1.4f;
		is_bare = true;
	}
	int r2 = rand() % 100;
	if (r2 < 20) {
		multiply_color = vec3(0.8f, 0.f, 0.f);
	} else if (r2 < 80) {
		multiply_color = vec3(0.f, 0.7f, 0.f);
	} else if (r2 < 100) {
		multiply_color = vec3(1.f, 1.f, 0.3f);
	}
	multiply_color *= 2.f;
	
	Motion& motion = createMotion(entity, OBSTACLE_MASK, position, tree_scale, 0.f);
	motion.radius = WorldSystem::TILE_SIZE / 4.f;
	motion.mass = 50.f;
	motion.sprite_offset.y += 10; // Shift down a bit so it looks better

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::TREE });

	RenderRequest& render_request = registry.renderRequests.insert(entity, { diffuse_id, normal_id }); // { DIFFUSE_ID::GRID, normal_id });
	render_request.normal_add_id = normal_add_id;
	render_request.specular = vec3(0.f);
	render_request.multiply_color = multiply_color*multiply_color_multiplier;
	render_request.wind_affected = 1.f;
	render_request.transparency_offset = 40.f;

	if (!is_bare) {
		createParticleGenerator(vec3(motion.position, motion.scale.y - 75.f), vec3(0, 0, -1), vec2(10.f), 1.f, DIFFUSE_ID::LEAF, 0.f, multiply_color, -1.f, 0.f);
	}
	return entity;
}

Entity createPoofEffect(vec3 position, vec3 direction, vec3 multiply_color, float speed_increase_factor, float bigger_poof, bool is_ignore_color)
{
	// Setup smoke particle animations:
	float anim_length0 = 1500.f;
	std::vector<int> frames0 = { 0,1,2,3,4,5,6,7 };
	float anim_interval0 = anim_length0 / frames0.size();
	auto all_anim_tracks0 = { frames0 };
	std::vector<float> all_anim_intervals0 = { anim_interval0 };

	Entity entity = createParticleGenerator(position, direction, vec2(25.f), 100.f*bigger_poof, DIFFUSE_ID::SMOKE, 0.1f, multiply_color, 100.f*bigger_poof,
		1.f, { 2000.f, 3000.f }, { 0.f, M_PI/2.f }, { 50.f + 50.f*speed_increase_factor, 100.f + 100.f*speed_increase_factor });
	ParticleGenerator& generator = registry.particleGenerators.get(entity);
	if (is_ignore_color) {
		generator.ignore_color = vec3(-10.f);
	}
	if (bigger_poof == 50.f) { // if this is a witch
		generator.is_random_color = true;
	}
	// Give the particle generator a sprite sheet as a reference for it's particles to use upon their creation
	auto& sprite_sheet = registry.spriteSheets.emplace(entity, 8, all_anim_tracks0, all_anim_intervals0);
	sprite_sheet.is_reference = true; // Necessary to prevent animation_system.cpp trying to update the particle generator's animations
	sprite_sheet.loop = false;
	sprite_sheet.sprite_index = 3; // Skip the first few frames

	return entity;
}

Entity createSpider(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, SPIDER_SCALE, 200.f);
	motion.radius = abs(motion.scale.x) / 4.f; // Lots of whitespace in the diffuse so making radius even smaller
	motion.mass = 20.f;

	// Setup animations
	std::vector<int> walk_track = { 6,5,4,3,2,1,0 };
	float walk_interval = 150.f;
	std::vector<int> sleep_track = { 8 }; // 7,8
	float sleep_interval = 750.f;
	std::vector<int> idle_track = { 7 };
	float idle_interval = 10000.f;

	auto all_tracks = { walk_track, sleep_track, idle_track };
	std::vector<float> track_intervals = { walk_interval, sleep_interval, idle_interval };
	auto& animation = registry.spriteSheets.emplace(entity, 9, all_tracks, track_intervals);

	// Add hp
	registry.healthies.emplace(entity, SPIDER_BASE_HP);

	// Create as enemy
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::SPIDER);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::SPIDER, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 32.f, 0.f)/255.f;

	return entity;
}

Entity createWorm(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, WORM_SCALE, 200.f);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// Setup animations
	std::vector<int> walk_track = {0,1};
	float walk_interval = 200.f;
	std::vector<int> sleep_track = {3, 4};
	float sleep_interval = 750.f;
	std::vector<int> idle_track = {2};
	float idle_interval = 10000.f;

	auto all_tracks = {walk_track, sleep_track, idle_track};
	std::vector<float> track_intervals = {walk_interval, sleep_interval, idle_interval};
	auto& animation = registry.spriteSheets.emplace(entity, 5, all_tracks, track_intervals);
	animation.set_track(2);

	// Add hp
	registry.healthies.emplace(entity, WORM_BASE_HP);

	// Create as enemy
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::WORM);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::WORM, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;

	return entity;
}

Entity createSnail(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, SNAIL_SCALE, 100.f);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without
	motion.max_speed = 30.f;
	// motion.angled = true;

	// Setup animations
	std::vector<int> walk_track = {0,1};
	float walk_interval = 300.f;

	auto all_tracks = {walk_track};
	std::vector<float> track_intervals = {walk_interval};
	registry.spriteSheets.emplace(entity, 2, all_tracks, track_intervals);

	// Add hp
	registry.healthies.emplace(entity, 100000000); // Practically immortal

	// Create as enemy
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::SNAIL);
	enemy_info.collision_damage = 3;
	enemy_info.invulnerable = true;
	registry.renderRequests.insert(entity, { DIFFUSE_ID::ANGRY_SNAIL, NORMAL_ID::ROUNDED });

	return entity;
}

Entity createRat(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, WORM_SCALE, 150.f);
	motion.radius = abs(motion.scale.x) / 3.f; // TODO: Test without
	motion.mass = 20.f;

	// Setup animations
	std::vector<int> walk_track = {0,1,2,3};
	float walk_interval = 250.f;
	std::vector<int> sleep_track = {4, 5};
	float sleep_interval = 750.f;
	std::vector<int> idle_track = {0};
	float idle_interval = 10000.f;

	auto all_tracks = {walk_track, sleep_track, idle_track};
	std::vector<float> track_intervals = {walk_interval, sleep_interval, idle_interval};
	registry.spriteSheets.emplace(entity, 6, all_tracks, track_intervals);

	// Add hp
	registry.healthies.emplace(entity, RAT_BASE_HP);

	// Create as enemy
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::RAT);
	enemy_info.collision_damage = 2;

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::RAT, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;

	// add sound 
	registry.sounds.emplace(entity);

	return entity;
}

Entity createSquirrel(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, SQUIRREL_SCALE, 225.f);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> anim1 = {0};
	std::vector<int> anim2 = {1};
	float anim1_interval = 1000;
	auto all_anim_tracks = {anim1, anim2};
	std::vector<float> all_anim_intervals = {anim1_interval, anim1_interval};
	registry.spriteSheets.emplace(entity, 2, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, SQUIRREL_BASE_HP);

	// Create as enemy
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::SQUIRREL);

	registry.rangedEnemies.emplace(entity);
	
	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::SQUIRREL, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;
	// add sound 
	registry.sounds.emplace(entity);
	return entity;
}

Entity createBear(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, BEAR_SCALE, 300.f);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> idle_track = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	std::vector<int> run_track = { 12, 13, 14, 15, 16 };
	std::vector<int> walk_track = { 17, 18, 19, 20, 21, 22, 23, 24 };
	float idle_track_interval = 250;
	float run_track_interval = 100;
	float walk_track_interval = 100;
	auto all_anim_tracks = { idle_track, run_track, walk_track };
	std::vector<float> all_anim_intervals = { idle_track_interval, run_track_interval, walk_track_interval };
	registry.spriteSheets.emplace(entity, 25, all_anim_tracks, all_anim_intervals);

	// Add hp
	Healthy& health = registry.healthies.emplace(entity, BEAR_BASE_HP);
	health.current_hp /= 1.2;

	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::BEAR);
	enemy_info.knock_back_immune = true;
	enemy_info.collision_damage = 2;

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::BEAR, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;
	// add sound 
	registry.sounds.emplace(entity);
	return entity;
}

Entity createBoar(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, BOAR_SCALE, 100);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> idle_track = { 0, 1, 2, 3, 4, 5, 6, 7 };
	std::vector<int> run_track = { 8, 9, 10, 11, 12, 13 };
	std::vector<int> walk_track = { 14, 15, 16, 17, 18, 19, 20, 21 };
	float idle_track_interval = 250;
	float run_track_interval = 100;
	float walk_track_interval = 100;
	auto all_anim_tracks = { idle_track, run_track, walk_track };
	std::vector<float> all_anim_intervals = { idle_track_interval, run_track_interval, walk_track_interval };
	registry.spriteSheets.emplace(entity, 22, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, BOAR_BASE_HP);

	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::BOAR);
	enemy_info.knock_back_immune = true;
	enemy_info.collision_damage = 3;

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::BOAR, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(199.f, 33.f, 13.f) / 255.f;
	// add sound 
	registry.sounds.emplace(entity);
	return entity;
}

Entity createDeer(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, DEER_SCALE, 250);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> idle_track = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	std::vector<int> run_track = { 10, 11, 12, 13, 14, 15};
	std::vector<int> walk_track = { 16, 17, 18, 19, 20, 21, 22, 23 };
	float idle_track_interval = 250;
	float run_track_interval = 100;
	float walk_track_interval = 100;
	auto all_anim_tracks = { idle_track, run_track, walk_track };
	std::vector<float> all_anim_intervals = { idle_track_interval, run_track_interval, walk_track_interval };
	registry.spriteSheets.emplace(entity, 24, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, DEER_BASE_HP);

	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::DEER);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::DEER, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;
	// add sound 
	registry.sounds.emplace(entity);
	return entity;
}

Entity createFox(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, FOX_SCALE, 130);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> idle_track = { 0, 1, 2, 3, 4, 5 };
	std::vector<int> run_track = { 6, 7, 8, 9, 10, 11 };
	std::vector<int> walk_track = { 12, 13, 14, 15, 16, 17, 18, 19 };
	float idle_track_interval = 250;
	float run_track_interval = 100;
	float walk_track_interval = 100;
	auto all_anim_tracks = { idle_track, run_track, walk_track };
	std::vector<float> all_anim_intervals = { idle_track_interval, run_track_interval, walk_track_interval };
	registry.spriteSheets.emplace(entity, 20, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, FOX_BASE_HP);

	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::FOX);
	enemy_info.collision_immune = true;

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::FOX, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;
	// add sound 
	registry.sounds.emplace(entity);
	return entity;
}

Entity createRabbit(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, RABBIT_SCALE, 120);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> hop_track = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	std::vector<int> idle_track = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
	std::vector<int> run_track = { 20, 21, 22, 23, 24, 25 };
	std::vector<int> rest_track = { 9 };
	float hop_track_interval = 100;
	float idle_track_interval = 200;
	float run_track_interval = 100;
	float rest_track_interval = 100;
	auto all_anim_tracks = { hop_track, idle_track, run_track, rest_track };
	std::vector<float> all_anim_intervals = { hop_track_interval, idle_track_interval, run_track_interval, rest_track_interval };
	registry.spriteSheets.emplace(entity, 26, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, RABBIT_BASE_HP);
	// add sound 
	registry.sounds.emplace(entity);
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::RABBIT);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::RABBIT, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;

	return entity;
}

Entity createAlphaWolf(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, WOLF_SCALE, 130);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> howl_track = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	std::vector<int> run_track = { 10, 11, 12, 13, 14, 15 };
	std::vector<int> walk_track = { 16, 17, 18, 19, 20, 21, 22, 23 };
	float howl_track_interval = 250;
	float run_track_interval = 100;
	float walk_track_interval = 100;
	auto all_anim_tracks = { howl_track, run_track, walk_track };
	std::vector<float> all_anim_intervals = { howl_track_interval, run_track_interval, walk_track_interval };
	registry.spriteSheets.emplace(entity, 24, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, WOLF_BASE_HP);

	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::ALPHAWOLF);
	enemy_info.collision_immune = true;
	enemy_info.collision_damage = 2;


	// add sound 
	registry.sounds.emplace(entity);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::ALPHA_WOLF, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;

	return entity;
}

Entity createWolf(vec2 position)
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, WOLF_SCALE / 1.4f, 130);
	motion.radius = abs(motion.scale.x) / 2.f; // TODO: Test without

	// animation
	std::vector<int> howl_track = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	std::vector<int> run_track = { 10, 11, 12, 13, 14, 15 };
	std::vector<int> walk_track = { 16, 17, 18, 19, 20, 21, 22, 23 };
	float howl_track_interval = 250;
	float run_track_interval = 100;
	float walk_track_interval = 100;
	auto all_anim_tracks = { howl_track, run_track, walk_track };
	std::vector<float> all_anim_intervals = { howl_track_interval, run_track_interval, walk_track_interval };
	registry.spriteSheets.emplace(entity, 24, all_anim_tracks, all_anim_intervals);

	// Add hp
	registry.healthies.emplace(entity, WOLF_BASE_HP / 1.4f);

	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::WOLF);
	enemy_info.collision_immune = true;

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::WOLF, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;
	// add sound 
	registry.sounds.emplace(entity);
	return entity;
}

Entity createWitch(vec2 position) // TODO: placeholder
{
	auto entity = Entity();

	// Initialize the motion
	Motion& motion = createMotion(entity, BEING_MASK, position, WITCH_SCALE, 1);
	motion.radius = 70.f; // TODO: Test without
	
	// animation
	std::vector<int> casting_track     = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9 };
	std::vector<int> death_track       = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
	std::vector<int> fireball_track    = { 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 };
	std::vector<int> idle_track        = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39 };
	std::vector<int> prep_attack_track = { 40, 41, 42, 43, 44, 45, 46, 47, 48, 49 };
	float casting_track_interval       = 200;
	float death_track_interval		   = 200;
	float fireball_track_interval      = 200;
	float idle_track_interval		   = 200;
	float prep_attack_track_interval   = 200;
	auto all_anim_tracks = { casting_track,
							 death_track,
							 fireball_track,
							 idle_track,
							 prep_attack_track };
	std::vector<float> all_anim_intervals = { casting_track_interval,
							 death_track_interval,
							 fireball_track_interval,
							 idle_track_interval,
							 prep_attack_track_interval };
	registry.spriteSheets.emplace(entity, 50, all_anim_tracks, all_anim_intervals);

	// Add hp
	Healthy& health = registry.healthies.emplace(entity, WITCH_BASE_HP);
	Enemy& enemy_info = registry.enemies.emplace(entity, ENEMY_TYPE::WITCH);
	registry.rangedEnemies.emplace(entity).shooting_interval = 250;
	enemy_info.knock_back_immune = true;
	registry.witches.emplace(entity);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::WITCH, NORMAL_ID::ROUNDED });
	render_request.transparency_offset = 20.f;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;
	render_request.specular = vec3(0.f);
	render_request.shininess = 1.f;

	PointLight& point_light = registry.pointLights.emplace(entity, 150.f, 0.f, entity, vec3(52.f, 30.f, 255.f) / 255.f);
	point_light.offset_position = vec3(0.f, 60.f, 0.f);
	registry.worldLightings.components[0].num_important_point_lights += 1;

	return entity;
}

Entity createProjectile(vec2 position, vec2 offset, vec2 direction, float initial_speed, Entity owner, DIFFUSE_ID diffuse_id)
{
	auto entity = Entity();

	Motion& motion = createMotion(entity, PROJECTILE_MASK, position + (direction * 10.f), { 20, 20 }, initial_speed);
	motion.sprite_offset = vec2(0, -20.f - (random_float()+0.5f)*abs(offset.x)); // How far off of the ground it will fire
	motion.velocity = direction * initial_speed;
	motion.current_speed = initial_speed;
	motion.friction = 1.f;
	motion.sprite_offset_velocity = 50.f; // Fired slightly upwards so it has positive velocity

	registry.projectiles.emplace(entity, owner);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { diffuse_id, NORMAL_ID::ROUNDED });
	render_request.ignore_color = vec3(-10.f);

	return entity;
}

Entity createAcornProjectile(vec2 position, vec2 offset, vec2 direction, Entity owner)
{
	auto entity = createProjectile(position, offset, direction, 400.f, owner, DIFFUSE_ID::ACORN);

	// Set damage
	registry.attacks.insert(entity, {1});

	return entity;
}

Entity createBreadcrumbProjectile(vec2 position, vec2 offset, vec2 direction, vec2 aimed_position, Entity player) {
	float speed = length(aimed_position - position) / 2.5f;
	auto entity = createProjectile(position, offset, direction, speed, player, DIFFUSE_ID::BREADCRUMBS);
	auto& motion = registry.motions.get(entity);
	
	auto& projectile = registry.projectiles.get(entity);
	// projectile.lifetime_ms = Upgrades::get_hansel_m2_duration();
	projectile.lifetime_ms = 5000.f;

	registry.healthies.emplace(entity, Upgrades::get_hansel_m2_hp());
	registry.enemyAttractors.emplace(entity);

	return entity;
}

BezierCurve& insertBezierCurve(Entity entity, vec2 position, vec2 direction) {
	float angle = atan2(direction.y, direction.x) + (rand() % 10 - 5) / 5.f;
	std::vector<vec2> curve_points;
	// Select one of the following different paths
	int r = rand() % 10;
	if (r == 0) {
		// zig-zag forwards than loop around player before speeding off
		curve_points = {
			position,
			position + direction * 200.f - rotate(vec2{0.f, 250.f}, angle),
			position + direction * 400.f + rotate(vec2{0.f, 350.f}, angle),
			position + direction * 600.f,
			position + direction * 600.f - rotate(vec2{0.f, 300.f}, angle),
			position + direction * 300.f,
			position - direction * 100.f + rotate(vec2{0, 400.f}, angle),
			position - direction * 200.f + rotate(vec2{-250.f, -100.f}, angle),
			position + direction * 100.f + rotate(vec2{200.f, -250.f}, angle),
			position + direction * 300.f + rotate(vec2{300.f, 0.f}, angle),
		};
	}
	else if (r == 1) {
		// same as previous, but zig zag in opposite order
		curve_points = {
			position,
			position + direction * 200.f + rotate(vec2{0.f, 350.f}, angle),
			position + direction * 400.f - rotate(vec2{0.f, 250.f}, angle),
			position + direction * 500.f,
			position + direction * 600.f - rotate(vec2{0.f, 300.f}, angle),
			position + direction * 300.f,
			position + direction * 300.f + rotate(vec2{600.f, 200.f}, angle),
			position - direction * 100.f + rotate(vec2{0, 400.f}, angle),
			position - direction * 200.f + rotate(vec2{-250.f, -100.f}, angle),
			position + direction * 100.f + rotate(vec2{200.f, -250.f}, angle),
		};
	}
	else if (r == 2) {
		// Spiral around player (oval shaped due to how Bezier works)
		curve_points = {
			position,
			position + direction * 200.f,
			position + rotate(direction * 200.f, M_PI / 2),
			position + rotate(direction * 200.f, M_PI),
			position + rotate(direction * 200.f, -M_PI / 2),
			position + direction * 300.f,
			position + rotate(direction * 300.f, M_PI / 2),
			position + rotate(direction * 300.f, M_PI),
			position + rotate(direction * 300.f, -M_PI / 2),
			position + direction * 400.f,
			position + rotate(direction * 400.f, M_PI / 2),
			position + rotate(direction * 400.f, M_PI),
			position + rotate(direction * 400.f, -M_PI / 2),
			position + direction * 500.f,
		};
	}
	else if (r == 3) {
		// Zig-zag slowly forwards, then shoot off 
		curve_points = {
			position,
			position + direction * 100.f + rotate(vec2{0.f, 300.f}, angle),
			position + direction * 200.f - rotate(vec2{0.f, 300.f}, angle),
			position + direction * 300.f,
			position + direction * 250.f + rotate(vec2{0.f, 300.f}, angle),
			position + direction * 150.f - rotate(vec2{0.f, 300.f}, angle),
			position + direction * 150.f - rotate(vec2{0.f, 500.f}, angle),
			position + direction * 150.f - rotate(vec2{200.f, 600.f}, angle),
		};
	}
	else {
		// Generate points that are always point_d units away from previous point, within the given turn radius
		float point_d = 250.f;
		curve_points = { position, position + direction * point_d };
		vec2 prev_direction = direction;
		for (int i = 0; i < 6; i++) {
			prev_direction = rotate(prev_direction, linearRand(-M_PI / 1.5f, M_PI / 1.5f));
			curve_points.push_back(
				curve_points[i + 1] + prev_direction * point_d
			);
		}
	}

	auto& curve = registry.bezierCurves.emplace(entity, std::move(curve_points));

	auto& motion = registry.motions.get(entity);
	motion.angled = true;
	curve.default_sprite_offset = motion.sprite_offset;

	return curve;
}

Entity createPlayerProjectile(vec2 position, vec2 offset, vec2 direction, const Entity player)
{
	using namespace Upgrades;

	// Retrieve variable stats based on player upgrades
	auto diffuse = get_hansel_m1_diffuse();
	int damage = get_hansel_m1_damage();
	float knockback = get_hansel_m1_knockback();

	// TODO: This is kinda buggy. Find out why.
	vec2 projectile_direction = player_has_upgrade(UPGRADE_ID::INHERIT_VELOCITY) ? 
		normalize(registry.motions.get(player).move_direction/2.f + direction)
		: direction;
	float projectile_speed = player_has_upgrade(UPGRADE_ID::HORSE_CHESTNUT) ? 400.f : 700.f;
	auto entity = createProjectile(position, offset, projectile_direction, projectile_speed, player, diffuse);
	auto& projectile = registry.projectiles.get(entity);
	
	if (player_has_upgrade(UPGRADE_ID::HORSE_CHESTNUT)) {
		projectile.lifetime_ms += 5000.f;
	}
	projectile.hits_left = Upgrades::get_hansel_m1_passthrough();

	if (player_has_upgrade(UPGRADE_ID::BEE_PROJECTILE)) {
		BezierCurve& curve = insertBezierCurve(entity, position, direction);
		projectile.lifetime_ms = curve.curve_duration_ms * curve.num_curves;
		Motion& motion = registry.motions.get(entity);
		motion.gravity_multiplier = 0.f;
		motion.sprite_offset_velocity = 0.f;
	}

	// Set damage
	registry.attacks.insert(entity, { damage, knockback });

	return entity;
}

Entity createFireballProjectile(vec2 position, vec2 offset, vec2 direction, Entity owner)
{
	auto entity = createProjectile(position, offset, direction, 400.f, owner, DIFFUSE_ID::FIREBALL);

	// animation
	std::vector<int> fireball_track = { 0, 1 };
	float fireball_track_interval = 250;
	auto all_anim_tracks = { fireball_track };
	std::vector<float> all_anim_intervals = { fireball_track_interval };
	registry.spriteSheets.emplace(entity, 2, all_anim_tracks, all_anim_intervals);

	Motion& motion = registry.motions.get(entity);
	motion.scale = FIREBALL_SCALE;
	motion.friction = 1.f;
	motion.angle = atan2(direction.y, direction.x);

	int rand_num = rand() % 3;
	if (rand_num == 1) {
		//PointLight& point_light = registry.pointLights.emplace(entity, 120.f, 50.f, entity, vec3(255.f, 0.f, 0.f) / 255.f);
	}

	RenderRequest& render_request = registry.renderRequests.get(entity);
	render_request.ignore_color.x = -10.f;
	render_request.casts_shadow = false;

	// Set damage
	registry.attacks.insert(entity, { 3 });

	return entity;
}

Entity createHellfireEffect(vec3 position, vec3 direction, vec3 multiply_color)
{
	// Setup fireball particle animations:
	std::vector<int> fireball_track = { 0, 1 };
	float fireball_track_interval = 250;
	auto all_anim_tracks = { fireball_track };
	std::vector<float> all_anim_intervals = { fireball_track_interval };

	float speed = 400.f;
	Entity entity = createParticleGenerator(position, direction, vec2(25.f), 100.f, DIFFUSE_ID::FIREBALL, 0.1f, multiply_color, 500.f,
		1.f, { 3000.f, 3500.f }, { 0.f, 0.f }, { speed, speed + 100.f }, { 0.f, 0.f }, { 80.f, 80.f, 0.f });
					//		max_angular_spread		speed				angular_speed       max_rect_spread
	ParticleGenerator& generator = registry.particleGenerators.get(entity);
	generator.ignore_color = vec3(-10.f);
	generator.angle = M_PI/2.f;

	// Give the particle generator a sprite sheet as a reference for it's particles to use upon their creation
	auto& sprite_sheet = registry.spriteSheets.emplace(entity, 2, all_anim_tracks, all_anim_intervals);
	sprite_sheet.is_reference = true; // Necessary to prevent animation_system.cpp trying to update the particle generator's animations
	sprite_sheet.loop = true;


	// Now create actual damage using a projectile that falls downwards in place
	Entity projectile_entity = createProjectile(vec2(position), vec2(-1.f), vec2(1.f), 1.f, entity, DIFFUSE_ID::BLACK);
	Motion& motion = registry.motions.get(projectile_entity);
	motion.sprite_offset = vec2(0, -position.z);
	motion.scale = vec2(80.f);
	motion.sprite_offset_velocity = -speed;

	RenderRequest& render_request = registry.renderRequests.get(projectile_entity);
	render_request.transparency = 1.f;
	render_request.casts_shadow = false;
	
	int rand_num = rand() % 3;
	if (rand_num == 1) {
		//PointLight& point_light = registry.pointLights.emplace(projectile_entity, 120.f, 50.f, projectile_entity, vec3(255.f, 0.f, 0.f) / 255.f);
	}

	// Set damage
	registry.attacks.insert(projectile_entity, { 3 });

	return entity;
}

Entity createHellfireWarning(vec2 position, Entity parent)
{
	//Entity entity = createGroundPiece();
	auto entity = Entity();

	// Setting initial motion values
	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position, { 80.f, 80.f }, 0.f);

	// Make child of parent (which is the witch in this case). Will cause attack sprite to follow parent
	registry.motions.get(parent).add_child(entity);
	motion.parent = parent;

	// Setup animations
	float animation_length = 2000.f;

	// Register as effect

	registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::ATTACK_WARNING, animation_length });
	RenderRequest& renderRequest = registry.renderRequests.insert(entity, { DIFFUSE_ID::WARNING_CIRCLE });
	renderRequest.casts_shadow = false;
	renderRequest.transparency = 0.4f;
	renderRequest.ignore_color.x = -10.f;

	return entity;
}
// This function now creates debug lines as well
Entity createColliderDebug(vec2 position, vec2 scale, DIFFUSE_ID diffuse_id, vec3 color, float transparency, vec2 offset)
{
	Entity entity = Entity();
	
	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position, scale, 0.f);
	motion.sprite_offset = offset;
	//motion.sprite_normal = (diffuse_id == DIFFUSE_ID::CIRCLE || diffuse_id == DIFFUSE_ID::BLACK) ? vec3(0,0,1) : motion.sprite_normal;
	motion.sprite_normal = { 0,0,1 };
	registry.debugComponents.emplace(entity);

	RenderRequest& render_request = registry.renderRequests.insert(entity, { diffuse_id });
	render_request.add_color = color;
	render_request.transparency = transparency;
	render_request.is_ground_piece = true;
	render_request.ignore_color = vec3(-10.f);
	//registry.groundPieces.emplace(entity);

	return entity;
}

Entity createWorldBounds(vec2 position, vec2 scale, DIFFUSE_ID diffuse_id, vec3 color, float transparency, vec2 offset)
{
	Entity entity = Entity();

	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position, scale, 0.f);
	motion.sprite_offset = offset;
	motion.sprite_normal = { 0,0,1 };

	RenderRequest& render_request = registry.renderRequests.insert(entity, { diffuse_id });
	render_request.add_color = color;
	render_request.transparency = transparency;
	render_request.is_ground_piece = true;
	render_request.ignore_color = vec3(255.f, 0.f, 0.f) / 255.f;

	registry.groundPieces.emplace(entity);

	return entity;
}

Entity createSpawner(vec2 position, std::vector<ENEMY_TYPE> enemies)
{
	auto entity = Entity();
	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, position, {50.f,50.f}, 0.f);
	motion.sprite_normal = { 0.f,0.f,1.f };

	EnemySpawner& spawner = registry.spawners.emplace(entity);
	spawner.enemies = enemies;

	//RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::CIRCLE, NORMAL_ID::FLAT });


	return entity;
}

Entity createCampFire(vec2 position)
{
	auto entity = Entity();
	Motion& motion = createMotion(entity, OBSTACLE_MASK, position, CAMP_FIRE_SCALE, 0.f);

	//WorldLighting& world_lighting = registry.worldLightings.components[0];
	registry.pointLights.emplace(entity, 300.f, 100.f, entity); // Testing
	//TODO: world_lighting.add_point_light ??
	WorldLighting& world_lighting = registry.worldLightings.components[0];
	world_lighting.num_important_point_lights += 1;

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::CAMP_FIRE });

	RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::CAMP_FIRE, NORMAL_ID::FLAT });
	render_request.is_normal_sprite_sheet = true;

	// Setup campfire animations
	float anim_length = 700.f;
	std::vector<int> anim_frames = { 0, 1, 2, 3, 4 };
	float anim_interval = anim_length / anim_frames.size();
	auto all_anim_tracks = { anim_frames };
	std::vector<float> all_anim_intervals = { anim_interval };
	registry.spriteSheets.emplace(entity, 5, all_anim_tracks, all_anim_intervals);

	// Setup smoke particle animations:
	float anim_length0 = 1500.f;
	std::vector<int> frames0 = { 0, 1, 2, 3, 4, 5, 6, 7 };
	float anim_interval0 = anim_length0 / frames0.size();
	auto all_anim_tracks0 = { frames0 };
	std::vector<float> all_anim_intervals0 = { anim_interval0 };

	Entity e = createParticleGenerator(vec3(motion.position, motion.scale.y / 3.f), vec3(0,0,1), vec2(50.f), 4.f, DIFFUSE_ID::SMOKE,
		0.1f, vec3(1.f), -1.f, 0.f, { 3000.f, 5000.f }, { 0.f, M_PI / 10.f });
	// Give the particle generator a sprite sheet as a reference for it's particles to use upon their creation
	auto& sprite_sheet = registry.spriteSheets.emplace(e, 8, all_anim_tracks0, all_anim_intervals0);
	sprite_sheet.is_reference = true; // Necessary to prevent animation_system.cpp trying to update the particle generator's animations
	sprite_sheet.loop = false;

	return entity;
}

Entity createButterflies(Room& room)
{
	float room_width = room.grid_size.x * WorldSystem::TILE_SIZE;
	float room_height = room.grid_size.y * WorldSystem::TILE_SIZE;
	float room_area = (room_width * room_height) / (WorldSystem::TILE_SIZE * WorldSystem::TILE_SIZE);
	float frequency = room_area * 0.005;
	vec2 room_center_pos = { room_width / 2.f,  room_height / 2.f };

	// Setup smoke particle animations:
	float anim_length0 = 500.f;
	std::vector<int> frames0 = { 0, 1, 2 };
	float anim_interval0 = anim_length0 / frames0.size();
	auto all_anim_tracks0 = { frames0 };
	std::vector<float> all_anim_intervals0 = { anim_interval0 };

	Entity e = createParticleGenerator(vec3(room_center_pos, 30.f), vec3(0, 0, 1), vec2(60.f), frequency, DIFFUSE_ID::BUTTERFLY,
		0.f, vec3(1.f), -1.f, 0.2f, { 6000.f, 10000.f }, { 0.f, M_PI / 2.f }, { 100.f, 100.f }, { 0.f, M_PI / 4.f },
		{ room_width + 200.f, room_height + 200.f, 0.f });
	registry.particleGenerators.get(e).is_random_color = true;
	Entity e2 = createParticleGenerator(vec3(room_center_pos, 30.f), vec3(0, 0, 1), vec2(30.f), frequency, DIFFUSE_ID::DRAGONFLY,
		0.f, vec3(1.f), -1.f, 0.2f, { 6000.f, 10000.f }, { 0.f, M_PI / 2.f }, { 100.f, 100.f }, { M_PI / 2.f, M_PI / 2.f },
		{ room_width + 200.f, room_height + 200.f, 0.f });
	registry.particleGenerators.get(e2).is_random_color = true;
	// Give the particle generator a sprite sheet as a reference for it's particles to use upon their creation
	auto& sprite_sheet = registry.spriteSheets.emplace(e, 3, all_anim_tracks0, all_anim_intervals0);
	sprite_sheet.is_reference = true; // Necessary to prevent animation_system.cpp trying to update the particle generator's animations
	auto& sprite_sheet2 = registry.spriteSheets.emplace(e2, 3, all_anim_tracks0, all_anim_intervals0);
	sprite_sheet2.is_reference = true; // Necessary to prevent animation_system.cpp trying to update the particle generator's animations

	std::vector<vec2> empty = { { 1, 3}, { 2, 4 }, { 6, 4 }, { 7, 5 } };
	BezierCurve& curve = registry.bezierCurves.emplace(e, empty);
	BezierCurve& curve2 = registry.bezierCurves.emplace(e2, empty);

	return e;
}

Entity createWorldLighting() // Could be moved into createRoom, and specified in a json
{
	float day_cycle_ms = 300000.f; // 300000.f // See WorldLighting struct for more info
	auto dir_light_entity = Entity();
	registry.dirLights.emplace(dir_light_entity);
	registry.lerpVec3s.emplace(dir_light_entity, dir_light_color_key_frames, day_cycle_ms);

	float random_latitude = (random_float() + 0.5f) * M_PI; // Usually was set to M_PI / 4.f
	float random_theta_offset = (random_float() + 0.5f) * 2.f * M_PI;
	auto world_lighting_entity = Entity();
	WorldLighting& world_lighting = registry.worldLightings.emplace(world_lighting_entity, dir_light_entity, M_PI / 4.f, day_cycle_ms);
	world_lighting.is_time_changing = true; // AKA; is_day/night_cycle_enabled
	world_lighting.theta_offset = random_theta_offset;
	return world_lighting_entity;
}

Entity createCamera()
{
	auto entity = Entity();
	registry.cameras.insert(entity, { M_PI / 4.f });
	return entity;
}

Entity createGroundPiece(vec2 pos, vec2 scale, float angle, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id, float repeat_texture, vec3 multiply_color, DIFFUSE_ID mask_id) {
	Entity entity = Entity();

	Motion& motion = createMotion(entity, UNCOLLIDABLE_MASK, pos, scale, 0.f);
	motion.sprite_normal = { 0,0,1 }; // normal points straight up
	motion.angle = angle;

	auto& render_request = registry.renderRequests.insert(entity, { diffuse_id, normal_id });
	render_request.mask_id = mask_id;
	render_request.diffuse_coord_loc = { 0.f, 0.f, repeat_texture, repeat_texture };
	render_request.normal_coord_loc = { 0.f, 0.f, repeat_texture, repeat_texture };
	render_request.normal_add_coord_loc = { 0.f, 0.f, repeat_texture, repeat_texture };
	render_request.specular = vec3(0.f); // Hacky fix to make grass not reflective, needs a better fix
	render_request.multiply_color = multiply_color;
	render_request.is_ground_piece = true;

	registry.groundPieces.emplace(entity);

	return entity;
}

ComplexPolygon& createComplexPolygon(Entity entity, std::vector<Edge>& edges, vec2 position, vec2 scale, float angle,
	bool is_only_edge = false, bool is_platform = false)
{
	std::vector<Edge> world_edges;
	Transform transform;
	transform.translate(position);
	transform.rotate(angle);
	transform.scale(scale);
	for (uint i = 0; i < edges.size(); i++) {
		Edge edge = edges[i];
		vec2 world_vertex1 = vec2(transform.mat * vec3(edge.vertex1.x, edge.vertex1.y, 1.0));
		vec2 world_vertex2 = vec2(transform.mat * vec3(edge.vertex2.x, edge.vertex2.y, 1.0));

		//Edge new_world_edge = (is_platform) ? Edge{world_vertex2, world_vertex1} : Edge{world_vertex1, world_vertex2};
		Edge new_world_edge = { world_vertex1, world_vertex2 };
		world_edges.push_back(new_world_edge);
	}
	ComplexPolygon& polygon = registry.polygons.emplace(entity, world_edges, position, is_only_edge, is_platform);
	SpatialGrid& spatial_grid = SpatialGrid::getInstance();
	float radius = (is_platform) ? 0.f : WorldSystem::TILE_SIZE;
	std::vector<Cell> cells = spatial_grid.raster_polygon(polygon, radius, is_only_edge);
	assert(registry.motions.get(entity).type_mask == POLYGON_MASK);
	Room& room = registry.rooms.components[0];
	for (uint i = 0; i < cells.size(); i++) {
		Cell& cell = cells[i];
		bool is_platform_here = false;
		if (!is_platform) {
			for (int j = 0; j < cell.num_entities; j++) {
				Motion& motion_other = registry.motions.get(cell.entities[j]);
				if (motion_other.type_mask == POLYGON_MASK) {
					ComplexPolygon& polygon = registry.polygons.get(cell.entities[j]);
					if (polygon.is_platform) { is_platform_here = true;  }
				}
			}
		}
		spatial_grid.add_entity_to_cell(cell.coords, entity);
		if (is_platform_here) { continue; }
		if (!is_only_edge) {
			vec2 cell_world_position = vec2(cell.coords.x + 0.5f, cell.coords.y + 0.5f) * WorldSystem::TILE_SIZE;
			if (is_point_within_polygon(polygon, cell_world_position)) { // Must check again so we don't include cells outside polygon
				if (is_platform) {
					room.generator.removeCollision({ cell.coords.x, cell.coords.y });
				} else {
					room.generator.addCollision({ cell.coords.x, cell.coords.y });
				}
			}
		}
	}
	return polygon;
}

ComplexPolygon& createComplexPolygon(Entity entity, std::vector<Edge>& edges, Motion& motion, bool is_only_edge = false, bool is_platform = false) { // Overloaded
	return createComplexPolygon(entity, edges, motion.position, motion.scale, motion.angle, is_only_edge, is_platform);
}

Entity createPlatform(RenderSystem* renderer, vec2 position, vec2 scale, float angle)
{
	Entity entity = createGroundPiece(position, scale, angle, DIFFUSE_ID::PLATFORM, NORMAL_ID::PLATFORM, 1.f);
	Motion& motion = registry.motions.get(entity);
	motion.type_mask = POLYGON_MASK;
	motion.sprite_offset.y = -5.f;

	RenderRequest& render_request = registry.renderRequests.get(entity);
	render_request.geometry_id = GEOMETRY_ID::PLANE;
	render_request.casts_shadow = true;
	render_request.diffuse_coord_loc.z = scale.x / 300.f;
	render_request.mask_coord_loc.z = render_request.diffuse_coord_loc.z;
	render_request.normal_coord_loc.z = render_request.diffuse_coord_loc.z;
	render_request.normal_add_coord_loc.z = render_request.diffuse_coord_loc.z;

	printf("yupp: %f\n", render_request.diffuse_coord_loc.z);

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::PLATFORM });

	Mesh& mesh = renderer->getMesh(render_request.geometry_id);
	registry.meshPtrs.emplace(entity, &mesh);

	ComplexPolygon& polygon = createComplexPolygon(entity, mesh.edges, motion, false, true);
	polygon.world_edges[0].is_collidable = false;
	polygon.world_edges[3].is_collidable = false;

	return entity;
}

Entity createDirtPatch(vec2 position, vec2 scale)
{
	vec3 multiply_color = vec3(255.f, 194.f, 124.f) / 255.f;
	float angle = (random_float() + 0.5f) * 2.f * M_PI;
	scale = scale + vec2(random_float() * 2.f);
	Entity dirt_patch = createGroundPiece(position, scale, angle, DIFFUSE_ID::DIRT, NORMAL_ID::DIRT, 1.f, multiply_color, DIFFUSE_ID::GROUND_MASK);

	RenderRequest& render_request = registry.renderRequests.get(dirt_patch);
	render_request.normal_add_id = NORMAL_ID::HILL2;

	return dirt_patch;
}

Entity createLake(RenderSystem* renderer, vec2 position, vec2 scale, float angle)
{
	// First create the background dirt that the lake overlays:
	vec3 multiply_color = vec3(255.f, 194.f, 124.f) / 255.f;
	Entity lake_dirt = createGroundPiece(position, scale, angle, DIFFUSE_ID::DIRT, NORMAL_ID::DIRT, 1.f, multiply_color, DIFFUSE_ID::LAKE);

	RenderRequest& render_request0 = registry.renderRequests.get(lake_dirt);
	render_request0.geometry_id = GEOMETRY_ID::LAKE;
	render_request0.extrude_size = 30.f;

	// Then create the lake:
	Entity lake = createGroundPiece(position, scale, angle, DIFFUSE_ID::LAKE, NORMAL_ID::WATER1);
	Motion& motion = registry.motions.get(lake);
	motion.type_mask = POLYGON_MASK;

	RenderRequest& render_request = registry.renderRequests.get(lake);
	render_request.geometry_id = GEOMETRY_ID::LAKE;
	render_request.normal_add_id = NORMAL_ID::WATER2;
	render_request.add_color = vec3(0.f, 0.f, 0.5f);
	render_request.specular = vec3(1.f);
	render_request.transparency = 0.3f;

	render_request.normal_coord_loc = { vec2(0.f), scale / 400.f };
	render_request.normal_add_coord_loc = { vec2(0.f), scale / 400.f };

	registry.waters.emplace(lake);
	registry.obstacles.insert(lake, { OBSTACLE_TYPE::WATER });

	Mesh& mesh = renderer->getMesh(render_request.geometry_id);
	registry.meshPtrs.emplace(lake, &mesh);

	createComplexPolygon(lake, mesh.edges, motion);

	return lake;
}

Entity createRock(vec2 position, TextureAtlas& atlas, vec4 frame)
{
	auto entity = Entity();

	float scale_multiplier = 0.7f;
	int r = rand() % 100;
	if (r < 50) { scale_multiplier = 1.f; }

	Motion& motion = createMotion(entity, PROP_MASK, position, vec2(frame.z, frame.w) * atlas.size_px * scale_multiplier, 0.f);
	motion.sprite_normal = { 0, 1 / sqrt(2), 1 / sqrt(2) };
	motion.sprite_offset.y += 5; // Shift down a bit so it looks better

	registry.props.insert(entity, { PROP_TYPE::FOLIAGE });
	//registry.obstacles.insert(entity, { OBSTACLE_TYPE::ROCK });

	RenderRequest& render_request = registry.renderRequests.insert(entity, { atlas.diffuse_id, atlas.normal_id });
	render_request.specular = vec3(0.f);
	render_request.casts_shadow = false;

	render_request.diffuse_coord_loc = frame;
	render_request.normal_coord_loc = frame;
	render_request.mask_coord_loc = frame;

	return entity;
}

Entity createFoliageProp(vec2 position, TextureAtlas& atlas, bool is_on_water = false)
{
	vec4 rand_frame = atlas.pick_frame((is_on_water) ? water_type_index : -1);

	if (atlas.last_picked_type == 3) {
		return createRock(position, atlas, rand_frame);
	}

	auto entity = Entity();

	float scale_multiplier = 1.f;
	vec3 multiply_color = vec3(0.f, 0.7f, 0.f) * 1.5f;
	float multiply_color_multiplier = 1.f + random_float()/2.f;
	float wind_affected = 0.3f;
	if (atlas.last_picked_type == 2) {
		wind_affected = 0.f;
	}

	int r = rand() % 100;
	if (r < 50) { scale_multiplier = 1.5f; }

	if (atlas.last_picked_type == 0) { // Flowers
		scale_multiplier = 2.f;
		if (r < 33) {
			multiply_color = vec3(63, 72, 204)/255.f;
		} else if (r < 67) {
			multiply_color = vec3(93, 204, 233)/255.f;
		//} else if (r < 75) {
		//	multiply_color = vec3(253, 236, 75)/255.f;
		} else if (r < 100) {
			multiply_color = vec3(211, 65, 87) / 255.f;
		}
		multiply_color *= 2.f;
	}
	if (atlas.last_picked_type == 6) { // Mushrooms
		if (r < 33) {
			multiply_color = vec3(183, 140, 65)/255.f;
		} else if (r < 66) {
			multiply_color = vec3(105, 91, 149)/255.f;
		} else if (r < 100) {
			multiply_color = vec3(106, 19, 0)/255.f;
		}
		multiply_color *= 2.f;
	}

	Motion& motion = createMotion(entity, PROP_MASK, position, vec2(rand_frame.z, rand_frame.w) * atlas.size_px * scale_multiplier, 0.f);
	motion.sprite_normal = { 0, 1/sqrt(2), 1/sqrt(2) };
	motion.sprite_offset.y += 5; // Shift down a bit so it looks better

	registry.props.insert(entity, { PROP_TYPE::FOLIAGE });

	RenderRequest& render_request = registry.renderRequests.insert(entity, { atlas.diffuse_id, atlas.normal_id });
	render_request.specular = vec3(0.f);
	render_request.multiply_color = multiply_color * multiply_color_multiplier;
	render_request.wind_affected = wind_affected;
	render_request.casts_shadow = false;

	render_request.diffuse_coord_loc = rand_frame;
	render_request.normal_coord_loc = rand_frame;
	render_request.mask_coord_loc = rand_frame;

	return entity;
}

void createProceduralProps(float density_per_tile, int type_weights_index)
{
	// Setting of foliage texture atlas:

	if (type_weights_index == -1) {
		type_weights_index = rand() % (type_weights_list.size()-1) + 1;
	}
	TextureAtlas foliage_atlas = TextureAtlas(DIFFUSE_ID::FOLIAGE_ATLAS, NORMAL_ID::FOLIAGE_ATLAS, type_ranges, type_weights_list[type_weights_index]);
	rapidjson::Document dom = foliage_atlas.loadFromJSONFile("foliage.json");

	vec2 atlas_size = { dom["meta"]["size"]["w"].GetInt(), dom["meta"]["size"]["h"].GetInt() };
	foliage_atlas.size_px = atlas_size;

	for (int i = 0; i < foliage_names.size(); i++) // Use this size since texture arrays have empty space
	{
		const std::string& name = foliage_names[i];

		if (dom["frames"].HasMember(name.c_str())) {
			const rapidjson::Value& frame = dom["frames"][name.c_str()]["frame"];
			vec2 pos_loc = { frame["x"].GetInt(), frame["y"].GetInt() };
			vec2 size_loc = { frame["w"].GetInt(), frame["h"].GetInt() };
			foliage_atlas.frames.push_back({ pos_loc / atlas_size, size_loc / atlas_size });
		}
	}

	// Calculating number of props to place:
	Room& room = registry.rooms.components[0];
	int bounds_tiles_addition = 10; // How many tiles outside of the map to spawn obstacles
	float max_tile_dimension = max(room.grid_size.x, room.grid_size.y) + bounds_tiles_addition;
	float room_tile_area = max_tile_dimension * max_tile_dimension;
	float max_dimension = max_tile_dimension * WorldSystem::TILE_SIZE;
	vec2 room_center_pos = { room.grid_size.x * WorldSystem::TILE_SIZE / 2.f,  room.grid_size.y * WorldSystem::TILE_SIZE / 2.f };

	createButterflies(room);

	SpatialGrid& spatial_grid = SpatialGrid::getInstance();
	std::vector<vec4> placed_positions_radii_water; // vec2 position, float radius, and float is_on_water are packed together
	int props_to_place = density_per_tile * room_tile_area;
	placed_positions_radii_water.reserve(props_to_place);
	int num_props_placed = 0;
	int num_failed_tries = 0;

	printf("room_area_tiles = %f\n", room_tile_area);
	printf("props_to_place = %d\n", props_to_place);

	// Placing props:
	while (num_props_placed < props_to_place)
	{
		if (num_failed_tries > 100) {
			printf("Failed to place all props\n");
			printf("Props placed = %d\n", num_props_placed);
			break;
		}

		vec2 random_position = room_center_pos + max_dimension * vec2(random_float(), random_float());
		ivec2 cell_coords = spatial_grid.get_grid_cell_coords(random_position);
		float random_radius = 50.f; // + (50.f * (random_float() + 0.5f));

		// Don't place if it's too close to an obstacle or a prop that has already been placed:
		bool is_on_water = false;
		bool good_placement = true;
		std::vector<Entity> entities_found = spatial_grid.query_radius(cell_coords, random_radius);
		// First do dynamic-dynamic collisions
		for (uint j = 0; j < entities_found.size(); j++) {
			Entity entity_other = entities_found[j];
			Motion& motion_other = registry.motions.get(entity_other);
			if (motion_other.type_mask == POLYGON_MASK) {
				ComplexPolygon& polygon = registry.polygons.get(entity_other);
				if (polygon.is_only_edge) { continue; }
				vec2 is_colliding = is_circle_polygon_colliding(polygon, random_position, random_radius - random_radius/1.3f);
				if (is_colliding.x != 0.f || is_colliding.y != 0.f) { // TODO: Add a check if it's within polygon then make it water
					if (polygon.is_platform || registry.obstacles.get(entity_other).type != OBSTACLE_TYPE::WATER) {
						is_on_water = false;
						good_placement = false; // No foliage on platforms
					} else { // Maybe add another check to see if it's close to lake edges below
						is_on_water = true;
						random_radius = 70.f; // Bigger distance between water foliage
					}
				}
				
			} else {
				float radius_check = random_radius;
				if ((motion_other.type_mask == OBSTACLE_MASK && registry.obstacles.get(entity_other).type == OBSTACLE_TYPE::CAMP_FIRE)
					|| motion_other.type_mask == DOOR_MASK) {
					radius_check += 30.f;
				}
				if (length(motion_other.position - random_position) < radius_check) {
					good_placement = false;
					break;
				}
			}
		}
		// Need this too to check against other props since props don't use the spatial grid:
		for (int i = 0; i < placed_positions_radii_water.size(); i++) {
			if (is_on_water && placed_positions_radii_water[i].w == 1.f) {
				vec2 placed_pos = vec2(placed_positions_radii_water[i].x, placed_positions_radii_water[i].y);
				if (length(placed_pos - random_position) < placed_positions_radii_water[i].z) {
					good_placement = false;
					break;
				}
			}
			else if (!is_on_water && placed_positions_radii_water[i].w == 0.f) {
				vec2 placed_pos = vec2(placed_positions_radii_water[i].x, placed_positions_radii_water[i].y);
				if (length(placed_pos - random_position) < placed_positions_radii_water[i].z) {
					good_placement = false;
					break;
				}
			}
		}
		if (!good_placement) { num_failed_tries++; continue; }

		createFoliageProp(random_position, foliage_atlas, is_on_water);
		placed_positions_radii_water.push_back(vec4(random_position, random_radius, is_on_water));
		num_props_placed++;
		num_failed_tries = 0;

		int rand_num = rand() % 30;
		if (rand_num == 1) {
			createDirtPatch(random_position, { 190.f, 230.f });
		}
	}
}

Entity createRoomGround(const Room& room, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id, float repeat_texture, vec3 multiply_color)
{
	float room_width = room.grid_size.x * WorldSystem::TILE_SIZE;
	float room_height = room.grid_size.y * WorldSystem::TILE_SIZE;
	vec2 room_center_pos = { room_width / 2.f,  room_height / 2.f };
	Entity entity = createGroundPiece(room_center_pos, { 10000, 10000 }, 0.f, diffuse_id, normal_id, repeat_texture, multiply_color);
	RenderRequest& render_request = registry.renderRequests.get(entity);
	if (diffuse_id == DIFFUSE_ID::GRASS) {
		render_request.normal_add_id = NORMAL_ID::HILL2; // TODO: Testing
		//render_request.normal_add_coord_loc = { vec2(0.f), vec2(1.f / 2.f) };
	}

	registry.obstacles.emplace(entity); // Represents outer bounds of it
	// Creating outer bounds collider:
	vec2 verts[4] = { { -0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, -0.5f }, { -0.5f, -0.5f } };
	std::vector<Edge> edges = { {verts[1],verts[0]}, {verts[2],verts[1]}, {verts[3],verts[2]}, {verts[0],verts[3]} };
	//createMotion(entity, POLYGON_MASK, room_center_pos, { room_width, room_height }, 0.f);
	registry.motions.get(entity).type_mask = POLYGON_MASK;
	ComplexPolygon& polygon = createComplexPolygon(entity, edges, room_center_pos, { room_width, room_height }, 0.f, true);

	return entity;
}

void createWorldBounds()
{
	ComplexPolygon& polygon = registry.polygons.components[0];
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge edge = polygon.world_edges[i];

		vec2 edge_vector = edge.vertex2 - edge.vertex1;
		vec2 edge_center = edge.vertex1 + edge_vector / 2.f;
		Entity edge_line = createWorldBounds(edge_center, { length(edge_vector) + 5.f, 4.f }, DIFFUSE_ID::BLACK, vec3(1.f, 0.f, 0.f), 0.4f);
		registry.motions.get(edge_line).angle = atan2(edge_vector.y, edge_vector.x);
	}
}

Entity createExit(vec2 pos, int next_room_ind) {
	auto entity = Entity();

	RoomExit& exit = registry.exits.emplace(entity);
	exit.next_room_ind = next_room_ind;

	// Setting initial motion values
	Motion& motion = createMotion(entity, DOOR_MASK, pos, vec2(50.f), 0.f);
	motion.radius = abs(motion.scale.x) / 2.f;
	motion.sprite_normal = { 0,0,1 };

	RenderRequest& render_request = registry.renderRequests.insert(entity,{ DIFFUSE_ID::ROOM_EXIT_DISABLED });
	render_request.casts_shadow = false; // TODO: Possibly add it to ground pieces instead?
	render_request.specular = vec3(0.f);
	render_request.shininess = 1.f;
	render_request.is_ground_piece = true;

	registry.groundPieces.emplace(entity);

	return entity;
}

Entity createRoom(RenderSystem* renderer, float tile_size, std::string json_path) { // Leaving renderer here for future in case
	(void)renderer;

	Entity entity = Entity();
	
	Room& room = registry.rooms.emplace(entity);
	rapidjson::Document dom = room.loadFromJSONFile(json_path);

	// Set grid size
	room.grid_size.x = (int)dom["grid_size"]["num_cols"].GetInt();	// Grid width
	room.grid_size.y = (int)dom["grid_size"]["num_rows"].GetInt();	// Grid height
	
	int room_type = (dom.HasMember("room_type")) ? dom["room_type"].GetInt() : 1;
	if (room_type == 1) {   // DIFFUSE_ID::GRASS, NORMAL_ID::GRASS
		createRoomGround(room, DIFFUSE_ID::GRASS, NORMAL_ID::GRASS, 12.f, vec3(168.f, 255.f, 49.f) / 255.f * 1.5f); //vec3(0.4f, 0.7f, 0.f);
	} else {
		createRoomGround(room, DIFFUSE_ID::MENU, NORMAL_ID::FLAT, 3.f);
	}

	// Initializes room's path finder generator
	room.generator.setWorldSize({ (int) room.grid_size.x,(int) room.grid_size.y });
	room.generator.setHeuristic(AStar::Heuristic::euclidean);
	//room.generator.setDiagonalMovement(true);
	
	// Set player spawn
	room.player_spawn.x = dom["player_spawn"]["x"].GetFloat() + 0.5f;
	room.player_spawn.y = dom["player_spawn"]["y"].GetFloat() + 0.5f;
	room.player_spawn *= tile_size;
	
	const rapidjson::Value& enemy_spawners = dom["enemy_spawners"];
	for (rapidjson::SizeType i = 0; i < enemy_spawners.Size(); i++) {
		float spawn_x = enemy_spawners[i]["x"].GetFloat() + 0.5f;
		float spawn_y = enemy_spawners[i]["y"].GetFloat() + 0.5f;

		std::vector<ENEMY_TYPE> spawner_enemies;
		const rapidjson::Value& enemies = enemy_spawners[i]["enemies"];
		for (rapidjson::SizeType j = 0; j < enemies.Size(); j++) {
			std::string enemy_name = enemies[j].GetString();

			if (enemy_name == "worm") {
				spawner_enemies.push_back(ENEMY_TYPE::WORM);
			}
			else if (enemy_name == "rat") {
				spawner_enemies.push_back(ENEMY_TYPE::RAT);
			} 
			else if (enemy_name == "squirrel") {
				spawner_enemies.push_back(ENEMY_TYPE::SQUIRREL);
			}
			else if (enemy_name == "bear") {
				spawner_enemies.push_back(ENEMY_TYPE::BEAR);
			} 
			else if (enemy_name == "boar") {
				spawner_enemies.push_back(ENEMY_TYPE::BOAR);
			}
			else if (enemy_name == "deer") {
				spawner_enemies.push_back(ENEMY_TYPE::DEER);
			}
			else if (enemy_name == "fox") {
				spawner_enemies.push_back(ENEMY_TYPE::FOX);
			}
			else if (enemy_name == "rabbit") {
				spawner_enemies.push_back(ENEMY_TYPE::RABBIT);
			}
			else if (enemy_name == "wolf") {
				spawner_enemies.push_back(ENEMY_TYPE::ALPHAWOLF);
			}
			else if (enemy_name == "witch") {
				spawner_enemies.push_back(ENEMY_TYPE::WITCH);
			}
			else if (enemy_name == "skip") {
				spawner_enemies.push_back(ENEMY_TYPE::SKIP);
			} else {
				printf("Error: Uknown enemy name found in spawner data");
				assert(false);
			}
		}

		Entity spawner = createSpawner({ tile_size * spawn_x, tile_size * spawn_y }, spawner_enemies);
		room.num_waves = max(room.num_waves, (int) enemies.Size());
	}

	if (dom["obstacles"].HasMember("tree")) {
		const rapidjson::Value& obstacleSize = dom["obstacles"]["tree"]["size"];
		int obstacleWidth = obstacleSize["w"].GetInt();
		int obstacleHeight = obstacleSize["h"].GetInt();
		const rapidjson::Value& trees = dom["obstacles"]["tree"]["pos"];
		for (rapidjson::SizeType i = 0; i < trees.Size(); i++) {
			float spawn_x = trees[i]["x"].GetFloat() + 0.5f;;
			float spawn_y = trees[i]["y"].GetFloat() + 0.5f;;

			Entity tree = createTree(vec2(tile_size * spawn_x, tile_size * spawn_y));

			for (int x = (int)spawn_x; x < spawn_x + obstacleWidth; x++) {
				for (int y = (int)spawn_y; y < spawn_y + obstacleHeight; y++) {
					//printf("createRoom: %d : %d\n", x, y);
				}
			}
		}
	}
	if (dom["doors"].HasMember("exit")) {
		const rapidjson::Value& exits = dom["doors"]["exit"];
		for (rapidjson::SizeType i = 0; i < exits.Size(); i++) {
			float spawn_x = exits[i]["x"].GetFloat() + 0.5f;;
			float spawn_y = exits[i]["y"].GetFloat() + 0.5f;;
			
			int next_room_ind = -1;
            if (exits[i].HasMember("next_room_ind")) {
                next_room_ind = exits[i]["next_room_ind"].GetInt();
            }else if(exits[i].HasMember("load")){
				next_room_ind = LOAD_GAME_INDEX;
			}

      		Entity exit = createExit(vec2(tile_size * spawn_x, tile_size * spawn_y), next_room_ind);
		}
	}

	return entity;
}

Entity createNavigator(vec2 pos, float angle) {
	Entity entity = Entity();
	auto& ui_elem = registry.ui_elements.emplace(entity);
	ui_elem.interactive = false;
	registry.navigators.emplace(entity);
	registry.renderRequests.insert(entity, { DIFFUSE_ID::NAVIGATOR, NORMAL_ID::NORMAL_COUNT, EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	auto& motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = angle;
	motion.scale = vec2{ NAVIGATOR_SCALE.x, NAVIGATOR_SCALE.y };
	return entity;
}

Entity createIgnoreGroundPiece(RenderSystem* renderer, vec2 position, vec2 scale, float angle, DIFFUSE_ID diffuse_id)
{
	Entity entity = createGroundPiece(position, scale, angle, diffuse_id, NORMAL_ID::FLAT, 1.f);
	Motion& motion = registry.motions.get(entity);
	motion.type_mask = POLYGON_MASK;
	motion.sprite_offset.y = -5.f;

	RenderRequest& render_request = registry.renderRequests.get(entity);
	render_request.geometry_id = GEOMETRY_ID::PLANE;
	render_request.casts_shadow = true;

	registry.obstacles.insert(entity, { OBSTACLE_TYPE::PLATFORM });

	Mesh& mesh = renderer->getMesh(render_request.geometry_id);
	registry.meshPtrs.emplace(entity, &mesh);

	ComplexPolygon& polygon = createComplexPolygon(entity, mesh.edges, motion, false, false);
	polygon.world_edges[0].is_collidable = false;
	polygon.world_edges[1].is_collidable = false;
	polygon.world_edges[2].is_collidable = false;
	polygon.world_edges[3].is_collidable = false;

	return entity;
}
