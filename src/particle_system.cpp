// Header
#include "particle_system.hpp"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/random.hpp>

ParticleSystem::ParticleSystem() {}

void ParticleSystem::init() {}

void ParticleSystem::step(float elapsed_ms) {
	float step_seconds = elapsed_ms / 1000.f;
	auto& generators_registry = registry.particleGenerators;
	for (uint i = 0; i < generators_registry.components.size(); i++) {
		//if (i > 0) { continue; }
		Entity entity = generators_registry.entities[i];
		ParticleGenerator& generator = generators_registry.components[i];
		if (generator.enabled) { // && generator.num_particles < generator.max_particles
			generator.time_since_last += step_seconds;
			float generator_spacing = (1 / generator.frequency);
			if (generator.time_since_last > generator_spacing) {
				int num_particles = (int)(generator.time_since_last / generator_spacing);
				//generator.num_particles += num_particles;
				generator.time_since_last = generator.time_since_last - (num_particles * generator_spacing);
				createParticle(entity, generator);
			}
		}
	}

	auto& particles_registry = registry.particles;
	for (int i{ static_cast<int>(particles_registry.entities.size() - 1) }; i >= 0; --i) {
		Entity entity = particles_registry.entities[i];
		Particle& particle = particles_registry.components[i];
		particle.life_remaining_ms -= elapsed_ms;
		if (particle.life_remaining_ms < 0.f) {
			registry.remove_all_components_of(entity); // Not in the spatial_grid or anything important so it's fine to delete here
		} else if (particle.life_remaining_ms < 2000.f) {
			registry.renderRequests.get(entity).transparency += 0.02f; // Fade out when close to being destroyed
		}
	}
}