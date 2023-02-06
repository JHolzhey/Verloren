#pragma once

#include "common.hpp"
#include "tiny_ecs.hpp"
#include "components.hpp"
#include "tiny_ecs_registry.hpp"
#include "spatial_grid.hpp"

void remove_collider_debug(Entity entity);

struct WorldDebugInfo {
	Entity room_edge_top;
	Entity room_edge_bottom;
	Entity room_edge_left;
	Entity room_edge_right;
	Entity sun_spot;
	Entity center_of_sun_spot;
	Entity sun_across_line;
	Entity center_of_screen;
	Entity view_frustum;
};

// A simple physics system that moves rigid bodies and checks for collision
class PhysicsSystem
{
public:
	void step(float elapsed_ms);

	void update_motion_cells(Entity entity, Motion& motion);

	void moving_check_collisions(Entity entity, Motion& motion);
	void obstacle_transparency(Entity entity, Motion& motion);

	void update_debug();

	PhysicsSystem() {}
};

void toggle_debug();
