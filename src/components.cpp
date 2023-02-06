#include "components.hpp"
#include "render_system.hpp" // for gl_has_errors
#include "../ext/project_path.hpp"


#define STB_IMAGE_IMPLEMENTATION
#include "../ext/stb_image/stb_image.h"

// stlib
#include <iostream>
#include <sstream>
#include <fstream>

// JSON parser
// https://github.com/Tencent/rapidjson/#readme
#include "../ext/rapidjson/document.h"
#include "../ext/rapidjson/filereadstream.h"

Debug debugging;
float death_timer_counter_ms = 3000;


// Old and unused, keep for reference:
// Very, VERY simple OBJ loader from https://github.com/opengl-tutorials/ogl tutorial 7
// (modified to also read vertex color and omit uv and normals)
bool Mesh::loadFromOBJFile(std::string obj_path, std::vector<ColoredVertex>& out_vertices, std::vector<uint16_t>& out_vertex_indices, vec2& out_size)
{
	// disable warnings about fscanf and fopen on Windows
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

	printf("Loading OBJ file %s...\n", obj_path.c_str());
	// Note, normal and UV indices are not loaded/used, but code is commented to do so
	std::vector<uint16_t> out_uv_indices, out_normal_indices;
	std::vector<glm::vec2> out_uvs;
	std::vector<glm::vec3> out_normals;

	FILE* file = fopen(obj_path.c_str(), "r");
	if (file == NULL) {
		printf("Impossible to open the file ! Are you in the right path ? See Tutorial 1 for details\n");
		getchar();
		return false;
	}

	while (1) {
		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF)
			break; // EOF = End Of File. Quit the loop.

		if (strcmp(lineHeader, "v") == 0) {
			ColoredVertex vertex;
			int matches = fscanf(file, "%f %f %f %f %f %f\n", &vertex.position.x, &vertex.position.y, &vertex.position.z,
				&vertex.color.x, &vertex.color.y, &vertex.color.z);
			if (matches == 3)
				vertex.color = { 1,1,1 };
			out_vertices.push_back(vertex);
		}
		else if (strcmp(lineHeader, "vt") == 0) {
			glm::vec2 uv;
			fscanf(file, "%f %f\n", &uv.x, &uv.y);
			uv.y = -uv.y; // Invert V coordinate since we will only use DDS texture, which are inverted. Remove if you want to use TGA or BMP loaders.
			out_uvs.push_back(uv);
		}
		else if (strcmp(lineHeader, "vn") == 0) {
			glm::vec3 normal;
			fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			out_normals.push_back(normal);
		}
		else if (strcmp(lineHeader, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], normalIndex[3], uvIndex[3];

			int matches = fscanf(file, "%d %d %d\n", &vertexIndex[0], &vertexIndex[1], &vertexIndex[2]);
			if (matches == 1) // try again
			{
				// Note first vertex index is already consumed by the first fscanf call (match ==1) since it aborts on the first error
				matches = fscanf(file, "//%d %d//%d %d//%d\n", &normalIndex[0], &vertexIndex[1], &normalIndex[1], &vertexIndex[2], &normalIndex[2]);
				if (matches != 5) // try again
				{
					matches = fscanf(file, "%d/%d %d/%d/%d %d/%d/%d\n", &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
					if (matches != 8)
					{
						printf("File can't be read by our simple parser :-( Try exporting with other options\n");
						fclose(file);
						return false;
					}
				}
			}

			// -1 since .obj starts counting at 1 and OpenGL starts at 0
			out_vertex_indices.push_back((uint16_t)vertexIndex[0] - 1);
			out_vertex_indices.push_back((uint16_t)vertexIndex[1] - 1);
			out_vertex_indices.push_back((uint16_t)vertexIndex[2] - 1);
			out_uv_indices.push_back(uvIndex[0] - 1);
			out_uv_indices.push_back(uvIndex[1] - 1);
			out_uv_indices.push_back(uvIndex[2] - 1);
			out_normal_indices.push_back((uint16_t)normalIndex[0] - 1);
			out_normal_indices.push_back((uint16_t)normalIndex[1] - 1);
			out_normal_indices.push_back((uint16_t)normalIndex[2] - 1);
		}
		else {
			// Probably a comment, eat up the rest of the line
			char stupidBuffer[1000];
			fgets(stupidBuffer, 1000, file);
		}
	}
	fclose(file);

	// Compute bounds of the mesh
	vec3 max_position = { -99999,-99999,-99999 };
	vec3 min_position = { 99999,99999,99999 };
	for (ColoredVertex& pos : out_vertices)
	{
		max_position = glm::max(max_position, pos.position);
		min_position = glm::min(min_position, pos.position);
	}
	if(abs(max_position.z - min_position.z)<0.001)
		max_position.z = min_position.z+1; // don't scale z direction when everythin is on one plane

	vec3 size3d = max_position - min_position;
	out_size = size3d;

	// Normalize mesh to range -0.5 ... 0.5
	for (ColoredVertex& pos : out_vertices)
		pos.position = ((pos.position - min_position) / size3d) - vec3(0.5f, 0.5f, 0.5f);

	return true;
}



// Camilo allowed me to copy and paste these functions because the one above sucks
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**                                                                                     *
 * @file HMesh.cpp
 * @author Camilo Talero
 * @brief
 * @version 0.1
 * @date 2020-08-30
 *
 * @copyright Copyright (c) 2019
 *
 *                                                                                      */
 //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/** @cond */
#include <filesystem>
/** @endcond */


using namespace std;
namespace fs = std::filesystem;


inline void LefTrim(std::string& str)
{
	size_t first = str.find_first_not_of(" \n\r\t");
	if (std::string::npos == first) { return; }
	str = str.substr(first, str.size());
}

inline void RightTrim(std::string& str)
{
	size_t last = str.find_last_not_of(" \n\r\t");
	if (std::string::npos == last) { return; }
	str = str.substr(0, last + 1);
}

inline void Trim(std::string& str)
{
	LefTrim(str);
	RightTrim(str);
}

std::vector<std::string> Split(const std::string& str, char separator = ' ')
{
	std::vector<std::string> tokens;
	std::string accumulated = "";
	for (auto& c : str)
	{
		if (c != separator) accumulated += c;
		else
		{
			Trim(accumulated);
			tokens.push_back(accumulated);
			accumulated = "";
		}
	}
	Trim(accumulated);
	tokens.push_back(accumulated);

	return tokens;
}




std::tuple<
	vector<vec3>,  // Vertices
	vector<vec2>,  // Texture coordinates
	vector<vec3>,  // Normals
	vector<uint>,  // Vertex indices
	vector<uint>,  // UV indices
	vector<uint>>  // Normal indices
	Mesh::ParseObj(const string& path)
{
	vector<vec3> vertices;
	vector<vec2> uvs;
	vector<vec3> normals;

	assert(fs::exists(path), "File {0} does not exist.", path);
	std::ifstream file_stream(path);

	// First parse the file to get all the vertex, coords and normals
	string line;
	while (getline(file_stream, line))
	{
		istringstream string_stream(line);
		string first_token;
		string_stream >> first_token;

		if (first_token == "v")
		{
			string c1, c2, c3;
			string_stream >> c1;
			string_stream >> c2;
			string_stream >> c3;

			vertices.push_back(vec3(
				strtof(c1.c_str(), nullptr),
				strtof(c2.c_str(), nullptr),
				strtof(c3.c_str(), nullptr)));
		}
		else if (first_token == "vt")
		{
			string c1, c2;
			string_stream >> c1;
			string_stream >> c2;

			uvs.push_back(vec2(
				strtof(c1.c_str(), nullptr),
				strtof(c2.c_str(), nullptr)));
		}
		else if (first_token == "vn")
		{
			string c1, c2, c3;
			string_stream >> c1;
			string_stream >> c2;
			string_stream >> c3;

			normals.push_back(vec3(
				strtof(c1.c_str(), nullptr),
				strtof(c2.c_str(), nullptr),
				strtof(c3.c_str(), nullptr)));
		}
	}
	// On the next pass focus only on the faces and associate the face data together by
	// index.
	file_stream.clear();
	file_stream.seekg(0, ios::beg);
	vector<uint> vertex_indices;
	vector<uint> uv_indices;
	vector<uint> normal_indices;

	bool v_starts_at_0 = false;
	bool t_starts_at_0 = false;
	bool n_starts_at_0 = false;
	while (getline(file_stream, line))
	{
		istringstream string_stream(line);
		string first_token;
		string_stream >> first_token;

		if (first_token == "f")
		{
			// We assume a triangular mesh
			string c1, c2, c3;
			string_stream >> c1;
			string_stream >> c2;
			string_stream >> c3;

			const array<string, 3> string_info = { c1, c2, c3 };
			for (auto& vertex_info : string_info)
			{
				auto indices = Split(vertex_info, '/');
				assert(indices.size() >= 1, "");
				assert(indices[0] != "", "At least the vertex information should exist");
				vertex_indices.push_back(stoi(indices[0]));
				// If we see a 0 then the array starts at 0.
				v_starts_at_0 = v_starts_at_0 || vertex_indices.back() == 0;
				if (indices.size() >= 2 && indices[1] != "")
				{
					uv_indices.push_back(stoi(indices[1]));
					t_starts_at_0 = t_starts_at_0 || uv_indices.back() == 0;
				}
				if (indices.size() >= 3 && indices[2] != "")
				{
					normal_indices.push_back(stoi(indices[2]));
					n_starts_at_0 = n_starts_at_0 || normal_indices.back() == 0;
				}
			}
		}
	}
	// If the arrays don't start at 0, shift them back by 1.
	if (!v_starts_at_0) for (uint& i : vertex_indices) i--;
	if (!t_starts_at_0) for (uint& i : uv_indices) i--;
	if (!n_starts_at_0) for (uint& i : normal_indices) i--;

	return { vertices, uvs, normals, vertex_indices, uv_indices, normal_indices };
}

rapidjson::Document loadJSON(std::string json_path) {
#if defined _WIN32 || defined _WIN64
	FILE* fp = fopen(json_path.c_str(), "rb"); // non-Windows use "r"
#else
	FILE* fp = fopen(json_path.c_str(), "r"); // non-Windows use "r"
#endif

	// Parses json file into dom
	char readBuffer[8192];
	rapidjson::FileReadStream input_stream(fp, readBuffer, sizeof(readBuffer));
	rapidjson::Document dom;
	dom.ParseStream(input_stream);
	fclose(fp);

	return dom;
}


rapidjson::Document Room::loadFromJSONFile(std::string room_file_name) {
	std::string json_path = std::string(PROJECT_SOURCE_DIR) + "data/rooms/" + room_file_name;

	return loadJSON(json_path);
}

rapidjson::Document TextureAtlas::loadFromJSONFile(std::string atlas_file_name) {
	std::string json_path = std::string(PROJECT_SOURCE_DIR) + "data/atlases/" + atlas_file_name;

	return loadJSON(json_path);
}