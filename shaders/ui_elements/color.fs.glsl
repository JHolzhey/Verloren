#version 330
// TEXT FRAGMENT SHADER

// From vertex shader
in vec2 texcoord;

// Application data
uniform sampler2D sampler0;
uniform vec4 set_color;

// Output color
layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex_color = texture(sampler0, texcoord);

    // discard pixels transparent in original texture
    if (tex_color.w < 0.1) discard;

    // Overwrite texture color with set_color
	out_color = set_color;
}