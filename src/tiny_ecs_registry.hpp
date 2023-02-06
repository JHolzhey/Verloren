#pragma once
#include <vector>

#include "tiny_ecs.hpp"
#include "components.hpp"

class ECSRegistry
{
	// Callbacks to remove a particular or all entities in the system
	std::vector<ContainerInterface*> registry_list;

public:
	// Manually created list of all components this game has
	ComponentContainer<Motion> motions;
	ComponentContainer<RenderRequest> renderRequests;
	ComponentContainer<RoomExit> exits;
	ComponentContainer<Room> rooms;
	ComponentContainer<DeathTimer> deathTimers;
	ComponentContainer<Collision> collisions;
	ComponentContainer<Player> players;
	ComponentContainer<Mesh*> meshPtrs;
	ComponentContainer<ScreenState> screenStates;
	ComponentContainer<Obstacle> obstacles;
	ComponentContainer<Prop> props;
	ComponentContainer<Pickupable> pickupables;
	ComponentContainer<Projectile> projectiles;
	ComponentContainer<RangedEnemy> rangedEnemies;
	ComponentContainer<Enemy> enemies;
	ComponentContainer<DebugComponent> debugComponents;
	ComponentContainer<Healthy> healthies;
	ComponentContainer<HealthBar> healthBars;
	ComponentContainer<UI_Element> ui_elements;
	ComponentContainer<SpriteSheetAnimation> spriteSheets;
	ComponentContainer<TemporaryEffect> tempEffects;
	ComponentContainer<ColliderDebug> colliderDebugs;
	ComponentContainer<WorldLighting> worldLightings;
	ComponentContainer<DirLight> dirLights;
	ComponentContainer<PointLight> pointLights;
	ComponentContainer<Camera> cameras;
	ComponentContainer<GroundPiece> groundPieces;
	ComponentContainer<EnemySpawner> spawners;
	ComponentContainer<Attack> attacks;
	ComponentContainer<BezierCurve> bezierCurves;
	ComponentContainer<EnemyAttractor> enemyAttractors;
	ComponentContainer<Chest> chests;
	ComponentContainer<LerpSequence<vec3>> lerpVec3s;
	ComponentContainer<ComplexPolygon> polygons;

	ComponentContainer<SOUND_TYPE> sounds;

	ComponentContainer<Navigator> navigators;
	ComponentContainer<Witch> witches;
	ComponentContainer<Text> texts;
	ComponentContainer<ParticleGenerator> particleGenerators;
	ComponentContainer<Particle> particles;
	ComponentContainer<Water> waters;

	// constructor that adds all containers for looping over them
	// IMPORTANT: Don't forget to add any newly added containers!
	ECSRegistry()
	{
		registry_list.push_back(&motions);
		registry_list.push_back(&renderRequests);
		registry_list.push_back(&exits);
		registry_list.push_back(&rooms);
		registry_list.push_back(&deathTimers);
		registry_list.push_back(&collisions);
		registry_list.push_back(&players);
		registry_list.push_back(&meshPtrs);
		registry_list.push_back(&screenStates);
		registry_list.push_back(&obstacles);
		registry_list.push_back(&props);
		registry_list.push_back(&pickupables);
		registry_list.push_back(&projectiles);
		registry_list.push_back(&rangedEnemies);
		registry_list.push_back(&enemies);
		registry_list.push_back(&debugComponents);
		registry_list.push_back(&healthies);
		registry_list.push_back(&healthBars);
		registry_list.push_back(&ui_elements);
		registry_list.push_back(&spriteSheets);
		registry_list.push_back(&tempEffects);
		registry_list.push_back(&colliderDebugs);
		registry_list.push_back(&worldLightings);
		registry_list.push_back(&dirLights);
		registry_list.push_back(&pointLights);
		registry_list.push_back(&cameras);
		registry_list.push_back(&groundPieces);
		registry_list.push_back(&spawners);
		registry_list.push_back(&attacks);
		registry_list.push_back(&bezierCurves);
		registry_list.push_back(&enemyAttractors);
		registry_list.push_back(&chests);
		registry_list.push_back(&lerpVec3s);
		registry_list.push_back(&polygons);

		registry_list.push_back(&sounds);

		registry_list.push_back(&navigators);
		registry_list.push_back(&witches);
		registry_list.push_back(&texts);
		registry_list.push_back(&particleGenerators);
		registry_list.push_back(&particles);
		registry_list.push_back(&waters);

	}

	void clear_all_components() {
		for (ContainerInterface* reg : registry_list)
			reg->clear();
	}

	void clear_all_non_essential_components() {
		std::vector<ContainerInterface* > essentials = {
			&screenStates,			
		};
		for (ContainerInterface* reg : registry_list) {
			if (std::none_of(essentials.begin(), essentials.end(), 
				[reg](ContainerInterface* item){ return reg == item; })
			) {
				reg->clear();
			}
		}
	}

	void list_all_components() {
		printf("Debug info on all registry entries:\n");
		for (ContainerInterface* reg : registry_list)
			if (reg->size() > 0)
				printf("%4d components of type %s\n", (int)reg->size(), typeid(*reg).name());
	}

	void list_all_components_of(Entity e) {
		printf("Debug info on components of entity %u:\n", (unsigned int)e);
		for (ContainerInterface* reg : registry_list)
			if (reg->has(e))
				printf("type %s\n", typeid(*reg).name());
	}

	void remove_all_components_of(Entity e) {
		for (ContainerInterface* reg : registry_list)
			reg->remove(e);
	}
};

extern ECSRegistry registry;