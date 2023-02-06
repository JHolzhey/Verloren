#version 330
// A replica of textured.fs.glsl

// From vertex shader
in vec2 texcoord;

// Application data
uniform sampler2D sampler0;
uniform int sprite_sheet_num;
uniform vec3 add_color;
uniform bool icon_enabled;
// First element indicates glow yes/no. Remaining are color
uniform bool glow;

// Output color
layout(location = 0) out vec4 color;

// a simple switch on an icon. Does not support advance animation (num > 2)
// determine texcoord 
vec2 detTexcoord(vec2 texcoord) {
	vec2 new_texcoord;
	float sheet_size = 1.0 / sprite_sheet_num;
	if (!icon_enabled)
		new_texcoord = vec2(texcoord.x, sheet_size + texcoord.y * sheet_size);
	else 
		new_texcoord = vec2(texcoord.x, texcoord.y * sheet_size);
	return new_texcoord;
}

void main()
{
	vec2 new_texcoord = detTexcoord(texcoord);
	if (glow) {
		// Draw a blurred circle center at the sprite's midpoint
		float d = distance(vec2(0.5, 0.5), texcoord) * 2;
		float start_glow = 0.4;
		float end_glow = 1;
		float glow_range = end_glow - start_glow;
		// float alpha = d < start_glow ? 1 : mix(1, 0, d - start_glow);
		float alpha = d < start_glow ? 1 : 1 - (d - start_glow)/glow_range;
		color = vec4(add_color, alpha);
	} else {
		color = texture(sampler0, new_texcoord) + vec4(add_color, 0.0);
	}

}
