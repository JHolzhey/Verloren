// internal
#include "render_system.hpp"

#include <array>
#include <fstream>

#include "../ext/stb_image/stb_image.h"

// This creates circular header inclusion, that is quite bad.
#include "tiny_ecs_registry.hpp"

// stlib
#include <iostream>
#include <sstream>

// World initialization
bool RenderSystem::init(GLFWwindow* window_arg)
{
	this->window = window_arg;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // vsync

	// Load OpenGL function pointers
	const int is_fine = gl3w_init();
	assert(is_fine == 0);

	// Create a frame buffer
	frame_buffer = 0;
	glGenFramebuffers(1, &frame_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer);
	gl_has_errors();

	// For some high DPI displays (ex. Retina Display on Macbooks)
	// https://stackoverflow.com/questions/36672935/why-retina-screen-coordinate-value-is-twice-the-value-of-pixel-value
	int frame_buffer_width_px, frame_buffer_height_px;
	glfwGetFramebufferSize(window, &frame_buffer_width_px, &frame_buffer_height_px);  // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays
	if (frame_buffer_width_px != window_width_px)
	{
		printf("WARNING: retina display! https://stackoverflow.com/questions/36672935/why-retina-screen-coordinate-value-is-twice-the-value-of-pixel-value\n");
		printf("glfwGetFramebufferSize = %d,%d\n", frame_buffer_width_px, frame_buffer_height_px);
		printf("window width_height = %d,%d\n", window_width_px, window_height_px);
	}

	// Hint: Ask your TA for how to setup pretty OpenGL error callbacks. 
	// This can not be done in mac os, so do not enable
	// it unless you are on Linux or Windows. You will need to change the window creation
	// code to use OpenGL 4.3 (not suported on mac) and add additional .h and .cpp
	// glDebugMessageCallback((GLDEBUGPROC)errorCallback, nullptr);

	// We are not really using VAO's but without at least one bound we will crash in
	// some systems.
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	gl_has_errors();

	initScreenTexture();
    initializeGlTextures();
	initializeGlEffects();
	initializeShaderStorageBuffers();
	initializeGlGeometryBuffers();

	this->fonts = {
		{FONT::SYLFAEN, FontInfo{
			DIFFUSE_ID::FONT_SYLFAEN,
			ivec2{512, 256},
			32,
			{}
		}},
		{FONT::YAHEI_THICC, FontInfo{
			DIFFUSE_ID::FONT_YAHEI_THICC,
			ivec2{512, 256},
			32,
			{}
		}},
		{FONT::SMALL_FONTS, FontInfo{
			DIFFUSE_ID::FONT_SMALL_FONTS,
			ivec2{512, 256},
			32,
			{}
		}},
		{FONT::VERDANA, FontInfo{
			DIFFUSE_ID::FONT_VERDANA,
			ivec2{512, 256},
			32,
			{}
		}},
		{FONT::VERDANA_BOLD, FontInfo{
			DIFFUSE_ID::FONT_VERDANA_BOLD,
			ivec2{512, 256},
			32,
			{}
		}},
	};
	assert(fonts.size() == (size_t) FONT::FONT_COUNT); // Triggers if a font is missing from the above map

	for (int i = 0; i < (int) FONT::FONT_COUNT; i++) {
		initializeFont((FONT) i);
	}

	return true;
}

void RenderSystem::initializeFont(FONT font)
{
	// Assumption: all font maps start at character 32 and contain at least the first 128 ASCII characters
	auto& font_info = fonts[font];
	auto& characters = font_info.characters;
	const int cs = font_info.cell_size; // Cell Size. Assume cells are always square
	
	// Load font image data
	const std::string& path = texture_paths[(int)TEXTURE_TYPE::DIFFUSE][(int)font_info.texture_id];
	ivec2& dimensions = texture_dimensions[(int)TEXTURE_TYPE::DIFFUSE][(int)font_info.texture_id];
	stbi_uc* data = stbi_load(path.c_str(), &dimensions.x, &dimensions.y, NULL, 4);
	if (data == NULL)
	{
		const std::string message = "Could not load the file " + path + ".";
		fprintf(stderr, "%s", message.c_str());
		assert(false);
	}
	// Make sure actual image dimensions match the hard-coded image dimensions
	assert(dimensions.x == font_info.image_shape.x);
	assert(dimensions.y == font_info.image_shape.y);

	// Configuration	
	int min_width = cs/12;
	int default_height = cs;
	int space_width_px = cs/4;
	int padding_px_threshold = 2;

	// Create default character info for each char in [32, 128]
	for (uint8_t i = 0; i < 96; i++) {
		ivec2 position = {(i * cs) % dimensions.x, ((i * cs) / dimensions.x) * cs};

		CharacterInfo char_info = {
			position.x/(float)dimensions.x,  // x position in image
			position.y/(float)dimensions.y,  // y position in image
			
			min_width/(float)dimensions.x,  // width in image (defaults to minimum character width)
			default_height/(float)dimensions.y,  // height in image

			min_width, // pixel width
			default_height,  // pixel height

			cs, // padding. Set to high value, will be updated for all non-empty chars
		};
		characters[(char) (i + 32)] = char_info;
	}

	// printf("font image dimensions: x: %d, y: %d\n", dimensions.x, dimensions.y);

	// Find width of each character
	// Iterate over all pixels, find owner, set width if wider
	int image_size = dimensions.x * dimensions.y;
	for (int i = 0; i < image_size; i++) {
		// Calculate the to which character's cell the pixel belongs
		ivec2 pixel_position = {i % dimensions.x, i / dimensions.x}; // (x,y) posiiton of the pixel in the font map
		auto char_offset = pixel_position/cs; // position in the cell grid
		int cell_num = char_offset.x + (char_offset.y * dimensions.x/cs);  // Number of cell, counting to the right and down
		char pixel_owner = (char) (32 + cell_num);

		// Each pixel has rgba data. Accessing data[i] only returns a single channel. So we access with a stride of 4
		// If pixel is not black (proxy by checking red != 0), set width if larger than current max
		if (data[i*4] != 0) {
			// printf("%d\n", (uint8_t) pixel_owner);
			int char_px_width = (pixel_position.x % cs);
			auto& char_info = characters[pixel_owner];
			
			// Store right-most "active" pixel of character
			if (char_info.px_width < char_px_width) {
				char_info.px_width = char_px_width;
				// printf("%c cell: %d\n", pixel_owner, pixel_position.x/cs);
				// printf("%c width found: %d\n", pixel_owner, char_px_width);
			}

			// Store left-most "active" pixel of character
			if (char_info.px_padding > char_px_width) {
				char_info.px_padding = char_px_width;
			}
		}
	}

	// Calculate actual width of the characters
	for (uint8_t i = 0; i < 96; i++) {
		char c = (char) (i + 32);
		auto& char_info = characters[c];
		
		if (char_info.px_padding >= padding_px_threshold) {
			char_info.px_width += char_info.px_padding;
		} else {
			char_info.px_width += 1;
		}
		char_info.width = char_info.px_width / (float) dimensions.x;

		// Print width of each character
		// printf("%c pixel width: %d, image width: %f\n", i + 32, char_info.px_width, char_info.width);
		if (font == FONT::SYLFAEN && (c == 'm' || c == 'a')) {
			printf("%c width_px: %d, px_padding: %d\n", c, char_info.px_width, char_info.px_padding);
		}
	}

	// Set width of space character manually
	auto& space_info = characters[' '];
	space_info.width = space_width_px / (float) dimensions.x;
	space_info.px_width = space_width_px;

	stbi_image_free(data);
}

void RenderSystem::initializeGlTextures() // Go through each of our texture_paths (in .hpp) and initialize them
{
	for (int type = 0; type < 2; type++) {
		glGenTextures((GLsizei)texture_type_sizes[type], texture_gl_handles[type].data());

		for (int i = 0; i < texture_type_sizes[type]; i++) // Use this size since texture arrays have empty space
		{
			const std::string& path = texture_paths[type][i];
			ivec2& dimensions = texture_dimensions[type][i];

			stbi_uc* data;
			data = stbi_load(path.c_str(), &dimensions.x, &dimensions.y, NULL, 4);
			if (data == NULL)
			{
				printf("type = %d  |  type size = %d  |  i = %d\n", type, texture_type_sizes[type], i);
				const std::string message = "Could not load the file " + path + ".";
				fprintf(stderr, "%s\n", message.c_str());
				assert(false);
			}
			glBindTexture(GL_TEXTURE_2D, texture_gl_handles[type][i]); // Create new texture given a gl 'name' (which is GLuint)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dimensions.x, dimensions.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data); // Fill in texture
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // Specify texture params
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // GL_NEAREST // GL_LINEAR
			gl_has_errors();
			stbi_image_free(data);
		}
		gl_has_errors();
	}
}

void RenderSystem::initializeGlEffects()
{
	for(uint i = 0; i < effect_paths.size(); i++)
	{
		const std::string vertex_shader_name = effect_paths[i] + ".vs.glsl";
		const std::string fragment_shader_name = effect_paths[i] + ".fs.glsl";

		bool is_valid = loadEffectFromFile(vertex_shader_name, fragment_shader_name, effects[i]);
		assert(is_valid && (GLuint)effects[i] != 0);
	}

	map_texture_index.reserve(50);
	// Get the max allowed texture units on any shader stage defined by hardware drivers:
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &MAX_TEXTURE_UNITS);
	MAX_TEXTURE_UNITS = min(MAX_TEXTURE_UNITS, 32);
	printf("MAX_TEXTURE_UNITS = %d\n", MAX_TEXTURE_UNITS);
	//GLint combined_texture_units = 0; glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &combined_texture_units);
	//printf("texture_units = %d\n", texture_units); printf("combined_texture_units = %d\n", combined_texture_units);
}

// One could merge the following two functions as a template function...
template <class T>
void RenderSystem::bindVBOandIBO(GEOMETRY_ID gid, std::vector<T> vertices, std::vector<uint16_t> indices)
{
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[(uint)gid]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
	gl_has_errors();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffers[(uint)gid]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(), GL_STATIC_DRAW);
	gl_has_errors();
}

void extend_vbo_and_ibo(std::vector<TexturedVertex>& vertices, std::vector<uint16_t>& indices, int num_instances)
{
	int NUM_VERTICES = vertices.size();
	int NUM_INDICES = indices.size();

	for (int i = 0; i < num_instances - 1; i++) {
		for (int j = 0; j < NUM_VERTICES; j++) {
			TexturedVertex copied_vertex = vertices[j];
			vertices.push_back(copied_vertex);
		}
		for (int j = 0; j < NUM_INDICES; j++) {
			indices.push_back((uint16_t)(indices[j] + (NUM_VERTICES * (i + 1))));
		}
	}
}

// From original components.cpp obj function. Normalizes mesh to have vertex positions [-0.5, 0.5]
void normalize_mesh(std::vector<vec3>& vert_positions)
{
	// Compute bounds of the mesh
	vec3 max_position = { -99999,-99999,-99999 };
	vec3 min_position = { 99999,99999,99999 };
	for (vec3& pos : vert_positions)
	{
		max_position = glm::max(max_position, pos);
		min_position = glm::min(min_position, pos);
	}
	if (abs(max_position.z - min_position.z) < 0.001)
		max_position.z = min_position.z + 1; // don't scale z direction when everythiny is on one plane

	vec3 size3d = max_position - min_position;
	//out_size = size3d;

	// Now normalize mesh vertices to range [-0.5, 0.5]
	for (vec3& pos : vert_positions) {
		pos = ((pos - min_position) / size3d) - vec3(0.5f, 0.5f, 0.5f);
		pos.y = -pos.y; // Note: All meshes must get y flipped
	}
}

void add_to_outer_edges(std::vector<Edge>& outer_edges, Edge new_edge) // Used in algorithm in function below
{
	int to_be_deleted = -1;
	int num_times_to_be_deleted_been_set = 0;
	for (int i = 0; i < outer_edges.size(); i++) {
		if (outer_edges[i] == new_edge) {
			to_be_deleted = i;
			num_times_to_be_deleted_been_set++;
		}
	}
	assert(num_times_to_be_deleted_been_set < 2);
	if (to_be_deleted >= 0) {
		outer_edges.erase(outer_edges.begin() + to_be_deleted);
	} else {
		outer_edges.push_back(new_edge);
	}
}

void RenderSystem::initializeGlMeshes()
{
	for (uint i = 0; i < mesh_paths.size(); i++)
	{
		// Initialize meshes
		GEOMETRY_ID geom_index = mesh_paths[i].first;
		std::string name = mesh_paths[i].second;

		printf("Loading OBJ file using Camilo's function: %s\n", name.c_str());
		Entity entity = Entity();

		// Please leave print statement comments as they are very helpful for debugging bad meshes
		// None of the below works if the mesh is not entirely made out of triangles (and also probably with non-fully formed faces)

		auto [vert_positions, uvs, normals, vert_indices, uv_indices, normal_indices] = Mesh::ParseObj(name);

		normalize_mesh(vert_positions); // Convert vertices so all values dimensions are [-0.5, 0.5]
		
		TexturedVertex empty_textured_vertex = {vec3(-100.f), vec2(-100.f), vec2(-100.f)};
		std::vector<TexturedVertex> textured_vertices((int)vert_positions.size(), empty_textured_vertex);

		// Simply create a TexturedVertex array using the mesh data returned above
		std::vector<uint16_t> vert_indices_uint16; // Must convert the vert_indices given as ints to uint_16 one by one 
		for (int j = 0; j < vert_indices.size(); j++) {
			vert_indices_uint16.push_back((uint16_t)vert_indices[j]);

			TexturedVertex textured_vertex;
			textured_vertex.position = vert_positions[vert_indices[j]];
			textured_vertex.texcoord = uvs[uv_indices[j]];
			textured_vertex.texcoord.y = -textured_vertex.texcoord.y; // Invert V coordinate like all other textures
			textured_vertices[vert_indices[j]] = textured_vertex;
		}
		assert(textured_vertices.size() == vert_positions.size());
		for (int j = 0; j < textured_vertices.size(); j++) {
			if (!(textured_vertices[j].position == vert_positions[j])) {
				assert(textured_vertices[j].position == vert_positions[j]); // Check to see if conversion worked
			}
		}

		// Rudimentary algorithm to only get the outer edges/vertices of the mesh (can't use vert_positions since they are out of order)
		std::vector<Edge> outer_edges;
		for (uint f = 0; f < vert_indices.size()/3; f++) { // f for 'face'
			int start_face_index = (f * 3);
			for (uint i = 0; i < 3; i++) { // Go through each of the 3 indices for the face
				int index_vert1 = start_face_index + i;
				int index_vert2 = start_face_index + ((i + 1) % 3);
				// Note: I had to swap the below because the line-polygon algorithm doesn't work otherwise
				Edge new_edge = { vert_positions[vert_indices[index_vert2]], vert_positions[vert_indices[index_vert1]] };
				add_to_outer_edges(outer_edges, new_edge);
			}
		}
		meshes[(int)geom_index].edges = outer_edges; // This data is used in world_init to create ComplexPolygons

		// Now use the outer_edges data to get 2D normal information to be put into the vbo
		for (uint i = 0; i < textured_vertices.size(); i++) {
			TexturedVertex& vertex = textured_vertices[i];
			int count = 0; // count == 2 if the vertex is on the edge of a mesh, 0 otherwise
			vec2 normal_sum = {0.f, 0.f};
			for (uint j = 0; j < outer_edges.size(); j++) {
				Edge edge = outer_edges[j];
				if (edge.vertex1 == vec2(vertex.position) || edge.vertex2 == vec2(vertex.position)) {
					vec2 edge_vector = edge.vertex2 - edge.vertex1;
					normal_sum += normalize(vec2(edge_vector.y, -edge_vector.x)); // adding the current edge_normal
					count++;
				}
			}
			vertex.normal = (count == 2) ? normalize(normal_sum) : vec2(0.f); // If vertex is on edge of mesh, then give it a normal
		}

		extend_vbo_and_ibo(textured_vertices, vert_indices_uint16, MAX_MESH_INSTANCES);
		bindVBOandIBO(geom_index, textured_vertices, vert_indices_uint16);
		
		gl_has_errors();
	}
}

void RenderSystem::initializeShaderStorageBuffers()
{
	glGenBuffers(1, &instance_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, instance_ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(instance_data), &instance_data, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, instance_ssbo); // Binding number 3 (seen in layout func in shaders) // TODO: Try 0 or 1
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind (unnecessary but good cleanup)
	gl_has_errors();

	glGenBuffers(1, &light_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLight)*MAX_POINT_LIGHTS, registry.pointLights.components.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, light_ssbo); // Binding number 1
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	gl_has_errors();

	//glGenBuffers(1, &test_ssbo); // Useful for debugging
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, test_ssbo);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(test_data), &test_data, GL_DYNAMIC_DRAW);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, test_ssbo); // Binding number 4
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	gl_has_errors();
}

void RenderSystem::initializeGlGeometryBuffers()
{
	// Vertex Buffer creation.
	glGenBuffers((GLsizei)vertex_buffers.size(), vertex_buffers.data());
	// Index Buffer creation.
	glGenBuffers((GLsizei)index_buffers.size(), index_buffers.data());

	// Index and Vertex buffer data initialization.
	initializeGlMeshes();

	//////////////////////////
	// Initialize sprite
	// The position corresponds to the center of the texture.
	std::vector<TexturedVertex> sprite_vertices(4);
	sprite_vertices[0].position = { -1.f / 2, +1.f / 2, 0.0f };
	sprite_vertices[1].position = { +1.f / 2, +1.f / 2, 0.0f };
	sprite_vertices[2].position = { +1.f / 2, -1.f / 2, 0.0f };
	sprite_vertices[3].position = { -1.f / 2, -1.f / 2, 0.0f };
	sprite_vertices[0].texcoord = { 0.f, 1.f };
	sprite_vertices[1].texcoord = { 1.f, 1.f };
	sprite_vertices[2].texcoord = { 1.f, 0.f };
	sprite_vertices[3].texcoord = { 0.f, 0.f };
	sprite_vertices[0].normal = normalize(vec2(-1.f, 1.f));
	sprite_vertices[1].normal = normalize(vec2(1.f, 1.f));
	sprite_vertices[2].normal = normalize(vec2(1.f, -1.f));
	sprite_vertices[3].normal = normalize(vec2(-1.f, -1.f));


	// Counterclockwise as it's the default opengl front winding direction.
	std::vector<uint16_t> sprite_indices = { 0, 3, 1, 1, 3, 2 };

	extend_vbo_and_ibo(sprite_vertices, sprite_indices, MAX_SPRITE_INSTANCES); 
	bindVBOandIBO(GEOMETRY_ID::SPRITE, sprite_vertices, sprite_indices);

	////////////////////////
	// Initialize egg
	std::vector<ColoredVertex> egg_vertices;
	std::vector<uint16_t> egg_indices;
	constexpr float z = -0.1f;
	constexpr int NUM_TRIANGLES = 62;

	for (int i = 0; i < NUM_TRIANGLES; i++) {
		const float t = float(i) * M_PI * 2.f / float(NUM_TRIANGLES - 1);
		egg_vertices.push_back({});
		egg_vertices.back().position = { 0.5 * cos(t), 0.5 * sin(t), z };
		egg_vertices.back().color = { 0.8, 0.8, 0.8 };
	}
	egg_vertices.push_back({});
	egg_vertices.back().position = { 0, 0, 0 };
	egg_vertices.back().color = { 1, 1, 1 };
	for (int i = 0; i < NUM_TRIANGLES; i++) {
		egg_indices.push_back((uint16_t)i);
		egg_indices.push_back((uint16_t)((i + 1) % NUM_TRIANGLES));
		egg_indices.push_back((uint16_t)NUM_TRIANGLES);
	}
	int geom_index = (int)GEOMETRY_ID::EGG;
	meshes[geom_index].vertices = egg_vertices;
	meshes[geom_index].vertex_indices = egg_indices;
	bindVBOandIBO(GEOMETRY_ID::EGG, meshes[geom_index].vertices, meshes[geom_index].vertex_indices);

	//////////////////////////////////
	// Initialize debug line
	std::vector<ColoredVertex> line_vertices;
	std::vector<uint16_t> line_indices;

	constexpr float depth = -1.0f; // Depth is values between -1 and 1. The smaller the number, the closer to the screen
	constexpr vec3 red = { 0.8,0.1,0.1 };

	// Corner points
	line_vertices = {
		{{-0.5,-0.5, depth}, red},
		{{-0.5, 0.5, depth}, red},
		{{ 0.5, 0.5, depth}, red},
		{{ 0.5,-0.5, depth}, red},
	};

	// Two triangles
	line_indices = {0, 1, 3, 1, 2, 3};
	
	geom_index = (int)GEOMETRY_ID::DEBUG_LINE;
	meshes[geom_index].vertices = line_vertices;
	meshes[geom_index].vertex_indices = line_indices;
	bindVBOandIBO(GEOMETRY_ID::DEBUG_LINE, line_vertices, line_indices);

	//////////////////////////////////
	// Initialize dynamic health bar
	std::vector<ColoredVertex> health_bar_vertices;
	std::vector<uint16_t> health_bar_indices;

	constexpr vec3 green = { 0.1,0.8,0.1 };

	float hb_width = 0.1f;
	float hb_height = 0.03f;
	// Corner points
	health_bar_vertices = {
		{{-hb_width,-hb_height, depth}, green},
		{{-hb_width, hb_height, depth}, green},
		{{ hb_width, hb_height, depth}, green},
		{{ hb_width,-hb_height, depth}, green},
	};


	// Two triangles
	health_bar_indices = { 0, 1, 3, 1, 2, 3 };

	geom_index = (int)GEOMETRY_ID::HEALTH_BAR;
	meshes[geom_index].vertices = health_bar_vertices;
	meshes[geom_index].vertex_indices = health_bar_indices;
	bindVBOandIBO(GEOMETRY_ID::HEALTH_BAR, health_bar_vertices, health_bar_indices);

	///////////////////////////////////////////////////////
	// Initialize screen triangle (yes, triangle, not quad; its more efficient).
	std::vector<vec3> screen_vertices(3);
	screen_vertices[0] = { -1, -6, 0.f };
	screen_vertices[1] = { 6, -1, 0.f };
	screen_vertices[2] = { -1, 6, 0.f };

	// Counterclockwise as it's the default opengl front winding direction.
	const std::vector<uint16_t> screen_indices = { 0, 1, 2 };
	bindVBOandIBO(GEOMETRY_ID::SCREEN_TRIANGLE, screen_vertices, screen_indices);
}

RenderSystem::~RenderSystem()
{
	// Don't need to free gl resources since they last for as long as the program,
	// but it's polite to clean after yourself.
	glDeleteBuffers((GLsizei)vertex_buffers.size(), vertex_buffers.data());
	glDeleteBuffers((GLsizei)index_buffers.size(), index_buffers.data());
	for (uint type = 0; type < 2; type++) { // TODO: Ask TA
		glDeleteTextures((GLsizei)texture_gl_handles[type].size(), texture_gl_handles[type].data());
	}
	//glDeleteTextures((GLsizei)texture_gl_handles.size(), texture_gl_handles.data()); // TODO
	glDeleteTextures(1, &off_screen_render_buffer_color);
	glDeleteRenderbuffers(1, &off_screen_render_buffer_depth);
	gl_has_errors();

	for(uint i = 0; i < effect_count; i++) {
		glDeleteProgram(effects[i]);
	}
	// delete allocated resources
	glDeleteFramebuffers(1, &frame_buffer);
	gl_has_errors();

	// remove all entities created by the render system
	while (registry.renderRequests.entities.size() > 0)
	    registry.remove_all_components_of(registry.renderRequests.entities.back());
}

// Initialize the screen texture from a standard sprite
bool RenderSystem::initScreenTexture()
{

	int framebuffer_width, framebuffer_height;
	glfwGetFramebufferSize(const_cast<GLFWwindow*>(window), &framebuffer_width, &framebuffer_height);  // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays

	glGenTextures(1, &off_screen_render_buffer_color);
	glBindTexture(GL_TEXTURE_2D, off_screen_render_buffer_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl_has_errors();

	glGenRenderbuffers(1, &off_screen_render_buffer_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, off_screen_render_buffer_depth);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, off_screen_render_buffer_color, 0);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, off_screen_render_buffer_depth);
	gl_has_errors();

	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	return true;
}

bool gl_compile_shader(GLuint shader)
{
	glCompileShader(shader);
	gl_has_errors();
	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLint log_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
		std::vector<char> log(log_len);
		glGetShaderInfoLog(shader, log_len, &log_len, log.data());
		glDeleteShader(shader);

		gl_has_errors();

		fprintf(stderr, "GLSL: %s", log.data());
		return false;
	}

	return true;
}

bool loadEffectFromFile(
	const std::string& vs_path, const std::string& fs_path, GLuint& out_program)
{
	// Opening files
	std::ifstream vs_is(vs_path);
	std::ifstream fs_is(fs_path);
	if (!vs_is.good() || !fs_is.good())
	{
		fprintf(stderr, "Failed to load shader files %s, %s", vs_path.c_str(), fs_path.c_str());
		assert(false);
		return false;
	}

	// Reading sources
	std::stringstream vs_ss, fs_ss;
	vs_ss << vs_is.rdbuf();
	fs_ss << fs_is.rdbuf();
	std::string vs_str = vs_ss.str();
	std::string fs_str = fs_ss.str();
	const char* vs_src = vs_str.c_str();
	const char* fs_src = fs_str.c_str();
	GLsizei vs_len = (GLsizei)vs_str.size();
	GLsizei fs_len = (GLsizei)fs_str.size();

	GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vs_src, &vs_len);
	GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fs_src, &fs_len);
	gl_has_errors();

	// Compiling
	if (!gl_compile_shader(vertex))
	{
		fprintf(stderr, "Vertex compilation failed");
		assert(false);
		return false;
	}
	if (!gl_compile_shader(fragment))
	{
		fprintf(stderr, "Vertex compilation failed");
		assert(false);
		return false;
	}

	// Linking
	out_program = glCreateProgram();
	glAttachShader(out_program, vertex);
	glAttachShader(out_program, fragment);
	glLinkProgram(out_program);
	gl_has_errors();

	{
		GLint is_linked = GL_FALSE;
		glGetProgramiv(out_program, GL_LINK_STATUS, &is_linked);
		if (is_linked == GL_FALSE)
		{
			GLint log_len;
			glGetProgramiv(out_program, GL_INFO_LOG_LENGTH, &log_len);
			std::vector<char> log(log_len);
			glGetProgramInfoLog(out_program, log_len, &log_len, log.data());
			gl_has_errors();

			fprintf(stderr, "Link error: %s", log.data());
			assert(false);
			return false;
		}
	}

	// No need to carry this around. Keeping these objects is only useful if we recycle
	// the same shaders over and over, which we don't, so no need and this is simpler.
	glDetachShader(out_program, vertex);
	glDetachShader(out_program, fragment);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	gl_has_errors();

	return true;
}

