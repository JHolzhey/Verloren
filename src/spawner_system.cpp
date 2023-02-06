#include "spawner_system.hpp"
#include "world_init.hpp"


void spawn_enemy(ENEMY_TYPE enemy_type, vec2 position) {
    switch (enemy_type) {
    case ENEMY_TYPE::RAT:
        createRat(position);
        break;
    case ENEMY_TYPE::SQUIRREL:
        createSquirrel(position);
        break;
    case ENEMY_TYPE::WORM:
        createWorm(position);
        break;
    case ENEMY_TYPE::BEAR:
        createBear(position);
        break;
    case ENEMY_TYPE::BOAR:
        createBoar(position);
        break;
    case ENEMY_TYPE::DEER:
        createDeer(position);
        break;
    case ENEMY_TYPE::FOX:
        createFox(position);
        break;
    case ENEMY_TYPE::RABBIT:
        createRabbit(position);
        break;
    case ENEMY_TYPE::ALPHAWOLF:
        createAlphaWolf(position);
        break;
    case ENEMY_TYPE::WITCH:
        createWitch(position);
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 30; j++) {
                createSpider({ j * 30.f, i * 30.f });
            }
        }
        createCampFire(position + vec2(50.f));
        break;
    case ENEMY_TYPE::SKIP:
        // Skips spawning enemy this wave
        break;
    default:
        printf("Error: Unknown enemy type to spawn");
        assert(false);
    }
}

bool no_more_enemies_to_kill() {
    size_t num_enemies = registry.enemies.entities.size();
    return num_enemies == 0 ||
        (num_enemies == 1 && registry.enemies.components[0].type == ENEMY_TYPE::SNAIL);
}

// Extermination-based system: Enemies spawn each time all enemies are dead
float SpawnerSystem::initial_wave_timer = 0;
void SpawnerSystem::step(float elapsed_ms) 
{
    (void) elapsed_ms;
    auto& room = registry.rooms.components[0];
    auto& spawners = registry.spawners.entities;

    if (initial_wave_timer > 0) {
        initial_wave_timer -= elapsed_ms;
        return;
    }

    // Spawn next wave if current one is dead
    if (room.current_wave < room.num_waves) {
        if (no_more_enemies_to_kill()) {
            for (Entity entity : spawners) {
                auto& enemies = registry.spawners.get(entity).enemies;
                if (!enemies.empty()) {
                    vec2 spawner_position = registry.motions.get(entity).position;
                    spawn_enemy(enemies.front(), spawner_position);
                    enemies.erase(enemies.begin());
                }
            }
            room.current_wave++;
        }
    }
    // Enable exits. Increment current_wave so it only happens once
    else if (room.current_wave == room.num_waves) {
        if (no_more_enemies_to_kill()) {
            for (Entity exit : registry.exits.entities) {
                registry.exits.get(exit).enabled = true;
                registry.renderRequests.get(exit).diffuse_id = DIFFUSE_ID::ROOM_EXIT;
            }
            room.current_wave++;
        }
    }

}