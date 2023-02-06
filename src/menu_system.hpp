#pragma once

// internal
#include "common.hpp"
#include "tiny_ecs_registry.hpp"
#include "world_system.hpp"
#include "render_system.hpp"

class MenuSystem
{
public:
	MenuSystem();
    void init();

private:
	Entity createMenuUI_PLAY();
    Entity createMenuUI_HELP();
};