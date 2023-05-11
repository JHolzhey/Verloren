#pragma once

// internal
#include "common.hpp"
#include "render_system.hpp"
#include "ui_system.hpp"


// stlib
#include <vector>
#include <random>
#include <functional>
#include <optional>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

#include "spatial_grid.hpp"

struct CharacterState {
	float max_hp, current_hp;
	PLAYER_CHARACTER character;
};

struct InputTracker {
	bool up_move{false}, down_move{ false }, left_move{ false }, right_move{ false }, torchlight{ false };
};

// Forward declaration. Do no delete. https://stackoverflow.com/a/628079/8132000
class UISystem;

// Container for all our entities and game logic. Individual rendering / update is
// deferred to the relative update() methods
class WorldSystem
{
public:
	WorldSystem();

	//void playAlfaWolfSound();

	// Creates a window
	GLFWwindow* create_window();

	// starts the game
	void init(RenderSystem* renderer, UISystem* ui);

	// Releases all associated resources
	~WorldSystem();

	void remove_entity(Entity entity);

	// Steps the game ahead by ms milliseconds
	bool step(float elapsed_ms);
	void update_window();

	// Check for collisions
	void handle_collisions();

	// Should the game be over ?
	bool is_over()const;

	static const float TILE_SIZE;

	// Currency
	int num_shinies = 0;

	// Game state
	void set_game_state(GameState, bool);
	GameState get_game_state();
	bool is_paused = false;

	// Audio
	void playEnemySound(Entity enemy);
	void cashRegisterSound();
	void crowSound();
	void toggle_music(bool enable);
	void toggle_sfx(bool enable);
private:
	// Start the game at main menu screen
	GameState game_state = GameState::MAIN_MENU;

	GameState next_game_state = GameState::GAME_STATE_COUNT;
	void freeze_and_transition(float transition_duration, GameState next_state, bool fade_to_black, bool restart_after);
	void end_transition();

	// Input callback functions
	void on_key(int key, int, int action, int mod);
	void on_mouse_move(vec2 pos);
	void on_mouse_click(int button, int action, int mods);
	vec2 mouse_position;
	void handle_mouse_click_attack();
	
	// Player character control
	void do_gretel_m1();
	void do_gretel_m2();
	void switch_character();

	// Deal damage to player character
	void damage_player(int damage);
	void damage_enemy(Entity enemy, int damage);
	void heal_player(float healing);

	// restarts the level and the player
	void restart_game();
	// restart level - does not affect player
	void restart_level();
	// Recreates all player components
	void reload_player();
	// Moves the players position to the room's player spawn
	void move_player_to_room_spawn();
	// Create navigation arrow that points to all enemies
	// 0 for enemy
	// 1 for portal
	void update_navigation(int id);

	// Load/Save
	void save_game(std::string save_path);
	void load_game(std::string load_path);

	// Audio
	Mix_Music* current_music;
	void playMusic();
	bool is_music_enabled = true;
	bool is_sfx_enabled = true;


	// OpenGL window handle
	GLFWwindow* window;

	// user interaction
	UISystem* ui;

	// Game state
	RenderSystem* renderer;
	InputTracker input_tracker;
	float current_speed;
	// Player entity should never change -- will lead to bugs
	const Entity player_entity;
	// Stores information about the switched-out character
	CharacterState other_character;
	// Info about current character -- used when switching rooms/states
	CharacterState current_character;
	// current and other char states when room was loaded
	CharacterState characters_checkpoint[2];

	std::vector<std::string> room_json_paths;
	int cur_room_ind;

	// music references
	Mix_Music* piano_music;
	Mix_Music* shop_music;
	Mix_Music* menu_music;
	Mix_Music* tutorial_music;
	Mix_Chunk* chicken_dead_sound;
	Mix_Chunk* chicken_eat_sound;
	// M4
	Mix_Chunk* hansel_hurt_sound;
	Mix_Chunk* gretel_hurt_sound;
	Mix_Chunk* rat_sound;
	Mix_Chunk* squirrel_sound;
	Mix_Chunk* bear_sound;
	Mix_Chunk* boar_sound;
	Mix_Chunk* player_melee_sound;
	Mix_Chunk* player_throw_sound;
	Mix_Chunk* food_pickup;
	Mix_Chunk* upgrade_pickup;
	Mix_Chunk* shiny_pickup;
	Mix_Chunk* portal_open;
	Mix_Chunk* chest_open;
	Mix_Chunk* fox_sound;
	Mix_Chunk* deer_sound;
	Mix_Chunk* rabbit_sound;
	Mix_Chunk* wolf_sound;
	Mix_Chunk* alfa_wolf_sound;
	Mix_Chunk* cash_register_sound;
	Mix_Chunk* crow_sound;

	// C++ random number generator
	std::default_random_engine rng;
	std::uniform_real_distribution<float> uniform_dist; // number between 0..1
};
