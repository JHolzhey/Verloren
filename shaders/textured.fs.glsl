#version 430

#define MAX_INSTANCES 1000
#define MAX_POINT_LIGHTS 16

// For debugging
struct TestData { vec4 test_stuff; };
layout(std430, binding = 4) buffer test_data_ssbo { TestData test_data[10]; };

mat4 dither_matrix = mat4(
	1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
	13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
	4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
	16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
	);

// From vertex shader
in vec2 texcoord;
in vec3 frag_position;
//in vec2 frag_proj_pos;
flat in int instance_id;

// Application data
uniform float time;
uniform float darken_screen_factor;
uniform float vignette_factor;
uniform vec2 window_size;

uniform vec2 player_position; // For debugging

uniform vec3 view_position; // World space
struct DirLight {
    vec3 direction; // World space
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};  
uniform DirLight dir_light;

// Point Light SSBO
struct PointLight {
	vec3 offset_position; int pad; // Unused in shader
    vec3 position; int pad2;
    
	float entity_id;
    float constant;
    float linear;
    float quadratic;  

    vec3 ambient; int pad3;
    vec3 diffuse; int pad4;
    vec3 specular; int pad5;

	float radius;
	float flicker_radius;
	float max_radius;
	float intensity; // Unused in shader
};
uniform int num_point_lights;
layout(std430, binding = 1) buffer light_data_ssbo
{
	PointLight point_lights[MAX_POINT_LIGHTS];
};

// Instance Data SSBO
struct InstanceData  // Needs 16-byte alignment (e.g. breaks with unpadded vec3's)
{
	mat4 transform;
	mat4 TBN;

	vec2 position;
	vec2 scale; int pad4[2];
	vec2 sprite_offset;
	vec3 sprite_normal; int pad0;
	float rotation;

	float entity_id;
	float wind_strength;
	float extrude_size; // extrusion happens along normals

	int texture_indices[4]; // diffuse, normal, normal_add, mask
	vec4 diffuse_coord_loc; // x,y is relative offset and z,w is the scale factor
	vec4 normal_coord_loc;
	vec4 normal_add_coord_loc;
	vec4 mask_coord_loc;

	vec3 specular; int pad1;
	vec3 add_color; int pad2;
	vec3 multiply_color; int pad3;
	vec3 ignore_color;
	float shininess;

	float transparency;
	float transparency_offset;

	float shadow_scale;
	int num_lights_affecting;
	int lights_affecting[MAX_POINT_LIGHTS];
};
layout(std430, binding = 3) buffer instance_data_ssbo
{
	InstanceData instance_data[MAX_INSTANCES];
};
uniform bool is_shadow;
uniform bool is_ground_piece;

uniform sampler2D textures[32];

// Output color
layout(location = 0) out  vec4 frag_color;

 // Phong Reflection Model
vec3 calc_dir_light(DirLight dir_light, InstanceData material, vec3 diffuse_color, vec3 normal, vec3 view_direction)
{
	// Diffuse shading
	float diff = max(dot(normal,  dir_light.direction), 0.0); // Worse way of doing it? Keep this for testing
	//float diff = dot(normal, dir_light.direction);
	//float _s = 0.5; // inverse strength of dot product. s = 0.5 means regular dot product. Higher values = less darkening effect
	//diff = max( (diff/(2.0*_s) + 1.0 - (0.5/_s)), 0.0 ); // https://www.desmos.com/calculator/rw9jkv4com

    // Specular shading
    vec3 reflect_direction = reflect(-dir_light.direction, normal);
	float view_dot = max(dot(view_direction, reflect_direction), 0.0);
    float spec = pow(view_dot, material.shininess); // https://www.desmos.com/calculator/blplj5s4u2

    // Combine results with Ambient
    vec3 ambient = dir_light.ambient * diffuse_color;
	vec3 diffuse = dir_light.diffuse * diff * diffuse_color;
    vec3 specular = dir_light.specular * spec * material.specular;
    return (ambient + diffuse + specular);
}

float calc_point_light_attenuation(PointLight light, float distance)
{
	//float distance = length(light.position - frag_position);	// time =- unique_id;  to get unique flicker for each light
	float frequency = 2.0;			// flicker frequency - https://www.desmos.com/calculator/ssqbdax3hq
	float time_o = time + light.entity_id;// * 4.0;
	float flicker_radius = (light.flicker_radius/2.74) * ( (sin(frequency * time_o/12) + cos(frequency * time_o/3)/2) + 1.24 );
	float intensity = (light.radius + flicker_radius) * (light.radius + flicker_radius);

    float attenuation = intensity / (light.constant + (light.linear * distance) + (light.quadratic * (distance*distance))) - 1.0;

	//if (attenuation < 0.0) { discard; } // For debugging
	//return (attenuation > 0.0) ? intensity / light.constant - 1.0 : 0.f; // For debugging

	attenuation = max(attenuation, 0.0); // Note: attenuation can be negative without this
	return attenuation;
}

vec3 calc_point_light(PointLight light, InstanceData material, vec3 diffuse_color, vec3 normal, vec3 view_direction)
{
	vec3 light_to_frag_vec = light.position - frag_position;
	float distance = length(light.position - frag_position); // TODO: Test if faster without 'distance'
	//if (distance > light.max_radius) { return 0; }

    vec3 light_direction = light_to_frag_vec/distance;
    // Diffuse shading
    float diff = max(dot(normal, light_direction), 0.0);

    // Specular shading
    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), material.shininess);

    // Attenuation
    float attenuation = calc_point_light_attenuation(light, distance);
    // Combine results with Ambient
    vec3 ambient  = light.ambient  * diffuse_color;
    vec3 diffuse  = light.diffuse  * diff * diffuse_color;
    vec3 specular = light.specular * spec * material.specular;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

float calc_point_light_shadow(PointLight light, float alpha, vec3 normal, bool yay)
{
	vec3 light_to_frag_vec = light.position - frag_position;
	float distance = length(light.position - frag_position); // TODO: Test if faster without 'distance' - silly

	//if (distance > light.max_radius) { return 0; }

	vec3 light_direction = light_to_frag_vec/distance;
	float alpha_minus = -alpha * max(dot(normal, light_direction), 0.0);

	//if (alpha_minus > 0.0) { return 100; } // For debugging

	if (yay) {
		//return -alpha * 1/calc_point_light_attenuation(light, distance);//, 0.0); 
		return -alpha*(1 - min(calc_point_light_attenuation(light, distance), 1.0));
		//return -alpha*min(distance/400, 1.0);
	} else {
		return min(alpha_minus * calc_point_light_attenuation(light, distance), 0.0);
	}
}

vec3 blend_hard_light(vec3 bg, vec3 fg) // base_color, fore_color // Unused
{
	float red = (fg.r < 0.5) ? (2.0 * bg.r * fg.r) : (1.0 - 2.0 * (1.0 - bg.r) * (1.0 - fg.r));
	float green = (fg.g < 0.5) ? (2.0 * bg.g * fg.g) : (1.0 - 2.0 * (1.0 - bg.g) * (1.0 - fg.g));
	float blue = (fg.b < 0.5) ? (2.0 * bg.b * fg.b) : (1.0 - 2.0 * (1.0 - bg.b) * (1.0 - fg.b));

	return vec3(red, green, blue);
}

vec3 fade_color(vec3 in_color) 
{
	vec2 frag_proj = (gl_FragCoord.xy / window_size) - vec2(0.5);
	float vignette = length(frag_proj) * vignette_factor;
	in_color -= darken_screen_factor * vec3(0.5);
	in_color += vignette * vec3(0.3, -0.3, -0.3);
	return in_color;
}

void main()
{
	InstanceData instance = instance_data[instance_id];

	// Scale then translate texture coordinates:
	vec2 mask_coord = texcoord * instance.mask_coord_loc.zw + instance.mask_coord_loc.xy;
	vec4 mask = texture(textures[instance.texture_indices[3]], mask_coord);

	// TODO: Could possibly make this 0.01 or something when it comes to masks
	if (mask.a < 0.5) { discard; } // Necessary to prevent writing pixels to the z-buffer with alpha < 0.5

	vec3 output_color = vec3(0.0);
	float output_alpha = 1.0;

	if (is_shadow) {
		vec3 normal = vec3(0.0,0.0,1.0);
		float shadow_alpha = 0.6; // 0.4

		for(int i = 0; i < num_point_lights; i++) {
			shadow_alpha += calc_point_light_shadow(point_lights[i], shadow_alpha, normal, (i == instance.num_lights_affecting));
		}
		
		if (instance.num_lights_affecting == -1) {
			shadow_alpha *= max(dot(normal, dir_light.direction),0.0);
		} else {
			shadow_alpha *= (1.2f - max(dot(normal, dir_light.direction),0.0));
		}

		//if (shadow_alpha < 0.0) { discard; } // For debugging
		//if (shadow_alpha > 0.3) { discard; } // For debugging

		output_alpha = shadow_alpha;

	} else { // If is not a shadow
		vec2 diffuse_coord = texcoord * instance.diffuse_coord_loc.zw + instance.diffuse_coord_loc.xy;
		vec4 diffuse = texture(textures[instance.texture_indices[0]], diffuse_coord);
		vec3 diffuse_color = diffuse.rgb;

		vec3 normal = vec3(0.0);
		if (is_ground_piece) {
			output_alpha = max(diffuse.a - instance.transparency, 0.0);

			// Ground Piece Normals (allows for overlapping normals)
			vec2 normal1_coord = texcoord * instance.normal_coord_loc.zw + instance.normal_coord_loc.xy;
			vec2 normal2_coord = texcoord * instance.normal_add_coord_loc.zw + instance.normal_add_coord_loc.xy;
			vec3 normal1 = texture(textures[instance.texture_indices[1]], normal1_coord).rgb;
			vec3 normal2 = texture(textures[instance.texture_indices[2]], normal2_coord).rgb;
			normal1 = (normal1 * 2.0 - 1.0); normal2 = (normal2 * 2.0 - 1.0);

			normal = normalize(normal1 + normal2);
			normal.y *= -1;

		} else { // If is not a ground piece
			
			// Old dithering
			//float dither_freq = 1500.0; float cutoff = 1.5;
			//float opaqueness = (instance.transparency+0.4)*(sin((frag_proj_pos.x+1.0)/2.0*dither_freq) * sin((frag_proj_pos.y/1.777+1.0)/2.0*dither_freq) - cutoff) + 1.0;
			//float opaqueness = (instance.transparency+0.4)*(sin(gl_FragCoord.x*dither_freq) * sin(gl_FragCoord.y*dither_freq) - cutoff) + 1.0;

			// Dithering transparency
			float c = 0.05;
			float sigmoid = ( (2/3.14159) * atan(c * (frag_position.z - instance.transparency_offset)) + 1 )/2; // https://www.desmos.com/calculator/mnlwtjgzem
			float opaqueness = (1.0 - sigmoid*instance.transparency) - dither_matrix[int(gl_FragCoord.x) % 4][int(gl_FragCoord.y) % 4];
			if (opaqueness <= 0.0) { discard; }

			// Normals
			vec2 normal1_coord = texcoord * instance.normal_coord_loc.zw + instance.normal_coord_loc.xy;
			vec3 normal1 = texture(textures[instance.texture_indices[1]], normal1_coord).rgb;
			normal1 = (normal1 * 2.0 - 1.0);

			normal = mat3(instance.TBN) * normal1; // Convert rgb normal to real world space normal (using TBN) 
		}

		if ((diffuse_color.x == diffuse_color.y) && (diffuse_color.y == diffuse_color.z)) { // If pixel is greyscale, then multiply blend
			diffuse_color = diffuse_color * instance.multiply_color;
		}

		diffuse_color += instance.add_color;

		if (instance.ignore_color.r == -10.f || all(equal(diffuse_color, instance.ignore_color))) {
			output_color = diffuse_color;
		} else {
			vec3 view_direction = normalize(view_position - frag_position);

			output_color += calc_dir_light(dir_light, instance, diffuse_color, normal, view_direction);

			for(int i = 0; i < num_point_lights; i++)
				output_color += calc_point_light(point_lights[i], instance, diffuse_color, normal, view_direction);
		}
		// Light culling attempts:	(all attempts were too much for my Mac to handle unfortunately)
		//int blah = instance.num_lights_affecting;
		//int lights_thing[MAX_POINT_LIGHTS] = instance.lights_affecting;
		//for(int i = 0; i < instance.num_lights_affecting; i++) // Only do expensive lighting calculations with lights affecting this entity
		//	output_color += calc_point_light(point_lights[instance.light1], instance, diffuse_color, normal, view_direction);

		//for(int i = 0; i < num_point_lights; i++) { // Only do expensive lighting calculations with lights affecting this entity
		//	int index = lights_thing[i];
			//PointLight light = point_lights[index];
			//output_color += calc_point_light(light, instance, diffuse_color, normal, view_direction);
		//	output_color += vec3(index, index, index)/2.0;
		//}
	}

	//if (vignette_factor + darken_screen_factor > 0.0) {
		output_color = fade_color(output_color);
	//}
	frag_color = vec4(output_color, output_alpha);


	if (false) { // Useful for debugging when set to true
		frag_color.xyz = vec3(0.f);
		frag_color.xyz += frag_position.z/200.f * vec3(1.0, 0.0, 0.0);

		vec3 val = vec3(player_position, 0.0);
		float radius = length(val - frag_position);
		if (radius < 100.0) frag_color.xyz += vec3(0.0, 0.0, 1.0);
	}
}