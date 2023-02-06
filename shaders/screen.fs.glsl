#version 330

uniform sampler2D screen_texture;
uniform float time;
uniform float darken_screen_factor;

in vec2 texcoord;

layout(location = 0) out vec4 color;

vec4 fade_color(vec4 in_color) 
{
	if (darken_screen_factor > 0 && darken_screen_factor < 1)
		in_color -= darken_screen_factor * vec4(0.8, 0.8, 0.8, 0);
	return in_color;
}

void main()
{
	vec2 coord = texcoord;

    vec4 in_color = texture(screen_texture, coord);
	color = fade_color(in_color);
}