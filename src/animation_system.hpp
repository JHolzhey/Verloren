#pragma once

#include "tiny_ecs_registry.hpp"
#include "common.hpp"

class AnimationSystem
{
public:
	void step(float elapsed_ms);
};
