// Header
#include "world_system.hpp"
#include "world_init.hpp"
#include "ui_system.hpp"
#include "physics_system.hpp"
#include "components.hpp"


#include "upgrades.hpp"
#include "spawner_system.hpp"

#include "../ext/rapidjson/document.h"
#include "../ext/rapidjson/filewritestream.h"
#include "../ext/rapidjson/filereadstream.h"
#include "../ext/rapidjson/writer.h"


// stlib
#include <cassert>
#include <sstream>
#include <iostream>
#include <chrono>
#include <algorithm>



// Game configuration
const size_t INVULN_TIME_MS = 250;
const float WorldSystem::TILE_SIZE = 100.f;

const float ENEMY_DAMAGED_EFFECT_MS = 250;

// Cooldowns for attacks
float hansel_m1_remaining_cd = 0,
gretel_m1_remaining_cd = 0,
hansel_m2_remaining_cd = 0,
gretel_m2_remaining_cd = 0;

using Clock = std::chrono::high_resolution_clock;

// keeps the total duration that a screen transition should last
float state_transition_duration = -1;
// tracks how long the current screen transition has lasted
float state_transition_timer = -1;
bool transition_to_black = false;
bool reset_after_transition = true;
bool load_game_after_transition = false;

// Tracks mouse clicks (for attacks)
bool m1_down = false;
bool m2_down = false;


const int TUTORIAL_ROOMS_START_IND = 1;

// Create the world
WorldSystem::WorldSystem() : player_entity(Entity()) {
	// Seeding rng with random device
	rng = std::default_random_engine(std::random_device()());

	// Initializing all rooms - Order of rooms is order of levels
	cur_room_ind = 0; // 17 is boss room
	room_json_paths = {
		// Game Menu
		"menu_room.json",

		// Tutorial Levels
	   "movement_tutorial.json",
	   "attack_tutorial.json",
	   "gretel_special_tutorial.json",
	   "switch_character_tutorial.json",
	   "hansel_special_tutorial.json",
	   "clear_room_tutorial.json",

		"wide_room.json",
		"squirrel_room.json",
		"square_room.json",

		// rooms with new enemy
		"bear_room.json",   // 10
		"boar_room.json",
		"deer_room.json",
		"rabbit_room.json", 
		"wolf_room.json",
		"fox_room.json",
		"mixed_room.json",
		"boss.json" // 17
	};

	CharacterState default_gretel = CharacterState{
		Upgrades::get_gretel_max_hp(),
		Upgrades::get_gretel_max_hp(),
		PLAYER_CHARACTER::GRETEL
	};
	CharacterState default_hansel = CharacterState{
		Upgrades::get_hansel_max_hp(),
		Upgrades::get_hansel_max_hp(),
		PLAYER_CHARACTER::HANSEL
	};

	this->characters_checkpoint[0] = default_gretel;
	this->characters_checkpoint[1] = default_hansel;
}

WorldSystem::~WorldSystem() {
	// Destroy music components
	if (piano_music != nullptr)
		Mix_FreeMusic(piano_music);
	if (shop_music != nullptr)
		Mix_FreeMusic(shop_music);
	if (menu_music != nullptr)
		Mix_FreeMusic(menu_music);
	if (chicken_dead_sound != nullptr)
		Mix_FreeChunk(chicken_dead_sound);
	if (chicken_eat_sound != nullptr)
		Mix_FreeChunk(chicken_eat_sound);

	// M4
	if (hansel_hurt_sound != nullptr)
		Mix_FreeChunk(hansel_hurt_sound);
	if (gretel_hurt_sound != nullptr)
		Mix_FreeChunk(gretel_hurt_sound);
	if (rat_sound != nullptr)
		Mix_FreeChunk(rat_sound);
	if (squirrel_sound != nullptr)
		Mix_FreeChunk(squirrel_sound);
	if (bear_sound != nullptr)
		Mix_FreeChunk(bear_sound);
	if (boar_sound != nullptr)
		Mix_FreeChunk(boar_sound);
	if (player_melee_sound != nullptr)
		Mix_FreeChunk(player_melee_sound);
	if (player_throw_sound != nullptr)
		Mix_FreeChunk(player_throw_sound);
	if (food_pickup != nullptr)
		Mix_FreeChunk(food_pickup);
	if (upgrade_pickup != nullptr)
		Mix_FreeChunk(upgrade_pickup);
	if (shiny_pickup != nullptr)
		Mix_FreeChunk(shiny_pickup);
	if (portal_open != nullptr)
		Mix_FreeChunk(portal_open);
	if (deer_sound != nullptr)
		Mix_FreeChunk(deer_sound);
	if (fox_sound != nullptr)
		Mix_FreeChunk(fox_sound);
	if (rabbit_sound != nullptr)
		Mix_FreeChunk(rabbit_sound);
	if (wolf_sound != nullptr)
		Mix_FreeChunk(wolf_sound);
	if (chest_open != nullptr)
		Mix_FreeChunk(chest_open);
	if (alfa_wolf_sound != nullptr)
		Mix_FreeChunk(alfa_wolf_sound);
	if (cash_register_sound != nullptr)
		Mix_FreeChunk(cash_register_sound);
	if (crow_sound != nullptr)
		Mix_FreeChunk(crow_sound);
	Mix_CloseAudio();

	// Destroy all created components
	registry.clear_all_components();

	// Close the window
	glfwDestroyWindow(window);
}

// Debugging
namespace {
	void glfw_err_cb(int error, const char* desc) {
		fprintf(stderr, "%d: %s", error, desc);
	}
}

// World initialization
// Note, this has a lot of OpenGL specific things, could be moved to the renderer
GLFWwindow* WorldSystem::create_window() {
	///////////////////////////////////////
	// Initialize GLFW
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW");
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// If you are on Linux or Windows, you can change these 2 numbers to 4 and 3 and
	// enable the glDebugMessageCallback to have OpenGL catch your mistakes for you.
	// GLFW / OGL Initialization
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#if __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, 0);

	// New and better
	// Create the main window (for rendering, keyboard, and mouse input)
	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	float aspect_ratio = window_width_px / (float) window_height_px; // 16:9
	float new_width = 0.7 * mode->width;
	float new_height = new_width * (1/aspect_ratio);

	window = glfwCreateWindow(new_width, new_height, "Verloren: A Grimm Tale", nullptr, nullptr);

	if (window == nullptr) {
		fprintf(stderr, "Failed to glfwCreateWindow");
		return nullptr;
	}

	Entity entity = Entity();
	registry.screenStates.emplace(entity);
	ScreenState& screen = registry.screenStates.components[0];
	screen.window_height_px = new_height;
	screen.window_width_px = new_width;


	// Setting callbacks to member functions (that's why the redirect is needed)
	// Input is handled using GLFW, for more info see
	// http://www.glfw.org/docs/latest/input_guide.html
	glfwSetWindowUserPointer(window, this);
	auto key_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2, int _3) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_key(_0, _1, _2, _3); };
	auto cursor_pos_redirect = [](GLFWwindow* wnd, double _0, double _1) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_move({ _0, _1 }); };
	auto mouse_click_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_click(_0, _1, _2); };
	glfwSetKeyCallback(window, key_redirect);
	glfwSetCursorPosCallback(window, cursor_pos_redirect);
	glfwSetMouseButtonCallback(window, mouse_click_redirect);
	//////////////////////////////////////
	// Loading music and sounds with SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL Audio");
		return nullptr;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
		fprintf(stderr, "Failed to open audio device");
		return nullptr;
	}

	piano_music = Mix_LoadMUS(audio_path("music.wav").c_str());
	tutorial_music = Mix_LoadMUS(audio_path("on_the_farm.wav").c_str());
	shop_music = Mix_LoadMUS(audio_path("owls.wav").c_str());
	menu_music = Mix_LoadMUS(audio_path("serene.wav").c_str());
	chicken_dead_sound = Mix_LoadWAV(audio_path("chicken_dead.wav").c_str());
	chicken_eat_sound = Mix_LoadWAV(audio_path("chicken_eat.wav").c_str());
	// M4 new sounds
	hansel_hurt_sound = Mix_LoadWAV(audio_path("Hansel_hurt.wav").c_str());
	gretel_hurt_sound = Mix_LoadWAV(audio_path("Gretal_hurt.wav").c_str());
	squirrel_sound = Mix_LoadWAV(audio_path("squirrel.wav").c_str());
	rat_sound = Mix_LoadWAV(audio_path("rat_hurt.wav").c_str());
	bear_sound = Mix_LoadWAV(audio_path("bear.wav").c_str());
	boar_sound = Mix_LoadWAV(audio_path("boar.wav").c_str());
	player_melee_sound = Mix_LoadWAV(audio_path("player_melee.wav").c_str());
	player_throw_sound = Mix_LoadWAV(audio_path("player_throw.wav").c_str());
	food_pickup = Mix_LoadWAV(audio_path("food_pickup.wav").c_str());
	upgrade_pickup = Mix_LoadWAV(audio_path("upgrade_pickup.wav").c_str());
	shiny_pickup = Mix_LoadWAV(audio_path("shiny_pickup.wav").c_str());
	portal_open = Mix_LoadWAV(audio_path("portal_open.wav").c_str());
	chest_open = Mix_LoadWAV(audio_path("chest_open.wav").c_str());
	fox_sound = Mix_LoadWAV(audio_path("fox.wav").c_str());
	deer_sound = Mix_LoadWAV(audio_path("deer.wav").c_str());
	rabbit_sound = Mix_LoadWAV(audio_path("rabbit.wav").c_str());
	wolf_sound = Mix_LoadWAV(audio_path("wolf.wav").c_str());
	alfa_wolf_sound = Mix_LoadWAV(audio_path("alfa_wolf.wav").c_str());
	cash_register_sound = Mix_LoadWAV(audio_path("cash register.wav").c_str());
	crow_sound = Mix_LoadWAV(audio_path("crow.wav").c_str());

	if (piano_music == nullptr || chicken_dead_sound == nullptr || chicken_eat_sound == nullptr) {
		fprintf(stderr, "Failed to load sounds\n %s\n %s\n %s\n make sure the data directory is present",
			audio_path("music.wav").c_str(),
			audio_path("chicken_dead.wav").c_str(),
			audio_path("chicken_eat.wav").c_str());
		return nullptr;
	}

	return window;
}

void WorldSystem::init(RenderSystem* renderer_arg, UISystem* ui_system) {
	this->renderer = renderer_arg;
	this->ui = ui_system;

	restart_game();
}

GameState WorldSystem::get_game_state() { return game_state; }
void WorldSystem::set_game_state(GameState new_state, bool do_restart=true)
{
	GameState old_state = game_state;
	game_state = new_state;
	if (new_state == old_state) return;

	//playMusic(); // Change music to correspond with the current game state
	if (new_state == GameState::GAME_FROZEN) return;

	switch (new_state)
	{
	case GameState::IN_GAME:
		characters_checkpoint[0] = current_character;
		characters_checkpoint[1] = other_character;

		if (do_restart) {
			registry.clear_all_non_essential_components();
			SpatialGrid::getInstance().clear_all_cells();
			restart_game();
			if(cur_room_ind == 0) return;
			save_game("save_data.json");
		}
		break;
	case GameState::SHOP:
		registry.clear_all_non_essential_components();
		SpatialGrid::getInstance().clear_all_cells();
		
		// Attempt to go to shop screen.
		// If this fails, go to next level
		if (!ui->createShopScreen(this)) {
			puts("shop failed");
			return set_game_state(GameState::IN_GAME);
		}
		break;
	case GameState::GAME_FROZEN:
		break;
	default:
		assert(false); // unsupported game state
	}
}

void WorldSystem::update_window() {
	// Updating window title with points
	std::stringstream title_ss;
	title_ss << "Shinies: " << num_shinies;
	glfwSetWindowTitle(window, title_ss.str().c_str());
}

// New function subject to major change. Necessary in order to cleanup things associated to this entity and create effects on death
void WorldSystem::remove_entity(Entity entity) { // Maybe add a death effect enum?
	if (registry.motions.has(entity)) {
		Motion& motion = registry.motions.get(entity);
		if (motion.cell_index != INT_MAX) {
			// Testing without: motion.type_mask != UNCOLLIDABLE_MASK && motion.type_mask != MELEE_ATTACK_MASK && motion.type_mask != POLYGON_MASK && 
			SpatialGrid::getInstance().remove_entity_from_cell(motion.cell_coords, motion.cell_index, entity);
			motion.cell_index = INT_MAX;
		}
		motion.type_mask = UNCOLLIDABLE_MASK;
		motion.max_speed = 0.f;
		if (debugging.in_debug_mode) { remove_collider_debug(entity); }

		for (int i = 0; i < motion.children.size(); i++) {
			remove_entity(motion.children[i]);
		}
	}
	if (registry.enemies.has(entity)) { // First call to this function removes Enemy components from enemies and add to tempEffects
		registry.enemies.remove(entity); // Removing this here means we go to the else case below on the next call
		registry.rangedEnemies.remove(entity);
		registry.spriteSheets.remove(entity);
		registry.tempEffects.insert(entity, { TEMP_EFFECT_TYPE::BEING_DEATH, 700.f });
		Motion& motion = registry.motions.get(entity);
		RenderRequest& render_request = registry.renderRequests.get(entity);
		render_request.casts_shadow = false;
		//render_request.transparency = 0.5f; // Speed up the transparency process (otherwise takes too long)
		//render_request.add_color = vec3(0.f);
	} else { // Second call to this function (after tempEffect timer runs out) fully removes the enemy
		if (registry.pointLights.has(entity)) {
			registry.worldLightings.components[0].num_important_point_lights -= 1;
		}
		registry.remove_all_components_of(entity);
	}
}

void WorldSystem::end_transition() {
	set_game_state(next_game_state, reset_after_transition);
	if (load_game_after_transition) {
		load_game("save_data.json");
		load_game_after_transition = false;
	}

	// Reset all game freeze-related values
	state_transition_timer = -1;
	state_transition_duration = -1;
	transition_to_black = false;
	reset_after_transition = false;
	next_game_state = GameState::GAME_STATE_COUNT;

	assert(registry.screenStates.components.size() <= 1);
	ScreenState& screen = registry.screenStates.components[0];
	screen.darken_screen_factor = 0;
}

void WorldSystem::freeze_and_transition(float transition_duration, GameState next_state, bool fade_to_black=true, bool reset_after=true)
{
	state_transition_duration = transition_duration;
	state_transition_timer = 0;
	transition_to_black = fade_to_black;
	reset_after_transition = reset_after;

	set_game_state(GameState::GAME_FROZEN);
	next_game_state = next_state;
}

// Update our game world
bool WorldSystem::step(float elapsed_ms_since_last_update) {

	if (game_state == GameState::GAME_FROZEN && next_game_state != GameState::GAME_STATE_COUNT) {
		state_transition_timer += elapsed_ms_since_last_update;

		if (transition_to_black) {
			assert(registry.screenStates.components.size() <= 1);
			ScreenState& screen = registry.screenStates.components[0];
			screen.darken_screen_factor = state_transition_timer / state_transition_duration;

		}

		if (state_transition_timer >= state_transition_duration) {
			end_transition();
			return true;
		}
	}

	// All for debugging lights:
	if (registry.pointLights.has(player_entity)) {
		Player& player = registry.players.get(player_entity);
		PointLight& point_light = registry.pointLights.get(player_entity);

		registry.remove_all_components_of(player.line1);
		registry.remove_all_components_of(player.line2);

		Entity best_entity = player_entity;
		float closest_distance = 100000.f;
		SpatialGrid& spatial_grid = SpatialGrid::getInstance();
		ivec2 cell_coords = spatial_grid.get_grid_cell_coords(point_light.position);
		std::vector<Entity> entities_found = spatial_grid.query_radius(cell_coords, point_light.max_radius / 2.f);
		for (int j = 0; j < entities_found.size(); j++) {
			Entity entity_other = entities_found[j];
			Motion& motion_other = registry.motions.get(entity_other);
			if (motion_other.type_mask == 128) continue;
			//RenderRequest& render_request = registry.renderRequests.get(entity_other);
			float distance = length(motion_other.position - vec2(point_light.position));
			if (distance < closest_distance && entity_other != player_entity) {
				best_entity = entity_other;
				closest_distance = distance;
			}
		}

		Motion& best_motion = registry.motions.get(best_entity);

		vec2 to_vector = best_motion.position - vec2(point_light.position);
		vec2 to_vector_right = normalize(vec2(to_vector.y, -to_vector.x));

		vec2 to_left_side = to_vector - to_vector_right * (best_motion.scale.x / 2.f);
		vec2 to_right_side = to_vector + to_vector_right * (best_motion.scale.x / 2.f);

		vec2 left_center = vec2(point_light.position) + to_left_side / 2.f;
		vec2 right_center = vec2(point_light.position) + to_right_side / 2.f;

		Entity line1 = createColliderDebug(left_center, { length(to_left_side), 5.f }, DIFFUSE_ID::BLACK, vec3(0.7f, 0.f, 0.f));
		registry.motions.get(line1).angle = atan2(to_left_side.y, to_left_side.x);

		Entity line2 = createColliderDebug(right_center, { length(to_right_side), 5.f }, DIFFUSE_ID::BLACK, vec3(0.f, 0.f, 0.7f));
		registry.motions.get(line2).angle = atan2(to_right_side.y, to_right_side.x);

		player.line1 = line1;
		player.line2 = line2;
	}

	// Apply effects to player character(s)
	for (Entity entity : registry.players.entities) {
		{ // Count down invulnerability timers and turn player red if collided with
			Player& player = registry.players.get(player_entity);
			if (player.torchlight_enabled) {
				player.torchlight_timer = min(player.torchlight_timer + elapsed_ms_since_last_update, player.torchlight_time_max);
				float torch_factor = player.torchlight_timer / player.torchlight_time_max;

				PointLight& point_light = registry.pointLights.get(player_entity);
				Entity e = player_entity;
				point_light.set_radius(120.f + 100.f*torch_factor, 50.f*torch_factor, e);
			} else if (registry.pointLights.has(player_entity)) {
				player.torchlight_timer -= elapsed_ms_since_last_update;
				player.torchlight_timer = max(player.torchlight_timer, 0.f);
				float torch_factor = player.torchlight_timer / player.torchlight_time_max;
				PointLight& point_light = registry.pointLights.get(player_entity);
				Entity e = player_entity;
				if (player.torchlight_timer <= 0.f) {
					//printf("numPointLights: %d\n", registry.pointLights.size());
					registry.pointLights.remove(player_entity);
					registry.worldLightings.components[0].num_important_point_lights -= 1;
				} else {
					point_light.set_radius(120.f + 100.f * torch_factor, 50.f * torch_factor, e);
				}
			}
			float& timer = player.invulnerable_timer;
			timer -= elapsed_ms_since_last_update;
		}
	}

	// Remove white coloring effect from damaged enemies
	for (uint i = 0; i < registry.enemies.entities.size(); i++) {
		Entity entity = registry.enemies.entities[i];
		Enemy& enemy = registry.enemies.components[i];
		if (enemy.damaged_color_timer > 0) {
			enemy.damaged_color_timer -= elapsed_ms_since_last_update;
			if (enemy.damaged_color_timer < 0) {
				registry.renderRequests.get(entity).add_color = vec3(0.f);
			}
		}
	}

	if (registry.witches.components.size() > 0) {
		Witch& witch_spell = registry.witches.components[0];
		Entity witch = registry.witches.entities[0];

		witch_spell.light_time_remaining_ms -= elapsed_ms_since_last_update;
		PointLight& point_light = registry.pointLights.get(witch);
		if (witch_spell.light_time_remaining_ms < 900.f && witch_spell.light_time_remaining_ms > 800.f) {
			float factor = 1.f - ((witch_spell.light_time_remaining_ms - 800.f) / 100.f);
			point_light.set_radius(150.f + 170.f * factor, 50.f * factor, witch);
		}
		else if (witch_spell.light_time_remaining_ms < 500.f && witch_spell.light_time_remaining_ms >= 0.f) {
			float factor = max((witch_spell.light_time_remaining_ms / 500.f), 0.f);
			point_light.set_radius(150.f + 170.f * factor, 50.f * factor, witch);
		}
	}

	// Remove temporary effects when their timers run out
	// Iterate backwards to be able to remove without interfering with the next object to visit
	for (int i{ static_cast<int>(registry.tempEffects.entities.size() - 1) }; i >= 0; --i) {
		Entity entity = registry.tempEffects.entities[i];
		TemporaryEffect& effect = registry.tempEffects.components[i];

		if (effect.type == TEMP_EFFECT_TYPE::BEING_DEATH) {
			registry.renderRequests.get(entity).transparency += 0.05f; // Fade out effect
		} else if (effect.type == TEMP_EFFECT_TYPE::FADE_OUT) {
			registry.renderRequests.get(entity).transparency += 0.15f; // Fade out effect
		}

		effect.time_remaining_ms -= elapsed_ms_since_last_update;
		if (effect.time_remaining_ms <= 0) {
			// Remove from parent's child list before deleting // TODO: Maybe can move this into remove_entity?
			if (registry.motions.has(entity)) {
				Motion& motion = registry.motions.get(entity);
				if (motion.parent && registry.motions.has(*motion.parent)) {
					registry.motions.get(*motion.parent).remove_child(entity);
				}

				// Spawn loot when chest effect is over
				if (effect.type == TEMP_EFFECT_TYPE::CHEST_OPENING) {
					std::optional<Upgrades::PlayerUpgrade*> upgrade = Upgrades::pick_upgrade();
					if (upgrade) {
						createUpgradePickupable(motion.position - vec2{ 0, motion.scale.y / 2 }, *upgrade);
					}
					else {
						createShinyHeap(motion.position);
					}
				}
			}

			if (effect.type == TEMP_EFFECT_TYPE::ATTACK_WARNING) {
				Motion& motion = registry.motions.get(entity);
				createHellfireEffect(vec3(motion.position, 500.f), vec3(0,0,-1));
			}

			remove_entity(entity);
		}
	}

	auto& projectiles_registry = registry.projectiles;
	for (int i{ static_cast<int>(projectiles_registry.entities.size() - 1) }; i >= 0; --i) {
		Projectile& projectile = projectiles_registry.components[i];
		Entity entity = projectiles_registry.entities[i];
		projectile.last_enemy_hit_timer -= elapsed_ms_since_last_update;
		float& time_alive_ms = projectile.time_alive_ms;
		time_alive_ms += elapsed_ms_since_last_update;
		if (time_alive_ms > projectile.lifetime_ms) { // !!! TODO: Maybe also delete when speed < 3?
			remove_entity(projectiles_registry.entities[i]);
		} else if (projectile.lifetime_ms - time_alive_ms < 2000.f) {
			if (registry.attacks.has(entity)) { registry.attacks.remove(entity); }
			if (registry.pointLights.has(entity)) {
				PointLight& point_light = registry.pointLights.get(entity);
				float factor = (projectile.lifetime_ms - time_alive_ms) / 2000.f;
				point_light.set_radius(120.f * factor, 50.f * factor, entity);
			}
			Entity entity = projectiles_registry.entities[i];
			RenderRequest& render_request = registry.renderRequests.get(entity);
			render_request.transparency += 0.04f; // Fade out when close to being destroyed
			render_request.casts_shadow = false;
		}
	}

	// count down attack cooldowns
	hansel_m1_remaining_cd -= elapsed_ms_since_last_update;
	gretel_m1_remaining_cd -= elapsed_ms_since_last_update;
	hansel_m2_remaining_cd -= elapsed_ms_since_last_update;
	gretel_m2_remaining_cd -= elapsed_ms_since_last_update;

	// Remove dead (hp <= 0) enemies 
	// Chance to place food or upgrades where they died
	for (Entity entity : registry.enemies.entities) {
		if (registry.healthies.get(entity).current_hp <= 0) {
			vec2 entity_position = registry.motions.get(entity).position;
			int r = rand() % 100;
			vec3 poof_particle_colour = vec3(1.f);
			if (r < 2) { // 2
				createChest(entity_position);
				poof_particle_colour = (vec3(204, 164, 61) / 255.f) * 1.2f;
			}
			else if (r < 15) { // 15
				createFood(entity_position);
				poof_particle_colour = (vec3(250, 128, 114) / 255.f);
			}
			else if (r < 65) { // 65
				createShiny(entity_position);
				poof_particle_colour = (vec3(70, 130, 180) / 255.f);
			}
			createWoodHitEffect(vec3(entity_position, 20.f), vec3(0, 0, 1), poof_particle_colour, 0.1f, 1.4f);
			remove_entity(entity);
		}
	}

	for (int i{ static_cast<int>(registry.enemyAttractors.entities.size() - 1) }; i >= 0; --i) {
		auto& attractor_data = registry.enemyAttractors.components[i];
		if (attractor_data.hits_left <= 0) {
			remove_entity(registry.enemyAttractors.entities[i]);
		};
	}

	// If any button is held down, handle it as if it was a mouse click
	if (m1_down || m2_down) {
		handle_mouse_click_attack();
	}

	// handle navigation system
	update_navigation(registry.enemies.size() == 0 ||
			   registry.enemies.size() == 1 && registry.enemies.components[0].type == ENEMY_TYPE::SNAIL ? 1 : 0);

	
	// Apply upgrade effects
	{
		// HP Regen
		float regen = current_character.character == PLAYER_CHARACTER::HANSEL ? 
			Upgrades::get_hansel_hp_regen() : Upgrades::get_gretel_hp_regen();
		heal_player(regen*elapsed_ms_since_last_update / 1000);
	}

	// Call text update functions
	for (Text& t : registry.texts.components) {
		if (t.update_func != nullptr) t.update_func(t, elapsed_ms_since_last_update);
	}

	return true;
}

void WorldSystem::restart_game() {
	current_character = characters_checkpoint[0];
	other_character = characters_checkpoint[1];

	restart_level();
	reload_player();

	if (input_tracker.torchlight) {
		Entity e = player_entity;
		Player& player = registry.players.get(e);
		player.torchlight_enabled = true;
		PointLight& point_light = registry.pointLights.emplace(e, 220.f, 50.f, e, vec3(255.f, 30.f, 30.f) / 255.f);
		point_light.offset_position = vec3(0.f, 40.f, 0.f);
		registry.worldLightings.components[0].num_important_point_lights += 1;
	}

	m1_down = false;
	m2_down = false;
	
	ui->init(current_character.current_hp, current_character.max_hp,
		other_character.current_hp, other_character.max_hp);
}

// Reset the level to its initial state
void WorldSystem::restart_level() {
	printf("Restarting Level\n");

	// Reset the game speed
	current_speed = 1.f;

	registry.clear_all_non_essential_components();
	SpatialGrid::getInstance().clear_all_cells();

	// Debugging for memory/component leaks
	registry.list_all_components();

	createWorldLighting(); // Must be created before creating room
	// Don't do day/night-cycle at menu screen
	registry.worldLightings.components[0].is_time_changing = cur_room_ind != 0;

	Entity camera_entity = createCamera();
	Entity roomEntity = createRoom(renderer, TILE_SIZE, room_json_paths[cur_room_ind]);
	Room& room = registry.rooms.get(roomEntity);

	// If in tutorial, add tutorial textures to the ground
	if (cur_room_ind >= TUTORIAL_ROOMS_START_IND && cur_room_ind < TUTORIAL_ROOMS_START_IND + 6) {
		vec2 texture_pos = { room.grid_size.x * TILE_SIZE / 2.f, 400.f };
		vec2 texture_scale = { 400.f , 400.f };

		switch (cur_room_ind) {
			case TUTORIAL_ROOMS_START_IND:
				createIgnoreGroundPiece(renderer, texture_pos, texture_scale, 0.f, DIFFUSE_ID::ARROW_KEY_TUTORIAL);
				break;
			case TUTORIAL_ROOMS_START_IND + 1:
				createLake(renderer, { 400.f, 400.f }, { 400.f, 400.f }, M_PI); // Testing
				createPlatform(renderer, { 400.f, 400.f }, { 500.f, 100.f }, 0.f);
				//createPlatform(renderer, { 400.f, 600.f }, { 400.f, 100.f }, 0.f);
				createIgnoreGroundPiece(renderer, texture_pos + vec2(100.f), texture_scale, 0.f, DIFFUSE_ID::MOUSE_ATTACK_TUTORIAL);
				break;
			case TUTORIAL_ROOMS_START_IND + 2:
				createIgnoreGroundPiece(renderer, texture_pos, texture_scale, 0.f, DIFFUSE_ID::GRETEL_SPECIAL_TUTORIAL);
				break;
			case TUTORIAL_ROOMS_START_IND + 3:
				createIgnoreGroundPiece(renderer, texture_pos, texture_scale, 0.f, DIFFUSE_ID::SWITCH_CHARACTER_TUTORIAL);
				break;
			case TUTORIAL_ROOMS_START_IND + 4:
				createIgnoreGroundPiece(renderer, texture_pos, texture_scale, 0.f, DIFFUSE_ID::HANSEL_SPECIAL_TUTORIAL);
				break;
			case TUTORIAL_ROOMS_START_IND + 5:
				// createLake(renderer, { 400.f, 400.f }, { 300.f, 300.f }, M_PI); // Testing
				createIgnoreGroundPiece(renderer, texture_pos, texture_scale, 0.f, DIFFUSE_ID::CLEARING_ROOMS_TUTORIAL);
				break;
			default:
				printf("Error: Unsupported tutorial room ind found");
				assert(false);
		}
	}
	if (cur_room_ind == 15) {
		createLake(renderer, { 400.f, 400.f }, { 400.f, 400.f }, M_PI); // Testing
	}

	if (cur_room_ind == 0) {
		createFurniture({ room.grid_size.x * TILE_SIZE - 85, 65.f }, { 150.f,150.f }, DIFFUSE_ID::MENUUI_PIANO_WHITE);
		createFurniture({ 67.f, 67.f }, { 150.f,150.f }, DIFFUSE_ID::MENUUI_PIANO);

		createGroundPiece({ room.grid_size.x * TILE_SIZE / 2.f + 200, room.grid_size.y * TILE_SIZE / 2 }, { 350.f , 350.f }, 0.f, DIFFUSE_ID::ARROW_KEY_TUTORIAL);
		createGroundPiece({ room.grid_size.x * TILE_SIZE / 2.f - 450, 850.f }, { 180.f,180.f }, 0.f, DIFFUSE_ID::MENUUI_LOAD);
		createGroundPiece({ room.grid_size.x * TILE_SIZE / 2.f, 150.f }, { 700.f,300.f }, 0.f, DIFFUSE_ID::MENUUI_TITLE);
		createGroundPiece({ room.grid_size.x * TILE_SIZE / 2.f - 450, 450.f }, { 180.f,180.f }, 0.f, DIFFUSE_ID::MENUUI_PLAY);
		createGroundPiece({ room.grid_size.x * TILE_SIZE / 2.f - 400, 650.f }, { 280.f,180.f }, 0.f, DIFFUSE_ID::MENUUI_HELP);
	} else {
		for (int i{ static_cast<int>(registry.props.entities.size() - 1) }; i >= 0; --i) {
			remove_entity(registry.props.entities[i]); // Temporary hack to fix double placement
		}
		if (cur_room_ind == 17) {
			createProceduralProps(1.8f, 0);
			createTree(vec2(100.f), true);
			createTree(vec2(1000.f, 200.f), true);
			createTree(vec2(1900.f, 100.f), true);

			createTree(vec2(300.f, 1000.f), true);
			createTree(vec2(1200.f, 800.f), true);
			createTree(vec2(1800.f, 1200.f), true);

			createTree(vec2(200.f, 1900.f), true);
			createTree(vec2(1000.f, 1700.f), true);
			createTree(vec2(1900.f, 1800.f), true);
		} else {
			createCampFire(vec2(750, 220));
			createProceduralProps(1.8f);
		}

		Entity exit_entity = registry.exits.entities[0];
		int next_room_ind = registry.exits.get(exit_entity).next_room_ind;
		vec2 position = registry.motions.get(exit_entity).position;

		remove_entity(exit_entity);
		Entity new_exit = createExit(position, next_room_ind);
	}

	input_tracker = { false, false, false, false, input_tracker.torchlight };

	// Perform side effects of upgrades
	Upgrades::on_level_load();

	vec2 player_spawn_position = room.player_spawn;
	Entity exit_entity = registry.exits.entities[0];
	vec2 exit_position = registry.motions.get(exit_entity).position;
	vec2 diff = exit_position - player_spawn_position;
	auto& bezier = registry.bezierCurves.emplace(camera_entity, std::move(std::vector<vec2>{
		player_spawn_position,
			(player_spawn_position + diff / 3.f),
			(player_spawn_position + (diff / 3.f) * 2.f),
			exit_position,
			(player_spawn_position + (diff / 3.f) * 2.f),
			(player_spawn_position + diff / 3.f),
			player_spawn_position,
			player_spawn_position,
	}));
	// Make curve duration proportional to curve length
	bezier.curve_duration_ms = max(1.f, length(player_spawn_position - exit_position) / 300) * 1000.f;

	// Prevent enemies from spawning until camera movement has completed
	float curve_duration = bezier.curve_duration_ms * bezier.num_curves;

	SpawnerSystem::initial_wave_timer = curve_duration;

	debugging.in_debug_mode = false;

	// Create helper text for main menu
	if (cur_room_ind == 0) {
		Entity helper_text = Entity();
		Text& text = registry.texts.emplace(helper_text, "Move onto a portal\nto select game mode", vec2{1, -1});
		text.is_visible = true;
		text.font = FONT::VERDANA;
		text.size_multiplier = 8;
		text.update_func = [](Text& t, float ms_elapsed){
			static bool going_down;
			float add = ms_elapsed / 2500;
			float& green = t.text_color.y;
			float min_threshold = 0.4;

			green += going_down ? -add : add;
			if (going_down && green < min_threshold) {
				going_down = false;
			}
			else if (!going_down && green > 1) {
				going_down = true;
			}
		};

		registry.tempEffects.insert(helper_text, {TEMP_EFFECT_TYPE::TEXT, 10*1000});
	}
}

// Recreates all player components
// Note: Does not reset upgrades
void WorldSystem::reload_player() {
	Room& room = registry.rooms.components[0];
	vec2 player_spawn_position = room.player_spawn;

	if (current_character.character == PLAYER_CHARACTER::HANSEL) {
		current_character.max_hp = Upgrades::get_hansel_max_hp();
		other_character.max_hp = Upgrades::get_gretel_max_hp();
	} else {
		current_character.max_hp = Upgrades::get_gretel_max_hp();
		other_character.max_hp = Upgrades::get_hansel_max_hp();
	}

	createPlayer(
		player_entity,
		player_spawn_position,
		current_character
	);
}


// Saves the game to the save file with the given name (must end with .json)
void WorldSystem::save_game(std::string save_file_name) {
	const std::string save_path = std::string(PROJECT_SOURCE_DIR) + "data/saves/" + save_file_name;

	rapidjson::Document saveDom;
	saveDom.SetObject();
	rapidjson::Document::AllocatorType& allocator = saveDom.GetAllocator();

	// Save current room index
	saveDom.AddMember("room_ind", cur_room_ind, allocator);
	// Save number of shinies
	saveDom.AddMember("num_shinies", num_shinies, allocator);

	// Save player data
	rapidjson::Value playerData;
	playerData.SetObject();
	float hanselHealth, gretelHealth;

	bool otherCharacterIsHansel = other_character.character == PLAYER_CHARACTER::HANSEL;
	(otherCharacterIsHansel ? hanselHealth : gretelHealth) = other_character.current_hp;
	(otherCharacterIsHansel ? gretelHealth : hanselHealth) = registry.healthies.get(player_entity).current_hp;
	
	const char* selected_character_str = otherCharacterIsHansel ? "gretel" : "hansel";
	rapidjson::Value selected_character;
	selected_character = rapidjson::StringRef(selected_character_str);

	rapidjson::Value upgrades_arr(rapidjson::kArrayType);
	std::unordered_map<Upgrades::UPGRADE_ID, int> player_upgrades = *Upgrades::get_player_upgrades();

	for (auto& [key, value] : player_upgrades) {
		rapidjson::Value upgrade_info;
		upgrade_info.SetObject();

		rapidjson::Value upgrade_id, upgrade_amount;
		upgrade_id.SetInt((int)key);
		upgrade_amount.SetInt(value);

		upgrade_info.AddMember("upgrade_id", upgrade_id, allocator);
		upgrade_info.AddMember("upgrade_amount", upgrade_amount, allocator);

		upgrades_arr.PushBack(upgrade_info, allocator);
	}
	playerData.AddMember("upgrades", upgrades_arr, allocator);

	playerData.AddMember("hansel", hanselHealth, allocator);
	playerData.AddMember("gretel", gretelHealth, allocator);
	playerData.AddMember("selected", selected_character, allocator);

	saveDom.AddMember("player", playerData, allocator);


	// Write to json file
#if defined _WIN32 || defined _WIN64
	FILE* fp = fopen(save_path.c_str(), "wb"); // non-Windows use "w"
#else
	FILE* fp = fopen(save_path.c_str(), "w"); // non-Windows use "w"
#endif // _WIN32 || defined _WIN64

	char writeBuffer[8192];
	rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

	rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
	saveDom.Accept(writer);

	fclose(fp);
}

// Loads the game from the save file with the given name (must end with .json)
void WorldSystem::load_game(std::string save_file_name) {
	std::string load_path = std::string(PROJECT_SOURCE_DIR) + "data/saves/" + save_file_name;

#if defined _WIN32 || defined _WIN64
	FILE* fp = fopen(load_path.c_str(), "rb"); // non-Windows use "r"
#else
	FILE* fp = fopen(load_path.c_str(), "r"); // non-Windows use "r"
#endif

	// Parses json file into dom
	char readBuffer[8192];
	rapidjson::FileReadStream input_stream(fp, readBuffer, sizeof(readBuffer));
	rapidjson::Document dom;
	dom.ParseStream(input_stream);
	fclose(fp);

	num_shinies = dom["num_shinies"].GetInt();

	rapidjson::Value upgrades_arr = dom["player"]["upgrades"].GetArray();
	for (rapidjson::SizeType i = 0; i < upgrades_arr.Size(); i++) {
		Upgrades::UPGRADE_ID upgrade_id = (Upgrades::UPGRADE_ID)upgrades_arr[i]["upgrade_id"].GetInt();
		int upgrade_amount = upgrades_arr[i]["upgrade_amount"].GetInt();
		Upgrades::PlayerUpgrade* upgrade = Upgrades::get_upgrade_info(upgrade_id);

		for (int j = 0; j < upgrade_amount; j++) {
			Upgrades::apply_upgrade(upgrade);
			createUI_UPGRADE(upgrade->id);
		}
	}

	cur_room_ind = dom["room_ind"].GetInt();

	restart_game();
	Upgrades::reset_dropped_upgrades();

	
	CharacterState hansel = {
		Upgrades::get_hansel_max_hp(),
		(int)dom["player"]["hansel"].GetFloat(),
		PLAYER_CHARACTER::HANSEL
	};
	CharacterState gretel = {
		Upgrades::get_gretel_max_hp(),
		(int)dom["player"]["gretel"].GetFloat(),
		PLAYER_CHARACTER::GRETEL
	};
	if (dom["player"]["selected"].GetString() == "hansel") {
		current_character = hansel;
		other_character = gretel;
	}
	else {
		current_character = gretel;
		other_character = hansel;
	}
}


void apply_damage_effect(Entity enemy, Motion& enemy_motion, vec2 knockback_direction, float knockback_magnitude, bool turn_white = true) {
	// Apply knockback
	enemy_motion.velocity = knockback_direction * knockback_magnitude;

	// Apply damage color effect
	registry.enemies.get(enemy).damaged_color_timer = ENEMY_DAMAGED_EFFECT_MS;
	if (turn_white) { registry.renderRequests.get(enemy).add_color = vec3(1.f); }
}

void WorldSystem::heal_player(float healing)
{
	if (healing > 0) {
		auto& [max_hp, current_hp] = registry.healthies.get(player_entity);
		current_hp = min(max_hp, current_hp + healing);

		ui->createHealthBars(current_hp, max_hp, other_character.current_hp, other_character.max_hp);
	}
}

void WorldSystem::damage_enemy(Entity enemy, int damage) {
	float& enemy_curr_hp = registry.healthies.get(enemy).current_hp;
	enemy_curr_hp -= damage;
	// If damage was more than remaining hp, dealt damage is reduced to the difference to 0
	float damage_dealt = max(
		0.f, 
		enemy_curr_hp < 0 ? damage + enemy_curr_hp : damage
	);
	
	// Apply lifesteal upgrade effect
	// BUG: This will apply the wrong character's lifesteal amount if switching occurs before attack causes damage
	float steal = current_character.character == PLAYER_CHARACTER::HANSEL ?
		Upgrades::get_hansel_life_steal() : Upgrades::get_gretel_life_steal();

	//printf("steal percent: %f\n", steal);
	//printf("hp stolen: %f\n", steal * damage_dealt);
	heal_player(steal * damage_dealt);
	//printf("new hp: %f\n", registry.healthies.get(player_entity).current_hp);
}

void WorldSystem::damage_player(int damage) {

	Player& player_data = registry.players.get(player_entity);
	if (player_data.invulnerable_timer <= 0) {
		// If not already dying/dead:
		// Subtract hp, scream, and become invulnerable for a short amount of time
		Healthy& playerHealth = registry.healthies.get(player_entity);
		int damage_taken = Upgrades::calculate_player_damage_taken(damage);
		playerHealth.current_hp -= (float) damage_taken;
		std::cout << "hp: " << playerHealth.current_hp << std::endl;
		if (registry.players.get(player_entity).character == PLAYER_CHARACTER::HANSEL) {
			Mix_PlayChannel(-1, hansel_hurt_sound, 0);
		}
		else if (registry.players.get(player_entity).character == PLAYER_CHARACTER::GRETEL) {
			Mix_PlayChannel(-1, gretel_hurt_sound, 0);
		}
		player_data.invulnerable_timer = INVULN_TIME_MS;

		if (playerHealth.current_hp <= 0) { // Abstract into function 'character.take_damage()'
			// If both characters are below 0 hp, die. Otherwise force character switching
			if (other_character.current_hp <= 0) {
				registry.motions.get(player_entity).velocity = { 0,0 };
				// fade out and restart game;
				freeze_and_transition(3000, GameState::IN_GAME);
			}
			else {
				switch_character();

			}
		}
	}
}

Entity to_be_deleted[200] = {}; // 200 as a generous upper limit
void WorldSystem::handle_collisions()
{
	int num_deleted = 0;
	auto& collisions_registry = registry.collisions;
	for (int i = 0; i < collisions_registry.components.size(); i++) {
		Entity entity = collisions_registry.entities[i];
		Entity entity_other = collisions_registry.components[i].other;

		Motion& motion = registry.motions.get(entity);
		// TODO: Find out why some entities are being deleted before collision is handled
		if (!registry.motions.has(entity_other)) continue;
		Motion& motion_other = registry.motions.get(entity_other);
		uint32 combined_type_mask = collisions_registry.components[i].combined_type_mask;
		switch (combined_type_mask & ~PLAYER_MASK) // Ignore the possible PLAYER_MASK
		{
		case (BEING_MASK | BEING_MASK): // Player is always 'entity_other'
			// Handle player collision special cases
			if (entity_other == player_entity) {
				// Damage player if colliding with enemy
				if (registry.enemies.has(entity)) {
					damage_player(registry.enemies.get(entity).collision_damage);
					auto& curr_char = registry.healthies.get(player_entity);
					auto& other_char = other_character;
					ui->createHealthBars(curr_char.current_hp, curr_char.max_hp, other_char.current_hp, other_char.max_hp);
				}
			}
			break;
		case (BEING_MASK | OBSTACLE_MASK): // player/being is always 'entity', obstacle is always 'entity_other'
			// Players open chests running into them
			if (entity == player_entity && registry.chests.has(entity_other)) {
				// Delete chest and replace it with the chest opening effect
				createChestOpening(motion_other.position);
				Mix_PlayChannel(-1, chest_open, 0);
				to_be_deleted[num_deleted++] = entity_other;
			}
			break;
		case (BEING_MASK | PICKUPABLE_MASK):
			if (entity == player_entity) {
				auto& pickup = registry.pickupables.get(entity_other);
				switch (pickup.type)
				{
				case PICKUP_ITEM::FOOD:
				{
					Healthy& health = registry.healthies.get(entity);
					health.current_hp = min(health.current_hp + 2, health.max_hp);
					Mix_PlayChannel(-1, food_pickup, 0);
					createHealingEffect(motion.position, entity);
					auto& curr_char = registry.healthies.get(player_entity);
					auto& other_char = other_character;
					ui->createHealthBars(curr_char.current_hp, curr_char.max_hp, other_char.current_hp, other_char.max_hp);
					break;
				}
				case PICKUP_ITEM::UPGRADE:
				{
					Mix_PlayChannel(-1, upgrade_pickup, 0);
					pickup.on_pickup_callback();
					break;
				}
				case PICKUP_ITEM::SHINY:
				{
					num_shinies++;
					Mix_PlayChannel(-1, shiny_pickup, 0);
					break;
				}
				case PICKUP_ITEM::SHINY_HEAP:
				{
					num_shinies += 10;
					Mix_PlayChannel(-1, shiny_pickup, 0);
					break;
				}
				default:
					assert(false);
				}
				to_be_deleted[num_deleted++] = entity_other;
			}
			break;
		case (BEING_MASK | PROJECTILE_MASK):
		{
			Projectile& projectile = registry.projectiles.get(entity_other);
			// TODO: Do something better than the time_alive check 
			if (!(entity == projectile.owner)) {
				if (registry.healthies.has(entity) && registry.attacks.has(entity_other)) {
					int attack_damage = registry.attacks.get(entity_other).damage;

					// Handle player-projectile collision
					if (entity == player_entity) {
						damage_player(attack_damage);
						auto& curr_char = registry.healthies.get(player_entity);
						auto& other_char = other_character;
						ui->createHealthBars(curr_char.current_hp, curr_char.max_hp, other_char.current_hp, other_char.max_hp);
						--projectile.hits_left;
					}

					// Handle enemy-projectile collision
					else if (registry.enemies.has(entity) &&
						(projectile.last_hit_entity != entity || projectile.last_enemy_hit_timer < 0)
						) {
						projectile.last_hit_entity = entity;
						projectile.last_enemy_hit_timer = 250.f;
						registry.healthies.get(entity).current_hp -= attack_damage;

						// M4 play enemy hurt sound
						if (registry.sounds.has(entity)) {
							playEnemySound(entity);
						}
						if (registry.enemies.get(entity).knock_back_immune) { // ugly but needed for knock-back immune type enemies
							apply_damage_effect(
								entity,
								motion,
								vec2(0, 0),
								0
							);
						}
						else {
							apply_damage_effect(
								entity,
								motion,
								normalize(motion.position - motion_other.position),
								registry.attacks.get(entity_other).knockback
							);
						}

						--projectile.hits_left;
					}

					if (projectile.hits_left <= 0) {
						to_be_deleted[num_deleted++] = entity_other;
					}
				}
				else if (registry.enemies.has(entity) && registry.enemyAttractors.has(entity_other)) {
					if (registry.enemies.get(entity).type == ENEMY_TYPE::BEAR) {
						Healthy& bear_health = registry.healthies.get(entity);
						bear_health.current_hp = bear_health.max_hp; // restore to full health?
						createHealingEffect(motion.position, entity);
						Mix_PlayChannel(-1, chicken_eat_sound, 0); // maybe a bear chew sound?
						to_be_deleted[num_deleted++] = entity_other;
					}
				}
			}
		}
		break;
		case (BEING_MASK | MELEE_ATTACK_MASK):
			// TODO: Better solution (temporary fix for character colliding with their own attack)
			if (entity != player_entity) {
				Motion& player_motion = registry.motions.get(player_entity);
				int damage = registry.attacks.get(entity_other).damage;

				damage_enemy(entity, damage);
				if (registry.sounds.has(entity)) {
					playEnemySound(entity);
				}

				apply_damage_effect(
					entity,
					motion,
					normalize(motion.position - player_motion.position),
					registry.attacks.get(entity_other).knockback * !registry.enemies.get(entity).knock_back_immune,
					damage > 0
				);
			} else {
				int damage = registry.attacks.get(entity_other).damage;
				damage_player(damage);
				auto& curr_char = registry.healthies.get(player_entity);
				auto& other_char = other_character;
				ui->createHealthBars(curr_char.current_hp, curr_char.max_hp, other_char.current_hp, other_char.max_hp);
			}
			break;
		case (BEING_MASK | DOOR_MASK):
			// Checking Player - Exit collisions
			if (registry.exits.has(entity_other) && registry.exits.get(entity_other).enabled) {

				// Store player info
				auto& player_health = registry.healthies.get(player_entity);
				current_character = {
					player_health.max_hp,
					player_health.current_hp,
					registry.players.components[0].character
				};

				RoomExit& exit = registry.exits.get(entity_other);
                if (exit.next_room_ind == -1) {
                    cur_room_ind = (cur_room_ind + 1) % room_json_paths.size();

					// Reset shinies when going back to menu
					if (cur_room_ind == 0) {
						num_shinies = 0;
					}
					if (cur_room_ind > 5) {
						freeze_and_transition(1000, GameState::SHOP);
					} else {
						freeze_and_transition(1000, GameState::IN_GAME);
					}
                }
                else if(exit.next_room_ind == LOAD_GAME_INDEX){
					load_game_after_transition = true;
					freeze_and_transition(1000, GameState::IN_GAME);
                }else{
					cur_room_ind = exit.next_room_ind;

					// Reset shinies when going back to menu
					if (cur_room_ind == 0) {
						num_shinies = 0;
					}
					if (cur_room_ind > 8) {
						freeze_and_transition(1000, GameState::SHOP);
					} else {
						freeze_and_transition(1000, GameState::IN_GAME);
					}
				}
				
				// Setting game state deletes (almost) all components. Continuing here will lead to crash
				return;
			}
			break;
		case (OBSTACLE_MASK | PROJECTILE_MASK):
		{
			Obstacle& obstacle = registry.obstacles.get(entity);
			if (obstacle.type == OBSTACLE_TYPE::TREE || obstacle.type == OBSTACLE_TYPE::FURNITURE) {
				vec3 position = vec3(motion.position, -motion_other.sprite_offset.y);
				vec3 direction = vec3(-motion_other.velocity, 0.4f);
				vec3 colour = (vec3(93, 62, 80) / 255.f) * 1.5f;
				if (obstacle.type == OBSTACLE_TYPE::FURNITURE) {
					//colour = (vec3(194, 126, 86) / 255.f) * 1.5f;
					colour = vec3(1.f);
				}
				createWoodHitEffect(position, direction, colour);
				to_be_deleted[num_deleted++] = entity_other;
			}
		}
		break;
		case (PROJECTILE_MASK | MELEE_ATTACK_MASK):
			// TODO: Figure out why this (seemingly redundant) if statement is needed
			if (registry.projectiles.has(entity)) { // Works fine without it for me?
				// Player attack removes enemy projectiles it collides with
				Projectile& projectile = registry.projectiles.get(entity);
				if (registry.enemies.has(projectile.owner)) {
					to_be_deleted[num_deleted++] = entity;
				}
			}
			break;
		case (OBSTACLE_MASK | MELEE_ATTACK_MASK):
		{
			Obstacle& obstacle = registry.obstacles.get(entity);
			if (obstacle.type == OBSTACLE_TYPE::TREE || obstacle.type == OBSTACLE_TYPE::FURNITURE) {
				vec3 position = vec3(motion.position, -motion_other.sprite_offset.y);
				vec3 direction = vec3(-motion_other.velocity, 0.4f);
				createWoodHitEffect(position, direction, vec3(93, 62, 80) / 255.f * 1.5f);
			}
		}
			break;
		}
	}

	for (uint i = 0; i < num_deleted; i++) { // delete all entities after loop to avoid bug
		remove_entity(to_be_deleted[i]);
	}
	registry.collisions.clear();
}

void WorldSystem::playMusic() {
	if (!is_music_enabled) return;

	// Select piece to play based in game state
	Mix_Music* next_music;
	switch (game_state){
	case GameState::IN_GAME:
		next_music = cur_room_ind == 0 ? menu_music :
			cur_room_ind <= 4 ? tutorial_music : piano_music;
		break;
	case GameState::SHOP: 
		next_music = shop_music;
		break;	
	default: 
		next_music = current_music; 
		break;
	}

	// Only set music if it is not already playing
	if (next_music != current_music) {
		Mix_FadeOutMusic(1000);
		Mix_FadeInMusic(next_music, -1, 1000);
	}
	
	current_music = next_music;
}

void WorldSystem::cashRegisterSound() {
	if (!is_sfx_enabled) return;
	Mix_PlayChannel(-1, cash_register_sound, 0);
}
void WorldSystem::crowSound() {
	if (!is_sfx_enabled) return;
	Mix_PlayChannel(-1, crow_sound, 0);
}

void WorldSystem::playEnemySound(Entity enemy) {
	if (!is_sfx_enabled) return;
	Entity player = registry.players.entities[0];
	Motion& player_motion = registry.motions.get(player);
	Motion& enemy_motion = registry.motions.get(enemy);
	float x_diff = player_motion.position.x - enemy_motion.position.x;
	float y_diff = player_motion.position.y - enemy_motion.position.y;
	float diff = sqrt(x_diff * x_diff + y_diff * y_diff);
	float volum_ratio = diff/100;
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::SQUIRREL) {
		Mix_PlayChannel(1, squirrel_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::RAT) {
		Mix_PlayChannel(1, rat_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::BEAR) {
		Mix_PlayChannel(1, bear_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::BOAR) {
		Mix_PlayChannel(1, boar_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::DEER) {
		Mix_PlayChannel(1, deer_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::FOX) {
		Mix_PlayChannel(1, fox_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::RABBIT) {
		Mix_PlayChannel(1, rabbit_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::WOLF) {
		Mix_PlayChannel(1, wolf_sound, 0);
	}
	if (registry.enemies.get(enemy).type == ENEMY_TYPE::ALPHAWOLF) {
		Mix_PlayChannel(1, wolf_sound, 0);
	}
	Mix_Volume(1, 128 / volum_ratio);
}

// Should the game be over ?
bool WorldSystem::is_over() const {
	return bool(glfwWindowShouldClose(window));
}

void WorldSystem::switch_character() {
	// Don't allow switching if other character is dead
	if (other_character.current_hp <= 0) return;

	// Copy switched-out character info to a temporary struct
	CharacterState temp_char = other_character;
	Healthy& player_health = registry.healthies.get(player_entity);

	// Save info of current character to other_character
	other_character.max_hp = current_character.character == PLAYER_CHARACTER::HANSEL ? Upgrades::get_hansel_max_hp() : Upgrades::get_gretel_max_hp();
	other_character.current_hp = player_health.current_hp;
	other_character.character = registry.players.get(player_entity).character;

	// Overwrite current info with that of previously switched-out character
	player_health.max_hp = temp_char.character == PLAYER_CHARACTER::HANSEL ? Upgrades::get_hansel_max_hp() : Upgrades::get_gretel_max_hp();
	player_health.current_hp = temp_char.current_hp;
	registry.players.get(player_entity).character = temp_char.character;
	
	// Also store in current_char struct
	current_character.max_hp = player_health.max_hp;
	current_character.current_hp = player_health.current_hp;
	current_character.character = temp_char.character;

	// New player sprite
	vec2 scale;
	if (temp_char.character == PLAYER_CHARACTER::HANSEL) {
		registry.renderRequests.get(player_entity).diffuse_id = DIFFUSE_ID::HANSEL;
		scale = vec2(150.f) * vec2(1.f, 1.05f);
	}
	else if (temp_char.character == PLAYER_CHARACTER::GRETEL) {
		registry.renderRequests.get(player_entity).diffuse_id = DIFFUSE_ID::GRETEL;
		scale = vec2(130.f) * vec2(1.f, 1.05f);
	}
	registry.motions.get(player_entity).scale = scale;
	registry.motions.get(player_entity).sprite_offset = vec2(0, -scale.y / 2.f);

	// flip health bars of current and other character
	ui->createHealthBars(temp_char.current_hp, temp_char.max_hp, other_character.current_hp, other_character.max_hp);
}

// On key callback
void WorldSystem::on_key(int key, int, int action, int mod) {

	// Only respond to keypresses in game
	if (game_state != GameState::IN_GAME) return;

	// On any key press, if camera is moved by curve, stop it
	if (action == GLFW_RELEASE) {
		Entity camera_entity = registry.cameras.entities[0];
		if (registry.bezierCurves.has(camera_entity)) {
			registry.bezierCurves.remove(camera_entity);
			registry.cameras.components[0].phi = M_PI / 4.f;
			SpawnerSystem::initial_wave_timer = 0;
			if (game_state == GameState::GAME_FROZEN) {
				end_transition();
			}
			return;
		}
	}

	Motion& player_motion = registry.motions.get(player_entity);
	if (key == GLFW_KEY_W) {
		if (action == GLFW_RELEASE) { input_tracker.up_move = false; }
		else if (action == GLFW_PRESS) { input_tracker.up_move = true; }
	}
	if (key == GLFW_KEY_S) {
		if (action == GLFW_RELEASE) { input_tracker.down_move = false; }
		else if (action == GLFW_PRESS) { input_tracker.down_move = true; }
	}
	if (key == GLFW_KEY_A) {
		if (action == GLFW_RELEASE) { input_tracker.left_move = false; }
		else if (action == GLFW_PRESS) { input_tracker.left_move = true; }
	}
	if (key == GLFW_KEY_D) {
		if (action == GLFW_RELEASE) { input_tracker.right_move = false; }
		else if (action == GLFW_PRESS) { input_tracker.right_move = true; }
	}


	player_motion.move_direction.y = -(float)input_tracker.up_move + (float)input_tracker.down_move;
	player_motion.move_direction.x = -(float)input_tracker.left_move + (float)input_tracker.right_move;

	// Hotfix for Julian's mac not being able to use the primary mouse button
	if (action == GLFW_PRESS && key == GLFW_KEY_E) {
		on_mouse_click(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
	}

	if (key == GLFW_KEY_Q && action == GLFW_PRESS) { // Toggle torchlight
		Entity e = player_entity;
		Player& player = registry.players.get(e);
		if (player.torchlight_enabled) {
			input_tracker.torchlight = false;
			player.torchlight_enabled = false;
			//registry.pointLights.remove(player_entity);
		} else {
			input_tracker.torchlight = true;
			player.torchlight_enabled = true;
			//player.torchlight_timer = 0;
			if (!registry.pointLights.has(player_entity)) {
				PointLight& point_light = registry.pointLights.emplace(e, 0.f, 0.f, e, vec3(255.f, 30.f, 30.f) / 255.f);
				point_light.offset_position = vec3(0.f, 40.f, 0.f);
				registry.worldLightings.components[0].num_important_point_lights += 1;
			}
		}
	}

	// interaction with ui elements
	// F1, F2 to turn off music and sfx, respectively
	// escape to return to main menu
	if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE) {
		//goto_main_menu(registry.ui_elements.components[0]);
	}
	if (action == GLFW_RELEASE && key == GLFW_KEY_F1) {
		bool music_enabled = ui->toggle_music();
		toggle_music(music_enabled);
	}
	if (action == GLFW_RELEASE && key == GLFW_KEY_F2) {
		bool sfx_enabled = ui->toggle_sfx();
		toggle_sfx(sfx_enabled);
	}
	if (key == GLFW_KEY_P && action == GLFW_PRESS) {
		is_paused = ui->toggle_pause();
	}

	if (action == GLFW_RELEASE && key == GLFW_KEY_SPACE) {
		switch_character();
	}

	// Debugging
	if (key == GLFW_KEY_F && action == GLFW_PRESS) {
		toggle_debug();
	}

	WorldLighting& world_lighting = registry.worldLightings.components[0]; // Move the sun's position around
	if (key == GLFW_KEY_O && mod & GLFW_MOD_SHIFT) {
		if (action == GLFW_RELEASE) { world_lighting.phi_change -= 0.03f; }
		else if (action == GLFW_PRESS) { world_lighting.phi_change += 0.03f; }
	}
	else if (key == GLFW_KEY_O) {
		if (action == GLFW_RELEASE) { world_lighting.theta_change -= 0.03f; }
		else if (action == GLFW_PRESS) { world_lighting.theta_change += 0.03f; }
	}
	else if (key == GLFW_KEY_I && mod & GLFW_MOD_SHIFT) {
		if (action == GLFW_RELEASE) { world_lighting.phi_change += 0.03f; }
		else if (action == GLFW_PRESS) { world_lighting.phi_change -= 0.03f; }
	}
	else if (key == GLFW_KEY_I) {
		if (action == GLFW_RELEASE) { world_lighting.theta_change += 0.03f; }
		else if (action == GLFW_PRESS) { world_lighting.theta_change -= 0.03f; }
	}

	Camera& camera = registry.cameras.components[0]; // Tilt the camera
	if (key == GLFW_KEY_L) {
		if (action == GLFW_RELEASE) { camera.phi_change += 0.015f; }
		else if (action == GLFW_PRESS) { camera.phi_change -= 0.015f; }
	}
	else if (key == GLFW_KEY_K) {
		if (action == GLFW_RELEASE) { camera.phi_change -= 0.015f; }
		else if (action == GLFW_PRESS) { camera.phi_change += 0.015f; }
	}
}

vec2 get_true_mouse_position(vec2 mouse_pos) {
	Camera& camera = registry.cameras.components[0];
	ScreenState& screen = registry.screenStates.components[0];
	vec2 camera_offset_from_center = camera.position - vec2(screen.window_width_px / 2.f, screen.window_height_px / 2.f);
	vec2 relative_mouse_pos = mouse_pos + camera_offset_from_center;

	return relative_mouse_pos / camera.scale_factor;
}

void WorldSystem::on_mouse_move(vec2 mouse_pos) {
	ui->handle_hover_event(mouse_pos, this);
	mouse_position = mouse_pos;

	// Stop if not in-game
	if (game_state != GameState::IN_GAME) return;

	if (!registry.deathTimers.has(player_entity)) {
		Camera& camera = registry.cameras.components[0];
		Motion& player_motion = registry.motions.get(player_entity);

		player_motion.look_direction = normalize((get_true_mouse_position(mouse_pos) - player_motion.position / camera.scale_factor));
		// Flip sprite if going right
		registry.renderRequests.get(player_entity).flip_texture = player_motion.look_direction.x < 0;
	}
}

// Currently sets sfx to nullptrs, and reloads when turned back on. 
// Could be inefficient in performance.
void WorldSystem::toggle_sfx(bool sfx_enabled) {
	is_sfx_enabled = sfx_enabled;
	
	// Halt all channels
	Mix_HaltChannel(-1);
}

void WorldSystem::toggle_music(bool music_enabled) {
	is_music_enabled = music_enabled;
	if (music_enabled) {
		playMusic();  // Make sure right music is selected when resuming
		Mix_ResumeMusic();
	}
	else {
		Mix_PauseMusic();
	}
}

// Spawns attack effect, creates attack bounding box, checks collision and deals damage
void WorldSystem::do_gretel_m1() {
	// Create bounding box for the attack box. Enemies overlaping with it take damage
	Motion& player_motion = registry.motions.get(player_entity);
	BBox player_bbox = get_bbox(player_motion);

	vec2 melee_attack_size = Upgrades::get_gretel_m1_area();
	// By default, place attack at player center
	vec2 attack_position = player_motion.position;
	float player_scale = min(abs(player_motion.scale.x), abs(player_motion.scale.y));
	attack_position += (player_scale*0.8f) * normalize(player_motion.look_direction);
	vec2 attack_scale = { melee_attack_size.x / 2.f, melee_attack_size.y / 2.f };
	BBox attack_box = { attack_position.x - attack_scale.x, attack_position.x + attack_scale.x,
		attack_position.y - attack_scale.y, attack_position.y - attack_scale.y / 2.f};
	Entity attack = createAttack(
		attack_position,
		player_motion.look_direction,
		player_entity
	);

	// Damage all enemies that collide with the attack's hit box.
	// Implementation assumes it's faster to check whether something
	// collides than it is to check if it is an enemy

	//const int num_rays = 1;
	//vec2 ray_vectors[num_rays] = { player_motion.look_direction * 100.f };
	std::vector<Entity> entities_found = SpatialGrid::getInstance().query_radius(player_motion.cell_coords, 200.f);
	// First do dynamic-dynamic collisions
	for (int i = 0; i < entities_found.size(); i++) {
		Entity entity_other = entities_found[i];
		if (entity_other == player_entity) continue;

		Motion& motion_other = registry.motions.get(entity_other);
		BBox other_bbox = get_bbox(motion_other);
		if (registry.obstacles.has(entity_other)) {
			other_bbox = get_bbox(motion_other.position, vec2(motion_other.radius), vec2(0.f));
		}
		
		if (is_bbox_colliding(attack_box, other_bbox)) {
			if (!registry.collisions.has(entity_other)) {
				registry.collisions.emplace(entity_other, attack, MELEE_ATTACK_MASK | motion_other.type_mask);
			}
		}

		//for (int j = 0; j < num_rays; j++) {
		//	vec2 is_colliding = is_circle_line_colliding(motion_other.position, motion_other.radius, player_motion.position, player_motion.position + ray_vectors[j]);
		//	if (is_colliding.x != 0.f || is_colliding.y != 0.f) {
		//		registry.collisions.emplace(entity_other, attack, MELEE_ATTACK_MASK | motion_other.type_mask);
		//		break;
		//	}
		//}
	}

	//for (int i = (int)registry.motions.size() - 1; i >= 0; --i)
	//{
	//	Entity entity = registry.motions.entities[i];
	//	Motion& motion = registry.motions.components[i];

	//	if (is_bbox_colliding(attack_box, get_bbox(motion))) {
	//		registry.collisions.emplace(entity, attack, MELEE_ATTACK_MASK | motion.type_mask);
	//	}
	//}
}

void WorldSystem::do_gretel_m2() {
	// Shove away all enemies in a radius
	// TODO: Should we change this to be a stun instead? Would maybe require more assets (stun effect?)

	float radius = 200.f;
	vec2 center = registry.motions.get(player_entity).position;

	Entity attack = createPush(
		center,
		player_entity
	);

	for (int i = (int)registry.motions.size() - 1; i >= 0; --i)
	{
		Entity entity = registry.motions.entities[i];
		if (entity == player_entity) continue;
		Motion& motion = registry.motions.components[i];

		if (is_circle_colliding(center, radius, motion.position, motion.radius)) {
			if (!registry.collisions.has(entity)) {
				registry.collisions.emplace(entity, attack, MELEE_ATTACK_MASK | motion.type_mask);
			}
		}
	}
}

void WorldSystem::on_mouse_click(int button, int action, int mod) {
	(void)mod; // get rid of "unreferenced formal paramter" warning

	// Stop early if paused or not in-game
	// So the player can't attack if game is paused or we're in a menu/shop screen
	if (is_paused || game_state != GameState::IN_GAME) {
		if (action == GLFW_RELEASE) {
			ui->handle_click_event(mouse_position, this);
		}
		return;
	}

	// Set mouse_down states
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		m1_down = action == GLFW_PRESS;
	if (button == GLFW_MOUSE_BUTTON_RIGHT)
		m2_down = action == GLFW_PRESS;

	// Only respond to mouse clicks
	if (action != GLFW_PRESS) return;

	// respond immediately
	handle_mouse_click_attack();
}

void WorldSystem::handle_mouse_click_attack() {
	// If current character is HANSEL
		// Left click: projectile attack
		// Right click: throw breadcrumb
	if (other_character.character == PLAYER_CHARACTER::GRETEL) {
		if (m1_down && hansel_m1_remaining_cd <= 0) {
			Motion& player_motion = registry.motions.get(player_entity);
			createPlayerProjectile(
				player_motion.position,
				player_motion.sprite_offset,
				player_motion.look_direction,
				player_entity
			);
			Mix_PlayChannel(-1, player_throw_sound, 0);

			// Set both m1 cooldowns. Prevents rapid character switching to fire twice
			hansel_m1_remaining_cd = Upgrades::get_hansel_m1_cd();
			gretel_m1_remaining_cd = Upgrades::get_gretel_m1_cd();
		}
		else if (m2_down && hansel_m2_remaining_cd <= 0) {
			Motion& player_motion = registry.motions.get(player_entity);
			createBreadcrumbProjectile(
				player_motion.position,
				player_motion.sprite_offset,
				player_motion.look_direction,
				get_true_mouse_position(mouse_position),
				player_entity
			);
			Mix_PlayChannel(-1, player_throw_sound, 0);

			hansel_m2_remaining_cd = Upgrades::get_hansel_m1_cd();
		}
	}
	// If current character is GRETEL
		// Left click: melee attack
		// Right click: shove
	else {
		if (m1_down && gretel_m1_remaining_cd <= 0) {
			do_gretel_m1();
			Mix_PlayChannel(-1, player_melee_sound, 0);
			// Set both m1 cooldowns. Prevents rapid character switching to fire twice
			hansel_m1_remaining_cd = Upgrades::get_hansel_m1_cd();
			gretel_m1_remaining_cd = Upgrades::get_gretel_m1_cd();
		}
		else if (m2_down && gretel_m2_remaining_cd <= 0) {
			do_gretel_m2();
			Mix_PlayChannel(-1, player_melee_sound, 0);
			gretel_m2_remaining_cd = Upgrades::get_gretel_m2_cd();
		}
	}
}

vec2 normalize_navigator(vec2 actual_vector) {
	vec2 unit_vector = normalize(actual_vector) * (1.f - NAVIGATOR_SCALE.x); // so navigator does not go out the window
	// This vector does not go through projection matrix
	// If not multiplied, x-coord will be flipped
	return { -unit_vector.x, unit_vector.y };
}

float calc_navigator_angle(vec2 unit_vector) {
	float angle;
	float epsilon = 0.1f;
	// When calculating inverse cos near x-axis, acos() gives NaN.
	// So when navigator gets close to x-axis, make it face straight to left or right
	if (unit_vector.y > -epsilon && unit_vector.y < epsilon) {
		angle = unit_vector.x < 0.f ? M_PI : 0;
	}
	else {
		angle = unit_vector.y > 0.f ? acos(unit_vector.x) : -acos(unit_vector.x);
	}
	return angle;
}

// comment out if want navigators to be always visible
void toggle_navigator(vec2 actual_vector, Entity navigator_i) {
	ScreenState& screen = registry.screenStates.components[0];
	// divide by 2 does not work well
	// divide by 3 will make navigator appears starting when enemies are almost out of windoww
	if (abs(actual_vector.x) <= screen.window_width_px / 3.2f &&
		abs(actual_vector.y) <= screen.window_height_px / 3.2f) {
		registry.ui_elements.get(navigator_i).visible = false;
	}
	else {
		registry.ui_elements.get(navigator_i).visible = true;
	}
}
// handle navigation system 
void WorldSystem::update_navigation(int id) {
	if (SpawnerSystem::initial_wave_timer > 0) {
		return;
	}

	Motion& player_motion = registry.motions.get(player_entity);
	switch (id) {
		// navigation to enemies
		case(0):
			// update each navigator by enemy's information
			if (registry.navigators.size() == registry.enemies.size()) {
				for (int i = 0; i < registry.navigators.size(); i++) {
					Entity navigator_i = registry.navigators.entities[i];
					Entity enemy_i = registry.enemies.entities[i];
					Motion& motion_navigator = registry.motions.get(navigator_i);
					Motion& motion_enemy = registry.motions.get(enemy_i);
					vec2 actual_vector = player_motion.position - motion_enemy.position;
					toggle_navigator(actual_vector, navigator_i);
					vec2 unit_vector = normalize_navigator(actual_vector);
					motion_navigator.position = unit_vector;
					motion_navigator.angle = calc_navigator_angle(unit_vector);
					RenderRequest& render_navigator = registry.renderRequests.get(navigator_i);
					render_navigator.add_color = { 1,0,0 };
				}
			}
			// initialize navigators at the beginning of a stage or
			// fill in navigators when next wave spawned
			else if (registry.navigators.size() < registry.enemies.size()) {
				for (int i = registry.navigators.size(); i < registry.enemies.size(); i++) {
					createNavigator({ 0, 0 }, 0);
				}
			}
			// remove navigators of enemies that are defeated
			// no need to worry about misplaced navigators to non-exiting enemies
			else if (registry.navigators.size() > registry.enemies.size()) {
				for (int i = registry.enemies.size(); i < registry.navigators.size(); i++) {
					registry.remove_all_components_of(registry.navigators.entities[i]);
				}
			}
			break;
		// navigation to portal
		case(1):
			// if # portals matches # of navigators, update navigators 
			if (registry.navigators.size() < registry.exits.size()) {
				for (int i = 0; i < registry.exits.size() - registry.navigators.size(); i++) {
					createNavigator({ 0, 0 }, 0);
				}
			}
			// case where all navigators are removed, recreate as many navigators as needed to point to each portal
			else if (registry.exits.size() == registry.navigators.size()) {
				for (int i = 0; i < registry.exits.size(); i++) {
					Entity portal_i = registry.exits.entities[i];
					Entity navigator_i = registry.navigators.entities[i];
					Motion& motion_navigator = registry.motions.get(navigator_i);
					vec2 actual_vector = player_motion.position - registry.motions.get(portal_i).position;
					toggle_navigator(actual_vector, navigator_i);
					vec2 unit_vector = normalize_navigator(actual_vector);
					motion_navigator.position = unit_vector;
					motion_navigator.angle = calc_navigator_angle(unit_vector);
					RenderRequest& render_navigator = registry.renderRequests.get(navigator_i);
					render_navigator.add_color = { 0,0,1 };
				}
			}
			else if (registry.navigators.size() > registry.exits.size()) {
				for (int i = 0; i < registry.navigators.size() - registry.exits.size(); i++) {
					registry.remove_all_components_of(registry.navigators.entities[i]);
				}
			}
			break;
	}
} 