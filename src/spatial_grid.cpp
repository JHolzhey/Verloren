// internal
#include "spatial_grid.hpp"


void Cell::remove_entity(int entity_index) {
	assert(entity_index >= 0 && entity_index < MAX_ENTITIES_PER_CELL && this->num_entities - 1 >= 0);
	//printf("Entity given: %d. At index: %d.    entity removed: %d.   entity at end: %d\n", e, entity_index, this->entities[entity_index], this->entities[this->num_entities - 1]);
	//printf("Num_entities before removing: %d. Array before removing:\n", this->num_entities);

	Entity entity_at_end = this->entities[this->num_entities - 1];
	this->entities[entity_index] = entity_at_end;
	Motion& motion = registry.motions.get(entity_at_end); // Kinda hacky
	motion.cell_index = entity_index;

	this->num_entities--;
	//printf("Num_entities after removing: %d\n", this->num_entities);
}
int Cell::add_entity(Entity entity) { // returns index of newly placed entity
	assert(this->num_entities >= 0 && this->num_entities < MAX_ENTITIES_PER_CELL && "Error or need to add more space to entity list");
	this->entities[this->num_entities] = entity;
	this->num_entities++;
	//printf("Num_entities after adding = %d\n", this->num_entities);
	return this->num_entities - 1;
}

int SpatialGrid::add_entity_to_cell(ivec2 cell_coords, Entity entity) {
	int index = this->grid[cell_coords.x][cell_coords.y].add_entity(entity);
	//printf("Adding entity [%d] to cell: [%d | %d] gives index: [%d]\n", (int)entity, cell_coords.x, cell_coords.y, index);
	return index;
}
void SpatialGrid::remove_entity_from_cell(ivec2 cell_coords, int entity_index, Entity e) {
	this->grid[cell_coords.x][cell_coords.y].remove_entity(entity_index);
}

void SpatialGrid::check_all_cells() {
	printf("Checking all cells\n");
	for (uint X = 0; X < NUM_CELLS; X++) {
		for (uint Y = 0; Y < NUM_CELLS; Y++) {
			Cell& cell = this->grid[X][Y];
			for (int i = 0; i < cell.num_entities; i++) {
				if (registry.motions.get(cell.entities[i]).type_mask == 128) {
					printf("Ok\n");
				}
			}
		}
	}
}

void SpatialGrid::clear_all_cells() {
	printf("Clearing all cells\n");
	for (uint X = 0; X < NUM_CELLS; X++) {
		for (uint Y = 0; Y < NUM_CELLS; Y++) {
			this->grid[X][Y].num_entities = 0;
		}
	}
}

bool SpatialGrid::are_cell_coords_out_of_bounds(ivec2 cell_coords)
{
	Room& cur_room = registry.rooms.components[0];
	return (cell_coords.x < 0 || cell_coords.y < 0 || cell_coords.x >= cur_room.grid_size.x || cell_coords.y >= cur_room.grid_size.y);
}

ivec2 SpatialGrid::get_grid_cell_coords(vec2 position)
{
	int cell_X = (int)floor(position.x / this->cell_size);
	int cell_Y = (int)floor(position.y / this->cell_size);
	return { cell_X, cell_Y };
}

// Unfinished
std::vector<Entity> SpatialGrid::query_rect(ivec2 top_left_min_XY, ivec2 bottom_right_max_XY)
{
	std::vector<Entity> entities_found; entities_found.reserve(10);
	for (int X = top_left_min_XY.x; X <= bottom_right_max_XY.x; X++) {
		for (int Y = top_left_min_XY.y; Y <= bottom_right_max_XY.y; Y++) {
			if (are_cell_coords_out_of_bounds({ X, Y })) continue;
			Cell& cell = this->grid[X][Y];
			for (int i = 0; i < cell.num_entities; i++) {
				entities_found.push_back(cell.entities[i]);
			}
		}
	}
	return entities_found;
}

std::vector<Entity> SpatialGrid::query_radius(ivec2 center_cell, float radius)
{
	std::vector<Entity> entities_found; entities_found.reserve(10);
	float cell_radius = ceil(radius / this->cell_size);
	ivec2 cell_radius_cells = ivec2(cell_radius);
	ivec2 min_XY = center_cell - cell_radius_cells;
	ivec2 max_XY = center_cell + cell_radius_cells;
	for (int X = min_XY.x; X <= max_XY.x; X++) {
		for (int Y = min_XY.y; Y <= max_XY.y; Y++) {
			if (are_cell_coords_out_of_bounds({ X, Y })) continue;
			Cell& cell = this->grid[X][Y];
			for (int i = 0; i < cell.num_entities; i++) {
				entities_found.push_back(cell.entities[i]);
			}
		}
	}
	return entities_found;
}

std::vector<Entity> SpatialGrid::query_ray_cast(vec2 line_start, vec2 line_end) // TODO: Add radius here too
{
	std::vector<Entity> entities_found; entities_found.reserve(10);
	std::vector<Cell> cells = raster_line(line_start, line_end);
	for (int i = 0; i < cells.size(); i++) {
		Cell& cell = cells[i];
		for (int j = 0; j < cell.num_entities; j++) {
			entities_found.push_back(cell.entities[j]);
		}
	}
	return entities_found;
}

// Unfinished - radius isn't working. Helper for the function below
void SpatialGrid::add_radius_cells(ivec2 center, int radius, std::vector<Cell>& cells_list, bool used_cell_grid[NUM_CELLS][NUM_CELLS])
{
	//for (int X = center.x - radius; X <= center.x + radius; X++) {
	//	for (int Y = center.y - radius; Y <= center.y + radius; Y++) {
	for (int X = center.x; X <= center.x; X++) {
		for (int Y = center.y; Y <= center.y; Y++) {
			if (are_cell_coords_out_of_bounds({ X, Y })) { continue; }
			//int dx = X - center.x;
			//int dy = Y - center.y;
			//float distance_squared = (dx * dx) + (dy * dy);

			//if (distance_squared <= (radius * radius)) {
				if ((used_cell_grid[X][Y])) {
					used_cell_grid[X][Y] = false;
					Cell& cell = this->grid[X][Y];
					cells_list.push_back(cell);
				}
			//}
		}
	}
}

std::vector<Cell> SpatialGrid::raster_line(vec2 line_start, vec2 line_end) // TODO: Add radius parameter
{
	std::vector<Cell> cells;

	vec2 line_vector = line_end - line_start;
	
	if (line_vector.x <= 0) {
		line_vector = -line_vector;
		vec2 holder = line_start;
		line_start = line_end;
		line_end = holder;
	}

	vec2 startRelativePos = line_start / (float)this->cell_size;
	vec2 endRelativePos = line_end / (float)this->cell_size;

	int lineYSign = (line_vector.y > 0) ? 1 : -1;
	float slopeM = line_vector.y / line_vector.x;

	ivec2 start_coords = ivec2(floor(startRelativePos.x), floor(startRelativePos.y));
	ivec2 end_coords = ivec2(floor(endRelativePos.x), floor(endRelativePos.y));
	int currentY = start_coords.y;

	int testRadius = 0;
	bool used_cell_grid[NUM_CELLS][NUM_CELLS];

	add_radius_cells(ivec2(start_coords.x, currentY), testRadius, cells, used_cell_grid);
	
	for (int X = start_coords.x; X <= end_coords.x - 1; X++) {
		int Y = (int)floor(slopeM * ((float)X - startRelativePos.x + 1) + startRelativePos.y);
		if (Y != currentY) {
			for (int Y0 = currentY + lineYSign; Y0 < INT_MAX; Y0 += lineYSign) {
				add_radius_cells(ivec2(X, Y0), testRadius, cells, used_cell_grid);
				if (Y0 == Y) { break; }
			}
			currentY = Y;
		}
		add_radius_cells(ivec2(X+1, Y), testRadius, cells, used_cell_grid);
	}
	if (currentY != end_coords.y) {
		for (int Y0 = currentY + lineYSign; Y0 < INT_MAX; Y0 += lineYSign) {
			add_radius_cells(ivec2(end_coords.x, Y0), testRadius, cells, used_cell_grid);
			if (Y0 == end_coords.y) { break; }
		}
	}
	return cells;
}

std::vector<Cell> SpatialGrid::raster_polygon(ComplexPolygon& polygon, float radius, bool is_only_edge)
{
	if (is_only_edge) { radius = cell_size; }

	std::vector<Cell> cells;
	// First find max extents of polygon:
	vec2 max_position = { -100000, -100000 };
	vec2 min_position = { 100000, 100000 };
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge& edge = polygon.world_edges[i];
		max_position = glm::max(max_position, edge.vertex1);
		min_position = glm::min(min_position, edge.vertex1);
		max_position = glm::max(max_position, edge.vertex2);
		min_position = glm::min(min_position, edge.vertex2);
	}
	vec2 size = max_position - min_position;
	int expand_amount = ceil(radius/cell_size);
	ivec2 min_cell = get_grid_cell_coords(min_position) - ivec2(expand_amount);
	ivec2 max_cell = get_grid_cell_coords(max_position) + ivec2(expand_amount);

	//printf("max_position = %f | %f\n", max_position.x, max_position.y);
	//printf("min_position = %f | %f\n", min_position.x, min_position.y);

	// Now test each of the cells within the max extents to see if they are in (or within given radius of) the polygon
	for (int X = min_cell.x; X <= max_cell.x; X++) {
		for (int Y = min_cell.y; Y <= max_cell.y; Y++) {
			if (are_cell_coords_out_of_bounds({ X, Y })) { continue; }
			vec2 cell_world_position = vec2(X + 0.5f, Y + 0.5f) * (float)cell_size;
			Cell& cell = this->grid[X][Y];

			if (!is_only_edge && is_point_within_polygon(polygon, cell_world_position)) {
				cells.push_back(cell);
			} else if (radius > 0.f) {
				for (uint i = 0; i < polygon.world_edges.size(); i++) {
					Edge edge = polygon.world_edges[i];
					vec2 collision = is_circle_line_colliding(cell_world_position, radius, edge.vertex1, edge.vertex2);
					if (collision.x != 0.f || collision.y != 0.f) {
						cells.push_back(cell);
						break;
					}
				}
			}
		}
	}
	return cells;
}

BBox get_bbox(vec2 position, vec2 scale, vec2 sprite_offset) // Overloaded
{
	vec2 offset = abs(scale) / 2.f; // abs in case negative scale is used for flipping
	vec2 center = position + sprite_offset;
	return {
		center.x - offset.x, center.x + offset.x,
		center.y - offset.y, center.y + offset.y,
	};
}

BBox get_bbox(const Motion& motion)
{
	return get_bbox(motion.position, motion.scale, motion.sprite_offset);
}

bool is_bbox_colliding(BBox bbox1, BBox bbox2)
{
	return (bbox1.x_low <= bbox2.x_high && bbox1.x_high >= bbox2.x_low)
		&& (bbox1.y_low <= bbox2.y_high && bbox1.y_high >= bbox2.y_low);
}

bool is_bbox_colliding(const Motion& motion1, const Motion& motion2)
{
	BBox bbox1 = get_bbox(motion1);
	BBox bbox2 = get_bbox(motion2);

	return is_bbox_colliding(bbox1, bbox2);
}

bool is_circle_colliding(const vec2 p1, const float r1, const vec2 p2, const float r2)
{
	vec2 dist = p2 - p1;
	float dist_squared = dot(dist, dist);
	float min_distance = r1 + r2;
	return dist_squared < min_distance * min_distance; // Avoid expensive sqrt calculation
}

bool is_circle_colliding(const Motion& motion1, const Motion& motion2, float radius_factor)
{
	return is_circle_colliding(motion1.position, motion1.radius* radius_factor, motion2.position, motion2.radius* radius_factor);
}

vec2 is_circle_line_colliding(vec2 circle_pos, float radius, vec2 line_start, vec2 line_end) // Without velocity
{
	vec2 line_vector = line_end - line_start;
	vec2 line_vector_unit = normalize(line_vector);
	vec2 line_normal = normalize(vec2(line_vector.y, -line_vector.x));

	vec2 start_to_center_vec = circle_pos - line_start;
	float dot_on_line = clamp(dot(line_vector_unit, start_to_center_vec), 0.f, length(line_vector));

	vec2 nearest_point_on_line = line_start + (line_vector_unit * dot_on_line);
	vec2 to_nearest_point_vector = circle_pos - nearest_point_on_line;

	float distance_to_nearest_point = length(to_nearest_point_vector); // TODO: Could use distance squared method
	if (distance_to_nearest_point <= radius) {
		//printf("multiplier: %f\n", (1.f + (radius - distance_to_nearest_point) / radius));

		float past_line = dot(line_normal, to_nearest_point_vector);

		return sign(past_line) * line_normal * distance_to_nearest_point;
	}
	return {0.f, 0.f}; // Return zero vector if not colliding
}

vec2 is_circle_line_colliding(vec2 circle_pos, float radius, vec2 velocity, vec2 line_start, vec2 line_end) // Overloaded with velocity
{
	vec2 push_velocity = { 0.f, 0.f };
	vec2 to_nearest_point_vector = is_circle_line_colliding(circle_pos, radius, line_start, line_end); // Better variable name?
	if (to_nearest_point_vector.x != 0.f || to_nearest_point_vector.y != 0.f) {
		float distance_to_nearest_point = length(to_nearest_point_vector);
		vec2 to_nearest_point_unit = normalize(to_nearest_point_vector);

		float push_extra = (1.f - distance_to_nearest_point / radius) * 5.f; // Modulate push so we don't phase through lines/polygons
		float push_amount = max(dot(-velocity, to_nearest_point_unit), 0.f);
		push_velocity += to_nearest_point_unit * (push_amount + push_extra * push_extra);
	}
	return push_velocity;
}

vec2 is_circle_polygon_colliding(ComplexPolygon& polygon, vec2 circle_pos, float radius) // Without velocity
{
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge edge = polygon.world_edges[i];
		if (!edge.is_collidable) { continue; }

		vec2 is_colliding = is_circle_line_colliding(circle_pos, radius, edge.vertex1, edge.vertex2);
		if (is_colliding.x != 0.f || is_colliding.y != 0.f) {
			return is_colliding;
		}
	}
	// If that didn't work, check if it's within the polygon:
	return vec2((float)(!polygon.is_only_edge && is_point_within_polygon(polygon, circle_pos))*-100.f);
}

vec2 is_circle_polygon_edge_colliding(ComplexPolygon& polygon, vec2 circle_pos, float radius, vec2 velocity) // With velocity
{
	vec2 push_velocity = { 0.f, 0.f };
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge edge = polygon.world_edges[i];
		if (!edge.is_collidable) { continue; }

		push_velocity += is_circle_line_colliding(circle_pos, radius, velocity + push_velocity, edge.vertex1, edge.vertex2);
	}
	//if (push_velocity.x == 0.f && push_velocity.y == 0.f) {
	//	//printf("Checking if within polygon\n");

	//	if (!polygon.is_only_edge && is_point_within_polygon(polygon, circle_pos)) {
	//		printf("It is within polygon\n");
	//		push_velocity += vec2(-100.f);
	//	}
	//}
	return push_velocity;
}

vec2 is_circle_polygon_edge_colliding(ComplexPolygon& polygon, const Motion& motion) // Overloaded
{
	return is_circle_polygon_edge_colliding(polygon, motion.position, motion.radius, motion.velocity);
}

// Modified version of: https://stackoverflow.com/a/1968345/8132000
// Returns intersection point if the lines intersect, otherwise {0,0}. Obviously breaks if the point of
// intersection is exactly at {0,0}, but that won't happen in our game especially due to floating point errors.
// Could alternatively return a large negative number if no intersection or something
vec2 get_line_intersection(vec2 p0, vec2 p1, vec2 p2, vec2 p3)
{
	vec2 s1 = p1 - p0;
	vec2 s2 = p3 - p2;

	float s, t;
	s = (-s1.y * (p0.x - p2.x) + s1.x * (p0.y - p2.y)) / (-s2.x * s1.y + s1.x * s2.y);
	t = (s2.x * (p0.y - p2.y) - s2.y * (p0.x - p2.x)) / (-s2.x * s1.y + s1.x * s2.y);

	if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
		return vec2{ p0.x + (t * s1.x), p0.y + (t * s1.y) };
	}

	return vec2(0.f); // No collision
}

bool is_line_polygon_edge_colliding(ComplexPolygon& polygon, vec2 line_start, vec2 line_end)
{
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge edge = polygon.world_edges[i];
		if (!edge.is_collidable) { continue; }
		// Test if current edge intersects with ray. If yes, increment intersections
		vec2 collision_point = get_line_intersection(edge.vertex1, edge.vertex2, line_start, line_end);
		if (collision_point.x != 0.f || collision_point.y != 0.f) {
			return true;
		}
	}
	return false;
}

bool is_point_within_polygon(ComplexPolygon& polygon, vec2 position)
{
	// Test the ray against all sides
	int num_intersections = 0;
	for (uint i = 0; i < polygon.world_edges.size(); i++) {
		Edge edge = polygon.world_edges[i];
		// Test if current edge intersects with ray. If yes, increment intersections
		vec2 collision_point = get_line_intersection(edge.vertex1, edge.vertex2, position, position + vec2(100000, 0));
		if (collision_point.x != 0.f || collision_point.y != 0.f) {
			num_intersections++;
		}
	}
	// If num_intersections is odd, then point is inside of polygon, else, outside of polygon
	return ((num_intersections & 1) == 1);
}

bool is_circle_pie_piece_colliding(vec2 circle_pos, float radius, vec2 pie_center_pos, float angle_min, float angle_max)
{
	return false;
}

// VERY NAIVE. Should probably be improved upon
bool box_intersects(vec2 start, vec2 stop, Motion& motion) {
	BBox box = get_bbox(motion);

	// Check diagonal lines between corners
	vec2 box_lines[8] = {
		{box.x_low, box.y_low}, {box.x_high, box.y_low},
		{box.x_low, box.y_low}, {box.x_low, box.y_high},
		{box.x_high, box.y_high}, {box.x_high, box.y_low},
		{box.x_high, box.y_high}, {box.x_low, box.y_high},
	};

	for (int i = 0; i < 8; i += 2) {
		vec2 intersection = get_line_intersection(start, stop, box_lines[i], box_lines[i + 1]);
		if (intersection.x != 0.f || intersection.y != 0.f) {
			return true;
		}
	}

	return false;
}
