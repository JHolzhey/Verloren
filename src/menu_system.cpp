#include "world_system.hpp"
#include "menu_system.hpp"
#include "tiny_ecs_registry.hpp"

MenuSystem::MenuSystem() {

}

void MenuSystem::init(){
    createMenuUI_HELP();
    createMenuUI_PLAY();
}

//private:
Entity MenuSystem::createMenuUI_PLAY(){
    auto entity = Entity();
	auto& ui = registry.ui_elements.emplace(entity);
	auto& motion = registry.motions.emplace(entity);
	motion.scale = ui.menu_option_scale;
	//return vec3(-0.725, 0.84, 0.0);;
	motion.position = { -0.725, 0.84 };
	registry.renderRequests.insert(entity, { DIFFUSE_ID::MENUUI_PLAY, NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}

Entity MenuSystem::createMenuUI_HELP(){
    auto entity = Entity();
	auto& ui = registry.ui_elements.emplace(entity);
	auto& motion = registry.motions.emplace(entity);
	motion.scale = ui.menu_option_scale;
	//return vec3(-0.725, 0.84, 0.0);;
	motion.position = { -0.5, 0.5 };
	registry.renderRequests.insert(entity, { DIFFUSE_ID::MENUUI_HELP, NORMAL_ID::NORMAL_COUNT,
			EFFECT_ID::UI_ELEMENTS, GEOMETRY_ID::SPRITE });
	return entity;
}