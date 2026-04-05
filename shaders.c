#pragma once

#ifdef __SLANG__
typedef uint8_t char;
typedef int16_t short;
#else
typedef unsigned int half2;
typedef unsigned int uint;
#endif

#define PI 3.14159265358979323846f
#define fov_y_radians (60.0f * PI / 180.0f) // vertical field of view in radians (60°)
#define aspect_ratio (16.0f / 9.0f) // screen aspect ratio (width / height)
#define near_plane 5.0f // near clipping plane distance
#define divide_by_127 (1.0/127.0)
#define divide_by_511 (1.0/511.0)
#define divide_by_32767 (1.0/32767.0)

struct uniforms {
    float camera_x, camera_y, camera_z;
    float  camera_pitch_sin, camera_pitch_cos;
    float  camera_yaw_sin, camera_yaw_cos;
    float  time, delta; // seconds
    uint   selected_object_id;
    float  drag_rect_start_x, drag_rect_start_y;
    float  drag_rect_end_x, drag_rect_end_y;
    float  click_world_pos_x, click_world_pos_z;
    float  drag_world_pos_x, drag_world_pos_z;
};
enum mode {CHUNK_PASS, OBJECT_PASS, PREFIX_PASS, SCATTER_PASS, UI_MODE, FENCE_MODE, SEA_MODE, MESH_MODE, SKY_MODE};
struct object_metadata { int meshes_offset, mesh_count; float radius;};
typedef enum object_type_enum {TERRAIN, UNIT, BUILDING, VEGETATION} object_type;
struct chunk { object_type object_type;};
struct object { float pos[2]; half2 vel; char cos, sin; char pad1,pad2;}; // ensure padding to 16b
struct mdi {uint index_count, instance_count, first_index, base_vertex, first_instance;};
struct di {uint group_count_x, group_count_y, group_count_z;};
struct instance { uint object_id, animation;};
struct unit { short x, y, next_x, next_y;};
struct bucket{uint ids[16];};
