#version 430
// TEXT FRAGMENT SHADER

// From vertex shader
layout (location = 1) in vec2 in_texcoord;
layout (location = 2) in vec4 in_charinfo;

// Application data
uniform sampler2D sampler0;
uniform vec4 text_color;

// Output color
layout(location = 0) out vec4 color;

void main()
{
    float chx = in_charinfo.x,
          chy = in_charinfo.y,
          chwidth = in_charinfo.z,
          chheight = in_charinfo.w;

    vec2 new_texcoord = in_texcoord;

    new_texcoord.x = chx + in_texcoord.x*chwidth;
    new_texcoord.y = chy + in_texcoord.y*chheight;

    color = texture(sampler0, new_texcoord);

    // Make black background transparent
    if (color.x < 0.1) discard;

    // If uniform text_color is completely transparent, use texture color
	if (text_color.w == 0) {
        color = vec4(color.xyz, 1.0);
    } else {
        color = text_color;
    }
}