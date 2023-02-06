#pragma once

// internal
#include "common.hpp"
#include "tiny_ecs_registry.hpp"
#include "world_init.hpp"

class ParticleSystem
{
public:
	ParticleSystem();
	void init();
	void step(float elapsed_ms_since_last_update);

private:
	
};
