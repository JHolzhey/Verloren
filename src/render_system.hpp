#pragma once

#include <array>
#include <utility>

#include "common.hpp"
#include "components.hpp"
#include "tiny_ecs.hpp"
#include "spatial_grid.hpp"


#define MAX_INSTANCES 1000 // Make sure this value is the same in all shaders that use it (just textured)
#define MAX_SPRITE_INSTANCES 1000
#define MAX_MESH_INSTANCES 50 // For lakes, rivers etc. Never going to have more than 50 lakes so this should be fine
#define MAX_POINT_LIGHTS 16 // 16-byte alignment, must be same as in shaders

//struct TestData { vec4 test_stuff; };

struct InstanceData  // Needs 16-byte alignment (e.g. breaks with unpadded vec3's)
{
	mat4 transform;
	mat4 TBN;

	vec2 position;
	vec2 scale; int pad4[2];
	vec2 sprite_offset;
	vec3 sprite_normal; int pad0;
	float rotation; // If rotation == 100.0 then don't calc the transform matrix and use input transform

	float entity_id;
	float wind_strength;
	float extrude_size; // extrusion happens along normals

	int texture_indices[4]; // diffuse, normal, normal_add, mask
	vec4 diffuse_coord_loc; // x,y is relative offset and z,w is the scale factor
	vec4 normal_coord_loc;
	vec4 normal_add_coord_loc;
	vec4 mask_coord_loc;

	vec3 specular; int pad1;
	vec3 add_color; int pad2;
	vec3 multiply_color; int pad3;
	vec3 ignore_color;
	float shininess;

	float transparency;
	float transparency_offset;

	float shadow_scale;
	int num_lights_affecting;
	int lights_affecting[MAX_POINT_LIGHTS];
};

// NOTE: Changing the structure of this struct, specifically the first four elements
// will affect code that relies on this specific layout
struct CharacterInfo {
	// Data used during actual rendering: defines the rectangle within
	// the font image that represents the character
	float x_pos, y_pos,
		width, height;
	// Below is helper data: Useful to have for various calculations

	// The width and height of the rectangle in pixels
	int px_width, px_height;

	// Some characters have whitespace on their left side. 
	// This whitespace must be mirrored over to their right side 
	// so the character is centered within its rectangle
	int px_padding;  // Tracks the left-most white pixel. Should be 0 for most characters
};

struct FontInfo {
	DIFFUSE_ID texture_id;
	ivec2 image_shape;
	int cell_size;
	int space_width = -1;
	int padding_px_threshold = -1;

	// Consider changing the map into an array if slow (index by casting char to int)
	std::unordered_map<char, CharacterInfo> characters;
};

// Stretches out vertically
const vec2 inverse_aspect_ratio = vec2{ 1, -(float)window_width_px / window_height_px };
// Shrinks inwards horizontally
const vec2 inverse_aspect_ratio_2 = vec2{ (float)window_height_px / window_width_px, -1 };


// System responsible for setting up OpenGL and for rendering all the
// visual entities in the game
class RenderSystem {
	/**
	 * The following arrays store the assets the game will use. They are loaded
	 * at initialization and are assumed to not be modified by the render loop.
	 *
	 * Whenever possible, add to these lists instead of creating dynamic state
	 * it is easier to debug and faster to execute for the computer.
	 */
	
	// Make sure these paths remain in sync with the associated enumerators.
	// Associated id with .obj path
	const std::vector < std::pair<GEOMETRY_ID, std::string>> mesh_paths =
	{
		  std::pair<GEOMETRY_ID, std::string>(GEOMETRY_ID::LAKE, mesh_path("lake.obj")),
		   std::pair<GEOMETRY_ID, std::string>(GEOMETRY_ID::PLANE, mesh_path("plane.obj")),
		  std::pair<GEOMETRY_ID, std::string>(GEOMETRY_ID::SUBDIVIDED_SQUARE, mesh_path("subdivided_square.obj")),
		  // specify meshes of other assets here
	};

	// Array of gl_handles aka 'names' to associate a texture to
	std::array<std::array<GLuint, texture_count>, texture_type_count> texture_gl_handles;
	// Stores dimension of texture. First it's dimensions are loaded from file to this, then is used to fill in the texture in openGL
	std::array<std::array<ivec2, texture_count>, texture_type_count> texture_dimensions;

	std::array<int, texture_type_count> texture_type_sizes = {
		diffuse_count,
		normal_count,
	};
	// May be some empty space in these texture arrays but it's fine as long as the above counts are correct
	const std::array<std::string, texture_count> diffuse_paths = {
		// Obstacles: // maple_tree.png?
		textures_path("fir_tree.png"),
		textures_path("spruce_tree.png"),
		textures_path("chestnut_tree.png"),
		textures_path("pine_tree1.png"),
		textures_path("pine_tree2.png"),
		textures_path("pine_tree1_bare.png"),
		textures_path("pine_tree2_bare.png"),
		textures_path("pine_tree2_bare.png"), // chestnut_tree_bare
		textures_path("camp_fire.png"),
		// Complex Obstacles:
		textures_path("lake.png"),
		// Beings:
		textures_path("Hansel.png"),
		textures_path("Gretel.png"),
		textures_path("rat.png"),
		textures_path("squirrel.png"),
		textures_path("spider.png"),
		textures_path("worm.png"),
		textures_path("Wild Animals/Bear/Bear.png"),
		textures_path("Wild Animals/Boar/Boar.png"),
		textures_path("Wild Animals/Deer/Deer.png"),
		textures_path("Wild Animals/Fox/Fox.png"),
		textures_path("Wild Animals/Rabbit/Rabbit.png"),
		textures_path("Wild Animals/Wolf/Wolf.png"),
		textures_path("Wild Animals/Wolf/Alpha_wolf.png"),
		textures_path("angry_snail.png"),
		// Boss:
		textures_path("Boss/witch.png"),
		// Projectiles:
		textures_path("rock.png"),
		textures_path("acorn.png"),
		textures_path("orange_fireball.png"),
		textures_path("chestnut.png"),
		textures_path("bee.png"),
		textures_path("breadcrumbs.png"),
		// Upgrades icons:
		textures_path("upgrades/bat.png"),
		textures_path("upgrades/beehive.png"),
		textures_path("upgrades/butter.png"),
		textures_path("upgrades/fast_bullets.png"),
		textures_path("upgrades/heart.png"),
		textures_path("upgrades/longer_baguette.png"),
		textures_path("upgrades/question_mark.png"),
		textures_path("upgrades/regen.png"),
		textures_path("upgrades/snail_shell.png"),
		textures_path("upgrades/strength_up.png"),
		// Shinies
		textures_path("shinies/shiny_button.png"),
		textures_path("shinies/shiny_coin.png"),
		textures_path("shinies/shiny_coin_pile.png"),
		textures_path("shinies/shiny_gem.png"),
		textures_path("shinies/shiny_hook.png"),
		textures_path("shinies/shiny_marble.png"),
		textures_path("shinies/shiny_nail.png"),
		textures_path("shinies/shiny_spoon.png"),
		// Props
		textures_path("foliage_atlas.png"),
		textures_path("chest.png"),
		textures_path("chest_opening.png"),
		textures_path("grass_tuft.png"),
		textures_path("bush2.png"),
		// Particles:
		textures_path("leaf.png"),
		textures_path("smoke.png"),
		textures_path("butterfly.png"),
		textures_path("dragonfly.png"),
		// Temporary Effects:
		textures_path("attack.png"),
		textures_path("healing_effect.png"),
		textures_path("sparkle_effect.png"),
		textures_path("push_effect.png"),
		textures_path("flying_breadcrumbs.png"),
		// Doors:
		textures_path("room_exit.png"),
		textures_path("room_exit_disabled.png"),
		// Ground Layers:
		textures_path("grass.png"),
		textures_path("dirt.png"),
		textures_path("platform.png"),
		textures_path("ground_mask.png"),
		textures_path("ui_elements/wood_floor.png"),
		// Pickupables:
		textures_path("meat.png"),
		// Tutorial:
		textures_path("tutorials/arrow_keys_tutorial.png"),
		textures_path("tutorials/chest_tutorial.png"),
		textures_path("tutorials/clearing_rooms_tutorial.png"),
		textures_path("tutorials/currency_shop_tutorial.png"),
		textures_path("tutorials/gretel_special_tutorial.png"),
		textures_path("tutorials/hansel_special_tutorial.png"),
		textures_path("tutorials/mouse_attack_tutorial.png"),
		textures_path("tutorials/switch_character_tutorial.png"),
		// UI:
		textures_path("ui_elements/health_bar_frame.png"),
		textures_path("ui_elements/exit_icon.png"),
		textures_path("ui_elements/help_icon.png"),
		textures_path("ui_elements/music_icon.png"),
		textures_path("ui_elements/sfx_icon.png"),
		textures_path("ui_elements/pause_icon.png"),
		// menu:
		textures_path("ui_elements/play_icon.png"),
		textures_path("ui_elements/tutorial.png"),
		textures_path("ui_elements/verloren_title.png"),
		textures_path("ui_elements/piano.png"),
		textures_path("ui_elements/piano_white.png"),
		textures_path("ui_elements/load_icon.png"),
		// Screens
		textures_path("shop/shop_screen.png"),
		textures_path("shop/shop_screen_speech_1.png"),
		textures_path("shop/shop_screen_speech_2.png"),
		textures_path("shop/shop_screen_speech_3.png"),
		textures_path("shop/shop_screen_speech_4.png"),
		textures_path("shop/shop_screen_speech_5.png"),
		textures_path("shop/shop_screen_speech_6.png"),
		textures_path("shop/next_sign.png"),
		// Debug:
		textures_path("white.png"),
		textures_path("black.png"),
		textures_path("circle.png"),
		textures_path("warning_circle.png"),
		textures_path("box.png"),
		textures_path("grid.png"),
		// Enemy navigator
		textures_path("arrow.png"),
		// Text:
		textures_path("text/sylfaen_start32_size512x256_cell32x32.bmp"),
		textures_path("text/YaHei-UI-THICC_start32_size512x256_cell32x32.bmp"),
		textures_path("text/small-fonts_start32_size512x256_cell32x32.bmp"),
		textures_path("text/verdana_start32_size512x256_cell32x32.bmp"),
		textures_path("text/verdana-bold_start32_size512x256_cell32x32.bmp"),
		// textures_path("text/small-fonts_start32_size512x256_cell32x32.bmp"),
		// textures_path("text/small-fonts_start32_size512x256_cell32x32.bmp"),
	};

	const std::array<std::string, texture_count> normal_paths = {
		// Misc:
		textures_path("flat_normal.png"),
		textures_path("test_normal.png"),
		textures_path("sphere_normal.png"),
		textures_path("rounded_normal.png"),
		// Obstacles:
		textures_path("maple_tree_normal.png"), // Currently unused, needs fixing
		textures_path("spruce_tree_normal.png"),
		textures_path("fir_tree_normal.png"),
		textures_path("pine_tree1_normal.png"),
		textures_path("pine_tree2_normal.png"),
		textures_path("chestnut_tree_normal.png"),
		textures_path("pine_tree1_bare_normal.png"),
		textures_path("pine_tree2_bare_normal.png"),
		textures_path("pine_tree2_bare_normal.png"), // chestnut_tree_bare_normal
		textures_path("camp_fire_normal.png"),
		// Props:
		textures_path("foliage_atlas_normal.png"),
		textures_path("grass_tuft_normal.png"),
		textures_path("bush2_normal.png"),
		// Ground Layers:
		textures_path("grass_normal9.9.png"),
		textures_path("dirt_normal.png"),
		textures_path("platform_normal.png"),
		textures_path("water1_normal.png"),
		textures_path("water2_normal.png"),
		textures_path("hill1_normal.png"),
		textures_path("hill2_normal.png"),
		textures_path("mountain_normal.png"),
	};

	const std::array<const std::array<std::string, texture_count>, texture_type_count> texture_paths = {
		diffuse_paths, // TODO: Ask TA why it needs to be below or else memory overflow!!!
		normal_paths, // Ask about const!
	};

	std::array<GLuint, effect_count> effects;
	// Make sure these paths remain in sync with the associated enumerators.
	const std::array<std::string, effect_count> effect_paths = {
		shader_path("ui_elements/health_bar"),
		shader_path("ui_elements/ui_elements"),
		shader_path("ui_elements/text"),
		shader_path("ui_elements/color"),
		shader_path("textured"),
		shader_path("screen") };

	std::array<GLuint, geometry_count> vertex_buffers;
	std::array<GLuint, geometry_count> index_buffers;
	std::array<Mesh, geometry_count> meshes;
	std::unordered_map<FONT, FontInfo> fonts;

public:
	// Initialize the window
	bool init(GLFWwindow* window);

	template <class T>
	void bindVBOandIBO(GEOMETRY_ID gid, std::vector<T> vertices, std::vector<uint16_t> indices);

	void initializeGlTextures();

	void initializeGlEffects();

	void initializeFont(FONT font);

	void initializeGlMeshes();
	Mesh& getMesh(GEOMETRY_ID id) { return meshes[(int)id]; };

	void initializeShaderStorageBuffers();

	void initializeGlGeometryBuffers();
	// Initialize the screen texture used as intermediate render target
	// The draw loop first renders to this texture, then it is used for the wind
	// shader
	bool initScreenTexture();

	// Destroy resources associated to one or all entities created by the system
	~RenderSystem();

	// Draw all entities
	void draw(GameState game_state);

	mat3 createProjectionMatrix();

private:
	void set_vbo_and_ibo(const GLuint program, GEOMETRY_ID geometry_id, int MAX_INSTANCES_VBO_IBO = INT_MAX);
	void set_dir_light_and_view(const GLuint program);
	void set_point_lights(const GLuint program);

	mat3 calc_shadow_transform(const Motion& motion, vec3 sprite_normal, vec3 light_position, float& scale_x_increase);
	mat3 calc_TBN(vec3 sprite_normal);
	mat4 calc_model_matrix(const Motion& motion, vec3 sprite_normal, const mat3& transform, const mat3& TBN);
	float calc_depth(const Motion& motion);

	// Internal drawing functions for each entity type
	void drawUI(Entity entity);
	void updateText(Text& text);
	void drawText(Entity entity, Text& text);
	void drawColor(DIFFUSE_ID base_texture, vec4 color, vec2 pos, vec2 scale);
	void drawToScreen();

	void setup_textured_drawing();

	// Batched Drawing
	void get_texture_indices(int texture_indices[4], const RenderRequest& render_request, bool is_shadow);
	int get_texture_index(GLuint texture_gl_handle);

	void drawBatchFlush(int MAX_INSTANCES_VBO_IBO);
	InstanceData& addToBatch(Entity entity, RenderRequest& render_request, Motion& motion, bool is_shadow, int MAX_INSTANCES_VBO_IBO);

	void drawGroundPieces();
	void drawDebugComponents();
	void drawTexturedSprites(bool is_shadow, int MAX_INSTANCES_VBO_IBO);

	GLint MAX_TEXTURE_UNITS;
	std::unordered_map<GLuint, int> map_texture_index;
	int num_instances;
	int num_textures;

	vec3 camera_direction = { 0,0,1 }; // Unfinished optimization, only calculate certain transformations if camera is changing

	// Window handle
	GLFWwindow* window;

	// Screen texture handles
	GLuint frame_buffer;
	GLuint off_screen_render_buffer_color;
	GLuint off_screen_render_buffer_depth;

	GLuint instance_ssbo;
	InstanceData instance_data[MAX_INSTANCES];
	GLuint light_ssbo;
	GLuint test_ssbo;
	//TestData test_data[MAX_INSTANCES]; // Useful for debugging
};

bool loadEffectFromFile(
	const std::string& vs_path, const std::string& fs_path, GLuint& out_program);
