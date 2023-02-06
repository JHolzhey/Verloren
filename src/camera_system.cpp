#include "camera_system.hpp"


const float deg_90 = M_PI/2.f,
            deg_15 = M_PI/12.f,
            deg_20 = M_PI/9.f,
            deg_30 = M_PI/6.f;

const float initial_tilt = deg_90 - deg_15;

void CameraSystem::step(float elapsed_ms) 
{
	Entity entity = registry.cameras.entities[0];
	Camera& camera = registry.cameras.components[0];
	if (registry.bezierCurves.has(entity)) {
        auto& curve = registry.bezierCurves.get(entity);
        float& t = curve.t;
        if (t < 1) {
            t += elapsed_ms / curve.curve_duration_ms;
            vec4 time = {t*t*t, t*t, t, 1};
            camera.position = time * curve.basis;
        } else if (curve.has_more_joints()) {
            curve.joint_next_point();
        }
        else {
            registry.bezierCurves.remove(entity);
            return;
        }

        // interpolate camera tilt over complete duration of curve
        float total_curve_duration = curve.curve_duration_ms * curve.num_curves;
        float current_curve_duration = curve.curve_duration_ms * (curve.current_curve - 1 + t);
        float t_tilt = current_curve_duration / total_curve_duration;
        // Simple Bezier interpolation of tilt
        float tilt_coeff = t_tilt * t_tilt * (3 - 2*t_tilt);
        camera.phi = initial_tilt - tilt_coeff * deg_30;
	} else {
		vec2 player_position = registry.motions.get(registry.players.entities[0]).position;
		camera.position = player_position; // + player_motion.sprite_offset;
	}

	camera.phi += camera.phi_change;
	camera.phi = glm::clamp(camera.phi, 0.f, M_PI / 2.f); // Clamp so we don't see an upside down world
	camera.direction = { 0.f, sin(camera.phi), cos(camera.phi) };
	camera.scale_factor = { 1.f, cos(camera.phi) };
	//printf("%f : %f : %f\n", camera.direction.x, camera.direction.y, camera.direction.z);
}