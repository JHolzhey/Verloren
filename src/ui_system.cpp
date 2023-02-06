// Header
#include <string>

#include "ui_system.hpp"
#include "physics_system.hpp"

// Note that the UI_Element pointer cannot be counted on to remain correct
// So it should only be used within a single cycle, and never be stored anywhere
struct UI_Target {
	Entity e;
	UI_Element* ui_elem;
};

namespace GLOW_COLOR {
	const vec3 RED 		= {1, 0, 0};
	const vec3 ORANGE	= {1, 0.5, 0};
	const vec3 GREEN 	= {0, 1, 0};
	const vec3 BLUE 	= {0, 0, 1};
	const vec3 YELLOW 	= {1, 0.8, 0};
};

Entity create_glow(Entity target, UI_Element* ui_target, vec3 color);

UISystem::UISystem() {
}

// We can prevent icons from stretching by multiplying their size with the inverse of the aspect ratio
// TODO: ScreenState& screen = registry.screenStates.components[0];
vec2 upgrade_icon_position = {-0.9, -0.9};
void createUI_UPGRADE(Upgrades::UPGRADE_ID upgrade_id) {
	auto* upgrade_info = Upgrades::get_upgrade_info(upgrade_id);
	auto diffuse = upgrade_info->icon_diffuse;
    vec2 icon_scale = vec2{0.12f, 0.12f} * inverse_aspect_ratio;

	{ // The icon background
		auto entity = Entity();
		auto& ui_elem = registry.ui_elements.emplace(entity);
		ui_elem.depth = -10;
        ui_elem.interactive = false;

		auto& motion = registry.motions.emplace(entity);
		motion.position = upgrade_icon_position;
		motion.scale = icon_scale;
		registry.renderRequests.insert(entity, { DIFFUSE_ID::WHITE, NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
    }
	{ // The upgrade icon itself
		auto entity = Entity();
		auto& ui_elem = registry.ui_elements.emplace(entity);
		ui_elem.interactive = true;
		ui_elem.hover_glow_enabled = false;
		registry.texts.emplace(entity, upgrade_info->description, vec2());
		ui_elem.hover_text = entity;

		auto& motion = registry.motions.emplace(entity);
		motion.position = upgrade_icon_position;
		motion.scale = icon_scale * 0.9f;
		registry.renderRequests.insert(entity, { diffuse, NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	}

	upgrade_icon_position.x += icon_scale.x;
}

void UISystem::init(float curr_curr_hp, float curr_max_hp, float other_curr_hp, float other_max_hp) {
	createHealthBarFrame();
	createHealthBars(curr_curr_hp, curr_max_hp, other_curr_hp, other_max_hp);

	//this->settings_icon = createUI_SETTINGS();
	this->pause_icon = createUI_PAUSE();
	this->exit_icon = createUI_EXIT();
	//this->help_icon = createUI_HELP();
	this->music_icon = createUI_MUSIC();
	this->sfx_icon = createUI_SFX();

	// If UI icons are removed at each reset, recreate them here
	// Minor problem: Order of upgrades will not be maintained
	upgrade_icon_position = {-0.9, -0.9};
	for (auto& [id, count] : *Upgrades::get_player_upgrades()) {
		for (int _ = 0; _ < count; _++) {
			createUI_UPGRADE(id);
		}
	}
}

std::optional<ShopItem> createShopItem() {
	auto upgrade = Upgrades::pick_upgrade();
	if (!upgrade) return std::nullopt;

	int num_player_upgrades = 0;
	auto* player_upgrades = Upgrades::get_player_upgrades();
	for (auto& [upgrade, count] : *player_upgrades) {
		num_player_upgrades += count;
	}
	return ShopItem{
		Entity(),
		*upgrade,
        // Increase the cost of upgrades as player acquires more
		clamp(num_player_upgrades, 3, 10) + (rand() % 6)
	};
}

// The shop screen only has space for three items
vec2 shop_item_slot_locations[6] = {
	{0.12, 0.02},
	{0.38, 0.02},
	{0.65, 0.02},
	{0.12, -0.35},
	{0.38, -0.35},
	{0.65, -0.35},
};

// return false if no shop screen could not be created. True otherwise
bool UISystem::createShopScreen(WorldSystem* world) {
	num_shop_items = 0;
	for (int i = 0; i < 6; i++) {
		auto shop_upgrade = createShopItem();
		if (shop_upgrade) {
			shop_items[i] = *shop_upgrade;
			num_shop_items++;
		} else {
			break;
		}
	}

    // If no items could be created, don't create the shop screen
	if (num_shop_items == 0) {
		return false;
	}

    // Create shop screen background: a single texture that covers the whole window
    shop_background = Entity();
    Motion& bg_motion = registry.motions.emplace(shop_background);
    bg_motion.position = {0, 0};
    bg_motion.scale = {2, -2};
    registry.renderRequests.insert(shop_background, {
        DIFFUSE_ID::SHOP_SCREEN,
        NORMAL_ID::NORMAL_COUNT,
        EFFECT_ID::UI_ELEMENTS,
        GEOMETRY_ID::SPRITE,
    });
    auto& ui_background = registry.ui_elements.emplace(shop_background);
    ui_background.always_visible = true;
    ui_background.visible = true;
    ui_background.depth = -10;
	ui_background.interactive = false;

	// Create "next" sign
	shop_next_sign = Entity();
    Motion& next_motion = registry.motions.emplace(shop_next_sign);
    next_motion.position = {0.6, -0.8f};
    next_motion.scale = {0.4, -0.4f};
    registry.renderRequests.insert(shop_next_sign, {
        DIFFUSE_ID::NEXT_SIGN,
        NORMAL_ID::NORMAL_COUNT,
        EFFECT_ID::UI_ELEMENTS,
        GEOMETRY_ID::SPRITE,
    });
    auto& ui_next = registry.ui_elements.emplace(shop_next_sign);
    ui_next.always_visible = true;
	ui_next.hover_text_enabled = false;
    ui_next.visible = true;
	ui_next.glow_offset = {0, 0.1};

    // Add the items to the shop 
	for (int i = 0; i < num_shop_items; i++) {
		auto [item_entity, upgrade, price, _1, _2] = shop_items[i];
		
		// Render item icon
		registry.renderRequests.insert(item_entity, {
            upgrade->icon_diffuse,
            NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::UI_ELEMENTS,
            GEOMETRY_ID::SPRITE,
        });

		// Position icon and create a ui_element for it
		auto& motion = registry.motions.emplace(item_entity);
		motion.position = shop_item_slot_locations[i];
		motion.scale = vec2{ 0.15, 0.15 } * inverse_aspect_ratio;
		auto& ui_element = registry.ui_elements.emplace(item_entity);
		ui_element.always_visible = true;
		ui_element.visible = true;
		ui_element.glow_scale_multiplier = 1.2f;
		ui_element.hover_text = item_entity;
		
		// Hover text -- item name. Make new entity for hover text, so we can make price text share entity with upgrade icon
		registry.texts.emplace(item_entity, upgrade->name, vec2());
		// for (auto & c: ui_element.hover_text) c = toupper(c);
		// ui_element.hover_text += "\n\n";
		// ui_element.hover_text += upgrade->description;

		// Text -- item price
		{
			Entity e = Entity();
			Text& price_text = registry.texts.emplace(e, "$" + std::to_string(price), shop_item_slot_locations[i] + vec2{0.045, -0.22});
			price_text.is_visible = true;
			// price_text.is_box_enabled = false;
			price_text.box_color = {0, 0, 0, 0.7};
			price_text.box_margin = {0.03, 0};
			price_text.text_color = world->num_shinies < price ? vec4{1, 0, 0, 1} : vec4{1, 1, 1, 1};
			price_text.size_multiplier = 7;
			shop_items[i].price_text_entity = e;
		}
		
	}

	return true;
}


void UISystem::createHealthBars(float curr_curr_hp, float curr_max_hp,
	float other_curr_hp, float other_max_hp) {
	createHealthBar((int)curr_curr_hp/1, (int)curr_max_hp/1, true);
	createHealthBar((int)other_curr_hp/1, (int)other_max_hp/1, false);
}

void UISystem::step(float elapsed_ms, WorldSystem* world) {
	// In the shop, switch screens every X seconds
	// Always switches between back and forth between the base screen
	//  and one of a set of screens where the crow talks
	if (world->get_game_state() == GameState::SHOP) {
		const int num_shop_screens = 6;
		const int base_screen_duration = 8 * 1000;
		const int speech_screen_duration = 3 * 1000;

		// Timer variable for the current screen
		static float current_screen_ms_remaining;
		current_screen_ms_remaining -= elapsed_ms;
		
		if (current_screen_ms_remaining < 0) {
			if (registry.renderRequests.has(shop_background)) {
				auto& rr = registry.renderRequests.get(shop_background);

				// If at base screen, switch to a speech screen and play the crow sound
				if (rr.diffuse_id == DIFFUSE_ID::SHOP_SCREEN) {
					rr.diffuse_id = (DIFFUSE_ID) ((int) DIFFUSE_ID::SHOP_SCREEN_SPEECH_1 + rand() % num_shop_screens);
					world->crowSound();
					current_screen_ms_remaining = speech_screen_duration;
				} 
				// If at a speech screen, switch back to the base screen
				else {
					rr.diffuse_id = DIFFUSE_ID::SHOP_SCREEN;
					current_screen_ms_remaining = base_screen_duration;
				}
			}
		}
	}
}

// create Health bar
Entity UISystem::createHealthBar(int current_hp, int max_hp, bool is_primary) {
	std::string hp_str = std::to_string(current_hp) + " / " + std::to_string(max_hp);
	// case when initializing the health bars
	if (registry.healthBars.size() <= 1) {
		Entity entity = Entity();
		registry.motions.emplace(entity);
		auto& ui_elem = registry.ui_elements.emplace(entity);
		ui_elem.always_visible = true;
		ui_elem.visible = true;
		ui_elem.interactive = false;
		auto& hb = registry.healthBars.emplace(entity);
		// character starts with full health to begin the game
		// if needed for a partial health to start the game, modify the healthBar fields
		hb.current_hp = current_hp;
		hb.max_hp = max_hp;
		hb.is_primary = is_primary;
		RenderRequest& render_request = registry.renderRequests.insert(entity, { DIFFUSE_ID::DIFFUSE_COUNT, NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::HEALTH_BAR, GEOMETRY_ID::HEALTH_BAR });
		render_request.add_color = vec3(0.f, 1.f, 0.f);

		Text& text = registry.texts.emplace(entity, hp_str, vec2{ -0.88, is_primary ? 0.89 : 0.79 });
		text.is_visible = true;
		text.is_box_enabled = false;
		text.size_multiplier = 3.5;
		text.text_color = {0, 0, 0, 1};
		text.font = FONT::VERDANA_BOLD;
		text.perform_repositioning = false;

		return entity;
	}
	// case when health bars are created, only need to modify, not to recreate
	// previous health gets the current health of the last instance created
	// current health gets the new current health
	// TODO: interpolate between previous to current health
	else {
		// Primary hb has index 0, secondary hb has index 1
		int hb_index = is_primary ? 0 : 1;
		Entity& healthbar_entity = registry.healthBars.entities[hb_index];
		HealthBar& hb = registry.healthBars.components[hb_index];

		// Only update hp text if hp actually changes
		if (current_hp != hb.current_hp || max_hp != hb.max_hp) {
			Text& text = registry.texts.get(healthbar_entity);
			// Create new buffer if text changes size, in case current buffer too small
			text.create_new_buffers = text.content.size() != hp_str.size();
			text.update_render_data = true;
			text.content = hp_str;
		}

		hb.current_hp = current_hp;
		hb.max_hp = max_hp;		

		return healthbar_entity;
	}
}

// create Health bar frame
Entity UISystem::createHealthBarFrame() {
	auto entity = Entity();
	auto& ui_elem = registry.ui_elements.emplace(entity);
	ui_elem.always_visible = true;
	ui_elem.visible = true;
	ui_elem.interactive = false;
	auto& motion = registry.motions.emplace(entity);
	motion.position = { -0.725, 0.84 };
	motion.scale = vec2{ 0.5, -0.25 };
	registry.renderRequests.insert(entity, { DIFFUSE_ID::HEALTH_BAR_FRAME, NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}

// create exit icon
Entity UISystem::createUI_EXIT() {
	auto entity = Entity();
	auto& ui_element = registry.ui_elements.emplace(entity);
	auto& text_element = registry.texts.emplace(entity, "EXIT GAME", vec2());
	text_element.font = FONT::VERDANA_BOLD;
	text_element.box_margin = {0.02, 0.02};
	text_element.size_multiplier = 5.f;
	ui_element.hover_text = entity;
	
	auto& motion = registry.motions.emplace(entity);
	motion.position = { 0.6, 0.9 };
	motion.scale = vec2{ 0.1, 0.1 } * inverse_aspect_ratio;
	registry.renderRequests.insert(entity, {
		DIFFUSE_ID::UI_EXIT,
	  	NORMAL_ID::NORMAL_COUNT,
	  	EFFECT_ID::UI_ELEMENTS,
		GEOMETRY_ID::SPRITE 
	});

	return entity;
}

// create settings icon
Entity UISystem::createUI_SETTINGS() {
	auto entity = Entity();
	auto& ui_element = registry.ui_elements.emplace(entity);
	auto& text_element = registry.texts.emplace(entity, "SETTINGS", vec2());
	text_element.font = FONT::VERDANA_BOLD;
	text_element.box_margin = { 0.02, 0.02 };
	text_element.size_multiplier = 5.f;
	ui_element.hover_text = entity;

	auto& motion = registry.motions.emplace(entity);
	motion.position = { 0.45, 0.9 };
	motion.scale = vec2{ 0.1, 0.1 } *inverse_aspect_ratio;
	registry.renderRequests.insert(entity, { DIFFUSE_ID::UI_HELP, NORMAL_ID::NORMAL_COUNT,
		EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}

// create help icon
Entity UISystem::createUI_HELP() {
	auto entity = Entity();
	auto& ui_element = registry.ui_elements.emplace(entity);
	auto& text_element = registry.texts.emplace(entity, "HELP", vec2());
	text_element.font = FONT::VERDANA_BOLD;
	text_element.box_margin = {0.02, 0.02};
	text_element.size_multiplier = 5.f;
	ui_element.hover_text = entity;

	auto& motion = registry.motions.emplace(entity);
	motion.position = { 0.6, 0.9 };
	motion.scale = vec2{ 0.1, 0.1 } * inverse_aspect_ratio;
	registry.renderRequests.insert(entity, { DIFFUSE_ID::UI_HELP, NORMAL_ID::NORMAL_COUNT,
		EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}

// create bg music icon
Entity UISystem::createUI_MUSIC() {
	auto entity = Entity();
	auto& ui_element = registry.ui_elements.emplace(entity);
	ui_element.sprite_sheet_num = 2;
	auto& text_element = registry.texts.emplace(entity, "TOGGLE MUSIC", vec2());
	text_element.font = FONT::VERDANA_BOLD;
	text_element.box_margin = {0.02, 0.02};
	text_element.size_multiplier = 5.f;
	ui_element.hover_text = entity;

	auto& motion = registry.motions.emplace(entity);
	motion.position = { 0.75, 0.9 };
	motion.scale = vec2{ 0.1, 0.1} * inverse_aspect_ratio;
	registry.renderRequests.insert(entity, { DIFFUSE_ID::UI_MUSIC, NORMAL_ID::NORMAL_COUNT,
		EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}

// create sfx icon
Entity UISystem::createUI_SFX() {
	auto entity = Entity();
	auto& ui_element = registry.ui_elements.emplace(entity);
	ui_element.sprite_sheet_num = 2;
	auto& text_element = registry.texts.emplace(entity, "TOGGLE SFX", vec2());
	text_element.font = FONT::VERDANA_BOLD;
	text_element.box_margin = {0.02, 0.02};
	text_element.size_multiplier = 5.f;
	ui_element.hover_text = entity;

	auto& motion = registry.motions.emplace(entity);
	motion.position = { 0.9, 0.9 };
	motion.scale = vec2{ 0.1, 0.1 } * inverse_aspect_ratio;
	registry.renderRequests.insert(entity, { DIFFUSE_ID::UI_SFX, NORMAL_ID::NORMAL_COUNT,
		EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}

Entity UISystem::createUI_PAUSE() {
	Entity entity = Entity();
	
	registry.renderRequests.insert(entity, { DIFFUSE_ID::UI_PAUSE, NORMAL_ID::NORMAL_COUNT, EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	auto& ui_elem = registry.ui_elements.emplace(entity);
	ui_elem.interactive = false;
	auto& motion = registry.motions.emplace(entity);
	motion.position = {0, 0};
	motion.scale = vec2{0.6, 0.6} * inverse_aspect_ratio;
	
	return entity;
}

bool UISystem::toggle_sfx() {
	UI_Element& icon = registry.ui_elements.get(sfx_icon);
	icon.icon_enabled = !icon.icon_enabled;
	return icon.icon_enabled;
}

bool UISystem::toggle_music() {
	UI_Element& icon = registry.ui_elements.get(music_icon);
	icon.icon_enabled = !icon.icon_enabled;
	return icon.icon_enabled;
}

bool UISystem::toggle_pause() {
	static bool already_paused;
	bool game_paused = !already_paused;
	
	// If game just got paused
	if (game_paused && !already_paused) {
		// Uncomment this if ui icons don't overlap as they should:
		// registry.ui_elements.sort([](Entity ui1, Entity ui2){ 
		// 	return registry.ui_elements.get(ui1).depth > registry.ui_elements.get(ui2).depth; 
		// });
		for (auto& ui_elem : registry.ui_elements.entities) {
			if (registry.navigators.has(ui_elem)) continue;
			registry.ui_elements.get(ui_elem).visible = true;
		}
	}
	// If game just got unpaused
	else if (already_paused && !game_paused) {
		for (auto& ui_elem : registry.ui_elements.components) {
			ui_elem.visible = ui_elem.always_visible;
		}
	}
	
	already_paused = game_paused;

	return already_paused;
}

std::optional<UI_Target> get_ui_elem_at(vec2 position) {
	BBox cursor_bbox = {position.x, position.x, position.y, position.y};

	for (size_t i = 0; i < registry.ui_elements.entities.size(); i++) {
		Entity e = registry.ui_elements.entities[i];
		UI_Element& ui_elem = registry.ui_elements.components[i];

		if (
			ui_elem.visible && 
			ui_elem.interactive &&
			is_bbox_colliding(cursor_bbox, get_bbox(registry.motions.get(e)))
		) {
			return UI_Target{e, &ui_elem};
		}
	}

	return std::nullopt;
}

// return true if a ui icon was clicked on. False otherwise
bool UISystem::handle_click_event(vec2 position, WorldSystem* world) {
	ScreenState& screen = registry.screenStates.components[0];
	vec2 screen_position = {position.x / screen.window_width_px, position.y / screen.window_height_px};
	screen_position = ((screen_position * 2.f) - 1.f) * vec2{1.f, -1.f};
	
	auto target = get_ui_elem_at(screen_position);
	if (target) {
		auto [entity, ui_elem] = *target;
		if (entity == sfx_icon) {
			world->toggle_sfx(toggle_sfx());
		} 
		else if (entity == music_icon) {
			world->toggle_music(toggle_music());
		}
		else if (entity == shop_next_sign) {
			world->set_game_state(GameState::IN_GAME, true);
		}
		// Check for upgrade item collision
		else {
			for (int i = 0; i < num_shop_items; i++) {
				auto& [item, upgrade, price, already_bought, price_entity] = shop_items[i];
				if (entity == item && !already_bought) {
					assert(world->get_game_state() == GameState::SHOP);

					// Return early if player cannot afford item
					// TODO: Have crow reprimand player
					if (price > world->num_shinies) {
						continue;
					}
					world->num_shinies -= price;
					world->cashRegisterSound();
					Upgrades::apply_upgrade(upgrade);
					already_bought = true;

					// Update price text color based on whether player can still afford the item
					for (int j = 0; j < num_shop_items; j++) {
						auto price_j = shop_items[j].price;
						auto price_entity_j = shop_items[j].price_text_entity;
						if (price_entity_j) {
							Text& price_text = registry.texts.get(*price_entity_j);
							price_text.text_color = world->num_shinies < price_j ? vec4{1, 0, 0, 1} : vec4{1, 1, 1, 1};
						}
					}

					registry.remove_all_components_of(item);
					registry.remove_all_components_of(*price_entity);
					shop_items[i].price_text_entity = std::nullopt;

					// reset hover effect as well
					if (prev_hover) {
						prev_hover = std::nullopt;
					}
					if (hover_glow) {
						registry.remove_all_components_of(*hover_glow);
						hover_glow = std::nullopt;
					}
					if (hover_text) {
						registry.remove_all_components_of(*hover_text);
						hover_text = std::nullopt;
					}
				}
			}
		}
		return true;
	}
	return false;
}

// Creates a glow element behind the given entity
Entity create_glow(Entity target, UI_Element* ui_target, vec3 color=GLOW_COLOR::YELLOW) {
	auto& target_motion = registry.motions.get(target);
	vec2 target_position = target_motion.position;
	vec2 target_scale = target_motion.scale;

	auto entity = Entity();
	auto& ui_element = registry.ui_elements.emplace(entity);
	ui_element.is_glow = true;
	ui_element.visible = true;
	ui_element.depth = -2;
    ui_element.interactive = false; // Don't trigger hover/click effects on glows
	auto& motion = registry.motions.emplace(entity);
	motion.position = target_position + ui_target->glow_offset;
	motion.scale = target_scale * ui_target->glow_scale_multiplier;

	// Content here doesn't really matter -- will get overwritten by color
	RenderRequest& render_request = registry.renderRequests.insert(entity, {
		DIFFUSE_ID::WHITE,
	  	NORMAL_ID::NORMAL_COUNT,
	  	EFFECT_ID::UI_ELEMENTS,
		GEOMETRY_ID::SPRITE 
	});
	render_request.add_color = color;

	// Sort ui elements, so glow appears behind target. 
    // Should be quick as most elements should already be in order
	registry.ui_elements.sort([](Entity ui1, Entity ui2){
		return registry.ui_elements.get(ui1).depth < registry.ui_elements.get(ui2).depth; 
	});

	return entity;
}

// Return shop_item if entity is that item. If entity is not a shop item return nullopt
std::optional<ShopItem*> UISystem::get_if_shop_item(Entity e) {
	for (int i = 0; i < num_shop_items; i++) {
		if (shop_items[i].entity == e) return &shop_items[i];
	}
	return std::nullopt;
}

bool UISystem::handle_hover_event(vec2 position, WorldSystem* world) {
	ScreenState& screen = registry.screenStates.components[0];
	vec2 screen_position = {position.x / screen.window_width_px, position.y / screen.window_height_px};
	screen_position = ((screen_position * 2.f) - 1.f) * vec2{1.f, -1.f};
	
	auto target = get_ui_elem_at(screen_position);
	if (target) {
		auto [entity, ui_elem] = *target;
		
        // Only fire event if we weren't hovering over the element already
		if (prev_hover != entity) {
			prev_hover = entity;
			
			// If hover text already exists: Remove it
			if (hover_text) {
				// If a hover effect already exists, remove the existing one
				registry.texts.get(*hover_text).is_visible = false;
				hover_text = std::nullopt;
			}

			if (ui_elem->hover_text_enabled) {
				// If target has hover text, render it
				if (ui_elem->hover_text) {
					registry.texts.get(*ui_elem->hover_text).is_visible = true;
					hover_text = ui_elem->hover_text;
				}
			}
			
            // If hover glow effect already exists: Remove it
            if (hover_glow) {
				// If a hover effect already exists, remove the existing one
				// NOTE!!! ui_element will possible no longer point to valid memory after this 
                registry.remove_all_components_of(*hover_glow);
                hover_glow = std::nullopt;
            }

			if (ui_elem->hover_glow_enabled) {
				// Create hover glow effect on current target
				if (auto shop_item = get_if_shop_item(entity)) {
					// Color red if player can't afford. Green otherwise
					vec3 color = (*shop_item)->price > world->num_shinies ? GLOW_COLOR::RED : GLOW_COLOR::GREEN;
					hover_glow = create_glow(entity, ui_elem, color);
				} else {
					hover_glow = create_glow(entity, ui_elem);
				}
			}
            
		} 
		// Update position of existing hover text
		if (hover_text) {
			Text& text = registry.texts.get(*hover_text);
			text.start_position = screen_position;
			text.update_render_data = true;
		}

		return true;
	} 
    // If no hover target exists
    else {
        // If a hover effects exist, remove them
        if (hover_glow) {
            registry.remove_all_components_of(*hover_glow);
            hover_glow = std::nullopt;
        }
		// If a hover text exists, make it not visible
		if (hover_text) {
			// If a hover effect already exists, remove the existing one
			registry.texts.get(*hover_text).is_visible = false;
			hover_text = std::nullopt;
		}
        prev_hover = std::nullopt;
        return false;
    }
}

