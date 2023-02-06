#version 430

#define MAX_INSTANCES 1000
#define MAX_POINT_LIGHTS 16

// For debugging
struct TestData { vec4 test_stuff; };
layout(std430, binding = 4) buffer test_data_ssbo { TestData test_data[10]; };

// Input attributes
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texcoord;
layout (location = 2) in vec2 in_normal;

// Passed and interpolated to fragment shader
out vec2 texcoord;
out vec3 frag_position;
//out vec2 frag_proj_pos;
flat out int instance_id;

// Application data
uniform mat3 projection_matrix;
uniform float time;
uniform vec3 camera_direction; // Unfinished
uniform float camera_scale_factor_y; // Unfinished

struct InstanceData  // Needs 16-byte alignment (e.g. breaks with unpadded vec3's)
{
	mat4 transform;
	mat4 TBN;

	vec2 position;
	vec2 scale; int pad4[2];
	vec2 sprite_offset;
	vec3 sprite_normal; int pad0;
	float rotation; // If rotation == 100.0 then don't calc the transform matrix and use input transform

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
	float shininess;
	float transparency;

	int pad5;
	int num_lights_affecting;
	int lights_affecting[MAX_POINT_LIGHTS];
};
layout(std430, binding = 3) buffer instance_data_ssbo
{
	InstanceData instance_data[MAX_INSTANCES];
};
uniform int vertices_per_instance;

void main()
{
	instance_id = int(gl_VertexID/vertices_per_instance); // 4 vertices for sprite. Pass this as a flat (not interpolated) to frag shader
	
	InstanceData instance = instance_data[instance_id];

	texcoord = in_texcoord;

	vec2 local_position = in_position.xy; // Positions local to the model

	mat3 transform;
	if (instance.rotation == 100.0) {
		transform = mat3(instance.transform);
	} else {
		mat3 translate = mat3( 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, instance.position.x, instance.position.y, 1.f );

		vec2 scale_factor = vec2(1.0, dot(camera_direction, instance.sprite_normal) / camera_scale_factor_y);
		mat3 scale_camera = mat3( scale_factor.x, 0.0, 0.0, 0.0, scale_factor.y, 0.0, 0.0, 0.0, 1.0 );

		mat3 offset = mat3( 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, instance.sprite_offset.x, instance.sprite_offset.y, 1.f );

		float c = cos(instance.rotation);
		float s = sin(instance.rotation);
		mat3 rotate = mat3( c, s, 0.0, -s, c, 0.0, 0.0, 0.0, 1.0 );
		mat3 scale = mat3( instance.scale.x, 0.0, 0.0, 0.0, instance.scale.y, 0.0, 0.0, 0.0, 1.0 );

		//transform = translate * scale_camera * offset * mat3(instance.shadow_transform);
		transform = translate * scale_camera * offset * rotate * scale;
	}

	vec3 transform_pos = transform * vec3(local_position, 1.0) + vec3(in_normal * instance.extrude_size, 0.0);

	mat4 model_matrix; // TODO: Maybe don't need the stuff in else, just use tranform_pos
	if (instance.sprite_normal.z < 0.95) {
		vec3 offset_3D = vec3(0.0, instance.sprite_offset.y * instance.sprite_normal.z, -instance.sprite_offset.y * instance.sprite_normal.y);
		vec3 world_3D_position = vec3(instance.position, 0.f) + offset_3D;
		mat3 TBN_scaled = mat3(instance.TBN) * mat3(instance.scale.x,0,0, 0,instance.scale.y,0, 0,0,1);
		model_matrix = mat4(vec4(TBN_scaled[0], 0.0), vec4(-TBN_scaled[1], 0.0), vec4(TBN_scaled[2], 0.0), vec4(world_3D_position, 1.0));
	}
	else {
		model_matrix = mat4(vec4(transform[0],0.f), vec4(transform[1],0.f), vec4(0.f,0.f,1.f,0.f), vec4(transform[2].x,transform[2].y,0.f,1.f));
	}

	frag_position = vec3(model_matrix * vec4(local_position, 0.0, 1.0));

	//if (instance.wind_strength > 0.1) { // Allows variable wind_strength
	float y = local_position.y - 0.5; // Subtracting 0.5 so that bottom of tree is weighted 0 (i.e doesn't move when mutliplied below)
	float time_id = time + instance.entity_id;// * 4.0;
	float x_offset = y * instance.wind_strength * (sin(time_id/12) + cos(time_id/3)/2 - 0.9); // https://www.desmos.com/calculator/qyrlqw2ltf
	transform_pos.x += x_offset;
	frag_position.x += x_offset;
	//}

	vec3 proj_pos = projection_matrix * transform_pos;
	//frag_proj_pos = proj_pos.xy;

	gl_Position = vec4(proj_pos.xy, (-(instance.position.y + 1000) / (10000) + 1.f), 1.0); // depth maps to [0,1]
}