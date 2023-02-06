#version 430
// A replica of textured.vs.glsl

// Input attributes
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texcoord;
layout (location = 2) in vec2 in_normal; // Unused here
layout (location = 3) in vec4 in_charinfo;
layout (location = 4) in mat3 in_transform;

// Passed to fragment shader
layout (location = 1) out vec2 out_texcoord;
layout (location = 2) out vec4 out_charinfo;

void main()
{
	out_texcoord = in_texcoord;
	out_charinfo = in_charinfo;

	vec3 pos = in_transform * vec3(in_position.xy, 1.0);
	gl_Position = vec4(pos.xy, in_position.z, 1.0);
}