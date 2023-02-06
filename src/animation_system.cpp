#include "animation_system.hpp"


void update_texcoord_locs(Entity entity, std::vector<int> track, int num_sprites, int sprite_index)
{
    RenderRequest& render_request = registry.renderRequests.get(entity);

    float flip = ((float)render_request.flip_texture - 0.5f) * -2.f;
    vec2 scale = { 1.f / num_sprites * flip, 1.f };
    vec2 position = { (track[sprite_index] + render_request.flip_texture) * abs(scale.x), 0.f };

    render_request.diffuse_coord_loc = vec4(position, scale);
    if (render_request.is_normal_sprite_sheet) {
        render_request.normal_coord_loc = render_request.diffuse_coord_loc;
    }
    if (render_request.mask_id == DIFFUSE_ID::DIFFUSE_COUNT) {
        render_request.mask_coord_loc = render_request.diffuse_coord_loc;
    }
}


void AnimationSystem::step(float elapsed_ms)
{
    // Animate sprites:
    for (uint i = 0; i < registry.spriteSheets.components.size(); i++) {
        SpriteSheetAnimation& animation = registry.spriteSheets.components[i];
        if (animation.is_reference) { continue; }
        Entity entity = registry.spriteSheets.entities[i];

        float& frame_elapsed = animation.frame_elapsed_time;
        frame_elapsed += elapsed_ms;
        std::vector<int> track = animation.tracks[animation.track_index];
        if (frame_elapsed >= animation.track_intervals[animation.track_index]) {
            size_t track_length = track.size();
            
            if (animation.loop) {
                animation.sprite_index = (animation.sprite_index + 1) % track_length;
            } else {
                animation.sprite_index = min(animation.sprite_index + 1, animation.num_sprites - 1);
            }
            frame_elapsed -= animation.track_intervals[animation.track_index];

            update_texcoord_locs(entity, track, animation.num_sprites, animation.sprite_index);

        } else if (!animation.is_updated) {
            update_texcoord_locs(entity, track, animation.num_sprites, animation.sprite_index);
        }
    }

    // Animate water:
    for (int i = 0; i < registry.waters.entities.size(); i++) {
        Entity entity = registry.waters.entities[i];
        Water& water = registry.waters.components[i];
        water.time_ms += elapsed_ms;

        RenderRequest& render_request = registry.renderRequests.get(entity);
        render_request.normal_coord_loc.x += -0.001f;
        render_request.normal_add_coord_loc.y += 0.001f;
        render_request.extrude_size = sin(water.time_ms/1000.f)*10.f;
    }
}