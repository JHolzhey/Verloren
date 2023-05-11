// Header
#include "lighting_system.hpp"
#include "world_system.hpp"

LightingSystem::LightingSystem() {}

void LightingSystem::init() {} // Leave as might be needed in future

// Possibly move these functions elsewhere
template <typename T>
vec3 lerp(T val1, T val2, float fraction) {
	return (val2 - val1) * fraction + val1;
}

template <typename T>
void update_lerp_sequence(LerpSequence<T>& lerp_sequence, float elapsed_ms)
{
	if (!lerp_sequence.enabled) return;
	assert(0.f <= lerp_sequence.elapsed_time_frac && lerp_sequence.elapsed_time_frac <= 1.f && "time_frac out of bounds");
	float& elapsed_time_frac = lerp_sequence.elapsed_time_frac;
	elapsed_time_frac += elapsed_ms / lerp_sequence.cycle_ms;
	//KeyFrame<T>& prev_key_frame = lerp_sequence.key_frames[lerp_sequence.key_index];
	KeyFrame<T>& next_key_frame = lerp_sequence.key_frames[lerp_sequence.key_index + 1];
	if (elapsed_time_frac >= next_key_frame.time_frac) {
		lerp_sequence.key_index = (lerp_sequence.key_index + 1);
		if (lerp_sequence.key_index >= lerp_sequence.key_frames.size() - 1) {
			if (lerp_sequence.loops) {
				lerp_sequence.key_index = 0;
				elapsed_time_frac -= next_key_frame.time_frac;
				//printf("Looping. Prev time_frac = %f\n", next_key_frame.time_frac);
			}
			else {
				lerp_sequence.enabled = false;
			}
		}
	}
}

template <typename T>
void set_lerp_sequence_time_frac(LerpSequence<T>& lerp_sequence, float time_frac)
{
	assert(0.f <= time_frac && time_frac <= 1.f && "time_frac out of bounds");
	lerp_sequence.elapsed_time_frac = time_frac;
	for (uint i = 1; i < lerp_sequence.key_frames.size(); i++) {
		if (time_frac >= lerp_sequence.key_frames[i-1].time_frac && time_frac < lerp_sequence.key_frames[i].time_frac) {
			lerp_sequence.key_index = i-1;
			break;
		}
	}
}

template <typename T>
T get_lerp_sequence_value(LerpSequence<T>& lerp_sequence)
{
	assert(0.f <= lerp_sequence.elapsed_time_frac && lerp_sequence.elapsed_time_frac <= 1.f && "time_frac out of bounds");
	KeyFrame<vec3>& prev_key_frame = lerp_sequence.key_frames[lerp_sequence.key_index];
	KeyFrame<vec3>& next_key_frame = lerp_sequence.key_frames[lerp_sequence.key_index + 1];
	float prev_time_frac = prev_key_frame.time_frac;
	float index_time_frac = (lerp_sequence.elapsed_time_frac - prev_time_frac) / (next_key_frame.time_frac - prev_time_frac);
	// printf("prev_key_frame.value:  %f : %f : %f\n", prev_key_frame.value.x, prev_key_frame.value.y, prev_key_frame.value.z);
	// printf("prev_key_frame.value:  %f : %f : %f\n", next_key_frame.value.x, next_key_frame.value.y, next_key_frame.value.z);
	return lerp(prev_key_frame.value, next_key_frame.value, index_time_frac);
}

void update_dir_light_color()
{
	WorldLighting& world_lighting = registry.worldLightings.components[0];
	DirLight& dir_light = registry.dirLights.get(world_lighting.dir_light);
	LerpSequence<vec3>& lerp_vec3 = registry.lerpVec3s.get(world_lighting.dir_light);
	if (world_lighting.is_time_changing) {
		set_lerp_sequence_time_frac(lerp_vec3, world_lighting.time_of_day / 24.f);
		dir_light.diffuse = get_lerp_sequence_value(lerp_vec3);
	} else {
		dir_light.diffuse = vec3(1.f);
	}
	
	//printf("dirlight diffuse:  %f : %f : %f\n", dir_light.diffuse.x, dir_light.diffuse.y, dir_light.diffuse.z);
	dir_light.ambient = dir_light.diffuse * vec3(clamp(dir_light.strength, 0.1f, 0.3f)); // Needs tweaking
	dir_light.diffuse *= dir_light.strength;
	dir_light.specular = vec3(dir_light.strength);
	//printf("Strength = %f\n", dir_light.strength); // Leave here, needed for tweaking colors
	//printf("dirlight diffuse:  %f : %f : %f\n", dir_light.diffuse.x, dir_light.diffuse.y, dir_light.diffuse.z);
}

void LightingSystem::step(float elapsed_ms) {
	auto& lerp_vec3s_registry = registry.lerpVec3s; // TODO: Move somewhere else
	for (int i = 0; i < lerp_vec3s_registry.components.size(); i++) {
		Entity entity = lerp_vec3s_registry.entities[i];
		LerpSequence<vec3>& lerp_vec3 = lerp_vec3s_registry.components[i];
		update_lerp_sequence(lerp_vec3, elapsed_ms);
	}
	
	update_dir_light_position(elapsed_ms);
	update_dir_light_color();

	for (int i = 0; i < registry.pointLights.components.size(); i++) {
		PointLight& point_light = registry.pointLights.components[i];
		Entity entity = registry.pointLights.entities[i];
	}

	WorldLighting& world_lighting = registry.worldLightings.components[0];
	int num_lights = registry.pointLights.components.size();
	//printf("Lighting num_lights: %d\n", num_lights);
	assert(world_lighting.num_important_point_lights >= 0 && world_lighting.num_important_point_lights <= num_lights);

	SpatialGrid& spatial_grid = SpatialGrid::getInstance();
	for (int i = 0; i < world_lighting.num_important_point_lights; i++) {
		PointLight& point_light = registry.pointLights.components[i];
		Entity entity = registry.pointLights.entities[i];
		ivec2 cell_coords = spatial_grid.get_grid_cell_coords(point_light.position);

		//std::unordered_map<unsigned int, bool> map_ground_got; // So that we don't have duplicate lights_affecting for ground pieces
		//map_ground_got.reserve(10);

		std::vector<Entity> entities_found = spatial_grid.query_radius(cell_coords, point_light.max_radius/2.f);
		for (int j = 0; j < entities_found.size(); j++) {
			Entity entity_other = entities_found[j];
			Motion& motion = registry.motions.get(entity_other);
			if (motion.type_mask == 128) continue;
			RenderRequest& render_request = registry.renderRequests.get(entity_other);

			assert(render_request.num_lights_affecting < 16); // 16 being MAX_POINT_LIGHTS
			render_request.lights_affecting[render_request.num_lights_affecting] = i;
			render_request.num_lights_affecting += 1; // TODO

			//if (render_request.is_ground_piece && map_ground_got.count(entity_other) <= 0) { // If the entity doesn't exist in the map already
			//	render_request.lights_affecting[render_request.num_lights_affecting] = i;
			//	render_request.num_lights_affecting += 1; // TODO
			//	//printf("polygon = %d\n", render_request.num_lights_affecting);
			//	map_ground_got[entity_other] = true;
			//} else {
			//	//printf("regular = %d\n", render_request.num_lights_affecting);
			//	render_request.lights_affecting[render_request.num_lights_affecting] = i;
			//	render_request.num_lights_affecting += 1; // TODO
			//}
		}
	}
}

float debug_stop_time_ms = 10000.f;
float debugging_timer_ms = 0.f;
void LightingSystem::update_dir_light_position(float elapsed_ms)
{
	WorldLighting& world_lighting = registry.worldLightings.components[0];
	DirLight& dir_light = registry.dirLights.get(world_lighting.dir_light);
	if (world_lighting.is_new) {
		world_lighting.time_of_day = time_of_day_save;
		world_lighting.is_new = false;
	}

	if (world_lighting.theta_change || world_lighting.phi_change) { // Only for debugging
		debugging_timer_ms = debug_stop_time_ms;
		world_lighting.theta += world_lighting.theta_change;
		world_lighting.phi += world_lighting.phi_change;
		world_lighting.time_of_day = fmodf(24.f*(world_lighting.theta + world_lighting.theta_offset) / (2 * M_PI), 24.f);
		if (world_lighting.time_of_day < 0.f) world_lighting.time_of_day = 24.f + world_lighting.time_of_day;
	}
	if (debugging_timer_ms < 0.f) {
		if (world_lighting.is_time_changing) {
			world_lighting.time_of_day = fmodf((world_lighting.time_of_day + 24.f * (elapsed_ms / world_lighting.day_cycle_ms)), 24.f);
			time_of_day_save = world_lighting.time_of_day;
			float elevation_offset = 0.5f;
			float latitude_factor = (M_PI - 2.f * abs(world_lighting.latitude)) / M_PI;
			float dir_light_elevation = (cos(M_PI * (world_lighting.time_of_day / 6.f)) / 2.f + elevation_offset) * latitude_factor;
			world_lighting.phi = (1.f - dir_light_elevation) * (M_PI / 2.f);
			world_lighting.theta = (2.f * M_PI * (world_lighting.time_of_day)) / 24.f - world_lighting.theta_offset;
		}
	} else {
		debugging_timer_ms -= elapsed_ms;
	}
	world_lighting.is_day = (world_lighting.phi <= M_PI/2.f && world_lighting.time_of_day >= 6.f && world_lighting.time_of_day <= 18.f);
	
	dir_light.strength = max(cos(world_lighting.phi), 0.f); // Maybe add/subtract constant so it's not so dark at sunset/sunrise
	float x = sin(world_lighting.phi) * cos(world_lighting.theta); // Converting spherical coords to cartesian
	float y = sin(world_lighting.phi) * sin(world_lighting.theta);
	float z = cos(world_lighting.phi);
	dir_light.direction = { x, y, z };
	//printf("%f : %f : %f\n", dir_light.direction.x, dir_light.direction.y, dir_light.direction.z);
}