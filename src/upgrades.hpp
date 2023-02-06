#pragma once
#include <unordered_map>
#include <map>
#include <optional>

#include "common.hpp"
#include "components.hpp"

namespace Upgrades {
    enum class UPGRADE_ID : int {

        // Advanced effects
        INHERIT_VELOCITY,
        HORSE_CHESTNUT,
        POISONED_BREADCRUMB,
        BEE_PROJECTILE,
        BOUNCY_SHOT,
        KNOCKBACK_OBJECT_COLLISION_DAMAGE,
        SNAIL_SHELL,
        HP_REGEN,
        LIFE_STEAL,
        SPLITTING_SHOT,

        // Basic stat increases
        RANGED_DAMAGE_BONUS,
        RANGED_DAMAGE_MULTIPLIER,
        MELEE_DAMAGE_KNOCKBACK_BONUS,
        MELEE_DAMAGE_BONUS,
        MELEE_DAMAGE_MULTIPLIER,
        PLUS_MELEE_FREQUENCY,
        PLUS_RANGED_FREQUENCY,
        PLUS_MELEE_KNOCKBACK,
        INCREASE_HP,

        // Other stat increases
        PROJECTILE_PASS_THROUGH,
        PLUS_MELEE_AREA,
        HEAVY_PROJECTILES, // +dmg, -speed
    };

    enum class UPGRADE_RARITY : int {
        COMMON,
        RARE,
        EPIC
    };

    struct PlayerUpgrade {
        std::string name,
                    description;
        UPGRADE_ID id;
        DIFFUSE_ID icon_diffuse;
        // How many time an upgrade can be applied
        int num_repeats;
        // Function to be called when the upgrade is acquired
        std::function<void(void)> callback;

        UPGRADE_RARITY rarity = UPGRADE_RARITY::COMMON;
    };    


    void init();
    // Called each time a level is loaded to run upgrade side-effects
    void on_level_load();
    // Returns the number of times that the given upgrade has been applied to the player
    int player_has_upgrade(UPGRADE_ID upgrade_id);
    // Same, but number of times it has been dropped (this room) and applied (total)
    int upgrade_has_dropped(UPGRADE_ID upgrade_id);
    void reset_dropped_upgrades();
    // Randomly pick one of the available upgrades
	std::optional<PlayerUpgrade*> pick_upgrade();
    void apply_upgrade(PlayerUpgrade* upgrade);
    std::unordered_map<UPGRADE_ID, int>* get_player_upgrades();
    PlayerUpgrade* get_upgrade_info(UPGRADE_ID);

// GETTERS
    // Sprites
    DIFFUSE_ID get_hansel_m1_diffuse();

    // Health
    float get_hansel_max_hp();
    float get_gretel_max_hp();

    float get_hansel_hp_regen();
    float get_gretel_hp_regen();

    float get_hansel_life_steal();
    float get_gretel_life_steal();

    // Attack cooldowns
	float get_hansel_m1_cd();
	float get_hansel_m2_cd();
	float get_gretel_m1_cd();
	float get_gretel_m2_cd();

    // Attack damage
	int get_hansel_m1_damage();
	int get_hansel_m2_damage();
	int get_gretel_m1_damage();
	int get_gretel_m2_damage();

    // Attack knockback
	float get_hansel_m1_knockback();
	float get_hansel_m2_knockback();
	float get_gretel_m1_knockback();
	float get_gretel_m2_knockback();

    // Attack area
    vec2 get_gretel_m1_area();
    vec2 get_gretel_m2_area();
    
    // Miscellaneous
    int get_hansel_m1_passthrough();
    float get_hansel_m2_duration();
    float get_hansel_m2_hp();
    int calculate_player_damage_taken(int base_damage_dealt);
}