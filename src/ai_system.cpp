#include <glm/gtc/random.hpp>
#include <iostream>
#include <optional>
#include <SDL_mixer.h>
#include <world_system.hpp>
//https://github.com/daancode/a-star
#include "../ext/pathfinder/AStar.cpp" 
// internal
#include "ai_system.hpp"
#include "world_init.hpp"
#include "physics_system.hpp"

const float AGGRO_RANGE = 600.f;
const float RANGED_STOP_RANGE = 300.f;
const float ATTRACTOR_RADIUS = 300.f;

const float EPS = 0.1f;

inline bool is_close(vec2 a, vec2 b) {
	return dot(a-b, a-b) < 1000.f;
}

bool is_target_path_clear(vec2 line_start, vec2 line_end)
{
	std::unordered_map<unsigned int, bool> entities_seen; // So that we don't do calculations for polygons multiple times
	entities_seen.reserve(20);
	std::vector<Entity> entities_found = SpatialGrid::getInstance().query_ray_cast(line_start, line_end);
	for (uint i = 0; i < entities_found.size(); i++) {
		Entity entity_other = entities_found[i];
		if (entities_seen.count(entity_other) > 0) { continue; } // If the entity exists in the map already, then skip
		entities_seen[entity_other] = true;
		Motion& motion_other = registry.motions.get(entity_other);
		if (motion_other.type_mask == POLYGON_MASK) {
			ComplexPolygon& polygon = registry.polygons.get(entity_other);
			if (!polygon.is_only_edge && is_line_polygon_edge_colliding(polygon, line_start, line_end)) {
				return false;
			}
		} else if (motion_other.type_mask == OBSTACLE_MASK) {
			vec2 is_colliding = is_circle_line_colliding(motion_other.position, motion_other.radius + 10.f, line_start, line_end);
			if (is_colliding.x != 0.f || is_colliding.y != 0.f) {
				return false;
			}
		}
	}
	return true;
}

//float euclidean_dist(Motion& motion, Motion& other_motion) {
//	return sqrt(pow(motion.position.x - other_motion.position.x, 2.f) + pow(motion.position.y - other_motion.position.y, 2.f));
//}

// Sets the moving direction of the src_motion to to pathfind to the target in the current room
// Source is src_motion.position, target is given position
void pathfind_to_target(Motion& src_motion, vec2 target_pos) {
	
	// If possible to go directly to target, do so
	// TODO: Find out why this doesn't work
	if (is_target_path_clear(src_motion.position, target_pos)) {
		src_motion.move_direction = normalize(target_pos - src_motion.position);
	} else {		
		AStar::Vec2i target = {
			(int)(target_pos.x / WorldSystem::TILE_SIZE),
			(int)(target_pos.y / WorldSystem::TILE_SIZE)
		};
		AStar::Vec2i source_pos = {
			(int)(src_motion.position.x / WorldSystem::TILE_SIZE),
			(int)(src_motion.position.y / WorldSystem::TILE_SIZE)
		};

		// Gets the pathfinder generator of the current room
		AStar::Generator& cur_generator = registry.rooms.components[0].generator;
		AStar::CoordinateList path = cur_generator.findPath(target, source_pos);

		AStar::Vec2i move_coordinate;
		if (path.size() >= 2) {
			// Move to the next tile in the path
			move_coordinate = path[1];
			vec2 move_dir = { move_coordinate.x - source_pos.x, move_coordinate.y - source_pos.y };
			src_motion.move_direction = normalize(move_dir);
		}
		else {
			// Go directly to target
			move_coordinate = path[0];
			src_motion.move_direction = normalize(target_pos - src_motion.position);
		}
	}
}

std::optional<Entity> get_closest_attractor(vec2 position) {
	std::optional<Entity> closest_attractor = std::nullopt;
	float closest_attractor_distance = ATTRACTOR_RADIUS;
	for (Entity e : registry.enemyAttractors.entities) {
		auto& attractor_motion = registry.motions.get(e);
		
		float distance = length(position - attractor_motion.position);
		if (distance < closest_attractor_distance) {
			closest_attractor_distance = distance;
			closest_attractor = e;
		}
	}
	return closest_attractor;
}

// If an attractor is close, move towards it. Otherwise move towards player
// Return the attractor position
void pathfind_to_attractor(Motion& src_motion, Entity attractor) {	

	// Pathfind to attractor
	vec2 attractor_position = registry.motions.get(attractor).position;

	// if already on attractor, hit it
	// This is a bad solution, because it ties the attractor hit rate to the ai update rate
	if (is_close(src_motion.position, attractor_position)) {
		// Stand still while eating
		src_motion.move_direction = {0, 0};
		
		auto& attractor_data = registry.enemyAttractors.get(attractor);
		attractor_data.hits_left--;
		printf("ate breadcrumb\n");
		createBreadcrumbEatingEffect((src_motion.position + attractor_position)/2.f);
	} else {
		pathfind_to_target(src_motion, attractor_position);
	}
}

void ratDT(Entity player, vec2 player_position, Entity entity, Enemy& enemy_data) {
	Motion& motion = registry.motions.get(entity);
	vec2 diff = player_position - motion.position;
	Healthy& health = registry.healthies.get(entity);
	int maxHp = (int) health.max_hp;

	// If attractor close, go to it
	auto closest_attractor = get_closest_attractor(motion.position);
	if (closest_attractor) {
		registry.spriteSheets.get(entity).set_track(0);
		pathfind_to_attractor(motion, *closest_attractor);
		enemy_data.target = closest_attractor;

		// If interacting with attractor, use idle pose
		if (motion.move_direction == vec2{0, 0}) {
			registry.spriteSheets.get(entity).set_track(2);
		}
	}
	// If player within range and enemy hp is high, attack
	else if (length(diff) < AGGRO_RANGE) {
		if (health.current_hp > 0.3 * maxHp) {
			pathfind_to_target(motion, player_position);
			registry.spriteSheets.get(entity).set_track(0);
			enemy_data.target = player;
		}
		else {
			// if low hp, run away from player
			motion.move_direction = -normalize(diff);
			registry.spriteSheets.get(entity).set_track(0);
			enemy_data.target = std::nullopt;
		}
	}
	// If out of range and hp low, sleep to heal
	else if (health.current_hp < maxHp) {
		registry.spriteSheets.get(entity).set_track(1);
		motion.move_direction = { 0, 0 };
		health.current_hp += 1;
		createHealingEffect(motion.position, entity);
		enemy_data.target = std::nullopt;
	}
	// full hp and player not within attack range: wander or idle
	else {
		int num = rand() % 10;// randomly decide if the enemy idles or wanders
		if (num > 5) {
			motion.move_direction = circularRand(1.f);
			registry.spriteSheets.get(entity).set_track(0);
		}
		else {
			motion.move_direction = { 0, 0 };
			registry.spriteSheets.get(entity).set_track(2);
		}
		enemy_data.target = std::nullopt;
	}
	// Flip sprite if going right
	registry.renderRequests.get(entity).flip_texture = motion.move_direction.x > 0;
}

void wormDT(Entity player, vec2 player_position, Entity entity, Enemy& enemy_data) {
	Healthy& health = registry.healthies.get(entity);
	Motion& motion = registry.motions.get(entity);

	// Worm behavior:
		// If undamaged, sleep or idle
		// If damaged and player in aggro range, attack
		// If damaged and player not in aggro range, wander

	// If attractor close, go to it
	auto closest_attractor = get_closest_attractor(motion.position);
	if (closest_attractor) {
		pathfind_to_attractor(motion, *closest_attractor);
		registry.spriteSheets.get(entity).set_track(0);
		enemy_data.target = closest_attractor;
	}
	else if (health.current_hp == health.max_hp) {
		// In case worm was moving for some reason (attractor)
		motion.move_direction = { 0, 0 };
		// 1/3 chance of idling. 2/3 chance to sleep
		if (rand() % 3) {
			registry.spriteSheets.get(entity).set_track(1);
		}
		else {
			registry.spriteSheets.get(entity).set_track(2);
		}
		enemy_data.target = std::nullopt;
	}
	else {
		vec2 diff = player_position - motion.position;
		// Attack player
		if (length(diff) < AGGRO_RANGE) {
			pathfind_to_target(motion, player_position);
			registry.spriteSheets.get(entity).set_track(0);
			enemy_data.target = player;
		}
		// Wander
		else {
			motion.move_direction = circularRand(1.f);
			registry.spriteSheets.get(entity).set_track(0);
			enemy_data.target = std::nullopt;
		}
	}

	registry.renderRequests.get(entity).flip_texture = motion.move_direction.x > 0;
}

void snailDT(Entity player, vec2 player_position, Entity enemy, Enemy& enemy_data) {
	Motion& motion = registry.motions.get(enemy);
	pathfind_to_target(motion, player_position);
	enemy_data.target = player;

	// Flip sprite if going right
	registry.renderRequests.get(enemy).flip_texture = motion.move_direction.x > 0;
}

void squirrelDT(Entity player, vec2 player_position, Entity enemy, Enemy& enemy_data) {
	assert(registry.rangedEnemies.has(enemy));
	RangedEnemy& rangedEnemy = registry.rangedEnemies.get(enemy);

	Motion& motion = registry.motions.get(enemy);
	Motion& player_motion = registry.motions.get(player);
	vec2 diff = player_position - motion.position;

	// If attractor close, go to it
	auto closest_attractor = get_closest_attractor(motion.position);
	if (closest_attractor) {
		pathfind_to_attractor(motion, *closest_attractor);
		rangedEnemy.can_shoot = false;
		enemy_data.target = closest_attractor;
	}
	else if (length(diff) < rangedEnemy.stop_chasing_range) {
		rangedEnemy.can_shoot = true;
		motion.look_direction = normalize(diff);
		if (registry.players.get(player).character == PLAYER_CHARACTER::HANSEL) {
			motion.move_direction = cos(player_motion.look_direction); // enemy trying to move away from player's aim
		}
		else { // player is gretel, enemy stay stationary while shooting
			motion.move_direction = { 0, 0 };
		}
		enemy_data.target = std::nullopt;
	}
	else  {
		rangedEnemy.can_shoot = false;

		if (length(diff) < AGGRO_RANGE) {
			// run to player 
			motion.move_direction = normalize(diff);
			motion.look_direction = normalize(diff);
			enemy_data.target = player;
		}
		else {
			// walk randomly
			int num = rand() % 10;
			if (num > 5) {
				motion.move_direction = circularRand(1.f);
			}
			else {
				motion.move_direction = { 0, 0 };
			}
			enemy_data.target = std::nullopt;
		}
	}
	// Flip sprite if looking right
	registry.renderRequests.get(enemy).flip_texture = motion.look_direction.x > 0;
}

void bearDT(vec2 player_position, Entity entity, Enemy& enemy_data) {
	// logic:
	// bear starts with half of its full health
	// bear is immune to knock back
	// if health above certain threshold:
	//	not hungry
	//	in run mode
	//	chase after player
	// else
	//  hungry
	//	in walk mode
	//  chase after food

	Motion& motion = registry.motions.get(entity);
	Healthy& health = registry.healthies.get(entity);
	float threshold = 0.5; // in (unit) % of health remaining
	motion.max_speed = 125;
	bool is_hungry;
	((float) health.current_hp / (float) health.max_hp) > threshold ? is_hungry = false : is_hungry = true;

	auto closest_attractor = get_closest_attractor(motion.position);
	if (closest_attractor) {
		pathfind_to_attractor(motion, *closest_attractor);
		enemy_data.target = closest_attractor;
	}
	else if (!is_hungry) { // not hungry, then chase player
		if (length(player_position - motion.position) < AGGRO_RANGE) {
			pathfind_to_target(motion, player_position);
			registry.spriteSheets.get(entity).set_track(1);
		}
	}
	else { // if senses food, chase after it
		motion.max_speed = 100;
		pathfind_to_target(motion, player_position);
		registry.spriteSheets.get(entity).set_track(2);
	}
	enemy_data.target = std::nullopt;
	motion.look_direction = motion.move_direction;
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

void boarDT(vec2 player_position, Entity entity, Enemy& enemy_data) {
	// logic:
	// boar can charge powerful attack
	// if player in range: 
	//	point towards player
	//	accelerate to max speed in that direction
	//	deaccelerate and comes to a stop after certain distance (or amount of time)
	// else:
	//	wander around and in walk mode
	Motion& motion = registry.motions.get(entity);
	vec2 diff = player_position - motion.position;

	if (length(diff) < AGGRO_RANGE) {
		if (motion.velocity != vec2(0, 0)) {
			motion.max_speed = 0; // brings boar to sudden stop
			enemy_data.next_decision_ms = 500; // wait for 1500ms until next charage
			registry.spriteSheets.get(entity).set_track(0);
		}
		else {
			motion.max_speed = 400;
			enemy_data.next_decision_ms = 1000; // wait for 2000ms until comes to a stop
			motion.move_direction = normalize(diff);
			registry.spriteSheets.get(entity).set_track(1);
		}
	}
	else {
		// walk randomly
		motion.max_speed = 100;
		int num = rand() % 10;
		if (num > 5) {
			motion.move_direction = circularRand(1.f);
			registry.spriteSheets.get(entity).set_track(2);
		}
		else {
			motion.move_direction = { 0, 0 };
			registry.spriteSheets.get(entity).set_track(0);
		}
	}
	motion.look_direction = motion.move_direction;
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

void deerDT(vec2 player_position, Entity entity, Entity player) {
	// logic:
	// deer has low health but runs very fast, very sensitive to beings and escapes
	// if being in range: 
	//	runs away from beings
	//	is smart enough to not get stuck in corners

	// get the dimension of room for calculating deer's move direction
	Room& cur_room = registry.rooms.components[0];
	float room_width = cur_room.grid_size.x * WorldSystem::TILE_SIZE;
	float room_height = cur_room.grid_size.y * WorldSystem::TILE_SIZE;
	registry.enemies.get(entity).next_decision_ms -= 500; // deer has shorter interval between decision trees
	
	Motion& motion = registry.motions.get(entity);
	// direction vector and its weight from player to deer
	vec2 diff_to_player = motion.position - player_position;
	float weight_to_player = 0.9f;
	// direction vector and its weight from closest enemy to deer
	Entity closest_enemy;
	vec2 diff_to_closest_enemy = { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
	for (Entity e : registry.enemies.entities) {
		// either enemy is deer itself, or enemy is outside of deer's range
		if (entity == e || length(registry.motions.get(e).position - motion.position) > AGGRO_RANGE) continue; 
		Motion& enemy_motion = registry.motions.get(e);
		vec2 diff_to_current_enemy = motion.position - enemy_motion.position;
		if (length(diff_to_current_enemy) < length(diff_to_closest_enemy)) { // search for closest enemy, and update the direction vector
			diff_to_closest_enemy = diff_to_current_enemy;
			closest_enemy = e;
		}
	}
	float weight_to_closest_enemy = 1 - weight_to_player;
	// direction vector and its weight from wall to deer
	vec2 weight_from_wall = { max(1 - motion.position.x / room_width, 1 - (room_width - motion.position.x) / room_width),
							  max(1 - motion.position.y / room_height, 1 - (room_height - motion.position.y) / room_height) }; // the weight is proportional to how close the deer is to the wall
	vec2 diff_from_wall;
	// move direction is normalized, so here only needs to determine the signs
	// later multiply by the weight to get the actual direction vector
	room_width  - motion.position.x > motion.position.x ? diff_from_wall.x =  1 : diff_from_wall.x = -1; 
	room_height - motion.position.y > motion.position.y ? diff_from_wall.y =  1 : diff_from_wall.y = -1; 
	// determines how much weight to put in for direction vector from entities and direction vector from walls
	// overall_weight     = weight of direction vector from entities, 
	// 1 - overall_weight = weight of direction vector from walls
	float overall_weight = 0.5;
	// in run aggro range, runs
	if (length(diff_to_player) < AGGRO_RANGE) {
		motion.max_speed = 250;
		registry.spriteSheets.get(entity).set_track(1); // run
		if (registry.enemies.has(closest_enemy)) { // both an enemy close by and a player
			motion.move_direction = overall_weight * normalize(weight_to_player * diff_to_player + weight_to_closest_enemy * diff_to_closest_enemy) + 
									(1 - overall_weight) * normalize(weight_from_wall * diff_from_wall);
		}
		else { // only player 
			motion.move_direction = overall_weight * normalize(diff_to_player) + 
									(1 - overall_weight) * normalize(weight_from_wall * diff_from_wall);
		}
	}
	// in safe range, idles
	else {
		motion.max_speed = 100;
		// walk randomly
		motion.max_speed = 100;
		int num = rand() % 10;
		if (num > 5) {
			motion.move_direction = circularRand(1.f);
			registry.spriteSheets.get(entity).set_track(2);
		}
		else {
			motion.move_direction = { 0, 0 };
			registry.spriteSheets.get(entity).set_track(0);
		}
		
	}

	motion.look_direction = motion.move_direction;
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

void rabbitDT(vec2 player_position, Entity entity, Entity player, Enemy& enemy_data) {
	// logic:
	// rabbits are skippy and hard to predict its movement
	// if player in range: 
	//	hops away from player
	// if player gets too close:
	//	runs away from player
	// drops rabbit foot (haven't decide for what purpose)

	Motion& motion = registry.motions.get(entity);
	vec2 diff =  motion.position - player_position;
	// different aggro ranges for different rabbit behaviors
	float HOP_AGGRO_RANGE = AGGRO_RANGE;
	float RUN_AGGRO_RANGE = AGGRO_RANGE / 2.5f;

	// force rabbit to stop and idle when not in run aggro range
	if (motion.velocity != vec2(0, 0) && length(diff) > RUN_AGGRO_RANGE) {
		motion.max_speed = 0; // brings rabbit to sudden stop
		enemy_data.next_decision_ms = -500; // wait for 500ms until next rest
		registry.spriteSheets.get(entity).set_track(3);
	}
	else {
		if (length(diff) < HOP_AGGRO_RANGE) {
			// in run aggro range, runs
			if (length(diff) < RUN_AGGRO_RANGE) {
				motion.max_speed = 150;
				registry.spriteSheets.get(entity).set_track(2);
			}
			// in hop aggro range, hops
			else {
				motion.max_speed = 120;
				registry.spriteSheets.get(entity).set_track(0);
			}
			// determine rabbit's move direction, 
			// does not move in the same direction as the directional vector from player to rabbit
			// instead, sometimes move slightly left, sometimes move slightly right
			float epsilon = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / 0.5f) + 0.75f; // random number in [0.75, 1.25]
			vec2 range = { epsilon * diff.x, epsilon * diff.y };
			motion.move_direction = normalize(range);
			motion.look_direction = normalize(range);
		}
		// in safe range, idles
		else {
			motion.move_direction = { 0, 0 };
			registry.spriteSheets.get(entity).set_track(1);
		}
	}
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

void foxDT(vec2 player_position, Entity entity, Enemy& enemy_data) {
	// logic:

	Motion& motion = registry.motions.get(entity);
	vec2 diff = player_position - motion.position;
	// Attack player
	if (length(diff) < AGGRO_RANGE) {
		motion.max_speed = 130;
		pathfind_to_target(motion, player_position);
		registry.spriteSheets.get(entity).set_track(1);
	}
	// outisde of aggro range, idle
	else {
		// walk randomly
		motion.max_speed = 100;
		int num = rand() % 10;
		if (num > 5) {
			motion.move_direction = circularRand(1.f);
			registry.spriteSheets.get(entity).set_track(2);
		}
		else {
			motion.move_direction = { 0, 0 };
			registry.spriteSheets.get(entity).set_track(0);
		}
	}
	motion.look_direction = motion.move_direction;
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

int howl_interval = 0;
void alphaWolfDT(vec2 player_position, Entity entity, Enemy& enemy_data) {
	// logic:
	// alpha wolf can howl and summon wolf companions
	// for every certain amount of interval, howl
	// chases after player
	Motion& motion = registry.motions.get(entity);
	vec2 diff = player_position - motion.position;

	howl_interval += 1000;

	// if didn't howl, howl, stop moving, summon a wolf companion behind the wolf
	if (!enemy_data.howled && enemy_data.curr_num_of_companions < enemy_data.max_num_of_companions) {
		motion.max_speed = 0; // brings wolf to sudden stop
		enemy_data.next_decision_ms = 1000; // howl for 2000ms
		enemy_data.curr_num_of_companions += 1;
		enemy_data.howled = true;

		registry.spriteSheets.get(entity).set_track(0);
		createWolf(motion.position); // note: this is not the same method used to create alpha wolves
	}
	// in aggro range, runs to player
	else if (length(diff) < AGGRO_RANGE && enemy_data.howled) {
		motion.max_speed = 160;
		pathfind_to_target(motion, player_position);
		// random interval: [20000ms, 30000ms]
		if (howl_interval > (rand() % 10000 + 20000) && enemy_data.curr_num_of_companions < enemy_data.max_num_of_companions) {
			enemy_data.howled = false;
			howl_interval = 0;
		}
		registry.spriteSheets.get(entity).set_track(1);
	}
	// outisde of aggro range, idle
	else {
		// walk randomly
		motion.max_speed = 130;
		motion.move_direction = circularRand(1.f);
		registry.spriteSheets.get(entity).set_track(2);
	}

	motion.look_direction = motion.move_direction;
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

void wolfDT(vec2 player_position, Entity entity, Enemy& enemy_data) {
	// logic:
	// wolf companions
	// chases after player

	Motion& motion = registry.motions.get(entity);
	vec2 diff = player_position - motion.position;
	auto closest_attractor = get_closest_attractor(motion.position);
	if (closest_attractor) {
		pathfind_to_attractor(motion, *closest_attractor);
		enemy_data.target = closest_attractor;
	}
	// in aggro range, runs to player
	else if (length(diff) < AGGRO_RANGE) {
		motion.max_speed = 100;
		pathfind_to_target(motion, player_position);
		registry.spriteSheets.get(entity).set_track(1);
	}
	// outisde of aggro range, idle
	else {
		// walk randomly
		motion.max_speed = 60;
		motion.move_direction = circularRand(1.f);
		registry.spriteSheets.get(entity).set_track(2);
	}
	motion.look_direction = motion.move_direction;
	registry.renderRequests.get(entity).flip_texture = motion.look_direction.x > 0;
}

void hellfire(vec2 player_position, Entity witch, Witch& witch_spell, Motion& witch_motion, SpriteSheetAnimation& spriteSheet) {
	float radius = witch_spell.hellfire_radius;
	// make sure warnings are only created when just began to cast this spell
	if (witch_spell.spell_counters[0] == 0.f) {
		int num = rand() % 5 + 1;
		switch (num) {
			// draw a square
			case(1): {
				int hellfire_per_side = (witch_spell.max_hellfire_allowed / 4);
				// distance between each hellfire circle
				float padding = radius / (float)hellfire_per_side;
				// initial positions correspond to 4 corners of a square
				// every time start at one corner, create hellfire in sequence in one direction (separated by padding) 
				vec2 initial_position_1 = witch_motion.position - vec2(radius / 2.f, radius / 2.f);
				vec2 initial_position_2 = witch_motion.position + vec2(radius / 2.f, radius / 2.f);
				vec2 initial_position_3 = witch_motion.position + vec2(radius / 2.f, -radius / 2.f);
				vec2 initial_position_4 = witch_motion.position - vec2(radius / 2.f, -radius / 2.f);
				for (int i = 0; i < hellfire_per_side; i++) {
					createHellfireWarning(vec2(initial_position_1.x + i * padding, initial_position_1.y), witch);
					createHellfireWarning(vec2(initial_position_2.x - i * padding, initial_position_2.y), witch);
					createHellfireWarning(vec2(initial_position_3.x, initial_position_3.y + i * padding), witch);
					createHellfireWarning(vec2(initial_position_4.x, initial_position_4.y - i * padding), witch);
				}
				break;
			}
			// draw a diamond
			case(2): {
				// the scale of diamond is random
				float scale = rand() % 3 + 2;
				int hellfire_per_side = (witch_spell.max_hellfire_allowed / 4);
				// distance between each hellfire circle
				float padding = radius / (float)hellfire_per_side;
				padding /= scale;
				// initial positions correspond to 4 corners of a diamond
				// every time start at one corner, create hellfire in sequence in one diagonal direction (separated by padding) 
				vec2 initial_position_1 = witch_motion.position;
				initial_position_1.y -= radius / scale;
				vec2 initial_position_2 = witch_motion.position;
				initial_position_2.y += radius / scale;
				vec2 initial_position_3 = witch_motion.position;
				initial_position_3.x -= radius / scale;
				vec2 initial_position_4 = witch_motion.position;
				initial_position_4.x += radius / scale;
				for (int i = 0; i < hellfire_per_side; i++) {
					createHellfireWarning(initial_position_1 + i * padding, witch);
					createHellfireWarning(initial_position_2 - i * padding, witch);
					createHellfireWarning(vec2(initial_position_3.x + i * padding,
						initial_position_3.y - i * padding), witch);
					createHellfireWarning(vec2(initial_position_4.x - i * padding,
						initial_position_4.y + i * padding), witch);
				}
				break;
			}
		// draw a cross at player's position 
		//case(3): {
		//	float circle_radius = radius / 2.f;
		//	for (float i = 0; i < 2 * M_PI; i += 0.628) {
		//		createHellfire(witch_motion.position + circle_radius * vec2(cos(i), sin(i)), witch);
		//	}
		//	break;
		//}
			case(3): {
				float hellfire_accuracy = witch_spell.hellfire_accuracy;
				createHellfireWarning(player_position, witch);
				for (int i = 0; i < witch_spell.max_hellfire_allowed / 2 - 1; i++) {
					vec2 offset;
					offset.x = rand() % (int) (100 - hellfire_accuracy) + hellfire_accuracy;
					offset.y = rand() % (int) (100 - hellfire_accuracy) + hellfire_accuracy;
					createHellfireWarning(player_position * offset / 100.f, witch);
				}
				break;
			}
			case(4): {
				float hellfire_accuracy = witch_spell.hellfire_accuracy;
				createHellfireWarning(player_position, witch);
				for (int i = 0; i < witch_spell.max_hellfire_allowed / 2 - 1; i++) {
					vec2 offset;
					offset.x = rand() % (int)(100 - hellfire_accuracy) + hellfire_accuracy;
					offset.y = rand() % (int)(100 - hellfire_accuracy) + hellfire_accuracy;
					createHellfireWarning(player_position * offset / 100.f, witch);
				}
				break;
			}
			case(5): {
				float hellfire_accuracy = witch_spell.hellfire_accuracy;
				createHellfireWarning(player_position, witch);
				for (int i = 0; i < witch_spell.max_hellfire_allowed / 2 - 1; i++) {
					vec2 offset;
					offset.x = rand() % (int)(100 - hellfire_accuracy) + hellfire_accuracy;
					offset.y = rand() % (int)(100 - hellfire_accuracy) + hellfire_accuracy;
					createHellfireWarning(player_position * offset / 100.f, witch);
				}
				break;
			}
		}
	}
	// make sure hellfires are only created when warnings lasted for certain time 
	//else if (witch_spell.spell_counters[0] == witch_spell.hellfire_duration - 2000.f) {
	//	spriteSheet.set_track(2);
	//	//createHellfire();
	//}
	witch_spell.spell_counters[0] += 1000;	
}
void swipe(vec2 witch_position, Entity witch, Witch& witch_spell) {
	if (witch_spell.spell_counters[1] == witch_spell.swipe_duration) {
		createPush(witch_position, witch);
	}
	witch_spell.spell_counters[1] += 1000;
}
void fireball(vec2 player_position, Motion& witch_motion, RangedEnemy& witch_range_info, Witch& witch_spell) {
	vec2 diff = player_position - witch_motion.position;
	witch_motion.move_direction = normalize(diff);
	witch_motion.look_direction = normalize(diff);
	if (witch_spell.spell_counters[2] != witch_spell.fireball_duration) {
		witch_range_info.can_shoot = true;
	}
	else {
		witch_range_info.can_shoot = false;
	}
	witch_spell.spell_counters[2] += 1000;
}
void dash(vec2 player_position, Motion& witch_motion, Witch& witch_spell) {
	if (witch_spell.spell_counters[3] == 0.f) {
		vec2 diff = player_position - witch_motion.position;
		witch_motion.move_direction = normalize(diff);
		witch_motion.look_direction = normalize(diff);
		witch_motion.max_speed = 250.f;
	}
	//else if (witch_spell.spell_counters[3] == 1000.f) {
	//	vec2 diff = - player_position + witch_motion.position;
	//	witch_motion.move_direction = normalize(diff);
	//	witch_motion.look_direction = normalize(diff);
	//	witch_motion.max_speed = 500.f;
	//}
	else if (witch_spell.spell_counters[3] == witch_spell.dash_duration) {
		witch_motion.max_speed = 0.f;
	}
	witch_spell.spell_counters[3] += 1000;
}
void witchDT(vec2 player_position, Entity witch, Enemy& witch_info) {
	Motion& witch_motion = registry.motions.get(witch);
	RangedEnemy& witch_range_info = registry.rangedEnemies.get(witch);
	Witch& witch_spell = registry.witches.get(witch);
	SpriteSheetAnimation& spriteSheet = registry.spriteSheets.get(witch);
	int casted = witch_spell.last_spell_casted; // not a pointer, modify the value doesn't work
	int num;
	// decide if the spell casted has ended
	// if ended, reset that counter, stop all, reset casted to -1
	if (casted > -1 && witch_spell.spell_counters[casted] > witch_spell.spell_durations[casted]) {
		// if adding 1000 ms to current spell counter exceeds the duration,
		// then activate cast spell interval for the witch
		witch_info.next_decision_ms += witch_spell.cast_spells_interval;
		witch_spell.spell_counters[casted] = 0.f;
		witch_spell.last_spell_casted = -1;
		num = -1;
		spriteSheet.set_track(0);
	}
	else if (casted > -1 &&
		witch_spell.spell_counters[casted] <= witch_spell.spell_durations[casted]) {
		num = witch_spell.last_spell_casted + 1; // 0-indexing to 1-indexing
	}
	else {
		num = rand() % 4 + 1; // all spells have equal weights for now
		witch_spell.last_spell_casted = num - 1; // 1-indexing to 0-indexing
	}
	///////////////////////////////////////////////////////////////////////
	switch (num) {
		case(1):
			spriteSheet.set_track(4);
			printf("Witch's next spell is hellfire\n");
			hellfire(player_position, witch, witch_spell, witch_motion, spriteSheet);
			break;
		case(2):
			spriteSheet.set_track(0);
			printf("Witch's next spell is swipe\n");
			swipe(witch_motion.position, witch, witch_spell);
			break;
		case(3):
			spriteSheet.set_track(2);
			printf("Witch's next spell is fireball\n");
			fireball(player_position, witch_motion, witch_range_info, witch_spell);
			witch_info.next_decision_ms -= 500.f;
			break;
		case(4):
			spriteSheet.set_track(3);
			printf("Witch's next spell is dash\n");
			dash(player_position, witch_motion, witch_spell);
			break;
		case(-1):
			spriteSheet.set_track(0);
			//skipped 
			break;
	}	
	registry.renderRequests.get(witch).flip_texture = witch_motion.look_direction.x > 0;
}

void ranged_shooting_update(Entity enemy, float elapsed_ms) {
	RangedEnemy& rangedEnemy = registry.rangedEnemies.get(enemy);
	Motion& motion = registry.motions.get(enemy);

	rangedEnemy.time_since_last_shot_ms += elapsed_ms;

	if (rangedEnemy.can_shoot && rangedEnemy.time_since_last_shot_ms > rangedEnemy.shooting_interval) {
		if (registry.enemies.get(enemy).type == ENEMY_TYPE::SQUIRREL) {
			createAcornProjectile(motion.position, vec2(0, -10), motion.look_direction, enemy);
		}
		else if (registry.enemies.get(enemy).type == ENEMY_TYPE::WITCH) {
			createFireballProjectile(motion.position, vec2(0, -10), motion.look_direction, enemy);
		}
		rangedEnemy.time_since_last_shot_ms = 0;
	}

	// Flip sprite if going right
	registry.renderRequests.get(enemy).flip_texture = motion.look_direction.x > 0;
}


void AISystem::step(float elapsed_ms)
{
	// Checks for each individual ranged enemy whether they can shoot yet
	// Individual timers so that shooting is not synchronized
	for (int i = 0; i < registry.rangedEnemies.entities.size(); i++) {
		ranged_shooting_update(registry.rangedEnemies.entities[i], elapsed_ms);
	}

	Entity player = registry.players.entities[0];
	vec2 player_position = registry.motions.get(player).position; // Future: + registry.motions.get(player).sprite_offset;
	auto& enemies = registry.enemies;

	for (int i = 0; i < enemies.components.size(); i++) {
		Entity entity = enemies.entities[i];
		Enemy& enemy = enemies.components[i];
		enemy.next_decision_ms -= elapsed_ms;
		enemy.next_pathfinding_ms -= elapsed_ms;

		if (enemy.next_decision_ms <= 0) {
			switch (enemy.type)
			{
			case ENEMY_TYPE::SPIDER:
				ratDT(player, player_position, entity, enemy);
				break;
			case ENEMY_TYPE::WORM:
				wormDT(player, player_position, entity, enemy);
				break;
			case ENEMY_TYPE::RAT:
				ratDT(player, player_position, entity, enemy);
				break;
			case ENEMY_TYPE::SQUIRREL:
				squirrelDT(player, player_position, entity, enemy);
				break;
			case ENEMY_TYPE::SNAIL:
				snailDT(player, player_position, entity, enemy);
				break;
			case ENEMY_TYPE::BEAR:
				bearDT(player_position, entity, enemy);
				break;
			case ENEMY_TYPE::BOAR:
				boarDT(player_position, entity, enemy);
				break;
			case ENEMY_TYPE::DEER:
				deerDT(player_position, entity, player);
				break;
			case ENEMY_TYPE::FOX:
				foxDT(player_position, entity, enemy);
				break;
			case ENEMY_TYPE::RABBIT:
				rabbitDT(player_position, entity, player, enemy);
				break;
			case ENEMY_TYPE::ALPHAWOLF:
				alphaWolfDT(player_position, entity, enemy);
				break;
			case ENEMY_TYPE::WOLF:
				wolfDT(player_position, entity, enemy);
				break;
			case ENEMY_TYPE::WITCH:
				witchDT(player_position, entity, enemy);
				break;
			default:
				assert(false);
			}
			enemy.next_decision_ms += 1000.f;
			//if (enemy.type != ENEMY_TYPE::WITCH) {
			//	enemy.next_decision_ms += 1000.f;
			//}
			// Decision trees also do pathfinding, so no need to do it again immediately
			enemy.next_pathfinding_ms = 200.f;
		}
		if (enemy.next_pathfinding_ms <= 0 && enemy.target) {
			// Target may have been removed from game
			if (registry.motions.has(*enemy.target)) {
				auto& target_motion = registry.motions.get(*enemy.target);
				auto& enemy_motion = registry.motions.get(entity);
				vec2 target_pos = target_motion.position;
				pathfind_to_target(registry.motions.get(entity), target_pos);
				if (registry.rangedEnemies.has(entity)) {
					registry.renderRequests.get(entity).flip_texture = enemy_motion.look_direction.x > 0;
				} else {
					registry.renderRequests.get(entity).flip_texture = enemy_motion.move_direction.x > 0;
				}
			} else {
				enemy.target = std::nullopt;
			}
			enemy.next_pathfinding_ms += 200.f;
		}
	}
}


