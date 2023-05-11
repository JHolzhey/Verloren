#pragma once

#include "common.hpp"
#include "tiny_ecs.hpp"
#include "components.hpp"
#include "tiny_ecs_registry.hpp"


#define NUM_CELLS 100
#define MAX_ENTITIES_PER_CELL 20

struct Cell
{
	// High, hard coded upper limit
	Entity entities[MAX_ENTITIES_PER_CELL] = { NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,
		NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,NULL_ENTITY,
		NULL_ENTITY,NULL_ENTITY,NULL_ENTITY }; // How to initialize an array to a value?
	int num_entities = 0; // How many entities are currently held in the array above
	ivec2 coords = { 0,0 };
	Cell() {}
	Cell(ivec2 coords) : coords(coords) {}

	void remove_entity(int entity_index);
	int add_entity(Entity entity);

	friend bool operator==(const Cell& lhs, const Cell& rhs) {
		return &lhs == &rhs;
	}
};

BBox get_bbox(vec2 position, vec2 scale, vec2 sprite_offset = { 0.f,0.f });
BBox get_bbox(const Motion& motion);
bool is_bbox_colliding(const Motion& motion1, const Motion& motion2);
bool is_bbox_colliding(const BBox bbox1, const BBox bbox2);
bool is_circle_colliding(const vec2 p1, const float r1, const vec2 p2, const float r2);
bool is_circle_colliding(const Motion& motion1, const Motion& motion2, float radius_factor = 1.f);

// The below returns a non-zero 'push' vector if there is a collision, zero vector otherwise
vec2 is_circle_line_colliding(vec2 center_pos, float radius, vec2 line_start, vec2 line_end); // Without velocity
vec2 is_circle_line_colliding(vec2 center_pos, float radius, vec2 velocity, vec2 line_start, vec2 line_end); // With velocity
vec2 is_circle_polygon_edge_colliding(ComplexPolygon& polygon, vec2 center_pos, float radius, vec2 velocity);
vec2 is_circle_polygon_edge_colliding(ComplexPolygon& polygon, const Motion& motion);
vec2 is_circle_polygon_colliding(ComplexPolygon& polygon, vec2 center_pos, float radius); // Without velocity

vec2 get_line_intersection(vec2 p0, vec2 p1, vec2 p2, vec2 p3);
bool is_point_within_polygon(ComplexPolygon& polygon, vec2 position);
bool is_line_polygon_edge_colliding(ComplexPolygon& polygon, vec2 line_start, vec2 line_end);

bool box_intersects(vec2 start, vec2 stop, Motion& motion);

// Unfinished and possibly unnecessary:
ComplexPolygon outset_polygon(ComplexPolygon& polygon, float outset);

class SpatialGrid
{
public:
    static SpatialGrid& getInstance()
    {
        static SpatialGrid instance; // Guaranteed to be destroyed.
        return instance;			 // Instantiated on first use.
    }

	int num_cells = NUM_CELLS;
	int cell_size = 100;
	Cell grid[NUM_CELLS][NUM_CELLS];

	int add_entity_to_cell(ivec2 cell_coords, Entity entity);
	void remove_entity_from_cell(ivec2 cell_coords, int entity_index, Entity e);
	void clear_all_cells();
	void check_all_cells();

	bool are_cell_coords_out_of_bounds(ivec2 cell_coords);
	ivec2 get_grid_cell_coords(vec2 position);
	std::vector<Entity> query_rect(ivec2 min_XY, ivec2 bottom_right_max_XY);
	std::vector<Entity> query_radius(ivec2 center_cell, float radius);
	std::vector<Entity> query_ray_cast(vec2 line_start, vec2 line_end);

	void add_radius_cells(ivec2 center, int radius, std::vector<Cell>& cells_list, bool used_cell_grid[NUM_CELLS][NUM_CELLS]);
	std::vector<Cell> raster_line(vec2 line_start, vec2 line_end);
	std::vector<Cell> raster_polygon(ComplexPolygon& polygon, float radius, bool is_only_edge = false);

private:
    SpatialGrid() {
        printf("Initializing new Spatial Grid\n");
		for (int X = 0; X < NUM_CELLS; X++) {
			for (int Y = 0; Y < NUM_CELLS; Y++) {
				this->grid[X][Y] = Cell({ X, Y });
			}
		}
    }

    SpatialGrid(SpatialGrid const&); // Don't Implement to avoid making copies
    void operator=(SpatialGrid const&); // Don't implement
};