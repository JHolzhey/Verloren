#include "upgrades.hpp"
#include "tiny_ecs_registry.hpp"
#include "world_init.hpp"

using namespace Upgrades;

// Health
const float HANSEL_BASE_HP = 12,
          GRETEL_BASE_HP = 10;

float hansel_bonus_hp,
    gretel_bonus_hp;

float hansel_hp_regen_per_sec,
      gretel_hp_regen_per_sec;

float hansel_life_steal_percent,
      gretel_life_steal_percent;

// DAMAGE
const int HANSEL_M1_BASE_DAMAGE = 1,
        HANSEL_M2_BASE_DAMAGE = 0,
        GRETEL_M1_BASE_DAMAGE = 2,
        GRETEL_M2_BASE_DAMAGE = 0;

    // Flat damage increase, added to attack base damage
int hansel_m1_damage_bonus,
    hansel_m2_damage_bonus,
    gretel_m1_damage_bonus,
    gretel_m2_damage_bonus;

    // Damage multiplier. Total damage = (base_dmg + bonus_dmg) * dmg_multiplier
float hansel_m1_damage_multiplier,
      hansel_m2_damage_multiplier,
      gretel_m1_damage_multiplier,
      gretel_m2_damage_multiplier;

// KNOCKBACK
const float HANSEL_M1_BASE_KNOCKBACK = 50.f,
            HANSEL_M2_BASE_KNOCKBACK = 0.f,
            GRETEL_M1_BASE_KNOCKBACK = 200.f,
            GRETEL_M2_BASE_KNOCKBACK = 1000.f;

float hansel_m1_knockback_multiplier,
      hansel_m2_knockback_multiplier,
      gretel_m1_knockback_multiplier,
      gretel_m2_knockback_multiplier;

// COOLDOWN
const float HANSEL_M1_BASE_CD = 600,
            GRETEL_M1_BASE_CD = 500,
            HANSEL_M2_BASE_CD = 5000,
            GRETEL_M2_BASE_CD = 750;

float hansel_m1_cd_modifier,
      gretel_m1_cd_modifier,
      hansel_m2_cd_modifier,
      gretel_m2_cd_modifier;

// ATTACK AREA/SIZE
const vec2 GRETEL_M1_BASE_AREA = {40, 100.f};
const vec2 GRETEL_M2_BASE_AREA = {300, 300.f};
float melee_area_multiplier;

// DURATIONS
const float HANSEL_M2_BASE_DURATION = 10000;
float hansel_m2_duration_multiplier;

// MISCELLANEOUS
int hansel_m1_num_passthroughs;
DIFFUSE_ID hansel_m1_diffuse;
int damage_reduction;

// ALL UPGRADES
std::unordered_map<UPGRADE_ID, PlayerUpgrade> all_upgrades = {
    {UPGRADE_ID::INHERIT_VELOCITY, PlayerUpgrade{
        "Run and Gun",
        "Projectiles now inherit your velocity.\nIncrease your firing-rate by 50%",
        UPGRADE_ID::INHERIT_VELOCITY,
        DIFFUSE_ID::FAST_BULLETS,
        1,
        [](){ hansel_m1_cd_modifier += 0.5f; }
    }},
    {UPGRADE_ID::HORSE_CHESTNUT, PlayerUpgrade{
        "Ow! I Stepped on Something!",
        "Hansel's slingshot now fires horse chestnuts.\nFired projectiles will stay on the ground for longer\nand will damage enemies that walk across them",
        UPGRADE_ID::HORSE_CHESTNUT,
        DIFFUSE_ID::CHESTNUT,
        1,
        [](){ hansel_m1_damage_bonus++; hansel_m1_diffuse = DIFFUSE_ID::CHESTNUT; }
    }},
    {UPGRADE_ID::MELEE_DAMAGE_KNOCKBACK_BONUS, PlayerUpgrade{
        "Workout",
        "Gretel does a workout and becomes stronger\n Deal +50% more melee damage and knockback",
        UPGRADE_ID::MELEE_DAMAGE_KNOCKBACK_BONUS,
        DIFFUSE_ID::STRENGTH_UP,
        2,
        [](){ gretel_m1_damage_multiplier += 0.5; gretel_m1_knockback_multiplier += 0.5; }
    }},
    {UPGRADE_ID::PROJECTILE_PASS_THROUGH, PlayerUpgrade{
        "Slippery Projectiles",
        "Projectiles now pass through 1 additional enemy",
        UPGRADE_ID::PROJECTILE_PASS_THROUGH,
        DIFFUSE_ID::BUTTER,
        3,
        [](){ hansel_m1_num_passthroughs++; }
    }},
    {UPGRADE_ID::PLUS_MELEE_AREA, PlayerUpgrade{
        "Longer Bread",
        "Your arms grow long, allowing you to reach further.\n+50% melee attack area",
        UPGRADE_ID::PLUS_MELEE_AREA,
        DIFFUSE_ID::LONGER_BAGUETTE,
        1,
        [](){ melee_area_multiplier += 0.5f; }
    }},
    {UPGRADE_ID::BEE_PROJECTILE, PlayerUpgrade{
        "BEEES",
        "Your shooter shoots bees. Pewzz pewzz!",
        UPGRADE_ID::BEE_PROJECTILE,
        DIFFUSE_ID::BEEHIVE,
        1,
        [](){ hansel_m1_damage_bonus = 2; hansel_m1_diffuse = DIFFUSE_ID::BEE; }
    }},
    {UPGRADE_ID::SNAIL_SHELL, PlayerUpgrade{
        "Magic Snail Shell",
        "You found the shell of a magical snail.\n Take reduced damage, but an angry immortal snail will follow you",
        UPGRADE_ID::SNAIL_SHELL,
        DIFFUSE_ID::SNAIL_SHELL,
        1,
        [](){ damage_reduction += 1; }
    }},

    // TODO: Adjust Healthy.max_hp on character switch and when upgrade is applied
    {UPGRADE_ID::INCREASE_HP, PlayerUpgrade{
        "Increase HP",
        "Increases the maximum hp of both characters by 5 points",
        UPGRADE_ID::INCREASE_HP,
        DIFFUSE_ID::HEART,
        10,
        [](){ 
            hansel_bonus_hp += 5;
            gretel_bonus_hp += 5; 
            if (registry.players.size() > 0) // If upgrade applied in shop, player doesn't exists
                registry.healthies.get(registry.players.entities[0]).max_hp += 5; 
            }
    }},
    {UPGRADE_ID::HP_REGEN, PlayerUpgrade{
        "Regenerate HP",
        "Every 10 seconds, the current character recovers 1 additional health point",
        UPGRADE_ID::HP_REGEN,
        DIFFUSE_ID::REGEN,
        10,
        [](){ hansel_hp_regen_per_sec += 0.1f; gretel_hp_regen_per_sec += 0.1f; }
    }},
    {UPGRADE_ID::LIFE_STEAL, PlayerUpgrade{
        "Vampirism",
        "The reverse garlic causes you to become vampiric.\nRecover +10% of damage dealt as hp",
        UPGRADE_ID::LIFE_STEAL,
        DIFFUSE_ID::BAT,
        5,
        [](){ hansel_life_steal_percent += 0.2f; gretel_life_steal_percent += 0.2f; }
    }},
    // {UPGRADE_ID::SPLITTING_SHOT, PlayerUpgrade{
    //     "Split shot",
    //     "Hansel fires one additional projectile from his slingshot, but at different angles",
    //     UPGRADE_ID::SPLITTING_SHOT,
    //     DIFFUSE_ID::QUESTION_MARK,
    //     2,
    //     []() { 0; }
    // }},
};

// List of all upgrades available. Will shrink as upgrades are taken by the player
std::vector<PlayerUpgrade*> available_upgrades;
std::unordered_map<UPGRADE_ID, int> player_upgrades;
std::unordered_map<UPGRADE_ID, int> dropped_upgrades;

void Upgrades::init() {
    available_upgrades.clear();
    for (auto& [id, upgrade] : all_upgrades) {
        assert(id == upgrade.id);
        available_upgrades.push_back(&upgrade);
    }
    player_upgrades.clear();
    dropped_upgrades.clear();

    // HP
    hansel_bonus_hp = 0;
    gretel_bonus_hp = 0;

    hansel_hp_regen_per_sec = 0;
    gretel_hp_regen_per_sec = 0;

    // damage
    hansel_m1_damage_bonus = 0;
    hansel_m2_damage_bonus = 0;
    gretel_m1_damage_bonus = 0;
    gretel_m2_damage_bonus = 0;

    hansel_m1_damage_multiplier = 1;
    hansel_m2_damage_multiplier = 1;
    gretel_m1_damage_multiplier = 1;
    gretel_m2_damage_multiplier = 1;

    // knockback
    hansel_m1_knockback_multiplier = 1;
    hansel_m2_knockback_multiplier = 1;
    gretel_m1_knockback_multiplier = 1;
    gretel_m2_knockback_multiplier = 1;

    // Cooldown
    hansel_m1_cd_modifier = 1;
    gretel_m1_cd_modifier = 1;
    hansel_m2_cd_modifier = 1;
    gretel_m2_cd_modifier = 1;
    
    // Misc.
    melee_area_multiplier = 1.f;
    hansel_m1_num_passthroughs = 1;
    hansel_m2_duration_multiplier = 1.f;
    hansel_m1_diffuse = DIFFUSE_ID::ROCK;
    damage_reduction = 0;
}

// The number of times each upgrade has been applied to the player. Missing => no applications
int Upgrades::player_has_upgrade(UPGRADE_ID upgrade_id) {
    return player_upgrades.count(upgrade_id) ? player_upgrades[upgrade_id] : 0;
}
// The number of times the upgrade has dropped + the number of times the player has applied it
int Upgrades::upgrade_has_dropped(UPGRADE_ID upgrade_id) {
    return dropped_upgrades.count(upgrade_id) ? dropped_upgrades[upgrade_id] : 0;
}
void Upgrades::reset_dropped_upgrades() {
    // Make available again upgrades that dropped but were not picked up
    available_upgrades.clear();
    for (auto& [id, upgrade] : all_upgrades) {
        if (player_upgrades.count(id) == 0 || player_upgrades[id] < upgrade.num_repeats) {
            available_upgrades.push_back(&upgrade);
        }
    }
    dropped_upgrades = player_upgrades;
}
std::unordered_map<UPGRADE_ID, int>* Upgrades::get_player_upgrades() {
    return &player_upgrades;
}
PlayerUpgrade* Upgrades::get_upgrade_info(UPGRADE_ID upgrade_id) {
    return &all_upgrades[upgrade_id];
}
DIFFUSE_ID Upgrades::get_hansel_m1_diffuse() {
    return hansel_m1_diffuse;
}

float Upgrades::get_hansel_max_hp() { return HANSEL_BASE_HP + hansel_bonus_hp; }
float Upgrades::get_gretel_max_hp() { return GRETEL_BASE_HP + gretel_bonus_hp; }
float Upgrades::get_hansel_hp_regen() { return hansel_hp_regen_per_sec; }
float Upgrades::get_gretel_hp_regen() { return gretel_hp_regen_per_sec; }
float Upgrades::get_hansel_life_steal() { return hansel_life_steal_percent; }
float Upgrades::get_gretel_life_steal() { return gretel_life_steal_percent; }

float Upgrades::get_hansel_m1_cd() { return HANSEL_M1_BASE_CD / hansel_m1_cd_modifier; }
float Upgrades::get_hansel_m2_cd() { return HANSEL_M2_BASE_CD / hansel_m2_cd_modifier; }
float Upgrades::get_gretel_m1_cd() { return GRETEL_M1_BASE_CD / gretel_m1_cd_modifier; }
float Upgrades::get_gretel_m2_cd() { return GRETEL_M2_BASE_CD / gretel_m2_cd_modifier; }

int Upgrades::get_hansel_m1_damage() { return (int) ((HANSEL_M1_BASE_DAMAGE + hansel_m1_damage_bonus) * hansel_m1_damage_multiplier); }
int Upgrades::get_hansel_m2_damage() { return (int) ((HANSEL_M2_BASE_DAMAGE + hansel_m2_damage_bonus) * hansel_m2_damage_multiplier); }
int Upgrades::get_gretel_m1_damage() { return (int) ((GRETEL_M1_BASE_DAMAGE + gretel_m1_damage_bonus) * gretel_m1_damage_multiplier); }
int Upgrades::get_gretel_m2_damage() { return (int) ((GRETEL_M2_BASE_DAMAGE + gretel_m2_damage_bonus) * gretel_m2_damage_multiplier); }

float Upgrades::get_hansel_m1_knockback() { return HANSEL_M1_BASE_KNOCKBACK * hansel_m1_knockback_multiplier; }
float Upgrades::get_hansel_m2_knockback() { return HANSEL_M2_BASE_KNOCKBACK * hansel_m2_knockback_multiplier; }
float Upgrades::get_gretel_m1_knockback() { return GRETEL_M1_BASE_KNOCKBACK * gretel_m1_knockback_multiplier; }
float Upgrades::get_gretel_m2_knockback() { return GRETEL_M2_BASE_KNOCKBACK * gretel_m2_knockback_multiplier; }

vec2 Upgrades::get_gretel_m1_area() { return GRETEL_M1_BASE_AREA * melee_area_multiplier; }
vec2 Upgrades::get_gretel_m2_area() { return GRETEL_M2_BASE_AREA * melee_area_multiplier; }

int Upgrades::get_hansel_m1_passthrough() { return hansel_m1_num_passthroughs; }
float Upgrades::get_hansel_m2_duration() { return HANSEL_M2_BASE_DURATION * hansel_m2_duration_multiplier; }
float Upgrades::get_hansel_m2_hp() { return 3; }
int Upgrades::calculate_player_damage_taken(int base_damage_dealt) {
    return max(0, base_damage_dealt - damage_reduction);
}

void Upgrades::on_level_load() {
    reset_dropped_upgrades();
    if (player_has_upgrade(UPGRADE_ID::SNAIL_SHELL)) {
        // Spawn snail at room exit
        Entity room_exit = registry.exits.entities[0];
        vec2 exit_position = registry.motions.get(room_exit).position;

        createSnail(exit_position);
    }
}

std::optional<PlayerUpgrade*> Upgrades::pick_upgrade() {
	size_t num_upgrades_available = available_upgrades.size();
	if (num_upgrades_available == 0) {
		printf("No more upgrades :(\n");
		return std::nullopt;
	}

	// Choose an upgrade at random
	int chosen_index = rand() % num_upgrades_available;
    //chosen_index = 6;
	PlayerUpgrade* upgrade = available_upgrades[chosen_index];

    // Discard upgrade if contradicting/incompatible upgrade already acquired/dropped
    // Then try again
    if (
        (upgrade->id == UPGRADE_ID::BEE_PROJECTILE && upgrade_has_dropped(UPGRADE_ID::HORSE_CHESTNUT)) ||
        (upgrade->id == UPGRADE_ID::HORSE_CHESTNUT && upgrade_has_dropped(UPGRADE_ID::BEE_PROJECTILE)) ||
        (upgrade->id == UPGRADE_ID::INHERIT_VELOCITY && upgrade_has_dropped(UPGRADE_ID::BEE_PROJECTILE))
    ) {
        available_upgrades.erase(available_upgrades.begin() + chosen_index);
        return pick_upgrade();
    }

    // Register upgrade as dropped. 
    if (dropped_upgrades.count(upgrade->id) > 0) {
		dropped_upgrades[upgrade->id]++;
	} else {
		dropped_upgrades[upgrade->id] = 1;
	}

    // If upgrade cannot be repeated any more times, remove from pool
	if (upgrade_has_dropped(upgrade->id) == upgrade->num_repeats) {
		available_upgrades.erase(available_upgrades.begin() + chosen_index);
	}

    return upgrade;
}

void Upgrades::apply_upgrade(PlayerUpgrade* upgrade) {
	printf("Got upgrade: %s\n", upgrade->name.c_str());

    // Register upgrade as having been retrieved by player
    if (player_upgrades.count(upgrade->id) > 0) {
		player_upgrades[upgrade->id]++;
	} else {
		player_upgrades[upgrade->id] = 1;
	}
    // apply upgrade effect
	upgrade->callback();
}

