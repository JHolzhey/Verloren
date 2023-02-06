// internal
#include "physics_system.hpp"
#include "render_system.hpp"
#include "world_init.hpp"

SpatialGrid& spatial_grid = SpatialGrid::getInstance();

bool hit_ground(Entity entity, Motion& motion) {
	if (motion.sprite_offset.y > -motion.scale.y / 2.f) { // If hit the ground
		bool should_bounce = true;
		if (motion.current_speed > 50.f) {
			if (!registry.tempEffects.has(entity)) {
				bool landed_in_water = false;
				ivec2 cell_coords = spatial_grid.get_grid_cell_coords(motion.position); // Can't use motion.cell_coords because particles don't have
				std::vector<Entity> entities_found = spatial_grid.query_radius(cell_coords, 0.f);
				for (int i = 0; i < entities_found.size(); i++) {
					Entity entity_other = entities_found[i];
					Motion& motion_other = registry.motions.get(entity_other);
					if (motion_other.type_mask == POLYGON_MASK) {
						ComplexPolygon& polygon = registry.polygons.get(entity_other);
						if (!polygon.is_only_edge) {
							if (is_point_within_polygon(polygon, motion.position)) {
								if (polygon.is_platform) {
									landed_in_water = false;
									break;
								}
								else {
									landed_in_water = true;
								}
							}
						}
					}
				}
				if (landed_in_water) {
					printf("ok\n");
					should_bounce = false;
					registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::FADE_OUT, 150.f });
				} else {
					// Create dirt effect
				}
			} else {
				should_bounce = false;
			}
		}
		//registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::FADE_OUT, 1000.f });
		if (should_bounce) {
			if (motion.type_mask == PROJECTILE_MASK) {
				motion.sprite_offset.y = -motion.scale.y / 2.f - 0.01f;
				motion.sprite_offset_velocity *= -0.5f; // Bounces off of the ground
				motion.velocity *= 0.5f; // Slow down it's regular motion but only once as it hits the ground
			}
			else if (motion.type_mask == PARTICLE_MASK) {
				motion.sprite_offset_velocity = 0.f; // Maybe add bounce_factor?
				motion.friction = 0.1f; // Slow down it's regular motion so that it comes to a stop eventually
			}
		}
		if (motion.current_speed < 10.f) {
			motion.moving = false;
		}
		return true;
	}
	return false;
}

void PhysicsSystem::step(float elapsed_ms) 
{
	float step_seconds = elapsed_ms / 1000.f;
	auto& motion_registry = registry.motions;

	Room& cur_room = registry.rooms.components[0];
	float room_width = cur_room.grid_size.x * WorldSystem::TILE_SIZE;
	float room_height = cur_room.grid_size.y * WorldSystem::TILE_SIZE;
	float clamp_inset = 10.f;

	Camera& camera = registry.cameras.components[0];
	camera.view_frustum = get_bbox(camera.position, camera.frustum_size / camera.scale_factor);

	for(uint i = 0; i < motion_registry.size(); i++)
	{
		Motion& motion = motion_registry.components[i];
		Entity entity = motion_registry.entities[i];

		motion.is_culled = !is_bbox_colliding(camera.view_frustum, get_bbox(motion));

		if (motion.moving) {
			vec2 old_position = motion.position;
			if (registry.bezierCurves.has(entity)) {
				auto& curve = registry.bezierCurves.get(entity);
				float& t = curve.t;
				if (t < 1) {
					// NOTE: Will bug out of framerate low
					t += elapsed_ms / curve.curve_duration_ms;
					vec4 time = {t*t*t, t*t, t, 1};
					motion.position = time * curve.basis;

					motion.move_direction = normalize(motion.position - old_position);
					//motion.sprite_offset = curve.default_sprite_offset;
				} else if (curve.has_more_joints()) {
					curve.joint_next_point();
				} else {
					registry.bezierCurves.remove(entity);
					motion.velocity = (motion.position - old_position) / step_seconds;
				}
			}	
			else {
				motion.position += motion.velocity * step_seconds; // Update position first for better collisions
				if (motion.type_mask & BEING_MASK) {
					motion.position.x = clamp(motion.position.x, clamp_inset, room_width - clamp_inset);
					motion.position.y = clamp(motion.position.y, clamp_inset, room_height - clamp_inset);
				}
			}

			if (length(motion.move_direction) > 0.f) {
				vec2 move_dir_unit = normalize(motion.move_direction);
				vec2 desired_velocity = move_dir_unit * (motion.max_speed + 0.01f); // Must add a constant to avoid x/0 below

				vec2 to_desired_velocity = desired_velocity - motion.velocity;

				if (length(motion.velocity - desired_velocity) > 8.f) { // Necessary to prevent velocity jittering
					motion.velocity += normalize(to_desired_velocity) * (motion.accel_rate * step_seconds);
				} else {
					motion.velocity = desired_velocity;
				}
				motion.current_speed = length(motion.velocity);

				if (motion.current_speed > motion.max_speed) {
					motion.velocity = normalize(motion.velocity) * motion.max_speed; // Effectively clamp velocity
					motion.current_speed = motion.max_speed;
				}
			} else if (length(motion.velocity) > 0.f) { // decel so velocity approaches 0 
				motion.velocity *= motion.friction;
				motion.current_speed = length(motion.velocity);
				if (motion.current_speed < 1.f) {
					motion.velocity = { 0.f, 0.f };
					motion.current_speed = 0.f;
				}
			}
			if (motion.type_mask & (PROJECTILE_MASK | PARTICLE_MASK) && !hit_ground(entity, motion)) {
				// Calculate push forces:
				vec3 push_velocity = motion.forces / motion.mass * step_seconds;
				motion.velocity += vec2(push_velocity);
				motion.sprite_offset_velocity += push_velocity.z;
				// Gravity
				float gravity = 120 * motion.gravity_multiplier;
				motion.sprite_offset_velocity -= gravity * step_seconds;
				motion.sprite_offset += vec2(0, -motion.sprite_offset_velocity * step_seconds);
			}

			// Move children equally
			vec2 d_pos = motion.position - old_position;
			for (Entity child : motion.children) {
				assert(registry.motions.has(child));
				registry.motions.get(child).position += d_pos;
			}

			// If angled, update angle
			if (motion.angled) {
				motion.angle = atan2(motion.move_direction.y, motion.move_direction.x) - M_PI / 2.f;
			} else {
				motion.angle += motion.angular_velocity * step_seconds;
			}
			
			if (motion.cell_index != INT_MAX) {
				moving_check_collisions(entity, motion);
				update_motion_cells(entity, motion);
			}
		} else if (!motion.is_culled && motion.type_mask == OBSTACLE_MASK && motion.scale.y > 150.f) { // If it's a big obstacle
			obstacle_transparency(entity, motion);
		}
	}
	Entity player = registry.players.entities[0];
	Motion& motion = registry.motions.get(player);
	if (length(motion.move_direction) > 0.f) {
		registry.spriteSheets.get(player).set_track(0);
	} else {
		registry.spriteSheets.get(player).set_track(1);
	}

	update_debug();
}


void motions_push(Motion& motion1, Motion& motion2) // Make two motions push each other
{
	vec2 separation_vec = motion2.position - motion1.position;
	float separation_distance = length(separation_vec);
	float combined_radius = (motion1.radius + motion2.radius);
	float push_strength = -1 / 8.f * (separation_distance - combined_radius);

	if (separation_distance > 0.f) {
		vec2 separation_unit = normalize(separation_vec);
		if (motion2.type_mask != OBSTACLE_MASK) { // motion2 will always be entity_other and is thus what we check
			motion1.velocity += -separation_unit * push_strength * motion2.mass;
			motion2.velocity -= -separation_unit * push_strength * motion1.mass;
		}
		else { // Colliding with an obstacle
			float push_extra = (1.f - separation_distance / combined_radius) * 100.f;
			float dot_product1 = max(dot(motion1.velocity, separation_unit), 0.f);
			float p = 2.f * (dot_product1) / (3.f);
			motion1.velocity = motion1.velocity - p * (2.f * separation_unit - 1.f * separation_unit) - separation_unit * push_extra;
		}
	}
}

bool is_motions_colliding(const Motion& motion_i, const Motion& motion_j, uint32 combined_type_mask)
{
	bool is_colliding = false;
	switch (combined_type_mask & ~PLAYER_MASK) // Ignore the possible PLAYER_MASK
	{
	case (BEING_MASK | BEING_MASK): // in handle_collisons: entity = being, entity_other = character
		//if (motion_j.type_mask & PLAYER_MASK) { index1 = i; index2 = j; } // Ugly but necessary (ensure entity1 is player)
		is_colliding = is_circle_colliding(motion_i, motion_j, 10.f);
		break;
	case (BEING_MASK | OBSTACLE_MASK): // in handle_collisons: entity = being, entity_other = obstacle
		is_colliding = is_circle_colliding(motion_i, motion_j);
		break;
	case (BEING_MASK | PICKUPABLE_MASK):
		is_colliding = is_circle_colliding(motion_i, motion_j); // Maybe make this a 'special BBox collide'
		break;
	case (BEING_MASK | PROJECTILE_MASK): // in handle_collisons: entity = being, entity_other = projectile
		is_colliding = is_circle_colliding(motion_i, motion_j); // Maybe make this a 'special BBox collide'
		break;
	case (BEING_MASK | DOOR_MASK): // Must check that it is the player and not an enemy:
		if (combined_type_mask & PLAYER_MASK) { is_colliding = is_circle_colliding(motion_i, motion_j); }
		break;
	case (OBSTACLE_MASK | PROJECTILE_MASK): // in handle_collisons: entity = obstacle, entity_other = projectile
		is_colliding = is_circle_colliding(motion_i, motion_j); // Maybe make this a 'special BBox collide'
		break;
	}
	return is_colliding;
}

void PhysicsSystem::update_motion_cells(Entity entity, Motion& motion)
{
	// Adding/removing entities from spatial_grid cells:
	ivec2 old_cell_coords = motion.cell_coords;
	ivec2 new_cell_coords = spatial_grid.get_grid_cell_coords(motion.position);
	if (spatial_grid.are_cell_coords_out_of_bounds(new_cell_coords)) return; // Ignore out of bounds (won't affect collisions)

	motion.cell_coords = new_cell_coords;
	assert(motion.cell_index != INT_MAX);
	if (old_cell_coords != motion.cell_coords) {
		spatial_grid.remove_entity_from_cell(old_cell_coords, motion.cell_index, entity);
		motion.cell_index = spatial_grid.add_entity_to_cell(motion.cell_coords, entity);
	}
	motion.checked_collisions = false;
}

void PhysicsSystem::moving_check_collisions(Entity entity, Motion& motion)
{
	// TODO: Are multiple collisions being added between entities?
	std::vector<Entity> obstacles_colliding; // Must do being-obstacle collision resolution after being-being

	vec2 radius_change = { motion.radius, motion.radius };
	ivec2 top_left_min_XY = spatial_grid.get_grid_cell_coords(motion.position - radius_change);
	ivec2 bottom_right_max_XY = spatial_grid.get_grid_cell_coords(motion.position + radius_change);

	//std::vector<Entity> entities_found = spatial_grid.query_radius(motion.cell_coords, motion.radius);
	std::vector<Entity> entities_found = spatial_grid.query_rect(top_left_min_XY, bottom_right_max_XY);
	// First do dynamic-dynamic collisions
	for (int i = 0; i < entities_found.size(); i++) {
		Entity entity_other = entities_found[i];
		Motion& motion_other = registry.motions.get(entity_other);
		if (motion_other != motion) {
			assert(motion_other.type_mask != UNCOLLIDABLE_MASK);
			if (motion_other.checked_collisions) { continue; }

			uint32 combined_type_mask = motion.type_mask | motion_other.type_mask;
			if ((combined_type_mask & ~PLAYER_MASK) == (BEING_MASK | POLYGON_MASK)) {
				obstacles_colliding.push_back(entity_other);
				continue;
			}
			if (is_motions_colliding(motion, motion_other, combined_type_mask)) {
				if ((combined_type_mask & ~PLAYER_MASK) == (BEING_MASK | OBSTACLE_MASK)) {
					obstacles_colliding.push_back(entity_other);
				} else if ((combined_type_mask & ~PLAYER_MASK) == BEING_MASK) {
					if (registry.enemies.has(entity) && registry.enemies.has(entity_other)) { // WHOLE THING IS HACKY
						if (registry.enemies.get(entity).collision_immune && registry.enemies.get(entity).collision_immune) {
							// TODO: ADJUST WOLF COLLISIONS SO NOT PERFECTLY OVERLAPPING?
						}   // Do not push entity if entity has knock-back immune
						else if (!registry.enemies.has(entity_other) || !registry.enemies.get(entity_other).knock_back_immune) {
							motions_push(motion, motion_other);
						}
					} else {
						motions_push(motion, motion_other);
					}
				}
				Entity entity1 = (motion_other.type_mask > motion.type_mask) ? entity : entity_other;
				Entity entity2 = (motion_other.type_mask > motion.type_mask) ? entity_other : entity;
				registry.collisions.emplace_with_duplicates(entity1, entity2, combined_type_mask); // Must be duplicates
			}
		}
	}
	// Last do dynamic-static collisions
	vec2 polygon_push_velocity = {0.f, 0.f};
	bool is_testing_platforms = false;
	for (int i = 0; i < obstacles_colliding.size(); i++) {
		Entity entity_other = obstacles_colliding[i];
		Motion motion_other = registry.motions.get(entity_other);
		if (motion_other.type_mask == POLYGON_MASK) {
			ComplexPolygon& polygon = registry.polygons.get(entity_other);
			if (is_testing_platforms) {
				if (polygon.is_platform) {
					motion.velocity += is_circle_polygon_edge_colliding(polygon, motion);
				}
			} else {
				vec2 push_velocity = is_circle_polygon_edge_colliding(polygon, motion);

				if (polygon.is_platform) {
					if (is_point_within_polygon(polygon, motion.position)) {
						is_testing_platforms = true;
						motion.velocity -= polygon_push_velocity;
					}
				}
				polygon_push_velocity += push_velocity;
				motion.velocity += push_velocity;
			}
		} else {
			motions_push(motion, motion_other);
		}
	}
}

void PhysicsSystem::obstacle_transparency(Entity entity, Motion& motion)
{
	vec2 top_left = motion.position - vec2(motion.scale.x/2.f, motion.scale.y);
	vec2 bottom_right = motion.position + vec2(motion.scale.x/2.f, 0.f);
	ivec2 top_left_min_XY = spatial_grid.get_grid_cell_coords(top_left);
	ivec2 bottom_right_max_XY = spatial_grid.get_grid_cell_coords(bottom_right);

	std::vector<Entity> entities_found = spatial_grid.query_rect(top_left_min_XY, bottom_right_max_XY);
	float transparency_change = -0.05f;
	for (int i = 0; i < entities_found.size(); i++) {
		Entity entity_other = entities_found[i];
		Motion& motion_other = registry.motions.get(entity_other);
		if (motion_other.type_mask & (BEING_MASK)) { // motion_other != motion - No need since motion is an obstacle anyways
			assert(motion_other.type_mask != UNCOLLIDABLE_MASK);
			if (abs(motion_other.position.x - motion.position.x) < motion.scale.x/2.f + motion_other.scale.x / 2.f
				&& motion_other.position.y < motion.position.y && motion_other.position.y > top_left.y) {
				transparency_change *= -1;
				break;
			}
		}
	}
	RenderRequest& render_request = registry.renderRequests.get(entity);
	render_request.transparency = clamp(render_request.transparency + transparency_change, 0.f, 0.6f);
}





// Debugging:
WorldDebugInfo world_debug_info; // Holds world debug info like play bounds and dir_light location
DIFFUSE_ID CIRCLE_ID = DIFFUSE_ID::CIRCLE; // For better readability
DIFFUSE_ID BOX_ID = DIFFUSE_ID::BOX;
DIFFUSE_ID LINE_ID = DIFFUSE_ID::BLACK;
const vec3 COLLISION_COLOR = vec3(0.7f, 0.f, 0.f);
const vec3 PARTITION_COLOR = vec3(1.f, 0.2f, 0.f);

void create_motion_collider_debug(int motion_index)
{
	Motion motion = registry.motions.components[motion_index]; // Copy constructor is much safer in this situation, bugs otherwise
	Entity entity = registry.motions.entities[motion_index];

	BBox bbox = get_bbox(motion);
	vec2 center = motion.position + motion.sprite_offset;
	Entity circle = createColliderDebug(motion.position, vec2(motion.radius * 2.0f), CIRCLE_ID);
	Entity box;
	if (!(motion.type_mask & DOOR_MASK)) {
		box = createColliderDebug(motion.position, motion.scale, BOX_ID, COLLISION_COLOR, 0.f, motion.sprite_offset);
	}
	Entity look_line;
	if (motion.type_mask & BEING_MASK) { // Only draw look_lines for beings
		look_line = createColliderDebug(vec2(0), { 80.f, 5.f }, LINE_ID);
	}
	vec2 cell_position = ((vec2)spatial_grid.get_grid_cell_coords(motion.position) + vec2(0.5f)) * WorldSystem::TILE_SIZE;
	Entity cell = createColliderDebug(cell_position, vec2(WorldSystem::TILE_SIZE - 2.f), LINE_ID, PARTITION_COLOR + vec3(0.f, 0.1f, 0.f), 0.5f);
	registry.colliderDebugs.emplace(entity, circle, box, look_line, cell);
}

void remove_collider_debug(Entity entity)
{
	assert(debugging.in_debug_mode);
	if (registry.colliderDebugs.has(entity)) {
		ColliderDebug& collider_debug = registry.colliderDebugs.get(entity);
		registry.remove_all_components_of(collider_debug.circle);
		registry.remove_all_components_of(collider_debug.box);
		registry.remove_all_components_of(collider_debug.look_line);
		registry.remove_all_components_of(collider_debug.cell);
		registry.colliderDebugs.remove(entity);
	}
}

std::vector<Entity> raster_line_debug = {};
int frame_num = 0;
void PhysicsSystem::update_debug()
{
	if (debugging.in_debug_mode) {
		//// For debugging raycasts and other functions: Should keep but can comment out
		frame_num++;
		if (frame_num % 30 == 0) {
			// Raster line:
			for (uint i = 0; i < raster_line_debug.size(); i++) {
				registry.remove_all_components_of(raster_line_debug[i]);
			}
			raster_line_debug.clear();
			Motion player_motion = registry.motions.get(registry.players.entities[0]); // Copy constructor to avoid corruption
			vec2 other_pos = player_motion.position + player_motion.look_direction * 300.f;
			vec2 ray_vector = other_pos - player_motion.position;
			Entity ray = createColliderDebug(other_pos - ray_vector/2.f, { length(ray_vector), 3.f}, LINE_ID, vec3(0.f, 1.f, 0.f));
			registry.motions.get(ray).angle = atan2(ray_vector.y, ray_vector.x);
			raster_line_debug.push_back(ray);

			std::vector<Cell> cells = spatial_grid.raster_line(player_motion.position, other_pos);
			Entity entity1 = createColliderDebug(other_pos, { 10.f, 10.f }, LINE_ID, vec3(0.f, 1.f, 0.f));
			
			raster_line_debug.push_back(entity1);
			for (uint i = 0; i < cells.size(); i++) {
				for (uint j = 0; j < cells.size(); j++) {
					if (j != i) { assert(cells[i].coords != cells[j].coords); }
				}
				Cell& cell = cells[i];
				//printf("cell.coords  =  %d : %d\n", cell.coords.x, cell.coords.y);
				vec3 color = (i == 0) ? vec3(1.f, 0.f, 1.f) : vec3(0.f, 0.f, 1.f);
				Entity entity = createColliderDebug(((vec2)cell.coords + vec2(0.5f)) * WorldSystem::TILE_SIZE, vec2(WorldSystem::TILE_SIZE - 2.f), LINE_ID, color, 0.8f);
				raster_line_debug.push_back(entity);
			}
			// Ray casting:
			std::vector<Entity> entities_found = spatial_grid.query_ray_cast(player_motion.position, other_pos);
			for (uint i = 0; i < entities_found.size(); i++) {
				Entity entity_other = entities_found[i];
				//if (registry.motions.get(entity_other).type_mask != 3) {
				//	printf("%d \n", registry.motions.get(entity_other).type_mask);
				//}
			}
			// Check if point is within polygon:
			//printf("Is in polygon = %d \n", is_point_within_polygon(registry.polygons.components[1], other_pos));
		}
		////

		auto& motion_registry = registry.motions;
		uint size_before_adding_new = (uint)motion_registry.components.size();
		for (uint i = 0; i < size_before_adding_new; i++)
		{
			Motion& motion_i = motion_registry.components[i];
			Entity entity_i = motion_registry.entities[i];
			if (!motion_i.type_mask || motion_i.type_mask == PARTICLE_MASK || motion_i.type_mask == PROP_MASK) continue;
			if (registry.colliderDebugs.has(entity_i)) {
				if (!motion_i.moving) continue;
				ColliderDebug& collider_debug = registry.colliderDebugs.get(entity_i);
				if (motion_registry.has(collider_debug.circle)) {
					motion_registry.get(collider_debug.circle).position = motion_i.position;
					if (motion_i.type_mask & DOOR_MASK) printf("YUP\n");
				}
				BBox bbox = get_bbox(motion_i);
				if (!(motion_i.type_mask & DOOR_MASK) && motion_registry.has(collider_debug.box)) {
					motion_registry.get(collider_debug.box).position = motion_i.position;
					motion_registry.get(collider_debug.box).sprite_offset = motion_i.sprite_offset;
				}
				if ((motion_i.type_mask & BEING_MASK) && motion_registry.has(collider_debug.look_line)) {
					motion_registry.get(collider_debug.look_line).position = motion_i.position + (40.f * motion_i.look_direction);
					motion_registry.get(collider_debug.look_line).angle =
						atan2(motion_i.look_direction.y, motion_i.look_direction.x) + M_PI;
				}
				if (motion_registry.has(collider_debug.cell)) {
					motion_registry.get(collider_debug.cell).position = ((vec2)motion_i.cell_coords + vec2(0.5f)) * WorldSystem::TILE_SIZE;
				}
			} else {
				create_motion_collider_debug(i);
			}
		}
		
		WorldLighting& world_lighting = registry.worldLightings.components[0];
		DirLight& dir_light = registry.dirLights.get(world_lighting.dir_light);
		Camera& camera = registry.cameras.components[0];
		vec2 camera_position = camera.position;
		float p = 500.f;																// TODO:
		vec2 dir_light_spot_size_scaled = vec2(90.f, 90.f) * (-1/(dir_light.direction.z - 1.5f)); //* (lighting.direction.z + 2.f)/3.f;
		vec2 dir_light_spot_offset = p * vec2(dir_light.direction.x, dir_light.direction.y);
		Motion& dir_light_spot_motion = registry.motions.get(world_debug_info.sun_spot);
		vec2 center_offset = vec2(0.f, 100.f); // Moves the sun debug info downwards on the screen a bit
		dir_light_spot_motion.position = camera_position + dir_light_spot_offset + center_offset;
		dir_light_spot_motion.scale = dir_light_spot_size_scaled;
		registry.motions.get(world_debug_info.center_of_sun_spot).position = dir_light_spot_motion.position;
		registry.motions.get(world_debug_info.sun_across_line).position = camera_position + center_offset;
		registry.motions.get(world_debug_info.sun_across_line).angle = world_lighting.theta;
		registry.motions.get(world_debug_info.center_of_screen).position = camera_position + center_offset;
		Motion& frustum_motion = registry.motions.get(world_debug_info.view_frustum);
		frustum_motion.position = camera_position;
		frustum_motion.scale = camera.frustum_size / camera.scale_factor;
	}
}

void create_polygon_debug(int motion_index, ComplexPolygon& polygon, vec3 color)
{
	Motion motion = registry.motions.components[motion_index];
	Entity entity = registry.motions.entities[motion_index];
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge edge = polygon.world_edges[i];
		Entity vertex1 = createColliderDebug(edge.vertex1, vec2(10.f), LINE_ID, color);
		Entity vertex2 = createColliderDebug(edge.vertex2, vec2(10.f), LINE_ID, color);

		vec2 edge_vector = edge.vertex2 - edge.vertex1;
		vec2 edge_center = edge.vertex1 + edge_vector / 2.f;
		Entity edge_line = createColliderDebug(edge_center, { length(edge_vector), 5.f }, LINE_ID, color);
		registry.motions.get(edge_line).angle = atan2(edge_vector.y, edge_vector.x);
	}
	registry.colliderDebugs.emplace(entity, Entity(), Entity(), Entity(), Entity());
}

void create_spatial_grid_debug()
{
	Room& cur_room = registry.rooms.components[0];
	float room_width = cur_room.grid_size.x * WorldSystem::TILE_SIZE;
	float room_height = cur_room.grid_size.y * WorldSystem::TILE_SIZE;
	float lT = 2.f;
	for (int X = 1; X < (int)room_width / WorldSystem::TILE_SIZE; X++) {
		createColliderDebug({X * WorldSystem::TILE_SIZE, room_height / 2}, { lT, room_height }, LINE_ID, PARTITION_COLOR);
	}
	for (int Y = 1; Y < (int)room_height / WorldSystem::TILE_SIZE; Y++) {  // Not implemented yet
		createColliderDebug({ room_width / 2, Y * WorldSystem::TILE_SIZE}, { room_width, lT }, LINE_ID, PARTITION_COLOR);
	}
}

void toggle_debug()
{	
	debugging.in_debug_mode = !debugging.in_debug_mode;
	if (debugging.in_debug_mode) {

		/// Debugging raster polygon:
		for (uint i = 0; i < registry.polygons.components.size(); i++) {
			ComplexPolygon& polygon = registry.polygons.components[i];
			vec3 color = (polygon.is_platform) ? vec3(1.f, 0.f, 1.f) : vec3(0.f, 1.f, 0.f);

			std::vector<Cell> cells = spatial_grid.raster_polygon(polygon, 100.f, i == 0);
			for (uint i = 0; i < cells.size(); i++) {
				vec2 cell_position = ((vec2)cells[i].coords + vec2(0.5f)) * WorldSystem::TILE_SIZE;
				createColliderDebug(cell_position, vec2(WorldSystem::TILE_SIZE - 2.f), LINE_ID, color, 0.4f);
			}
		}
		///

		Room& cur_room = registry.rooms.components[0];
		float room_width = cur_room.grid_size.x * WorldSystem::TILE_SIZE;
		float room_height = cur_room.grid_size.y * WorldSystem::TILE_SIZE;

		// World debug info creation:
		float lT = 20.f; // lineThickness
		world_debug_info.sun_spot = createColliderDebug(vec2(0.f), { 90.f, 90.f }, CIRCLE_ID);
		world_debug_info.center_of_sun_spot = createColliderDebug(vec2(0.f), { 15.f, 15.f }, LINE_ID);
		world_debug_info.sun_across_line = createColliderDebug(vec2(0.f), { 1000.f, 5.f }, LINE_ID);
		world_debug_info.center_of_screen = createColliderDebug(vec2(0.f), { 10.f, 10.f }, LINE_ID);
		world_debug_info.view_frustum = createColliderDebug(vec2(0.f), vec2(0.f), BOX_ID);

		auto& motion_registry = registry.motions;
		uint size_before_adding_new = (uint)motion_registry.components.size();
		for (uint i = 0; i < size_before_adding_new; i++) {
			Motion& motion_i = motion_registry.components[i];
			Entity entity_i = motion_registry.entities[i];
			// don't draw debugging visuals around non collidables
			if (!motion_i.type_mask)
				continue;
			if (motion_i.type_mask == POLYGON_MASK) {
				create_polygon_debug(i, registry.polygons.get(entity_i), vec3(0.f, 1.f, 0.f));
			} else if (motion_i.type_mask != PARTICLE_MASK && motion_i.type_mask != PROP_MASK) {
				create_motion_collider_debug(i);
			}
		}
		create_spatial_grid_debug();
		printf("Turned on debug mode\n");
	} else {
		// Remove all debug info
		while (registry.debugComponents.entities.size() > 0) {
			registry.remove_all_components_of(registry.debugComponents.entities.back());
			registry.colliderDebugs.clear();
		}	
		registry.list_all_components();
		printf("Turned off debug mode\n");
	}
}