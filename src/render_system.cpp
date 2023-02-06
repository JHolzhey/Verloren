// internal
#include "render_system.hpp"
#include <SDL.h>
#include <chrono>
#include <queue>

#include "tiny_ecs_registry.hpp"

using Clock = std::chrono::high_resolution_clock;

void RenderSystem::set_dir_light_and_view(const GLuint program) // Constant for all textured meshes
{
	// Passing dir_light's light direction and camera view position for lighting calculations
	WorldLighting& world_lighting = registry.worldLightings.components[0];
	DirLight& dir_light = registry.dirLights.get(world_lighting.dir_light);
	Camera& camera = registry.cameras.components[0];

	vec3 camera_3D_position = vec3(camera.position, 0.0) + camera.direction * 1000.f;
	glUniform3fv(glGetUniformLocation(program, "view_position"), 1, (float*)&camera_3D_position);
	glUniform3fv(glGetUniformLocation(program, "dir_light.direction"), 1, (float*)&dir_light.direction); // Don't normalize
	glUniform3fv(glGetUniformLocation(program, "dir_light.ambient"), 1, (float*)&dir_light.ambient);
	glUniform3fv(glGetUniformLocation(program, "dir_light.diffuse"), 1, (float*)&dir_light.diffuse);
	glUniform3fv(glGetUniformLocation(program, "dir_light.specular"), 1, (float*)&dir_light.specular);
	gl_has_errors();
}

void RenderSystem::set_point_lights(const GLuint program) // Constant for all textured meshes
{
	//WorldLighting& world_lighting = registry.worldLightings.components[0]; // TODO

	int num_point_lights = (int)registry.pointLights.components.size();
	glUniform1i(glGetUniformLocation(program, "num_point_lights"), num_point_lights);

	// Update point light positions:
	for (int i = 0; i < num_point_lights; i++) {
		PointLight& point_light = registry.pointLights.components[i];
		Motion& motion = registry.motions.get(registry.pointLights.entities[i]);
		vec3 offset_3D = vec3(0.0, motion.sprite_offset.y * motion.sprite_normal.z, -motion.sprite_offset.y * motion.sprite_normal.y);
		point_light.position = vec3(motion.position, 0.f) + offset_3D + point_light.offset_position;
	}

	// Update light_ssbo (which has already been bound to location 1)
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_ssbo); // Set implicit buffer to be light_ssbo, so we can update it line below
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(PointLight) * num_point_lights, registry.pointLights.components.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Cleanup
	gl_has_errors();
}

void RenderSystem::set_vbo_and_ibo(const GLuint program, GEOMETRY_ID geometry_id, int MAX_INSTANCES_VBO_IBO) {
	// Setting vertex (vbo) and index (ibo) buffers
	//GLuint geometry = (is_shadow) ? (GLuint)GEOMETRY_ID::SUBDIVIDED_SQUARE : (GLuint)geometry_id; // Testing
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[(GLuint)geometry_id]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffers[(GLuint)geometry_id]);
	// Setting vertex attributes: position and UV texcoord
	GLint in_position_loc = 0;
	glEnableVertexAttribArray(in_position_loc);
	glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)0); // '3' for vec3

	GLint in_texcoord_loc = 1;
	glEnableVertexAttribArray(in_texcoord_loc);
	glVertexAttribPointer(in_texcoord_loc, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)sizeof(vec3)); // '2' for vec2
	// Note the offset (last argument) to skip the preceeding vertex position. The sizeof(TexturedVertex) part gives us stride between each attribute

	GLint in_normal_loc = 2;
	glEnableVertexAttribArray(in_normal_loc);
	glVertexAttribPointer(in_normal_loc, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)(sizeof(vec3)+ sizeof(vec2))); // '2' for vec2
	// Offset in last argument is for both position and texcoord

	// TODO: Test if this is necessary
	if (MAX_INSTANCES_VBO_IBO != INT_MAX) { // See .hpp - If we are given an actual param for MAX_INSTANCES_VBO_IBO, then we are batching
		GLint vertices_size = 0;
		glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vertices_size);
		GLsizei vertices_per_instance = (vertices_size / sizeof(TexturedVertex)) / MAX_INSTANCES_VBO_IBO;
		glUniform1i(glGetUniformLocation(program, "vertices_per_instance"), vertices_per_instance);
	}
	gl_has_errors();
}

mat3 RenderSystem::calc_shadow_transform(const Motion& motion, vec3 sprite_normal, bool is_shadow)
{
	//Camera& camera = registry.cameras.components[0];
	Transform transform;
	transform.translate(motion.position); // Finally, move it to it's position in the world
	// Note: The scale below does nothing to entities with normal pointing straight up, the dot() is == 1 (so shadows don't need this)
	//transform.scale(vec2(1.f, dot(camera.direction, sprite_normal) / camera.scale_factor.y)); // Resize entity based on camera tilt
	if (is_shadow) {
		WorldLighting& world_lighting = registry.worldLightings.components[0];
		DirLight& dir_light = registry.dirLights.get(world_lighting.dir_light);

		vec3 dir_light_3D_position = vec3(0.0) + dir_light.direction * 1000.f;
		//float angle = ;

		transform.rotate(world_lighting.theta - M_PI / 2.f); // Rotate to opposite side of light
		if (motion.sprite_normal.z < 0.95f) {
			transform.scale(vec2(1.f, tan(world_lighting.phi))); // Vertically scale shadow based on sun's height
		} else {
			transform.translate(vec2(0.f, motion.sprite_offset.y * tan(world_lighting.phi))); // Offset by angle of sun with ground
			transform.scale(vec2(1.f, abs(tan(world_lighting.phi)) / 30.f + 1.f)); // Scale based on sun's height
			transform.rotate(-(world_lighting.theta - M_PI / 2.f)); // Pre rotate so when we rotate again it will have original rotation
			transform.translate(-motion.sprite_offset); // Remove the incorrect offsetting that happens below
		}
	}
	transform.translate(motion.sprite_offset); // Offset in the plane of the sprite_normal
	transform.rotate(motion.angle);
	transform.scale(motion.scale);
	return transform.mat;
}

mat3 RenderSystem::calc_TBN(vec3 sprite_normal)
{
	// TBN matrix construction: (first construct tangent vector given normal and bitangent):
	vec3 tangent = { 1.f,0.f,0.f }; // assumes all sprites rotate about the x axis (normal doesn't point in x or -x direction at all)
	vec3 bitangent = cross(sprite_normal, tangent);
	return mat3(tangent, -bitangent, sprite_normal); // TBN allows us to convert normals from tangent space to world space
}

mat4 RenderSystem::calc_model_matrix(const Motion& motion, vec3 sprite_normal, const mat3& transform, const mat3& TBN)
{
	mat4 model_matrix;
	if (sprite_normal.z < 0.95f) {
		vec3 offset_3D = vec3(0.0, motion.sprite_offset.y * sprite_normal.z, -motion.sprite_offset.y * sprite_normal.y);
		vec3 world_3D_position = vec3(motion.position, 0.f) + offset_3D;
		mat3 TBN_scaled = TBN * mat3({ motion.scale.x,0,0 }, { 0,motion.scale.y,0 }, { 0,0,1 });
		return mat4(vec4(TBN_scaled[0], 0.f), vec4(-TBN_scaled[1], 0.f), vec4(TBN_scaled[2], 0.f), vec4(world_3D_position, 1.f));
	}
	else {
		return mat4(vec4(transform[0],0.f), vec4(transform[1],0.f), vec4(0.f,0.f,1.f,0.f), vec4(transform[2].x,transform[2].y,0.f,1.f));
	}
}

float RenderSystem::calc_depth(const Motion& motion) // Note: max map_size.y must be < ~9900
{
	return -(motion.position.y + 1000) / (10000) + 1.f; // maps to [0,1]
}

double batch_elapsed = 0; double shadow_elapsed = 0; double sort_elapsed = 0;
int frame_num2 = 100;
void RenderSystem::drawBatchFlush(int MAX_INSTANCES_VBO_IBO)
{
	// assert(num_instances > 0);
	if (num_instances <= 0) return;

	gl_has_errors();
	// First update the instance ssbo (which has already been bound to location 3)
	// TODO: Try binding this only once before all calls?
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, instance_ssbo); // Set implicit buffer to be instance_ssbo, so we can update it line below
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(InstanceData) * num_instances, &instance_data);

	// Get number of indices from index buffer, which has elements uint16_t
	GLint indices_size = 0;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indices_size);
	GLsizei indices_per_instance = (indices_size / sizeof(uint16_t)) / MAX_INSTANCES_VBO_IBO;
	gl_has_errors();
	// Then draw all the instances
	glDrawElements(GL_TRIANGLES, indices_per_instance * num_instances, GL_UNSIGNED_SHORT, nullptr);
	num_instances = 0; num_textures = 0; map_texture_index.clear();
}

int RenderSystem::get_texture_index(GLuint texture_gl_handle)
{
	if (map_texture_index.count(texture_gl_handle) > 0) {
		return map_texture_index[texture_gl_handle];
	} else {
		glActiveTexture(GL_TEXTURE0 + num_textures); // Set the texture unit num_textures
		glBindTexture(GL_TEXTURE_2D, texture_gl_handle); // Bind the given texture handle
		map_texture_index[texture_gl_handle] = num_textures;
		num_textures++;
		return num_textures - 1;
	}
	gl_has_errors();
}

int DIFFUSE = (int)TEXTURE_TYPE::DIFFUSE;
int NORMAL = (int)TEXTURE_TYPE::NORMAL;
void RenderSystem::get_texture_indices(int texture_indices[4], const RenderRequest& render_request, bool is_shadow)
{
	if (!is_shadow) {
		texture_indices[0] = get_texture_index(texture_gl_handles[DIFFUSE][(GLuint)render_request.diffuse_id]);
		texture_indices[1] = get_texture_index(texture_gl_handles[NORMAL][(GLuint)render_request.normal_id]);
		texture_indices[2] = (render_request.normal_add_id != NORMAL_ID::NORMAL_COUNT)
			? get_texture_index(texture_gl_handles[NORMAL][(GLuint)render_request.normal_add_id]) : texture_indices[1];
		texture_indices[3] = (render_request.mask_id != DIFFUSE_ID::DIFFUSE_COUNT)
			? get_texture_index(texture_gl_handles[DIFFUSE][(GLuint)render_request.mask_id]) : texture_indices[0];
	} else {
		texture_indices[3] = (render_request.mask_id != DIFFUSE_ID::DIFFUSE_COUNT)
			? get_texture_index(texture_gl_handles[DIFFUSE][(GLuint)render_request.mask_id])
			: get_texture_index(texture_gl_handles[DIFFUSE][(GLuint)render_request.diffuse_id]);
	}
}

// Adds entity to batch and if max texture slots reached or max instances reached, flush batch and start from zero and repeat
void RenderSystem::addToBatch(Entity entity, RenderRequest& render_request, bool is_shadow, int MAX_INSTANCES_VBO_IBO)
{
	const Motion& motion = registry.motions.get(entity);
	
	int num_lights_affecting = render_request.num_lights_affecting;
	render_request.num_lights_affecting = 0; // Must reset this before culling

	// Frustum culling to avoid calculating matrices for entities outside of view space:
	if (motion.is_culled) return; // don't add entities outside of the frustum to the batch

	int texture_indices[4];
	get_texture_indices(texture_indices, render_request, is_shadow); // Try adding textures but if ends up past 32, then need to flush below

	if (num_textures >= MAX_TEXTURE_UNITS || num_instances >= MAX_INSTANCES_VBO_IBO - 1) {
		drawBatchFlush(MAX_INSTANCES_VBO_IBO); // Draw all entities currently in the batch, all in one draw call
		num_instances = 0; num_textures = 0; map_texture_index.clear();
		get_texture_indices(texture_indices, render_request, is_shadow); // Need to get again since they've been flushed
	}
	num_instances++; // Must remain below the above check otherwise will be writing to instance_data[-1]
	InstanceData& instance = instance_data[num_instances - 1];

	instance.entity_id = (float)entity;

	//Transform transform;
	//transform.rotate(motion.angle);
	//transform.scale(motion.scale);
	//instance.shadow_transform = transform.mat;

	instance.wind_strength = render_request.wind_affected * 10.f;

	memcpy(instance.texture_indices, texture_indices, 16); // 4 ints * 4 bytes/int = 16 bytes
	instance.mask_coord_loc = render_request.mask_coord_loc;

	if (is_shadow) {
		instance.transform = calc_shadow_transform(motion, vec3(0,0,1), is_shadow);
		instance.rotation = 100.f;
		instance.sprite_normal = vec3(0, 0, 1);
	}
	else { // Shadows don't need this stuff:
		if (!render_request.is_TBN_updated) {
			render_request.TBN = calc_TBN(motion.sprite_normal);
			render_request.is_TBN_updated = true;
		}
		instance.TBN = render_request.TBN;

		instance.position = motion.position;
		instance.scale = motion.scale;
		instance.sprite_offset = motion.sprite_offset;
		instance.sprite_normal = motion.sprite_normal;
		instance.rotation = motion.angle;

		instance.extrude_size = render_request.extrude_size;

		instance.diffuse_coord_loc = render_request.diffuse_coord_loc;
		instance.normal_coord_loc = render_request.normal_coord_loc;
		instance.normal_add_coord_loc = render_request.normal_add_coord_loc;

		instance.add_color = render_request.add_color;
		instance.multiply_color = render_request.multiply_color;
		instance.specular = render_request.specular;
		instance.shininess = render_request.shininess;
		instance.transparency = render_request.transparency;

		instance.num_lights_affecting = 1;
		instance.lights_affecting[0] = 0;

		//assert(render_request.num_lights_affecting <= 1);
		//instance.num_lights_affecting = render_request.num_lights_affecting;
		//memcpy(instance.lights_affecting, render_request.lights_affecting, sizeof(instance.lights_affecting)); // 16 ints * 4 bytes/int = 64 bytes
	}
}

void RenderSystem::drawTexturedSprites(bool is_shadow)
{
	const GLuint program = (GLuint)effects[(GLuint)EFFECT_ID::TEXTURED];
	glUniform1f(glGetUniformLocation(program, "is_ground_piece"), false);
	glUniform1f(glGetUniformLocation(program, "is_shadow"), is_shadow);
	(is_shadow) ? (glDisable(GL_DEPTH_TEST), glEnable(GL_BLEND)) : (glEnable(GL_DEPTH_TEST), glDisable(GL_BLEND));
	// num_instances = 0; num_textures = 0; map_texture_index.clear();
	for (size_t i = 0; i < registry.renderRequests.entities.size(); i++)
	{
		Entity entity = registry.renderRequests.entities[i];
		RenderRequest& render_request = registry.renderRequests.components[i];
		if (is_shadow && render_request.geometry_id == GEOMETRY_ID::PLANE) {
			addToBatch(entity, render_request, is_shadow, MAX_SPRITE_INSTANCES);
		}
		if (render_request.effect_id != EFFECT_ID::TEXTURED || render_request.is_ground_piece
			|| (is_shadow && !render_request.casts_shadow)) { continue; }

		addToBatch(entity, render_request, is_shadow, MAX_SPRITE_INSTANCES);
	}
	if (num_instances > 0) { drawBatchFlush(MAX_SPRITE_INSTANCES); } // Necessary to draw possible final batch of instances
	gl_has_errors();
}

Entity complex_meshes[100] = {};
void RenderSystem::drawGroundPieces()
{
	const GLuint program = (GLuint)effects[(GLuint)EFFECT_ID::TEXTURED];
	glUniform1f(glGetUniformLocation(program, "is_ground_piece"), true);
	int num_complex_meshes = 0;
	glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
	num_instances = 0; num_textures = 0; map_texture_index.clear();
	for (size_t i = 0; i < registry.groundPieces.entities.size(); i++)
	{
		Entity entity = registry.groundPieces.entities[i];
		RenderRequest& render_request = registry.renderRequests.get(entity);
		assert(render_request.effect_id == EFFECT_ID::TEXTURED && render_request.is_ground_piece);

		if (render_request.geometry_id != GEOMETRY_ID::SPRITE) { 
			complex_meshes[num_complex_meshes++] = entity; // TODO: Sort by geometry ID?
			continue;
		}

		addToBatch(entity, render_request, false, MAX_SPRITE_INSTANCES);
	}
	if (num_instances > 0) { drawBatchFlush(MAX_SPRITE_INSTANCES); } // Necessary to draw possible final batch of instances

	// Now draw all complex meshes:
	if (num_complex_meshes > 0) {
		GEOMETRY_ID curr_geometry_id = GEOMETRY_ID::GEOMETRY_COUNT;
		num_instances = 0; num_textures = 0; map_texture_index.clear();
		for (size_t i = 0; i < num_complex_meshes; i++) {
			Entity entity = complex_meshes[i];
			RenderRequest& render_request = registry.renderRequests.get(entity);

			if (render_request.geometry_id != curr_geometry_id) {
				if (i != 0) { drawBatchFlush(MAX_MESH_INSTANCES); }
				num_instances = 0; num_textures = 0; map_texture_index.clear();

				set_vbo_and_ibo(program, render_request.geometry_id, MAX_MESH_INSTANCES); // Change vbo and ibo to new geometry type
				curr_geometry_id = render_request.geometry_id;
			}
			addToBatch(entity, render_request, false, MAX_MESH_INSTANCES);
		}
		if (num_instances > 0) { drawBatchFlush(MAX_MESH_INSTANCES); } // Necessary to draw possible final batch of instances
		set_vbo_and_ibo(program, GEOMETRY_ID::SPRITE, MAX_SPRITE_INSTANCES); // Reset vbo and ibo back to sprites (hacky)
	}
	gl_has_errors();
}

void RenderSystem::drawDebugComponents()
{
	const GLuint program = (GLuint)effects[(GLuint)EFFECT_ID::TEXTURED];
	glUniform1f(glGetUniformLocation(program, "is_ground_piece"), true);
	glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
	num_instances = 0; num_textures = 0; map_texture_index.clear();
	for (size_t i = 0; i < registry.debugComponents.entities.size(); i++)
	{
		Entity entity = registry.debugComponents.entities[i];
		RenderRequest& render_request = registry.renderRequests.get(entity);
		assert(render_request.effect_id == EFFECT_ID::TEXTURED);

		addToBatch(entity, render_request, false, MAX_SPRITE_INSTANCES);
	}
	if (num_instances > 0) { drawBatchFlush(MAX_SPRITE_INSTANCES); } // Necessary to draw possible final batch of instances
}

void RenderSystem::setup_textured_drawing()
{
	const GLuint program = (GLuint)effects[(GLuint)EFFECT_ID::TEXTURED];
	glUseProgram(program);
	// Setting constants:
	int samplers[32]; for (int i = 0; i < 32; i++) samplers[i] = i;
	glUniform1iv(glGetUniformLocation(program, "textures"), 32, samplers);
	glUniform1f(glGetUniformLocation(program, "time"), (float)(glfwGetTime() * 10.0f));
	glUniformMatrix3fv(glGetUniformLocation(program, "projection_matrix"), 1, GL_FALSE, (float*)&createProjectionMatrix());
	ScreenState& screen = registry.screenStates.components[0];
	glUniform1f(glGetUniformLocation(program, "darken_screen_factor"), screen.darken_screen_factor);
	glUniform2fv(glGetUniformLocation(program, "window_size"), 1, (float*)&vec2(screen.window_width_px, screen.window_height_px));
	Player& player = registry.players.components[0];
	glUniform1f(glGetUniformLocation(program, "vignette_factor"), clamp((player.invulnerable_timer + 700.f)/2000.f, 0.f, 1.f));
	//glUniform2fv(glGetUniformLocation(program, "player_position"), 1, (float*)&registry.motions.get(registry.players.entities[0]).position);
	Camera& camera = registry.cameras.components[0];
	glUniform1f(glGetUniformLocation(program, "camera_scale_factor_y"), camera.scale_factor.y);
	glUniform3fv(glGetUniformLocation(program, "camera_direction"), 1, (float*)&camera.direction);
	set_dir_light_and_view(program);
	set_point_lights(program);
	set_vbo_and_ibo(program, GEOMETRY_ID::SPRITE, MAX_SPRITE_INSTANCES);
	gl_has_errors();
}

void RenderSystem::drawUI(Entity entity)
{
	Motion& motion = registry.motions.get(entity);
	Transform transform;
	transform.translate(motion.position);
	transform.rotate(motion.angle);
	transform.scale(motion.scale);

	assert(registry.renderRequests.has(entity));
	const RenderRequest& render_request = registry.renderRequests.get(entity);

	const GLuint effect_id_enum = (GLuint)render_request.effect_id;
	assert(effect_id_enum != (GLuint)EFFECT_ID::EFFECT_COUNT);
	const GLuint program = (GLuint)effects[effect_id_enum];

	// Setting shaders
	glUseProgram(program);
	gl_has_errors();

	// Input data location as in the vertex buffer
	if (render_request.effect_id == EFFECT_ID::UI_ELEMENTS)
	{
		assert(render_request.geometry_id == GEOMETRY_ID::SPRITE);
		set_vbo_and_ibo(program, render_request.geometry_id);

		// Enabling and binding texture to slot 0
		glActiveTexture(GL_TEXTURE0);
		gl_has_errors();

		// setup uniforms for ui elements
		auto& ui = registry.ui_elements.get(entity);
		glUniform1i(glGetUniformLocation(program, "icon_enabled"), ui.icon_enabled);
		glUniform1i(glGetUniformLocation(program, "sprite_sheet_num"), ui.sprite_sheet_num);
		glUniform1i(glGetUniformLocation(program, "glow"), ui.is_glow);
		gl_has_errors();

		assert(registry.renderRequests.has(entity));
		GLuint DIFFUSE_ID = texture_gl_handles[(int)TEXTURE_TYPE::DIFFUSE][(GLuint)registry.renderRequests.get(entity).diffuse_id];

		glBindTexture(GL_TEXTURE_2D, DIFFUSE_ID);
		gl_has_errors();
	}
	else if (render_request.effect_id == EFFECT_ID::HEALTH_BAR)
	{
		// Can't (and don't need to) use set_vbo_and_ibo here because it uses the old vertex type
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[(GLuint)render_request.geometry_id]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffers[(GLuint)render_request.geometry_id]);
		GLint in_position_loc = glGetAttribLocation(program, "in_position");
		GLint in_color_loc = glGetAttribLocation(program, "in_color");
		glEnableVertexAttribArray(in_position_loc);
		glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(ColoredVertex), (void*)0);
		glEnableVertexAttribArray(in_color_loc);
		glVertexAttribPointer(in_color_loc, 3, GL_FLOAT, GL_FALSE, sizeof(ColoredVertex), (void*)sizeof(vec3));
		gl_has_errors();

		glUniform1f(glGetUniformLocation(program, "current_hp"), (float) registry.healthBars.get(entity).current_hp);
		glUniform1f(glGetUniformLocation(program, "max_hp"), (float) registry.healthBars.get(entity).max_hp);
		glUniform1i(glGetUniformLocation(program, "is_primary"), registry.healthBars.get(entity).is_primary);
		glUniform1f(glGetUniformLocation(program, "time"), (float)(glfwGetTime() * 1000.f));
		gl_has_errors();
	}

	// Getting uniform locations for glUniform* calls
	glUniform3fv(glGetUniformLocation(program, "add_color"), 1, (float*)&render_request.add_color);
	glUniformMatrix3fv(glGetUniformLocation(program, "transform"), 1, GL_FALSE, (float*)&transform.mat);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr); // the 6 == num_indices, which is always 6 for UI
	gl_has_errors();
}

void RenderSystem::drawColor(DIFFUSE_ID base_texture, vec4 color, vec2 pos, vec2 scale) {
	// Setting shaders
	const GLuint program = (GLuint)effects[(GLuint)EFFECT_ID::COLOR];
	glUseProgram(program);
	gl_has_errors();

	set_vbo_and_ibo(program, GEOMETRY_ID::SPRITE);

	glActiveTexture(GL_TEXTURE0); // Enabling and binding texture to slot 0
	glBindTexture(GL_TEXTURE_2D, texture_gl_handles[(int)TEXTURE_TYPE::DIFFUSE][(int)base_texture]);
	gl_has_errors();

	Transform transform;
	transform.translate(pos);
	transform.scale(scale);

	// Setting uniform values to the currently bound program
	glUniformMatrix3fv(glGetUniformLocation(program, "transform"), 1, GL_FALSE, (float*)&transform.mat);
	glUniform4fv(glGetUniformLocation(program, "set_color"), 1, (float*)&color);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr); // the 6 == num_indices, which is always 6 for colored
	gl_has_errors();
}

void RenderSystem::updateText(Text& text)
{
	// Get text/font info
	float char_size_multiplier = text.size_multiplier;
	FontInfo& font_info = fonts[text.font];
	size_t n_chars = text.content.size();

	// Lazily calculate text offsets
	if (text.update_render_data) {
		text.update_render_data = false;

		// Contains scale and offset (from start position) for each character
		std::vector<vec2> character_scalings(n_chars);
		std::vector<vec2> position_offsets(n_chars);

		int newline_idx = 0; // Index counter used to insert newlines
		float max_width = 0; // Keeps the largest x offset in the text. Used to determine the final width of the text
		
		// Calculate the screen-space height of a character (All character have same height)
		auto [window_width, window_height, _] = registry.screenStates.components[0];
		float base_char_height = fonts[text.font].characters[text.content[0]].px_height / (float)window_height;
		float char_height = abs(base_char_height * inverse_aspect_ratio_2.y * text.size_multiplier);

		vec2 curr_position_offset = {0, 0}; // Keeps the cumulative offset of the characters
		// Calculate position and scaling of each character
		for (int i = 0; i < text.content.size(); i++) {
			// Apply newlines
			while (text.newlines.size() > newline_idx && text.newlines[newline_idx] == i) {
				curr_position_offset = {0, curr_position_offset.y - char_height * text.newline_height};
				++newline_idx;
			}
			const char c = text.content[i];

			CharacterInfo& char_info = font_info.characters[c];

			vec2 character_scaling ={(float)char_info.px_width / window_width, (float)char_info.px_height / window_height};
			character_scaling *= inverse_aspect_ratio_2; // Prevent stretching
			character_scaling *= char_size_multiplier; // Make twice as large

			// Move position right by half of this character's width, if it's not the first character of the line.
			if (curr_position_offset.x != 0) {
				curr_position_offset.x += character_scaling.x/2.f;
			}

			character_scalings[i] = character_scaling;
			position_offsets[i] = curr_position_offset;

			max_width = max(max_width, curr_position_offset.x + character_scaling.x/2.f);

			// Move subsequent characters right by half this character's width, plus a static spacing between characters
			curr_position_offset.x += character_scaling.x/2.f + char_size_multiplier/window_width;
		}
		text.text_bounds = {
			text.start_position.x - character_scalings[0].x/2.f,
			text.start_position.x + max_width,
			text.start_position.y + char_height/2.f,
			(text.start_position.y + position_offsets.back().y) - char_height/2.f
		};
		
		if (text.perform_repositioning) {
			// Adjust text position so box corner will appear under cursor
			vec2 corner_adjust = {
				character_scalings[0].x/2.f + text.box_margin.x,
				text.start_position.y - text.text_bounds.y_high + text.box_margin.y
			};
			text.start_position += corner_adjust;
			text.text_bounds = text.text_bounds + corner_adjust;

			// Adjust text position so it always tends toward center of screen;
			vec2 center_adjust = {0, 0};
			if (text.start_position.x > 0 ) {
				center_adjust.x = (text.text_bounds.x_high - text.text_bounds.x_low) + 2*text.box_margin.x;
			}
			if (text.start_position.y > 0 ) {
				center_adjust.y = (text.text_bounds.y_low - text.text_bounds.y_high) + 2*text.box_margin.y;
			}
			text.start_position -= center_adjust;
			text.text_bounds = text.text_bounds - center_adjust;
		}

		mat3 identity = mat3(1.0);
		std::vector<mat3> transforms(n_chars);
		std::vector<vec4> charinfos(n_chars);

		// Fill the buffers that will be copied into the VBOs
		for (int i = 0; i < n_chars; i++) {
			const char c = text.content[i];
			CharacterInfo& char_info = font_info.characters[c];

			Transform transform;
			transform.translate(text.start_position + position_offsets[i]);
			transform.scale(character_scalings[i]);
			
			transforms[i] = transform.mat;
			charinfos[i] = *((vec4 *) &char_info);
		}
		// Bind and fill VBOs. Reuse buffers if possible. Issues may arise if text is added and create_new_buffers=false
		// TODO: Make charinfo VBO actually be STATIC_DRAW by not updating it each time the text's position changes
		if (text.create_new_buffers) {
			glBindBuffer(GL_ARRAY_BUFFER, text.charinfoVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * n_chars, &charinfos[0], GL_STATIC_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, text.transformVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(mat3) * n_chars, &transforms[0], GL_DYNAMIC_DRAW);
			text.create_new_buffers = false;
			gl_has_errors();
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, text.charinfoVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec4) * n_chars, &charinfos[0]);

			glBindBuffer(GL_ARRAY_BUFFER, text.transformVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(mat3) * n_chars, &transforms[0]);
			gl_has_errors();
		}
	}
}

void RenderSystem::drawText(Entity, Text& text)
{	
	// Get text/font info
	FontInfo& font_info = fonts[text.font];
	size_t n_chars = text.content.size();

	// Setting shaders
	const GLuint program = (GLuint)effects[(GLuint)EFFECT_ID::TEXT];
	glUseProgram(program);
	gl_has_errors();
	
	set_vbo_and_ibo(program, GEOMETRY_ID::SPRITE);

	// Bind and set character info VBO
	glBindBuffer(GL_ARRAY_BUFFER, text.charinfoVBO);
	GLint in_charinfo_loc = 3;
	glEnableVertexAttribArray(in_charinfo_loc);
	glVertexAttribPointer(in_charinfo_loc, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void*)0);
	glVertexAttribDivisor(in_charinfo_loc, 1);

	// Bind and set character transforms VBO
	glBindBuffer(GL_ARRAY_BUFFER, text.transformVBO);
	GLint in_transform_loc = 4;
	for (int i = 0; i < 3; i++) {
		glEnableVertexAttribArray(in_transform_loc + i);
		glVertexAttribPointer(in_transform_loc + i, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(vec3), (void*)(i * sizeof(vec3)));
		glVertexAttribDivisor(in_transform_loc + i, 1);
	}

	// Enabling and binding texture to slot 0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_gl_handles[(int)TEXTURE_TYPE::DIFFUSE][(int)font_info.texture_id]);
	gl_has_errors();

	// Uniforms
	glUniform4fv(glGetUniformLocation(program, "text_color"), 1, (float*)&text.text_color);
	
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, n_chars); // the 6 == num_indices. Always 6 for text quads
	gl_has_errors();
}

void RenderSystem::drawToScreen()
{
	// Setting shaders
	// get the wind texture, sprite mesh, and program
	const GLuint screen_program = effects[(GLuint)EFFECT_ID::SCREEN];
	glUseProgram(screen_program);
	gl_has_errors();
	// Clearing backbuffer
	int w, h;
	glfwGetFramebufferSize(window, &w, &h); // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, w, h);
	glDepthRange(0, 10);
	glClearColor(1.f, 0, 0, 1.0);
	glClearDepth(1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl_has_errors();
	// Enabling alpha channel for textures
	glDisable(GL_BLEND);
	// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	// Draw the screen texture on the quad geometry
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[(GLuint)GEOMETRY_ID::SCREEN_TRIANGLE]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffers[(GLuint)GEOMETRY_ID::SCREEN_TRIANGLE]);
	// Note, GL_ELEMENT_ARRAY_BUFFER associates indices to the bound GL_ARRAY_BUFFER
	gl_has_errors();
	// Set clock
	glUniform1f(glGetUniformLocation(screen_program, "time"), (float)(glfwGetTime() * 10.0f));
	ScreenState& screen = registry.screenStates.components[0];
	glUniform1f(glGetUniformLocation(screen_program, "darken_screen_factor"), screen.darken_screen_factor);
	gl_has_errors();
	// Set the vertex position and vertex texture coordinates (both stored in the
	// same VBO)
	GLint in_position_loc = glGetAttribLocation(screen_program, "in_position");
	glEnableVertexAttribArray(in_position_loc);
	glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void *)0);
	gl_has_errors();

	// Bind our texture in Texture Unit 0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, off_screen_render_buffer_color);
	gl_has_errors();
	// Draw
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, nullptr); // one triangle = 3 vertices; nullptr indicates that there is
																 // no offset from the bound index buffer
	gl_has_errors();
}


// Render our game world
// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
void RenderSystem::draw(GameState game_state)
{
	// Getting size of window
	int w, h;
	glfwGetFramebufferSize(window, &w, &h); // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays

	// First render to the custom framebuffer
	//glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	gl_has_errors();
	// Clearing backbuffer
	glViewport(0, 0, w, h);
	glDepthRange(0.00001, 10);
	glClearColor(0.6f, 0.8f, 0.3f, 1.f);
	glClearDepth(10.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_has_errors();

	if (game_state == GameState::IN_GAME || game_state == GameState::GAME_FROZEN) {
		frame_num2++;
		Room& room = registry.rooms.components[0];
		// Sort on room initilization and every now and then to keep batching clean
		if (!room.is_render_updated || frame_num2 % 10000 == 0) {
			room.is_render_updated = true;
			auto sort_start = Clock::now();
			registry.renderRequests.sort([](Entity e1, Entity e2) {
				return (int)registry.renderRequests.get(e1).diffuse_id < (int)registry.renderRequests.get(e2).diffuse_id;
			});
			sort_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - sort_start)).count() / 1000;
			printf("Sort Elapsed:	%fms\n", sort_elapsed); sort_elapsed = 0;
		}

		setup_textured_drawing();
		drawGroundPieces(); // Draw ground pieces

		if (debugging.in_debug_mode) {
			drawDebugComponents();
		}

		auto shadow_start = Clock::now();
		drawTexturedSprites(true); // Draw all shadows
		shadow_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - shadow_start)).count() / 1000;

		auto batch_start = Clock::now();
		drawTexturedSprites(false); // Draw textured sprites
		batch_elapsed += (double)(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - batch_start)).count() / 1000;
		if (frame_num2 % 200 == 0) {
			printf("  Shadow Elapsed Avg:	%fms\n", shadow_elapsed / 200.f); shadow_elapsed = 0;
			printf("  Batch Elapsed Avg:	%fms\n", batch_elapsed / 200.f); batch_elapsed = 0;
		}
	}
	// Draw all ui elements
	glDisable(GL_DEPTH_TEST); // Don't depth test for UI
	glEnable(GL_BLEND); // Allow alpha blending for UI for transparency
	for (size_t i = 0; i < registry.ui_elements.entities.size(); i++)
	{
		Entity entity = registry.ui_elements.entities[i];
		UI_Element ui_elem = registry.ui_elements.components[i];
		if (!ui_elem.visible) // || !registry.motions.has(entity) - Unnecessary since it should break anyways without this
			continue;
		drawUI(entity);
	}

	// Draw all text
	using Clock = std::chrono::high_resolution_clock;
	auto t0 = Clock::now();
	// glEnable(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (size_t i = 0; i < registry.texts.entities.size(); i++)
	{
		Entity entity = registry.texts.entities[i];
		Text& text = registry.texts.components[i];
		if (!text.is_visible) continue;

		updateText(text);
		
		// Draw semi-transparent black box behind text
		if (text.is_box_enabled) {
			vec2 start_pos = {text.text_bounds.x_low, text.text_bounds.y_low};
			vec2 end_pos = {text.text_bounds.x_high, text.text_bounds.y_high};

			start_pos += text.box_margin * vec2{-1, 1};
			end_pos += text.box_margin * vec2{1, -1};

			vec2 extent = end_pos - start_pos;

			drawColor(
				DIFFUSE_ID::WHITE,
				text.box_color,
				start_pos + (extent)/2.f, // middle of text
				(extent)
			);
		}

		drawText(entity, text);
	}
	auto t1 = Clock::now();
	float dt = (float)(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)).count() / 1000;
	static int print_count;
	if (print_count++ % 20 == 0) {
		//printf("text rendering took: %.2fms\n", dt);
	}

	// Truly render to the screen
	//drawToScreen();

	// flicker-free display with a double buffer
	glfwSwapBuffers(window);
	gl_has_errors();
}

mat3 RenderSystem::createProjectionMatrix()
{
	Camera& camera = registry.cameras.components[0];
	ScreenState& screen = registry.screenStates.components[0];
	//printf("%d, %d\n", screen.window_width_px, screen.window_height_px);
	//camera.zoom = screen.window_height_px/400;

	// 2D orthogonal projection matrix. Zoom scales with zoom and vertical size scales with camera tilt
	float left = 0.f;
	float top = 0.f;

	gl_has_errors();
	float right = (float) window_width_px;
	float bottom = (float) window_height_px;

	float sx = camera.zoom / (right - left);
	float sy = camera.scale_factor.y * (camera.zoom / (top - bottom)); // Scale the world based on camera tilt
	float tx = -(camera.zoom * camera.position.x) / (right - left);
	float ty = camera.scale_factor.y * ((camera.zoom * camera.position.y) / bottom);
	return { {sx, 0.f, 0.f}, {0.f, sy, 0.f}, {tx, ty, 1.f} };
}