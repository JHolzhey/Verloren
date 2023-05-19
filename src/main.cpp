
#define GL3W_IMPLEMENTATION
#include <gl3w.h>

// stlib
#include <chrono>

// internal
#include "ai_system.hpp"
#include "physics_system.hpp"
#include "render_system.hpp"
#include "world_system.hpp"
#include "ui_system.hpp"
#include "animation_system.hpp"
#include "spawner_system.hpp"
#include "lighting_system.hpp"
#include "world_init.hpp"
#include "upgrades.hpp"
#include "camera_system.hpp"
#include "particle_system.hpp"
#include "common.hpp"

using Clock = std::chrono::high_resolution_clock;

// Entry point
int main()
{
	// WorldSystem's constructor relies on values initialized here
	Upgrades::init();

	// Global systems
	WorldSystem world;
	RenderSystem renderer;
	PhysicsSystem physics;
	AISystem ai;
	AnimationSystem animations;
	SpawnerSystem spawners;
	UISystem ui;
	CameraSystem camera;
	LightingSystem lighting;
	ParticleSystem particles;

	// Initializing window
	GLFWwindow* window = world.create_window();
	if (!window) {
		// Time to read the error message
		printf("Press any key to exit");
		getchar();
		return EXIT_FAILURE;
	}

	// initialize the main systems
	renderer.init(window);
	world.init(&renderer, &ui); // ui init in here, TODO: put this into ui_system

	// variable timestep loop
	int frame_num = 0;
	double physics_elapsed = 0; double ai_elapsed = 0; double cpu_elapsed = 0;
	double total_elapsed = 0; double specific_elapsed = 0; double render_elapsed = 0; double lighting_elapsed = 0;
	auto previous_t = Clock::now();
	auto t = Clock::now();
	while (!world.is_over()) {
		/*while (1) {
			auto current_t = Clock::now();
			float elapsed_ms = (float)(std::chrono::duration_cast<std::chrono::microseconds>(current_t - previous_t)).count() / 1000;
			if (elapsed_ms > 33.3333f) {
				previous_t = current_t;
				break;
			}
		}*/
		// Processes system messages, if this wasn't present the window would become unresponsive
		glfwPollEvents();

		// Calculating elapsed times in milliseconds from the previous iteration
		auto now = Clock::now();
		float elapsed_ms = (float)(std::chrono::duration_cast<std::chrono::microseconds>(now - t)).count() / 1000;
		if (elapsed_ms > 20.f) { printf("Lag\n"); }
		elapsed_ms = min(elapsed_ms, 20.f);
		t = now;
		total_elapsed += elapsed_ms;
		
		double specific_frame = 0; double render_frame = 0; double lighting_frame = 0;

		switch (world.get_game_state())
		{
		case GameState::MAIN_MENU:
			// We don't have a main menu, so just move straight into the game
			world.set_game_state(GameState::IN_GAME, true);
			break;
		case GameState::IN_GAME:
			if (!world.is_paused) {
					auto cpu_start = Clock::now();

					auto ai_start = Clock::now();
				ai.step(elapsed_ms);
					ai_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - ai_start)).count() / 1000;

				world.step(elapsed_ms);
				
					auto physics_start = Clock::now();
				physics.step(elapsed_ms);
					physics_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - physics_start)).count() / 1000;
				
				camera.step(elapsed_ms);
				
				// lighting.step() used to be here

					//auto specific_start = Clock::now();
				particles.step(elapsed_ms);
				spawners.step(elapsed_ms);
				world.handle_collisions();
					auto specific_start = Clock::now();
				animations.step(elapsed_ms);
					specific_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - specific_start)).count() / 1000;

					auto lighting_start = Clock::now();
				lighting.step(elapsed_ms); // Do lighting after collisions because projectiles with point lights may be deleted by it
					lighting_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - lighting_start)).count() / 1000;


					//specific_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - specific_start)).count() / 1000;
					cpu_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - cpu_start)).count() / 1000;
			}
			break;
		case GameState::SHOP:
			break;
		case GameState::GAME_FROZEN:
			world.step(elapsed_ms);
			animations.step(elapsed_ms);
			break;
		}

		ui.step(elapsed_ms, &world);
		world.update_window();
			auto render_start = Clock::now();
		renderer.draw(world.get_game_state());
			render_frame = (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - render_start)).count() / 1000;
			render_elapsed += render_frame;

		//if (specific_frame > 16.8 || elapsed_ms > 16.8) {
		//	printf("Specific Elapsed Over:		%fms\n", specific_frame);
		//	printf("Total Elapsed Over:		%fms\n", elapsed_ms);
		//}

		if (frame_num % 200 == 0) {
			printf("CPU Elapsed Avg:	%fms\n", cpu_elapsed / 200.0);
			printf("Physics Elapsed Avg:	%fms\n", physics_elapsed / 200.0);
			printf("AI Elapsed Avg:		%fms\n", ai_elapsed / 200.0);
			printf("Lighting Elapsed Avg:	%fms\n", lighting_elapsed / 200.0);
			printf("Specific Elapsed Avg:	%fms\n", specific_elapsed / 200.0);
			printf("Render Elapsed Avg:	%fms\n", render_elapsed / 200.0);
			printf("Total Elapsed Avg:	%fms\n\n", total_elapsed / 200.0);
			total_elapsed = 0; specific_elapsed = 0; render_elapsed = 0; lighting_elapsed = 0; physics_elapsed = 0; ai_elapsed = 0; cpu_elapsed = 0;
		}
		frame_num++;
	}

	return EXIT_SUCCESS;
}
