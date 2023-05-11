#pragma once

#include "common.hpp"
#include "tiny_ecs.hpp"
#include "render_system.hpp"
#include "world_system.hpp"
#include "upgrades.hpp"
#include "spatial_grid.hpp"

const int SPIDER_BASE_HP = 4,
		  WORM_BASE_HP  = 4,
		  RAT_BASE_HP  = 6,
		  SQUIRREL_BASE_HP = 3,
		  // wild animals have more health in general
		  BEAR_BASE_HP = 20,
		  BOAR_BASE_HP = 8,
		  DEER_BASE_HP = 6,
		  FOX_BASE_HP = 6,
		  RABBIT_BASE_HP = 5,
		  WOLF_BASE_HP = 9,
		  WITCH_BASE_HP = 300;

const int LOAD_GAME_INDEX = -2;
		

const vec2 SQUIRREL_SCALE = vec2(50.f, 50.f),
			SPIDER_SCALE = vec2(50.f, 26.f) * 2.f,
			WORM_SCALE = vec2(80.f, 80.f),
			BEAR_SCALE = vec2(64.f, 33.3) * 3.f,
			BOAR_SCALE = vec2(64.f, 40.f) * 2.f,
			DEER_SCALE = vec2(72.f, 52.f) * 2.f,
			FOX_SCALE = vec2(64.f, 36.f) * 2.f,
			RABBIT_SCALE = vec2(32.f, 26.f) * 2.f,
			WOLF_SCALE = vec2(64.f, 40.f) * 2.f,
			TREE_SCALE = vec2(92.f, 113.f) * 2.f,
			SNAIL_SCALE = vec2(50.f, 50.f),
			BASIC_TREE_SCALE = vec2(92.f, 113.f) * 2.f,
			SPRUCE_TREE_SCALE = vec2(50.f, 100.f) * 3.f,
			FIR_TREE_SCALE = vec2(43.f, 71.f) * 3.5f,
			PINE_TREE1_SCALE = vec2(64.f, 140.f) * 2.5f,
			PINE_TREE2_SCALE = vec2(48.f, 106.f) * 3.f,
			CHESTNUT_TREE_SCALE = vec2(176.f, 171.f) * 3.f,
			FOLIAGE_PROP_SCALE = vec2(60.f, 50.f) * 1.f,
			CAMP_FIRE_SCALE = vec2(64.f, 71.f) * 1.5f,
			WITCH_SCALE = vec2(32.f, 32.f) * 6.5f,
			NAVIGATOR_SCALE = vec2(900.f, 900.f) * 0.0001f,
			FIREBALL_SCALE = vec2(32.f, 32.f);


// Masks for collision calculations
const uint32 UNCOLLIDABLE_MASK	= 0,
			 PLAYER_MASK		= (1 << 0), // Player has this mask and the BEING_MASK
			 BEING_MASK			= (1 << 1), // Enemies only have this mask
			 OBSTACLE_MASK		= (1 << 2),
			 PROJECTILE_MASK	= (1 << 3),
			 MELEE_ATTACK_MASK	= (1 << 4),
			 PICKUPABLE_MASK	= (1 << 5),
			 DOOR_MASK  		= (1 << 6),
			 POLYGON_MASK		= (1 << 7),
			 PARTICLE_MASK		= (1 << 8),
			 PROP_MASK			= (1 << 9);


Motion& createMotion(Entity e, uint32 type, vec2 pos, vec2 scale, float max_speed);

BezierCurve& insertBezierCurve(Entity entity, vec2 position, vec2 direction);
// the player
Entity createPlayer(Entity entity, vec2 pos, CharacterState char_state);
// Effects
Entity createAttack(vec2 position, vec2 orientation, Entity player);
Entity createPush(vec2 position, Entity player);
Entity createWitchPush(vec2 position, Entity player);
Entity createHealingEffect(vec2 position, Entity parent);
Entity createSparkleEffect(vec2 position, Entity parent, vec3 multiply_color);
Entity createBreadcrumbEatingEffect(vec2 position);
// Particle generator
Entity createParticleGenerator(vec3 position, vec3 direction, vec2 scale, float frequency, DIFFUSE_ID diffuse_id, float transparency = 0.f,
	vec3 multiply_color = vec3(1.f), float generator_lifetime_ms = -1.f, float gravity_multiplier = 1.f, range particle_lifetime_ms = { 5000.f, 5000.f },
	range max_angular_spread = { 0.f, M_PI / 4.f }, range speed = { 100.f, 100.f }, range angular_speed = { 0.f, M_PI / 4.f },
	vec3 max_rect_spread = { 0.f, 0.f, 0.f }, float scale_spread = 10.f, vec3 ignore_color = { -1.f, -1.f, -1.f });
// Particles spawned by the generator
Entity createParticle(Entity generator_entity, ParticleGenerator& generator);
// Particle effect types:
Entity createWoodHitEffect(vec3 position, vec3 direction, vec3 multiply_color, float speed_increase_factor = 0.f, float bigger_poof = 1.f);
Entity createHellfireEffect(vec3 position, vec3 direction, vec3 multiply_color = {1.f,1.f,1.f});
// Pickups / upgrades
Entity createFood(vec2 position);
Entity createUpgradePickupable(vec2 pos, Upgrades::PlayerUpgrade* upgrade);
Entity createShiny(vec2 pos);
Entity createShinyHeap(vec2 pos);
Entity createChest(vec2 pos);
Entity createChestOpening(vec2 pos);
// camp fire
Entity createCampFire(vec2 position);
// the tree
Entity createTree(vec2 position, bool is_force_bare = false);
// the tree
Entity createFurniture(vec2 position, vec2 scale, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id = NORMAL_ID::FLAT);
// the enemy
Entity createSpider(vec2 position);
Entity createWorm(vec2 position);
Entity createRat(vec2 position);
Entity createSquirrel(vec2 position);
// the enemy (tracks: 0 for idle/special pose, 1 for walk, 2 for run)
Entity createBear(vec2 position);
Entity createBoar(vec2 position);
Entity createDeer(vec2 position);
Entity createFox(vec2 position);
Entity createRabbit(vec2 position);
Entity createAlphaWolf(vec2 position);
Entity createWolf(vec2 position);
Entity createSnail(vec2 position);
// the enemy (boss)
Entity createWitch(vec2 position); // TODO: implement this
// projectile
Entity createAcornProjectile(vec2 position, vec2 offset, vec2 direction, Entity owner);
Entity createFireballProjectile(vec2 position, vec2 offset, vec2 direction, Entity owner);
Entity createPlayerProjectile(vec2 position, vec2 offset, vec2 direction, Entity owner);
Entity createBreadcrumbProjectile(vec2 position, vec2 offset, vec2 direction, vec2 aimed_position, Entity player);
Entity createHellfireWarning(vec2 position, Entity parent);
// a box, circle, or line for debugging
Entity createColliderDebug(vec2 position, vec2 scale, DIFFUSE_ID diffuse_id, vec3 color = { 0.7f,0.f,0.f }, float transparency = 0.f, vec2 offset = { 0.f,0.f });
Entity createWorldBounds(vec2 position, vec2 scale, DIFFUSE_ID diffuse_id, vec3 color = { 0.7f,0.f,0.f }, float transparency = 0.f, vec2 offset = { 0.f,0.f });
// a room
Entity createRoom(RenderSystem* renderer, float tile_size, std::string json_path);
// the floor
Entity createRoomGround(const Room& room, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id, float repeat_texture = 1.f, vec3 multiply_color = { 1.f,1.f,1.f });
// a ground piece (Flat texture)
Entity createIgnoreGroundPiece(RenderSystem* renderer, vec2 position, vec2 scale, float angle, DIFFUSE_ID diffuse_id);
Entity createGroundPiece(vec2 pos, vec2 scale, float angle, DIFFUSE_ID diffuse_id, NORMAL_ID normal_id = NORMAL_ID::FLAT,
	float repeat_texture = 1.f, vec3 multiply_color = { 1.f,1.f,1.f }, DIFFUSE_ID mask_id = DIFFUSE_ID::DIFFUSE_COUNT);
Entity createDirtPatch(RenderSystem* renderer, vec2 position, vec2 scale, float angle);
// a lake
Entity createLake(RenderSystem* renderer, vec2 position, vec2 scale, float angle);
// a platform that might sit on top of a lake
Entity createPlatform(RenderSystem* renderer, vec2 position, vec2 scale, float angle);
// spawn props procedurally
void createProceduralProps(float density_per_tile, int type_weights_index = -1);
// an exit
Entity createExit(vec2 position, int next_room_ind);
// the world lighting controller
Entity createWorldLighting();
// a camera
Entity createCamera();
// an enemy navigator
Entity createNavigator(vec2 pos, float angle);