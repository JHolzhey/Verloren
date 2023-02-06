#pragma once

// internal
#include "common.hpp"
#include "tiny_ecs_registry.hpp"
#include "world_system.hpp"
#include "render_system.hpp"
#include "spatial_grid.hpp"

class LightingSystem
{
public:
	LightingSystem();
	void init();
	void step(float elapsed_ms_since_last_update);

private:
	void update_dir_light_position(float elapsed_ms);
};
