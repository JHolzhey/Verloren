#pragma once

#include "tiny_ecs_registry.hpp"
#include "common.hpp"
#define SDL_MAIN_HANDLED

class SpawnerSystem
{
public:
	// Countdown until enemy waves start spawning
	static float initial_wave_timer;
	void step(float elapsed_ms);
};
