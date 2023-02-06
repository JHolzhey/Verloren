#pragma once

// internal
#include "common.hpp"
#include "tiny_ecs_registry.hpp"
#include "render_system.hpp"
#include "world_system.hpp"
#include "upgrades.hpp"


// TODO: Add support for more than just selling upgrades (using variant?)
struct ShopItem {
	Entity entity;
	Upgrades::PlayerUpgrade* upgrade;
	int price;
	// If true, entity should no longer exist
    bool is_purchased = false;
	std::optional<Entity> price_text_entity = std::nullopt;
};

// Forward declaration, preventing compilation crash
class WorldSystem;

class UISystem
{
public:
	UISystem();
	// If not obvious, look at the following:
	// curr = current
	// curr_curr_hp = current character's current hp
	void createHealthBars(float curr_curr_hp, float curr_max_hp, float other_curr_hp, float other_max_hp);
	void init(float curr_curr_hp, float curr_max_hp, float other_curr_hp, float other_max_hp);
	// Steps the game ahead by ms milliseconds
	// possibly for interpolation
	void step(float elapsed_ms, WorldSystem* world);

	bool toggle_pause();
	bool toggle_music();
	bool toggle_sfx();
	bool handle_click_event(vec2 position, WorldSystem* world);
	bool handle_hover_event(vec2 position, WorldSystem* world);

	bool createShopScreen(WorldSystem* world);
private:
	Entity createHealthBar(int char_current_hp, int char_max_hp, bool is_primary);
	Entity createHealthBarFrame();
	Entity createUI_SETTINGS();
	Entity createUI_EXIT();
	Entity createUI_HELP();
	Entity createUI_MUSIC();
	Entity createUI_SFX();
	Entity createUI_PAUSE();

	Entity settings_icon;
	Entity exit_icon;
	Entity help_icon;
	Entity music_icon;
	Entity sfx_icon;
	Entity pause_icon;
	Entity shop_next_sign;
	Entity shop_background;
	// Shop screen can hold up to 3 items.
	ShopItem shop_items[6];
	// The number of shop items successfully created
	int num_shop_items;

	// The glow effect created on hover. nullopt if no hover effect exists
	std::optional<Entity> hover_glow = std::nullopt;
	std::optional<Entity> hover_text = std::nullopt;
	// The ui element which was previously hovered over. nullopt if nothing was hovered over last
	std::optional<Entity> prev_hover = std::nullopt;
	std::optional<ShopItem*> get_if_shop_item(Entity e);

};

void createUI_UPGRADE(Upgrades::UPGRADE_ID upgrade_id);