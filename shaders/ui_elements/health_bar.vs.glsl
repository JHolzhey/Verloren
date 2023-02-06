#version 330
// A replica of egg.vs.glsl

// Input attributes
in vec3 in_color;
in vec3 in_position; // Doesn't need in_normal because it uses ColoredVertex

out vec3 vertex_color;

// Application data
uniform float current_hp;
uniform float max_hp;
uniform int is_primary;
uniform float time;

// all time unit in ms (miliseconds)
const float timespan = 40.0;
const float offset = -0.1;
const float threshold = 0.25;

vec3 scale(float percentage_hp, float t) {
	vec3 scale = vec3(percentage_hp, 1.f, 1.f);
//	if (percentage_hp <= threshold)
//		return vec3(mix(0.9 * scale.x, scale.x, t), scale.y, scale.z);
	return scale;
}

vec3 translate(float percentage_hp, float t) {
	vec3 translate = vec3(-0.8, 0.9 + offset * (1 - is_primary), 0.0);
	vec3 hp_loss = vec3(-(1.f - percentage_hp)/10.f, 0.f, 0.f);
	if (percentage_hp <= threshold)
		return translate + mix(1.02 * hp_loss, 0.98 * hp_loss, t);
	return translate + hp_loss;
}

void main()
{
	vertex_color = in_color;
	// make sure t is [0, 1]
	float percentage_hp = max(0.f, min(1.f, current_hp / max_hp));
	float t = sin(time / timespan) * 0.5 + 0.5;
	vec3 pos = scale(percentage_hp, t) * vec3(in_position.xy, 1.0) + translate(percentage_hp, t);
	gl_Position = vec4(pos.xy, in_position.z, 1.0);
}