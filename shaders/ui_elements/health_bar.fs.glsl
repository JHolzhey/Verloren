#version 330
// A replica of egg.fs.glsl

// From Vertex Shader
in vec3 vertex_color;

// Application data
uniform vec3 add_color;

// Output color
layout(location = 0) out vec4 color;

void main()
{
	color = vec4(add_color * vertex_color, 1.0);
}