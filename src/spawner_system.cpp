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
        for (int i = 0; i < 5; i++) { // 5
            for (int j = 0; j < 30; j++) { // 30
                createSpider({ j * 30.f, i * 30.f });
            }
        }
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
                RenderRequest& render_request = registry.renderRequests.get(exit);
                render_request.diffuse_id = DIFFUSE_ID::ROOM_EXIT;
                render_request.add_color = -vec3(0.2f);

                PointLight& point_light = registry.pointLights.emplace(exit, 300.f, 200.f, exit, vec3(10.f, 10.f, 200.f) / 255.f);
                point_light.offset_position = vec3(0.f, 0.f, 60.f);              // 200
                WorldLighting& world_lighting = registry.worldLightings.components[0];
                world_lighting.num_important_point_lights += 1;


                Motion& motion = registry.motions.get(exit);

                // Setup smoke particle animations:
                float anim_length0 = (1500.f/8)*3.f;
                std::vector<int> frames0 = { 0, 1 };
                float anim_interval0 = anim_length0 / frames0.size();
                auto all_anim_tracks0 = { frames0 };
                std::vector<float> all_anim_intervals0 = { anim_interval0 };

                Entity e = createParticleGenerator(vec3(motion.position, 35.f), vec3(0, 0, 1), vec2(60.f), 4.f, DIFFUSE_ID::SMALL_SPARKLE_EFFECT,
                    0.1f, (vec3(200.f, 200.f, 255.f) / 255.f), -1.f, 0.f, { 2000.f, 2500.f }, { 0.f, M_PI / 10.f }, { 10.f, 10.f });
                // Give the particle generator a sprite sheet as a reference for it's particles to use upon their creation
                auto& sprite_sheet = registry.spriteSheets.emplace(e, 2, all_anim_tracks0, all_anim_intervals0);
                sprite_sheet.is_reference = true; // Necessary to prevent animation_system.cpp trying to update the particle generator's animations
                sprite_sheet.loop = false;
            }
            room.current_wave++;
        }
    }

}