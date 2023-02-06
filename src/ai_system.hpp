#pragma once

#include <vector>
#include <math.h>

#include "tiny_ecs_registry.hpp"
#include "common.hpp"
#include "world_init.hpp" // This includes spatial_grid already so don't need to include again?

class AISystem
{
public:
	void step(float elapsed_ms);
};