#include "header.h"
#include "platform/platform.h"

#include "mesh.inc"
#include "map.inc"

extern const unsigned char shaders[];
extern const unsigned char shaders_end[];
#define shaders_len ((size_t)(shaders_end - shaders))

// todo: avoid globals
#pragma region GLOBAL INPUT HANDLING
int g_want_pick = 0;
int g_mouse_x = 0, g_mouse_y = 0;
int selected_object_id = 0;
int buttons[BUTTON_COUNT];
int g_drag_active = 0;
int g_drag_start_x = 0, g_drag_start_y = 0;
int g_drag_end_x   = 0, g_drag_end_y   = 0;
float cam_x = 0, cam_y = 2, cam_z = -5, cam_yaw = 0, cam_pitch = 0;
void move_forward(int amount) {
    double rad = (double)(cam_yaw) * 3.14159265f / 32767.0f;
    short x_amount = (short)lround(sin(rad) * amount);
    short z_amount = (short)lround(cos(rad) * amount);
    cam_x += x_amount;
    cam_z += z_amount;
}
void move_sideways(int amount) {
    double rad = (double)(cam_yaw) * 3.14159265f / 32767.0f;
    short x_amount = (short)lround(cos(rad) * amount);
    short z_amount = (short)lround(sin(rad) * amount);
    cam_x += x_amount;
    cam_z -= z_amount;
}
int scaled(int value) {
    static const int min_in = 0;
    static const int max_in = 32767;
    static const int min_out = 1;
    static const int max_out = 1024;
    float slope = (float)(max_out - min_out) / (max_in - min_in);
    float scale = (min_out + (cam_y - min_in) * slope);
    return (int)((float)value * scale);
}
void key_input_callback(void* ud, enum BUTTON button, enum BUTTON_STATE state) {
    if (state == PRESSED) buttons[button] = 1;
    else buttons[button] = 0;
}
void mouse_input_callback(void* ud, int x, int y, enum BUTTON button, int state) {
    if (x < 50) buttons[MOUSE_MARGIN_LEFT] = 200;
    else buttons[MOUSE_MARGIN_LEFT] = 0;
    if (x > pf_window_width() - 50) buttons[MOUSE_MARGIN_RIGHT] = 200;
    else buttons[MOUSE_MARGIN_RIGHT] = 0;
    if (y < 50) buttons[MOUSE_MARGIN_TOP] = 150;
    else buttons[MOUSE_MARGIN_TOP] = 0;
    if (y > pf_window_height() - 50) buttons[MOUSE_MARGIN_BOTTOM] = 150;
    else buttons[MOUSE_MARGIN_BOTTOM] = 0;
    if (button == MOUSE_MOVED) {
        if (g_drag_active) {
            g_drag_end_x = x;
            g_drag_end_y = y;
        }
    }
    if (button == MOUSE_SCROLL) buttons[MOUSE_SCROLL] += scaled(state);
    if (button == MOUSE_SCROLL_SIDE) buttons[MOUSE_SCROLL_SIDE] -= scaled(state / 5);
    if (button == MOUSE_LEFT || button == MOUSE_RIGHT || button == MOUSE_MIDDLE) {
        if (state == PRESSED) {
            // mouse click location
            g_mouse_x = x;
            g_mouse_y = y;
            buttons[button] = 1;
            // mouse drag location
            g_drag_active  = 1;
            g_drag_start_x = x;
            g_drag_start_y = y;
            g_drag_end_x   = x;
            g_drag_end_y   = y;
        }
        else {
            buttons[button] = 0;
            g_drag_active = 0;
        }
    }
    if (button == MOUSE_LEFT && state == PRESSED) {
        g_want_pick = 1;
    }
}
void process_inputs() {
    if (buttons[KEYBOARD_ESCAPE]) {_exit(0);}
    int amount = 1;
    if (buttons[KEYBOARD_SHIFT]) { amount = 2; }
    if (buttons[KEYBOARD_W]) { move_forward(scaled(amount));}
    if (buttons[KEYBOARD_R]) { move_forward(-scaled(amount)); }
    if (buttons[KEYBOARD_A]) { move_sideways(-scaled(amount)); }
    if (buttons[KEYBOARD_S]) { move_sideways(scaled(amount)); }
    if (buttons[MOUSE_MARGIN_LEFT]) { cam_yaw -= buttons[MOUSE_MARGIN_LEFT]; }
    if (buttons[MOUSE_MARGIN_RIGHT]) { cam_yaw += buttons[MOUSE_MARGIN_RIGHT]; }
    if (buttons[MOUSE_MARGIN_TOP]) { cam_pitch -= buttons[MOUSE_MARGIN_TOP]; }
    if (buttons[MOUSE_MARGIN_BOTTOM]) { cam_pitch += buttons[MOUSE_MARGIN_BOTTOM]; }
    if (buttons[MOUSE_SCROLL]) {
        if (cam_y + buttons[MOUSE_SCROLL] < (32767 * 16) && cam_y + buttons[MOUSE_SCROLL] > 10)
            cam_y += buttons[MOUSE_SCROLL];
        buttons[MOUSE_SCROLL] = 0;
    }
    if (buttons[MOUSE_SCROLL_SIDE]) { cam_x -= buttons[MOUSE_SCROLL_SIDE]; buttons[MOUSE_SCROLL_SIDE] = 0; }
}
#pragma endregion

#include "vulkan/vk.h"
#include "vk_util.h"
#include "vk_machine.h"
#include "vk_swapchain.h"
#include "vk_texture.h"
#include "vulkan/buffers.h"
#pragma endregion

int main(void) {
    // setup the platform window
    pf_time_reset();
    pf_create_window(APP_NAME, NULL, key_input_callback,mouse_input_callback);
    pf_timestamp("Created platform window");

#pragma region PURE C BUFFERS
#include "plane.h"
#define MAX_TOTAL_MESH_COUNT 1024
        enum mesh_type {
            MESH_NONE = -1,
            MESH_PLANE = 0,
            MESH_BODY = 1,
            MESH_HEAD = 2,
            MESH_TYPE_COUNT
        };
        int mesh_offsets[MESH_TYPE_COUNT];
        static struct Mesh meshes[MESH_TYPE_COUNT] = {
            [MESH_PLANE] = {
                .num_animations = 0, // special case with no frames, only indices
                .radius = 150.0f, // todo: we 5x it later, kinda hacky
                .lods = {
                    {.num_indices = 5766, /* lod 0: 961 quads, 31x31  */ .indices = plane_32x32 },
                    {.num_indices = 1536, /* lod 1: 225 quads, 15x15  */ .indices = plane_16x16 },
                    {.num_indices = 384,  /* lod 2: 49 quads, 7x7     */ .indices = plane_8x8 },
                    {.num_indices = 54,   /* lod 3: 9 quads, 3x3      */ .indices = plane_4x4 },
                    {.num_indices = 6,    /* lod 4: 1 quad, 128m wide */ .indices = plane_2x2 }
                }
            },
            [MESH_BODY] = {0},
            [MESH_HEAD] = {0}
        };
        if (!load_mesh_blob(BODY, BODY_len, &meshes[MESH_BODY])) {
            printf("Failed to load BODY mesh\n");
        }
        if (!load_mesh_blob(HEAD, HEAD_len, &meshes[MESH_HEAD])) {
            printf("Failed to load HEAD mesh\n");
        }
        int total_mesh_count = 0;
        for (int m = 0; m < MESH_TYPE_COUNT; ++m) {
            mesh_offsets[m] = total_mesh_count;
            int amount = meshes[m].num_animations == 0 ? 1 : meshes[m].num_animations * ANIMATION_FRAMES; // special case for no animations
            total_mesh_count += amount * LOD_LEVELS;
            printf("Mesh %d has %d animations, total frames: %d, total LODs: %d, total mesh frames: %d\n",
                m, meshes[m].num_animations, amount, LOD_LEVELS, amount * LOD_LEVELS);
            printf("Mesh ID start: %d\n", mesh_offsets[m]);
        }
        if (total_mesh_count > MAX_TOTAL_MESH_COUNT) {
            printf("Too many total mesh frames: %d > %d\n", total_mesh_count, MAX_TOTAL_MESH_COUNT);
            _exit(1);
        } else {
            printf("Total mesh frames: %d\n", total_mesh_count);
        }

        enum object_type {
            OBJECT_TYPE_PLANE = 0,
            OBJECT_TYPE_UNIT  = 1,
            OBJECT_TYPE_COUNT
        };
#define MAX_MESH_LIST_SIZE 1024
        int object_mesh_types[MAX_MESH_LIST_SIZE] = {
            MESH_PLANE,
            MESH_BODY, MESH_HEAD
        };
        int object_mesh_offsets[MAX_MESH_LIST_SIZE] = {0};
        struct gpu_object_metadata {
            int meshes_offset;
            int mesh_count;
            float radius;
        } object_metadata[OBJECT_TYPE_COUNT] = {
            [OBJECT_TYPE_PLANE] = {
                .mesh_count = 1
            },
            [OBJECT_TYPE_UNIT] = {
                .mesh_count = 2
            },
        };
        int meshes_offset = 0;
        for (int i = 0; i < OBJECT_TYPE_COUNT; ++i) {
            object_metadata[i].meshes_offset = meshes_offset;
            meshes_offset += object_metadata[i].mesh_count;
            for (int m = 0; m < object_metadata[i].mesh_count; ++m) {
                object_mesh_offsets[object_metadata[i].meshes_offset + m] = mesh_offsets[object_mesh_types[object_metadata[i].meshes_offset + m]];
            }
            object_metadata[i].radius = meshes[object_mesh_types[object_metadata[i].meshes_offset]].radius * 5.0f; // double
            printf("Object type %d has radius %f\n", i, object_metadata[i].radius);
            printf("Object type %d has %d meshes, offset %d\n", i, object_metadata[i].mesh_count, object_metadata[i].meshes_offset);
        }

        int total_vertex_count = 0;
        int total_index_count = 0;
        struct mdi mesh_info[MAX_TOTAL_MESH_COUNT];
        int mesh_index = 0;
        for (int m = 0; m < MESH_TYPE_COUNT; ++m) {
            // loop over all frames of all animations, or once in special case for animations = 0
            int amount = meshes[m].num_animations == 0 ? 1 : meshes[m].num_animations * ANIMATION_FRAMES;
            for (int i = 0; i < amount; ++i) {
                for (int lod = 0; lod < LOD_LEVELS; ++lod) {
                    mesh_info[mesh_index].first_index   = total_index_count;
                    mesh_info[mesh_index].base_vertex = total_vertex_count;
                    mesh_info[mesh_index].index_count   = meshes[m].lods[lod].num_indices;
                    total_vertex_count += meshes[m].lods[lod].num_vertices;
                    total_index_count  += meshes[m].lods[lod].num_indices;
                    mesh_index++;
                }
            }
        }

#define OBJECTS_PER_CHUNK 64

#define PLANE_CHUNK_COUNT 6400

        struct unit {short x, y, next_x, next_y;};
        struct unit units[1] = {
            {.x = 0, .y = 0, .next_x = 5, .next_y = -10}
        };
#define UNIT_CHUNK_COUNT 1000

#define TOTAL_CHUNK_COUNT (PLANE_CHUNK_COUNT + UNIT_CHUNK_COUNT)

        struct gpu_chunk { enum object_type object_type; };
        static struct gpu_chunk gpu_chunks[TOTAL_CHUNK_COUNT] = {0};
        // first 6400 are plane objects, id is zero so default zeroed works out
        static struct object_chunks { int chunk_count, first_chunk; } scene[OBJECT_TYPE_COUNT] = {
            [OBJECT_TYPE_PLANE] = { .chunk_count = PLANE_CHUNK_COUNT, .first_chunk = 0 },
            [OBJECT_TYPE_UNIT]  = { .chunk_count = UNIT_CHUNK_COUNT,  .first_chunk = PLANE_CHUNK_COUNT }
        };

        struct gpu_object { float pos[2]; float step; char cos,sin; char pad[2];}; // padding to ensure 16b
        // todo: ideally we don't even have any plane objects/chunks in memory, as their data is not needed in the shader
        static struct gpu_object gpu_objects[TOTAL_CHUNK_COUNT * OBJECTS_PER_CHUNK];

        // move the unit objects to a square grid
        int first_unit_object = PLANE_CHUNK_COUNT * OBJECTS_PER_CHUNK;
        for (int i = 0; i < UNIT_CHUNK_COUNT; ++i) {
            gpu_chunks[scene[OBJECT_TYPE_UNIT].first_chunk + i].object_type = OBJECT_TYPE_UNIT;
        }
        for (int i = 0; i < UNIT_CHUNK_COUNT * OBJECTS_PER_CHUNK; ++i) {
            gpu_objects[first_unit_object + i].pos[0] = (float)(i % 8);
            gpu_objects[first_unit_object + i].pos[1] = (float)(i / 8);
            {
                float yaw = 0.0f;
                gpu_objects[first_unit_object + i].cos = (((uint8_t)lrint(cosf(yaw) * 127.0f)));
                gpu_objects[first_unit_object + i].sin = (((uint8_t)lrint(sinf(yaw) * 127.0f)));
            }
        }

        mesh_info[0].instance_count = PLANE_CHUNK_COUNT * OBJECTS_PER_CHUNK; // hardcode first iteration for 6400 to avoid loop
        mesh_info[0].first_instance = 0;
        int total_chunk_count = PLANE_CHUNK_COUNT;
        int total_object_count = PLANE_CHUNK_COUNT * OBJECTS_PER_CHUNK;
        int total_instance_count = PLANE_CHUNK_COUNT * OBJECTS_PER_CHUNK * 1; // plane objects are made up of only one mesh
        for (int i = 0; i < TOTAL_CHUNK_COUNT - PLANE_CHUNK_COUNT; ++i) {
            int chunk_id = PLANE_CHUNK_COUNT + i;
            int type = gpu_chunks[chunk_id].object_type;
            int mesh_count = object_metadata[type].mesh_count;
            for (int m = 0; m < mesh_count; ++m) {
                // only base mesh gets the objects counted, not the lods/frames after it
                int index = mesh_offsets[object_mesh_types[object_metadata[type].meshes_offset + m]]; // mesh id is the index of the base mesh in mesh_info
                mesh_info[index].instance_count += OBJECTS_PER_CHUNK; // count up the nr of each base mesh, set first instance later
            }
            total_instance_count += OBJECTS_PER_CHUNK * mesh_count; // one instance per mesh attached to object
            total_object_count += OBJECTS_PER_CHUNK;
            total_chunk_count += 1;
        }
        // set first instance for the mesh info
        for (int m = 1; m < total_mesh_count; ++m) {
            mesh_info[m].first_instance = mesh_info[m-1].first_instance + mesh_info[m-1].instance_count;
        }

        // movement grid (131kb)
        struct block { unsigned long long tiles[4][4]; }; // 16 8b tiles per block, 128 bytes matching double cacheline, 32x32 bit per block
        static struct block passable_tiles[32][32]; // 1024x1024 grid becomes 32x32 times a 32x32 block, each block 16 64bit, 1bit for passable vs impassable

        unsigned long long size_visible_chunk_ids = total_chunk_count * sizeof(uint32_t);
        unsigned long long size_indirect_workgroups = 3 * sizeof(uint32_t);
        unsigned long long size_visible_object_count = sizeof(uint32_t);
        unsigned long long size_count_per_mesh = total_mesh_count * sizeof(uint32_t);
        unsigned long long size_offset_per_mesh = total_mesh_count * sizeof(uint32_t);
        unsigned long long size_visible_ids = total_object_count * sizeof(uint32_t);
        unsigned long long size_rendered_instances = total_instance_count * sizeof(struct instance);
        unsigned long long size_draw_calls  = total_mesh_count * sizeof(VkDrawIndexedIndirectCommand);
        int bucket_count = 131072u; // assume for now 131k buckets, which means ok collisions up to about 60k units, could be fine for more, needs to be tested
        unsigned long long size_buckets = bucket_count * 16 * sizeof(uint32_t); // 16 max units in a 2mx2m bucket, 64 bytes -> 8mb
        unsigned long long size_units = sizeof(units);
        unsigned long long size_object_mesh_list = sizeof(object_mesh_offsets);
        unsigned long long size_object_metadata = sizeof(object_metadata);
        unsigned long long size_chunks = total_chunk_count * sizeof(struct gpu_chunk);
        unsigned long long size_objects = total_object_count * sizeof(struct gpu_object);
        unsigned long long size_mesh_info = total_mesh_count * sizeof(VkDrawIndexedIndirectCommand);
        unsigned long long size_positions = total_vertex_count * sizeof(uint32_t);
        unsigned long long size_normals   = total_vertex_count * sizeof(uint32_t);
        unsigned long long size_uvs       = total_vertex_count * sizeof(uint32_t);
        unsigned long long size_indices   = total_index_count  * sizeof(uint16_t);
#pragma endregion

    pf_timestamp("CPU buffer setup done");

    // can be created purely based on settings (~30ms, ~1000ms if loading nvidia driver)
#pragma region VULKAN BASE
#if defined(_WIN32) && DEBUG_VULKAN == 1
    extern int putenv(const char*);
    extern char* getenv(const char*);
    // set env to point to vk layer path to allow finding
    const char* sdk = getenv("VULKAN_SDK");
    char buf[1024];
    snprintf(buf, sizeof buf, "VK_LAYER_PATH=%s/Bin", sdk);
    putenv(strdup(buf));
#endif
#if USE_DISCRETE_GPU == 0 && !defined(_WIN32) // set env to avoid loading nvidia icd (1000ms)
    extern int putenv(const char*);
    extern char* getenv(const char*);
    putenv((char*)"VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.x86_64.json");
    putenv((char*)"VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/intel_icd.x86_64.json");
    pf_timestamp("Setup environment");
#endif

    // setup vulkan on the machine
    struct Machine machine = create_machine();
    pf_timestamp("Logical device and queues created");

    VkCommandPool             command_pool;
    VkCommandBuffer           command_buffers_per_image[MAX_SWAPCHAIN_IMAGES];
    int                       command_buffers_recorded[MAX_SWAPCHAIN_IMAGES];

    // create command pool
    VkCommandPoolCreateInfo cmd_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = machine.queue_family_graphics,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    VK_CHECK(vkCreateCommandPool(machine.device, &cmd_pool_info, NULL, &command_pool));

    // create per-image command buffers (needs the command pool)
    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_SWAPCHAIN_IMAGES
    };
    VK_CHECK(vkAllocateCommandBuffers(machine.device, &cmd_alloc_info, command_buffers_per_image));

    // COLOR FORMAT (might change, but would require pipeline rebuild as well so better higher level)
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(machine.physical_device, machine.surface, &format_count, NULL);
    VkSurfaceFormatKHR formats[32];
    if (format_count > 32) format_count = 32;
    vkGetPhysicalDeviceSurfaceFormatsKHR(machine.physical_device, machine.surface, &format_count, formats);
    VkSurfaceFormatKHR format = formats[0];
    for (uint32_t i = 0; i < format_count; ++i) {
        if (ENABLE_HDR) {
            if (formats[i].format == VK_FORMAT_R16G16B16A16_SFLOAT) { format = formats[i]; break; }
        } else {
            if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) { format = formats[i]; break; }
        }
    }
#pragma endregion

    // can be created purely based on buffer and texture descriptions (~20ms create, ~250ms upload)
#pragma region CREATE AND UPLOAD GPU BUFFERS AND TEXTURES
    // buffers
    VkBuffer buffer_chunks; VkDeviceMemory memory_chunks;
    VkBuffer buffer_visible_chunk_ids; VkDeviceMemory memory_visible_chunk_ids;
    VkBuffer buffer_indirect_workgroups; VkDeviceMemory memory_indirect_workgroups;
    VkBuffer buffer_visible_object_count; VkDeviceMemory memory_visible_object_count;
    VkBuffer buffer_objects; VkDeviceMemory memory_objects;
    VkBuffer buffer_count_per_mesh; VkDeviceMemory memory_count_per_mesh;
    VkBuffer buffer_offset_per_mesh; VkDeviceMemory memory_offset_per_mesh;
    VkBuffer buffer_visible_ids; VkDeviceMemory memory_visible_ids;
    VkBuffer buffer_visible; VkDeviceMemory memory_visible;
    VkBuffer buffer_object_mesh_list; VkDeviceMemory memory_object_mesh_list;
    VkBuffer buffer_object_metadata; VkDeviceMemory memory_object_metadata;
    VkBuffer buffer_mesh_info; VkDeviceMemory memory_mesh_info;
    VkBuffer buffer_draw_calls; VkDeviceMemory memory_draw_calls;
    VkBuffer buffer_positions; VkDeviceMemory memory_positions;
    VkBuffer buffer_normals; VkDeviceMemory memory_normals;
    VkBuffer buffer_uvs; VkDeviceMemory memory_uvs;
    VkBuffer buffer_index_ib; VkDeviceMemory memory_indices;
    VkBuffer buffer_buckets; VkDeviceMemory memory_buckets;
    VkBuffer buffer_units; VkDeviceMemory memory_units;
    // uniforms
    VkBuffer buffer_uniforms; VkDeviceMemory memory_uniforms;
    // readback from gpu
    VkBuffer pick_readback_buffer; VkDeviceMemory pick_readback_memory;
    VkBuffer pick_region_buffer; VkDeviceMemory pick_region_memory;
    VkBuffer depth_readback_buffer; VkDeviceMemory depth_readback_memory;

    // VISIBLE CHUNK IDS
    create_buffer_and_memory(machine.device, machine.physical_device, size_visible_chunk_ids,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible_chunk_ids, &memory_visible_chunk_ids);

    // INDIRECT WORKGROUPS
    create_buffer_and_memory(machine.device, machine.physical_device, size_indirect_workgroups,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_indirect_workgroups, &memory_indirect_workgroups);

    // VISIBLE OBJECT COUNT
    create_buffer_and_memory(machine.device, machine.physical_device, size_visible_object_count,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible_object_count, &memory_visible_object_count);

    // COUNT PER MESH
    create_buffer_and_memory(machine.device, machine.physical_device, size_count_per_mesh,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_count_per_mesh, &memory_count_per_mesh);

    // OFFSET PER MESH
    create_buffer_and_memory(machine.device, machine.physical_device, size_offset_per_mesh,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_offset_per_mesh, &memory_offset_per_mesh);

    // VISIBLE OBJECT IDS
    create_buffer_and_memory(machine.device, machine.physical_device, size_visible_ids,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible_ids, &memory_visible_ids);

    // RENDERED INSTANCES
    create_buffer_and_memory(machine.device, machine.physical_device, size_rendered_instances,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible, &memory_visible);

    // DRAW_CALLS
    create_buffer_and_memory(machine.device, machine.physical_device, size_draw_calls,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_draw_calls, &memory_draw_calls);

    // BUCKETS
    create_buffer_and_memory(machine.device, machine.physical_device, size_buckets,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_buckets, &memory_buckets);

    // UNIFORMS (host visible)
    create_buffer_and_memory(machine.device, machine.physical_device, sizeof(struct uniforms),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &buffer_uniforms, &memory_uniforms);

    // UNITS
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_units, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        units, // uploaded immediately here
        &buffer_units, &memory_units, 0);

    // OBJECT MESH LIST
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_object_mesh_list, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        object_mesh_offsets, // uploaded immediately here
        &buffer_object_mesh_list, &memory_object_mesh_list, 0);

    // OBJECT METADATA
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_object_metadata, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        object_metadata, // uploaded immediately here
        &buffer_object_metadata, &memory_object_metadata, 0);

    // MESH INFO / POSITIONS / NORMALS / UVS / INDICES / CHUNKS / OBJECTS
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_mesh_info, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        mesh_info, // uploaded immediately here
        &buffer_mesh_info, &memory_mesh_info, 0);
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_positions, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        NULL, // uploaded later per mesh
        &buffer_positions, &memory_positions, 0);
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_normals, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        NULL, // uploaded later per mesh
        &buffer_normals, &memory_normals, 0);
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_uvs, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        NULL, // uploaded later per mesh
        &buffer_uvs, &memory_uvs, 0);
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        NULL, // uploaded later per mesh
        &buffer_index_ib, &memory_indices, 0);
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_chunks, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        NULL, // uploaded later per mesh
        &buffer_chunks, &memory_chunks, 0);
    create_and_upload_device_local_buffer(
        machine.device, machine.physical_device, machine.queue_graphics, command_pool,
        size_objects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        NULL, // uploaded later per mesh
        &buffer_objects, &memory_objects, 0);

    // readback object id
    create_buffer_and_memory(
        machine.device, machine.physical_device,
        sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &pick_readback_buffer, &pick_readback_memory
    );
    // readback a region
    uint32_t pick_width  = pf_window_width();
    uint32_t pick_height = pf_window_height();
    VkDeviceSize pick_max_bytes = (VkDeviceSize)pick_width * pick_height * sizeof(uint32_t);
    create_buffer_and_memory(
        machine.device, machine.physical_device,
        pick_max_bytes,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &pick_region_buffer, &pick_region_memory);
    // readback depth
    create_buffer_and_memory(
        machine.device, machine.physical_device,
        sizeof(float),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &depth_readback_buffer, &depth_readback_memory);

    pf_timestamp("Buffers created");

    // upload the data per mesh (later, when data is huge, batch into one big command to upload everything)
    mesh_index = 0;
    for (int m = 0; m < MESH_TYPE_COUNT; ++m) {
        int anim_count = meshes[m].num_animations == 0 ? 1 : meshes[m].num_animations; // special case for no animations
        int frame_count = meshes[m].num_animations == 0 ? 1 : ANIMATION_FRAMES; // special case for no animations to one frame
        for (int a = 0; a < anim_count; ++a) {
            for (int f = 0; f < frame_count; ++f) {
                for (int lod = 0; lod < LOD_LEVELS; ++lod) {
                    // vertices
                    if (meshes[m].animations[a].frames[f].lods[lod].positions) {
                        VkDeviceSize bytes = meshes[m].lods[lod].num_vertices * sizeof(uint32_t);
                        VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].base_vertex * sizeof(uint32_t);
                        // Create a temp staging and copy into the already-created DEVICE_LOCAL buffer at dstOfs
                        // Reuse the helper by creating a temp device-local “alias” of same buffer & memory? Simpler: a small inline staging+copy:
                        {
                            VkBuffer staging; VkDeviceMemory staging_mem;
                            create_buffer_and_memory(machine.device, machine.physical_device, bytes,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &staging, &staging_mem);
                            upload_to_buffer(machine.device, staging_mem, 0, meshes[m].animations[a].frames[f].lods[lod].positions, (size_t)bytes);
                            VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
                            VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
                            vkCmdCopyBuffer(cmd, staging, buffer_positions, 1, &c);
                            end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
                            vkDestroyBuffer(machine.device, staging, NULL);
                            vkFreeMemory(machine.device, staging_mem, NULL);
                        }
                    }
                    // normals
                    if (meshes[m].animations[a].frames[f].lods[lod].normals) {
                        VkDeviceSize bytes = meshes[m].lods[lod].num_vertices * sizeof(uint32_t);
                        VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].base_vertex * sizeof(uint32_t);
                        VkBuffer staging; VkDeviceMemory staging_mem;
                        create_buffer_and_memory(machine.device, machine.physical_device, bytes,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &staging, &staging_mem);
                        upload_to_buffer(machine.device, staging_mem, 0, meshes[m].animations[a].frames[f].lods[lod].normals, (size_t)bytes);
                        VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
                        VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
                        vkCmdCopyBuffer(cmd, staging, buffer_normals, 1, &c);
                        end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
                        vkDestroyBuffer(machine.device, staging, NULL);
                        vkFreeMemory(machine.device, staging_mem, NULL);
                    }
                    // uvs
                    if (meshes[m].lods[lod].uvs) {
                        VkDeviceSize bytes = meshes[m].lods[lod].num_vertices * sizeof(uint32_t);
                        VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].base_vertex * sizeof(uint32_t);
                        VkBuffer staging; VkDeviceMemory staging_mem;
                        create_buffer_and_memory(machine.device, machine.physical_device, bytes,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &staging, &staging_mem);
                        upload_to_buffer(machine.device, staging_mem, 0, meshes[m].lods[lod].uvs, (size_t)bytes);
                        VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
                        VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
                        vkCmdCopyBuffer(cmd, staging, buffer_uvs, 1, &c);
                        end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
                        vkDestroyBuffer(machine.device, staging, NULL);
                        vkFreeMemory(machine.device, staging_mem, NULL);
                    }
                    // indices
                    {
                        VkDeviceSize bytes = meshes[m].lods[lod].num_indices * sizeof(uint16_t);
                        VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].first_index * sizeof(uint16_t);
                        VkBuffer staging; VkDeviceMemory staging_mem;
                        create_buffer_and_memory(machine.device, machine.physical_device, bytes,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &staging, &staging_mem);
                        upload_to_buffer(machine.device, staging_mem, 0, meshes[m].lods[lod].indices, (size_t)bytes);
                        VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
                        VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
                        vkCmdCopyBuffer(cmd, staging, buffer_index_ib, 1, &c);
                        end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
                        vkDestroyBuffer(machine.device, staging, NULL);
                        vkFreeMemory(machine.device, staging_mem, NULL);
                    }
                    mesh_index++;
                }
            }
        }
    }

    // chunks
    {
        VkDeviceSize bytes = TOTAL_CHUNK_COUNT * sizeof(struct gpu_chunk);
        VkDeviceSize dstOfs = (VkDeviceSize)0 * sizeof(struct gpu_chunk);
        VkBuffer staging; VkDeviceMemory staging_mem;
        create_buffer_and_memory(machine.device, machine.physical_device, bytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging, &staging_mem);
        upload_to_buffer(machine.device, staging_mem, 0, gpu_chunks, (size_t)bytes);
        VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
        VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
        vkCmdCopyBuffer(cmd, staging, buffer_chunks, 1, &c);
        end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
        vkDestroyBuffer(machine.device, staging, NULL);
        vkFreeMemory(machine.device, staging_mem, NULL);
    }

    // objects
    {
        VkDeviceSize bytes = TOTAL_CHUNK_COUNT * OBJECTS_PER_CHUNK * sizeof(struct gpu_object);
        VkDeviceSize dstOfs = (VkDeviceSize)0 * OBJECTS_PER_CHUNK * sizeof(struct gpu_object);
        VkBuffer staging; VkDeviceMemory staging_mem;
        create_buffer_and_memory(machine.device, machine.physical_device, bytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging, &staging_mem);
        upload_to_buffer(machine.device, staging_mem, 0, gpu_objects, (size_t)bytes);
        VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
        VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
        vkCmdCopyBuffer(cmd, staging, buffer_objects, 1, &c);
        end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
        vkDestroyBuffer(machine.device, staging, NULL);
        vkFreeMemory(machine.device, staging_mem, NULL);
    }

    // UPLOAD TEXTURES
    create_textures(&machine, command_pool);
    create_detail_region(531, 1041); // fills in the arrays
    upload_detail_texture_pair(&machine, command_pool,
        g_detail_terrain, g_detail_height, DETAIL_UPSCALED_W, DETAIL_UPSCALED_H);

    pf_timestamp("Uploads complete");
    #pragma endregion

    // can be created purely based on buffer and texture descriptions (~0.5ms)
#pragma region DEFINE LAYOUT BASED ON BUFFERS AND TEXTURES
    VkDescriptorSetLayout     common_set_layout; // local
    VkDescriptorPool          descriptor_pool; // local
    VkDescriptorSet           descriptor_set; // NEEDED IN FRAME
    #define BINDINGS 21
    #define UNIFORM_BINDING (BINDINGS-3)
    #define TEXTURES_BINDING (BINDINGS-2)
    #define DETAIL_TEXTURES_BINDING (BINDINGS-1)
    // LAYOUT
    VkDescriptorSetLayoutBinding bindings[BINDINGS];
    for (uint32_t i = 0; i < BINDINGS; ++i) {
        bindings[i] = (VkDescriptorSetLayoutBinding) {
            .binding         = i,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
        };
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    // uniforms
    bindings[UNIFORM_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[UNIFORM_BINDING].descriptorCount = 1;
    bindings[UNIFORM_BINDING].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    // textures
    bindings[TEXTURES_BINDING] = (VkDescriptorSetLayoutBinding) {
        .binding         = TEXTURES_BINDING,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_TEXTURES,
        .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
    };
    bindings[DETAIL_TEXTURES_BINDING] = (VkDescriptorSetLayoutBinding) {
        .binding         = DETAIL_TEXTURES_BINDING,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_DETAIL_TEXTURES,
        .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = BINDINGS,
        .pBindings    = bindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(machine.device, &set_layout_info, NULL, &common_set_layout));

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  BINDINGS - 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES + MAX_DETAIL_TEXTURES },
    };
    VkDescriptorPoolCreateInfo pool_info_desc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2,
        .poolSizeCount = (uint32_t)(sizeof(pool_sizes)/sizeof(pool_sizes[0])),
        .pPoolSizes = pool_sizes,
    };
    VK_CHECK(vkCreateDescriptorPool(machine.device, &pool_info_desc, NULL, &descriptor_pool));

    VkDescriptorSetAllocateInfo set_alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &common_set_layout
    };
    VK_CHECK(vkAllocateDescriptorSets(machine.device, &set_alloc_info, &descriptor_set));

    VkDescriptorBufferInfo buffer_infos[BINDINGS] = {
        {buffer_chunks, 0, size_chunks},
        {buffer_visible_chunk_ids, 0, size_visible_chunk_ids},
        {buffer_indirect_workgroups, 0, size_indirect_workgroups},
        {buffer_visible_object_count, 0, size_visible_object_count},
        {buffer_objects, 0, size_objects},
        {buffer_count_per_mesh, 0, size_count_per_mesh},
        {buffer_offset_per_mesh, 0, size_offset_per_mesh},
        {buffer_visible_ids, 0, size_visible_ids},
        {buffer_visible, 0, size_rendered_instances},
        {buffer_mesh_info, 0, size_mesh_info},
        {buffer_draw_calls, 0, size_draw_calls},
        {buffer_positions, 0, size_positions},
        {buffer_normals, 0, size_normals},
        {buffer_uvs, 0, size_uvs},
        {buffer_object_mesh_list, 0, size_object_mesh_list},
        {buffer_object_metadata, 0, size_object_metadata},
        {buffer_buckets, 0, size_buckets},
        {buffer_units, 0, size_units},
        {buffer_uniforms, 0, sizeof(struct uniforms)}
    };

    VkDescriptorImageInfo tex_infos[MAX_TEXTURES];
    fill_texture_descriptor_infos(tex_infos, MAX_TEXTURES);
    VkDescriptorImageInfo detail_tex_infos[MAX_DETAIL_TEXTURES];
    fill_detail_texture_descriptor_infos(detail_tex_infos, MAX_DETAIL_TEXTURES);
    VkWriteDescriptorSet writes[BINDINGS];
    for (uint32_t i = 0; i < BINDINGS; ++i) {
        writes[i] = (VkWriteDescriptorSet){0};
    }
    for (uint32_t i = 0; i < BINDINGS; ++i) {
        if (i == TEXTURES_BINDING || i == DETAIL_TEXTURES_BINDING) {
            continue;
        }
        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = i,
            .descriptorCount = 1,
            .descriptorType = (i != UNIFORM_BINDING)
                ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_infos[i]
        };
    }
    writes[TEXTURES_BINDING] = (VkWriteDescriptorSet){
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = descriptor_set,
        .dstBinding      = TEXTURES_BINDING,
        .dstArrayElement = 0,
        .descriptorCount = MAX_TEXTURES,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = tex_infos,
    };
    writes[DETAIL_TEXTURES_BINDING] = (VkWriteDescriptorSet){
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = descriptor_set,
        .dstBinding      = DETAIL_TEXTURES_BINDING,
        .dstArrayElement = 0,
        .descriptorCount = MAX_DETAIL_TEXTURES,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = detail_tex_infos,
    };
    vkUpdateDescriptorSets(machine.device, BINDINGS, writes, 0, NULL);
    pf_timestamp("Descriptors created (DEVICE_LOCAL)");
    #pragma endregion

    // create shader module (~30ms)
    VkShaderModuleCreateInfo smci={ .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize=shaders_len, .pCode=(const int*)shaders };
    VkShaderModule shader_module; VK_CHECK(vkCreateShaderModule(machine.device,&smci,NULL,&shader_module));
    pf_timestamp("create shader module");

    // use common layout from before for buffer/texture contract
    // rest can be created purely based on settings (~3ms)
#pragma region PIPELINES
    // pipelines
    VkPipelineLayout          common_pipeline_layout;
    VkPipeline                compute_pipeline;
    VkPipeline                main_pipeline;

    // COMMON LAYOUT
    VkPushConstantRange range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size   = sizeof(uint32_t)
    };
    VkPipelineLayoutCreateInfo common_pipeline_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &common_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &range
    };
    VK_CHECK(vkCreatePipelineLayout(machine.device, &common_pipeline_info, NULL, &common_pipeline_layout));

    // COMPUTE PIPELINE
    VkComputePipelineCreateInfo compute_info = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader_module, .pName = "cs_main" },
            .layout = common_pipeline_layout
        };
    VK_CHECK(vkCreateComputePipelines(machine.device,VK_NULL_HANDLE,1,&compute_info,NULL,&compute_pipeline));

    // RENDER PIPELINE
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = shader_module, .pName = "vs_main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = shader_module, .pName = "fs_main" }
    };
    VkVertexInputBindingDescription bindings_vi[1] = {
        { .binding = 0, .stride = sizeof(struct instance), .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE }
    };
    VkVertexInputAttributeDescription attrs_vi[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32_UINT, .offset = 0 },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32_UINT, .offset = 4 },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1, .pVertexBindingDescriptions   = bindings_vi,
        .vertexAttributeDescriptionCount = 2, .pVertexAttributeDescriptions = attrs_vi
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    VkViewport viewport = {
        .x = 0,
        .y = (float)pf_window_height(),   // flip Y
        .width  = (float)pf_window_width(),
        .height = -(float)pf_window_height(), // negative to flip
        .minDepth = 0.f, .maxDepth = 1.f
    };
    VkRect2D scissor = { .offset = {0,0}, .extent = {pf_window_width(), pf_window_height()}};
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        // todo: this can be dynamic in the frame loop so that we can switch swapchain size
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount  = 1, .pScissors  = &scissor
    };
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_BACK_BIT, // line for wireframe
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    VkPipelineColorBlendAttachmentState color_blend_attachment[2] = {
         { .colorWriteMask = 0xF },
         { .colorWriteMask = 0xF }
    };
    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments    = color_blend_attachment
    };
    VkGraphicsPipelineCreateInfo graphics_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState   = &multisample,
        .pColorBlendState    = &color_blend,
        .layout              = common_pipeline_layout
    };
    VkPipelineRenderingCreateInfo pr = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 2,
        // format can be derived earlier with settings, not necessarily swapchain needed here
        .pColorAttachmentFormats = (VkFormat[]){ format.format, VK_FORMAT_R32_UINT },
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
        // .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
    graphics_info.pNext = &pr;
    VkPipelineDepthStencilStateCreateInfo depth = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,          // OK to keep TRUE even for sky
      .depthCompareOp = VK_COMPARE_OP_GREATER,
    };
    graphics_info.pDepthStencilState = &depth;
    VK_CHECK(vkCreateGraphicsPipelines(machine.device, VK_NULL_HANDLE, 1, &graphics_info, NULL, &main_pipeline));
    #pragma endregion
    pf_timestamp("pipelines");

    // destroy the shader module after using it
    vkDestroyShaderModule(machine.device, shader_module,  NULL);

#pragma region VULKAN SEMAPHORES
    // render semaphore (ensure only one frame is being worked on gpu-side)
    VkSemaphoreTypeCreateInfo type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = 0
    };
    VkSemaphoreCreateInfo tsci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type_info };
    VkSemaphore render_semaphore;
    VK_CHECK(vkCreateSemaphore(machine.device, &tsci, NULL, &render_semaphore));

    // acquire image semaphore (ensure only one image is acquired at a time)
    VkSemaphore acquire_image_semaphore;
    VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VK_CHECK(vkCreateSemaphore(machine.device, &semaphore_info, NULL, &acquire_image_semaphore));
    #pragma endregion

#if DEBUG_APP == 1
    // keeping track of frames
    unsigned long long frame_id = 0;
    unsigned long long frame_id_per_slot[MAX_SWAPCHAIN_IMAGES];
    unsigned long long presented_frame_ids[MAX_SWAPCHAIN_IMAGES];
    float start_time_per_slot[MAX_SWAPCHAIN_IMAGES];
    for (unsigned int i = 0; i < MAX_SWAPCHAIN_IMAGES; ++i) {
        frame_id_per_slot[i] = -1;
        presented_frame_ids[i] = -1;
    }

    // gpu time mapping
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(machine.physical_device, &props);
    double gpu_ticks_to_ns = props.limits.timestampPeriod; // 1 tick == this many ns

    // available functions
    PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT =
    (PFN_vkGetCalibratedTimestampsEXT)vkGetDeviceProcAddr(machine.device, "vkGetCalibratedTimestampsEXT");
    if (!vkGetCalibratedTimestampsEXT) {
        printf("VK_KHR_calibrated_timestamps not supported.\n");
    }
    PFN_vkWaitForPresentKHR vkWaitForPresentKHR =
    (PFN_vkWaitForPresentKHR)vkGetDeviceProcAddr(machine.device, "vkWaitForPresentKHR");
    if (!vkWaitForPresentKHR) {
        printf("VK_KHR_present_wait not supported.\n");
    }
#endif

    struct Swapchain swapchain = create_swapchain(&machine, format.format, format.colorSpace);
    unsigned long long timeline_value = 0;
    unsigned int swap_image_index = 0;
    while (pf_poll_events()) {
        VkSemaphoreWaitInfo wi = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &render_semaphore,
            .pValues     = (uint64_t *)&timeline_value
        };
        VK_CHECK(vkWaitSemaphores(machine.device, &wi, UINT64_MAX));

        #if DEBUG_APP == 1
        if (vkWaitForPresentKHR) {
            for (uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; ++i) {
                uint64_t presented_frame_id = presented_frame_ids[i];
                if (presented_frame_id == -1) continue;
                uint64_t timeout = 0;
                // if (presented_frame_id < frame_id) timeout = UINT64_MAX;
                VkResult r = vkWaitForPresentKHR(machine.device, swapchain.swapchain, presented_frame_id, timeout); // timeout zero for non-blocking
                float present_time = (float) (pf_ns_now() - pf_ns_start()) / 1e6;
                if (r == VK_SUCCESS || r == VK_ERROR_DEVICE_LOST) { // device lost (-4) instead of success somehow...
                    printf("[%llu] presented at %.3f ms\n",presented_frame_id, present_time);
                    presented_frame_ids[i] = -1; // clear slot
                    float latency = present_time - start_time_per_slot[i];
                    // presented means its ready to be scanned out, still has to wait up to 16ms for vsync for actual scanout
                    printf("[%llu] latency until 'present' %.3f ms\n", presented_frame_id, latency);
                } else if (r == VK_TIMEOUT) printf("[%llu] still not presented at %.3f ms\n", presented_frame_id, present_time);
                else printf("[%llu] present wait FAILED: %s\n", presented_frame_id, vk_result_str(r));
            }
        } else printf("wait for present KHR not available\n");
        uint64_t last_frame_id = frame_id_per_slot[swap_image_index];
        uint32_t image = swap_image_index;
        if (last_frame_id != -1) {
            uint32_t q_first = image * QUERIES_PER_IMAGE;
            uint32_t q_last  = q_first + 1;
            uint64_t ticks[2] = {0,0};
            // calibrated sample
            uint64_t ts[2], maxDev = 0;
            double   offset_ns = 0.0;
            if (vkGetCalibratedTimestampsEXT) {
                VkCalibratedTimestampInfoEXT infos[2] = {
                    { .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT },
#ifdef _WIN32
                    { .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, .timeDomain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT }
#else
                    { .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, .timeDomain = VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT }
#endif
                };
                // Take the sample as close as possible to the read
                vkGetCalibratedTimestampsEXT(machine.device, 2, infos, ts, &maxDev);
                // Map GPU ticks -> CPU ns using your device period
                double gpu_now_ns = (double)ts[0] * gpu_ticks_to_ns;
                double cpu_now_ns = (double)ts[1];
                #ifdef _WIN32
                cpu_now_ns = (double) pf_ticks_to_ns(cpu_now_ns);
                #endif
                offset_ns = cpu_now_ns - gpu_now_ns;  // add this to any GPU ns to place on CPU timeline
            }
            // Now read the two GPU timestamps for that image (they’re done because we waited the fence)
            uint64_t t[QUERIES_PER_IMAGE] = {0};
            VkResult qr = vkGetQueryPoolResults(machine.device, swapchain.query_pool,
                                    q_first, QUERIES_PER_IMAGE, sizeof(t), t,
                                    sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
            if (qr == VK_SUCCESS) {
                double ns[QUERIES_PER_IMAGE];
                for (int i=0;i<QUERIES_PER_IMAGE;i++) ns[i] = (double)t[i] * gpu_ticks_to_ns;
                // Stage durations (ms)
                double ms_chunks          = (ns[Q_CHUNKS]          - ns[Q_BEGIN])             * 1e-6;
                double ms_objects    = (ns[Q_OBJECTS]    - ns[Q_CHUNKS])        * 1e-6;
                double ms_prepare = (ns[Q_PREPARE] - ns[Q_OBJECTS])  * 1e-6;
                double ms_scatter   = (ns[Q_SCATTER]   - ns[Q_PREPARE])*1e-6;
                double ms_buckets  = (ns[Q_BUCKETS]  - ns[Q_SCATTER])  * 1e-6;
                double ms_render    = (ns[Q_RENDER]    - ns[Q_BUCKETS]) * 1e-6;
                double ms_blit          = (ns[Q_BLIT]          - ns[Q_RENDER])  * 1e-6;
                double ms_end          = (ns[Q_END]                 - ns[Q_BLIT])        * 1e-6; // usually tiny
                double ms_total       = (ns[Q_END]                 - ns[Q_BEGIN])      * 1e-6;
                // (Optional) map to CPU timeline using your calibrated offset
                double cpu_ms[QUERIES_PER_IMAGE];
                for (int i=0;i<QUERIES_PER_IMAGE;i++) cpu_ms[i] = (offset_ns + ns[i] - pf_ns_start()) * 1e-6;
                printf("[%llu] gpu time %.3fms - %.3fms : chunks=%.3f, objects=%.3f, prepare=%.3f, scatter=%.3f, buckets=%.3f, render=%.3f, blit=%.3f, end=%.3f, [%.3f]\n",
                       (unsigned long long)last_frame_id, cpu_ms[Q_BEGIN], cpu_ms[Q_END],
                       ms_chunks, ms_objects, ms_prepare, ms_scatter, ms_buckets, ms_render, ms_blit, ms_end, ms_total);
            } else {printf("ERROR: %s\n", vk_result_str(qr));}
        }
        #endif

        // get the next swapchain image (block until one is available)
        VkResult acquire_result = vkAcquireNextImageKHR(machine.device, swapchain.swapchain, UINT64_MAX, acquire_image_semaphore, VK_NULL_HANDLE, &swap_image_index);
        // recreate the swapchain if the window resized
        // todo: the pipeline assumes screen width though, so this will be suspect
        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) { recreate_swapchain(&machine, &swapchain, format.format, format.colorSpace); continue; }
        if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) { printf("vkAcquireNextImageKHR failed: %s\n", vk_result_str(acquire_result)); break; }

        // HANDLE INPUT
        process_inputs();

#pragma region PICK OBJECT, PICK RECT, SELECT WORLD POSITION
        // click world pos
        static float click_world_x, click_world_y, click_world_z;
        static float drag_world_x, drag_world_y, drag_world_z;

        // drag rect
        float rect_x0 = 0.0f, rect_y0 = 0.0f;
        float rect_x1 = 0.0f, rect_y1 = 0.0f;
        int   rect_valid = 0;
        if (g_drag_active) {
            int sx0 = g_drag_start_x;
            int sy0 = g_drag_start_y;
            int sx1 = g_drag_end_x;
            int sy1 = g_drag_end_y;
            // normalize min/max
            int min_sx = sx0 < sx1 ? sx0 : sx1;
            int max_sx = sx0 > sx1 ? sx0 : sx1;
            int min_sy = sy0 < sy1 ? sy0 : sy1;
            int max_sy = sy0 > sy1 ? sy0 : sy1;
            // avoid degenerate rect
            if (max_sx > min_sx && max_sy > min_sy) {
                rect_valid = 1;
                rect_x0 = (float)min_sx / (float)swapchain.swapchain_extent.width;
                rect_y0 = (float)min_sy / (float)swapchain.swapchain_extent.height;
                rect_x1 = (float)max_sx / (float)swapchain.swapchain_extent.width;
                rect_y1 = (float)max_sy / (float)swapchain.swapchain_extent.height;
            }
            // printf("Drag rect screen: (%d,%d)-(%d,%d)  normalized: (%.3f,%.3f)-(%.3f,%.3f)\n",
            //     min_sx, min_sy, max_sx, max_sy,
            //     rect_x0, rect_y0, rect_x1, rect_y1
            // );
        }

        // region pick
        if (buttons[MOUSE_LEFT]) {
            // read back a rectangle of ids from the pick image
            // temp to get the rect
            // screen coords from your mouse callback (0..width-1, 0..height-1)
            int sx0 = g_drag_start_x;
            int sy0 = g_drag_start_y;
            int sx1 = g_drag_end_x;
            int sy1 = g_drag_end_y;

            // normalize to a rectangle
            int min_sx = sx0 < sx1 ? sx0 : sx1;
            int max_sx = sx0 > sx1 ? sx0 : sx1;
            int min_sy = sy0 < sy1 ? sy0 : sy1;
            int max_sy = sy0 > sy1 ? sy0 : sy1;

            // clamp to screen
            uint32_t w = swapchain.swapchain_extent.width;
            uint32_t h = swapchain.swapchain_extent.height;

            if (min_sx < 0)        min_sx = 0;
            if (min_sy < 0)        min_sy = 0;
            if (max_sx >= (int)w)  max_sx = (int)w - 1;
            if (max_sy >= (int)h)  max_sy = (int)h - 1;

            // width/height of the rectangle
            uint32_t rect_w = (uint32_t)(max_sx - min_sx + 1);
            uint32_t rect_h = (uint32_t)(max_sy - min_sy + 1);

            // early out if the rect is degenerate
            if (rect_w == 0 || rect_h == 0) {
                // nothing selected
                printf("No selection (degenerate rectangle)\n");
            }
            VkCommandBuffer cmd1 = begin_single_use_cmd(machine.device, command_pool);
            // Transition pick_image from COLOR_ATTACHMENT_OPTIMAL to TRANSFER_SRC_OPTIMAL
            VkImageMemoryBarrier2 to_src = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image         = swapchain.pick_image,
                .subresourceRange = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            };
            vkCmdPipelineBarrier2(cmd1, &(VkDependencyInfo){
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &to_src
            });
            // Copy rectangle
            VkBufferImageCopy region = {
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel   = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageOffset = { (int32_t)min_sx, (int32_t)min_sy, 0 },
                .imageExtent = { rect_w, rect_h, 1 },
                .bufferOffset = 0,
                .bufferRowLength   = 0, // 0 = tightly packed
                .bufferImageHeight = 0,
            };
            vkCmdCopyImageToBuffer(
                cmd1,
                swapchain.pick_image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                pick_region_buffer,
                1, &region
            );
            // (optional) transition back to COLOR_ATTACHMENT_OPTIMAL for next frame,
            // or you can let your render-time barrier handle oldLayout=TRANSFER_SRC_OPTIMAL
            // next frame.
            end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd1);
            // map and read the picked ids
            uint32_t* pixels = NULL;
            VkDeviceSize bytes = (VkDeviceSize)rect_w * rect_h * sizeof(uint32_t);
            VK_CHECK(vkMapMemory(machine.device, pick_region_memory, 0, bytes, 0, (void**)&pixels));
            // Temporary CPU list of IDs (we’ll dedupe after)
            static const uint32_t MAX_SELECTION = 65536;
            uint32_t tmp_ids[MAX_SELECTION];
            uint32_t tmp_count = 0;
            for (uint32_t y = 0; y < rect_h; ++y) {
                for (uint32_t x = 0; x < rect_w; ++x) {
                    uint32_t id = pixels[y * rect_w + x];
                    if (id == 0) continue; // 0 = background/no object (based on how we wrote the FS)
                    if (tmp_count >= MAX_SELECTION) break; // avoid overflow
                    tmp_ids[tmp_count++] = id;
                }
            }
            vkUnmapMemory(machine.device, pick_region_memory);
            // dedupe
            qsort(tmp_ids, tmp_count, sizeof(int), compare_int);
            // Unique pass
            uint32_t unique_count = 0;
            for (uint32_t i = 0; i < tmp_count; ++i) {
                if (i == 0 || tmp_ids[i] != tmp_ids[i - 1]) {
                    tmp_ids[unique_count++] = tmp_ids[i];
                }
            }
            // Use the result
            for (uint32_t i = 0; i < unique_count; ++i) {
                printf("Selected object: %u\n", tmp_ids[i]);
            }
        }

        // setup uniforms
        struct uniforms u = {0};
        float pitch_radians = cam_pitch / 32767.0f * PI;
        float yaw_radians = cam_yaw / 32767.0f * PI;
        u.camera_x = cam_x;
        u.camera_y = cam_y;
        u.camera_z = cam_z;
        u.camera_pitch_sin = sinf(pitch_radians);
        u.camera_pitch_cos = cosf(pitch_radians);
        u.camera_yaw_sin = sinf(yaw_radians);
        u.camera_yaw_cos = cosf(yaw_radians);

        // CLICK WORLD POSITION USING DEPTH
        if (buttons[MOUSE_RIGHT]) {
            int x, y;
            if (click_world_x == 0 && click_world_z == 0) {
                x = (uint32_t)g_drag_start_x;
                y = (uint32_t)g_drag_start_y;
            } else {
                x = (uint32_t)g_drag_end_x;
                y = (uint32_t)g_drag_end_y;
            }
            if (x >= swapchain.swapchain_extent.width || y >= swapchain.swapchain_extent.height) return 0;
            VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
            // transition depth image for read
            VkImageMemoryBarrier2 to_src = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image         = swapchain.depth_image,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0, .levelCount = 1,
                    .baseArrayLayer = 0, .layerCount = 1
                }
            };
            vkCmdPipelineBarrier2(cmd, &(VkDependencyInfo){
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &to_src
            });
            // copy 1×1 depth pixel
            VkBufferImageCopy region = {
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageOffset = { (int32_t)x, (int32_t)y, 0 },
                .imageExtent = { 1, 1, 1 },
            };
            vkCmdCopyImageToBuffer(
                cmd,
                swapchain.depth_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                depth_readback_buffer,
                1, &region
            );
            end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
            // read back the depth
            float depth = 0.0f;
            void* ptr = NULL;
            VK_CHECK(vkMapMemory(machine.device, depth_readback_memory, 0, sizeof(float), 0, &ptr));
            memcpy(&depth, ptr, sizeof(float));
            vkUnmapMemory(machine.device, depth_readback_memory);
            // interpret depth -> world position
            float proj_scale_y  = 1.0f / tanf(fov_y_radians * 0.5f); // must match shader -> need fix
            float proj_scale_x  = proj_scale_y / aspect_ratio;
            if (depth > 0.0f) {
                float ndc_x =  2.0f * ((x + 0.5f) / (float)swapchain.swapchain_extent.width)  - 1.0f;
                float ndc_y =  1.0f - 2.0f * ((y + 0.5f) / (float)swapchain.swapchain_extent.height);
                float vz_cm = near_plane / depth;
                float vx_cm = ndc_x * vz_cm / proj_scale_x;
                float vy_cm = ndc_y * vz_cm / proj_scale_y;
                float vx = vx_cm / 100.0f;
                float vy = vy_cm / 100.0f;
                float vz = vz_cm / 100.0f;
                float cp = u.camera_pitch_cos;
                float sp = u.camera_pitch_sin;
                float cy = u.camera_yaw_cos;
                float sy = u.camera_yaw_sin;
                float ypp = vy;
                float zpp = vz;
                float pos_y =  cp * ypp - sp * zpp;   // position.y in meters
                float z_yaw = sp * ypp + cp * zpp;    // yaw-space Z'
                float pos_x =  cy * vx + sy * z_yaw;  // position.x in meters
                float pos_z = -sy * vx + cy * z_yaw;  // position.z in meters

                float cam_world_x = u.camera_x / 10.0f;
                float cam_world_y = u.camera_y / 10.0f;
                float cam_world_z = u.camera_z / 10.0f;

                if (click_world_x == 0 && click_world_z == 0) {
                    click_world_x = cam_world_x + pos_x;
                    click_world_y = cam_world_y + pos_y;
                    click_world_z = cam_world_z + pos_z;
                } else {
                    drag_world_x = cam_world_x + pos_x;
                    drag_world_y = cam_world_y + pos_y;
                    drag_world_z = cam_world_z + pos_z;
                }

                printf("World position: %.3f, %.3f, %.3f\n", click_world_x, click_world_y, click_world_z);
            }
        }

        // SELECT OBJECT
        vkQueueWaitIdle(machine.queue_graphics);
        if (g_want_pick) {
            g_want_pick = 0;
            // Clamp to viewport
            if (g_mouse_x < swapchain.swapchain_extent.width && g_mouse_y < swapchain.swapchain_extent.height) {
                // Vulkan origin is top-left, your window likely matches.
                // If Y is flipped somewhere, adjust here.
                VkCommandBuffer cmd = begin_single_use_cmd(machine.device, command_pool);
                // Transition pick image to TRANSFER_SRC_OPTIMAL
                VkImageMemoryBarrier2 to_src = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image         = swapchain.pick_image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0, .levelCount = 1,
                        .baseArrayLayer = 0, .layerCount = 1
                    }
                };
                vkCmdPipelineBarrier2(cmd, &(VkDependencyInfo){
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &to_src
                });
                VkBufferImageCopy copy = {
                    .imageSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    },
                    .imageOffset = { g_mouse_x, g_mouse_y, 0 },
                    .imageExtent = { 1, 1, 1 },
                };
                vkCmdCopyImageToBuffer(
                    cmd,
                    swapchain.pick_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    pick_readback_buffer,
                    1, &copy
                );
                end_single_use_cmd(machine.device, machine.queue_graphics, command_pool, cmd);
                // read back uint
                uint32_t* id = NULL;
                VK_CHECK(vkMapMemory(machine.device, pick_readback_memory, 0, sizeof(uint32_t), 0, (void**)&id));
                uint32_t picked = *id;
                vkUnmapMemory(machine.device, pick_readback_memory);
                selected_object_id = picked;
                if (picked != 0) {
                    printf("Picked object id: %u\n", picked);
                    // interpret it as chunk/object index however you want
                } else {
                    printf("No object under cursor\n");
                }
                // Next frame, your barrier before rendering sets layout back to COLOR_ATTACHMENT_OPTIMAL,
                // starting from TRANSFER_SRC_OPTIMAL (you can update oldLayout accordingly).
            }
        }
#pragma endregion

        #pragma region upload uniforms
        static float frame_time = 0;
        float current_time = (float) (pf_ns_now() - pf_ns_start()) / 1e9; // seconds since application startup
        u.time = frame_time; // seconds since application startup
        u.delta = frame_time == 0 ? 0 : current_time - frame_time;
        frame_time = current_time;
        u.selected_object_id = selected_object_id;
        if (buttons[MOUSE_LEFT]) {
            u.drag_rect_start_x = rect_x0; u.drag_rect_start_y = rect_y0;
            u.drag_rect_end_x = rect_x1; u.drag_rect_end_y = rect_y1;
        }
        if (1 || buttons[MOUSE_RIGHT]) {
            u.drag_world_pos_x = drag_world_x;
            u.drag_world_pos_z = drag_world_z;
            u.click_world_pos_x = click_world_x;
            u.click_world_pos_z = click_world_z;
        } else {
            drag_world_x = 0; drag_world_z = 0; click_world_x = 0; click_world_z = 0;
        }
        void*dst=NULL;
        VK_CHECK(vkMapMemory(machine.device, memory_uniforms, 0, sizeof u, 0, &dst));
        memcpy(dst, &u, sizeof u);
        vkUnmapMemory(machine.device, memory_uniforms);
        #pragma endregion

        // actual recording the rendering commands
        float frame_start_time = (float) (pf_ns_now() - pf_ns_start()) / 1e6;
        VkCommandBuffer cmd = command_buffers_per_image[swap_image_index];
        if (!command_buffers_recorded[swap_image_index]) {
            VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            // VK_CHECK(vkResetCommandBuffer(cmd, 0)); // don't need to reset if recorded once and reused
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

            #if DEBUG_APP == 1
            uint32_t q0 = swap_image_index * QUERIES_PER_IMAGE + 0; // index of the first query of this frame
            vkCmdResetQueryPool(cmd, swapchain.query_pool, q0, QUERIES_PER_IMAGE);
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, swapchain.query_pool, q0 + Q_BEGIN);
            #endif

            // BLOCK UNTIL UNIFORM WRITES ARE VISIBLE
            VkMemoryBarrier2 cam = mem_barrier2(ST_HOST, AC_HWR, ST_CS | ST_VS, AC_SRD);
            cmd_barrier2(cmd, &cam, 1, NULL, 0, NULL, 0);

            // ZERO THE DRAW CALL BUFFER (todo: will not be needed anymore after adding counts pass setup)
            vkCmdFillBuffer(cmd, buffer_draw_calls, 0, size_draw_calls, 0);
            vkCmdFillBuffer(cmd, buffer_offset_per_mesh, 0, size_offset_per_mesh, 0);
            vkCmdFillBuffer(cmd, buffer_count_per_mesh, 0, size_count_per_mesh, 0);
            vkCmdFillBuffer(cmd, buffer_indirect_workgroups, 0, size_indirect_workgroups, 0);
            vkCmdFillBuffer(cmd, buffer_visible_object_count, 0, size_visible_object_count, 0);
            VkMemoryBarrier2 xfer_to_cs = mem_barrier2(ST_XFER, AC_TWR, ST_CS, AC_SRD | AC_SWR);
            cmd_barrier2(cmd, &xfer_to_cs, 1, NULL, 0, NULL, 0);

            // START COMPUTE PIPELINE
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,common_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);

            // VISIBLE CHUNKS PASS
            enum mode mode = CHUNK_PASS;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDispatch(cmd, (total_chunk_count+63)/64, 1, 1);
            VkBufferMemoryBarrier2 b[5];
            b[0] = buf_barrier2(ST_CS, AC_SWR, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, AC_IND, buffer_indirect_workgroups, 0, VK_WHOLE_SIZE);
            b[1] = buf_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD, buffer_visible_chunk_ids, 0, VK_WHOLE_SIZE);
            b[2] = buf_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD | AC_SWR, buffer_visible_ids, 0, VK_WHOLE_SIZE);
            b[3] = buf_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD | AC_SWR, buffer_visible_object_count, 0, VK_WHOLE_SIZE);
            b[4] = buf_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD | AC_SWR, buffer_count_per_mesh, 0, VK_WHOLE_SIZE);
            cmd_barrier2(cmd, NULL, 0, b, 5, NULL, 0);

            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, swapchain.query_pool, q0 + Q_CHUNKS);
            #endif

            // CLEAR COLLISION BUCKETS
            vkCmdFillBuffer(cmd, buffer_buckets, 0, size_buckets, 0);
            VkBufferMemoryBarrier2 bc_clear_to_cs =
                buf_barrier2(ST_XFER, VK_ACCESS_2_TRANSFER_WRITE_BIT, ST_CS, AC_SRD | AC_SWR, buffer_buckets, 0, VK_WHOLE_SIZE);
            cmd_barrier2(cmd, NULL, 0, &bc_clear_to_cs, 1, NULL, 0);

            // VISIBLE OBJECTS PASS
            mode = OBJECT_PASS;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDispatchIndirect(cmd, buffer_indirect_workgroups, 0);
            VkMemoryBarrier2 after_count_2 = mem_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD | AC_SWR);
            cmd_barrier2(cmd, &after_count_2, 1, NULL, 0, NULL, 0);

            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, swapchain.query_pool, q0 + Q_OBJECTS);
            #endif

            // PREFIX PASS
            mode = PREFIX_PASS;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDispatch(cmd, 1, 1, 1);
            VkBufferMemoryBarrier2 prepare_to_scatter_indirect =
                buf_barrier2(ST_CS, AC_SWR, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, AC_IND, // indirect args read by compute dispatchIndirect
                    buffer_indirect_workgroups, 0, VK_WHOLE_SIZE);
            cmd_barrier2(cmd, NULL, 0, &prepare_to_scatter_indirect, 1, NULL, 0);
            VkBufferMemoryBarrier2 prep2[2];
            prep2[0] = buf_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD,
                                    buffer_offset_per_mesh, 0, VK_WHOLE_SIZE);
            prep2[1] = buf_barrier2(ST_CS, AC_SWR, ST_CS, AC_SRD | AC_SWR,
                                    buffer_draw_calls, 0, VK_WHOLE_SIZE);
            cmd_barrier2(cmd, NULL, 0, prep2, 2, NULL, 0);

            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, swapchain.query_pool, q0 + Q_PREPARE);
            #endif

            // SCATTER PASS
            cmd_barrier2(cmd, NULL, 0, &bc_clear_to_cs, 1, NULL, 0);
            mode = SCATTER_PASS;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDispatchIndirect(cmd, buffer_indirect_workgroups, 0);
            VkBufferMemoryBarrier2 post[2];
            post[0] = buf_barrier2(ST_CS, AC_SWR, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, AC_IND, buffer_draw_calls, 0, VK_WHOLE_SIZE);
            post[1] = buf_barrier2(ST_CS, AC_SWR, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, AC_VA, buffer_visible, 0, VK_WHOLE_SIZE);
            cmd_barrier2(cmd, NULL, 0, post, 2, NULL, 0);
            // todo: BUCKET
            // barrier to read back bucket counts for bucket passes
            VkBufferMemoryBarrier2 bucket_counts_to_scan =
                buf_barrier2(ST_CS, AC_SWR,ST_CS, AC_SRD,buffer_buckets, 0, VK_WHOLE_SIZE);
            cmd_barrier2(cmd, NULL, 0, &bucket_counts_to_scan, 1, NULL, 0);

            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, swapchain.query_pool, q0 + Q_SCATTER);
            #endif

            // RENDER PASS
            VkImageMemoryBarrier2 to_depth = img_barrier2(
                VK_PIPELINE_STAGE_2_NONE, 0, ST_EFT, AC_DSWR | AC_DSRD,VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                swapchain.depth_image, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1);
            cmd_barrier2(cmd, NULL, 0, NULL, 0, &to_depth, 1);
            VkImageMemoryBarrier2 to_color =
                img_barrier2(VK_PIPELINE_STAGE_2_NONE, 0,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, AC_CWR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    swapchain.swapchain_images[swap_image_index], VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
            cmd_barrier2(cmd, NULL, 0, NULL, 0, &to_color, 1);
            VkImageMemoryBarrier2 to_pick =
                img_barrier2(VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, AC_CWR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    swapchain.pick_image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
            cmd_barrier2(cmd, NULL, 0, NULL, 0, &to_pick, 1);

            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, swapchain.query_pool, q0 + Q_BUCKETS);
            #endif

            VkRenderingAttachmentInfo color_atts[2];
            color_atts[0] = (VkRenderingAttachmentInfo){
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = swapchain.swapchain_views[swap_image_index],
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };
            color_atts[1] = (VkRenderingAttachmentInfo){
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = swapchain.pick_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,   // clear to 0 = "no object"
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = { .color = { .uint32 = {0,0,0,0} } },
            };;
            VkRenderingAttachmentInfo depth_att = {
              .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
              .imageView = swapchain.depth_view,
              .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .clearValue = { .depthStencil = { 0.0f, 0 } },
            };
            VkRenderingInfo ri_swap = {
              .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
              .renderArea = { .offset = {0,0}, .extent = swapchain.swapchain_extent },
              .layerCount = 1,
              .colorAttachmentCount = 2,
              .pColorAttachments = color_atts,
              .pDepthAttachment = &depth_att
            };
            vkCmdBeginRendering(cmd, &ri_swap);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, common_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
            // vertex buffers: [0]=VISIBLE_PACKED (instance), [1]=UVs (per-vertex)
            VkBuffer vbs[1]     = { buffer_visible };
            VkDeviceSize ofs[1] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, ofs);
            // index buffer: fixed 1152 indices (values 0..255 in your mesh order)
            vkCmdBindIndexBuffer(cmd, buffer_index_ib, 0, VK_INDEX_TYPE_UINT16);
            // ui draw
            mode = UI_MODE;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDraw(cmd, 3, 1, 0, 0); // ui fullscreen
            // map fence
            // mode = FENCE_MODE;
            // vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            // vkCmdDraw(cmd, 24, 1, 0, 0);
            // sea
            mode = SEA_MODE;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDraw(cmd, 6, 1, 0, 0);
            // one GPU-driven draw then a single fullscreen triangle for sky
            mode = MESH_MODE;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDrawIndexedIndirect(cmd, buffer_draw_calls, 0, total_mesh_count, sizeof(VkDrawIndexedIndirectCommand));
            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, swapchain.query_pool, q0 + Q_RENDER);
            #endif
            mode = SKY_MODE;
            vkCmdPushConstants(cmd, common_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(enum mode), &mode);
            vkCmdDraw(cmd, 3, 1, 0, 0); // sky fullscreen triangle
            vkCmdEndRendering(cmd);
            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, swapchain.query_pool, q0 + Q_BLIT);
            #endif
            VkImageMemoryBarrier2 to_present =
                img_barrier2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, AC_CWR, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    swapchain.swapchain_images[swap_image_index], VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
            cmd_barrier2(cmd, NULL, 0, NULL, 0, &to_present, 1);
            #if DEBUG_APP == 1
            #endif

            #if DEBUG_APP == 1
            vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, swapchain.query_pool, q0 + Q_END);
            #endif
            VK_CHECK(vkEndCommandBuffer(cmd));
            command_buffers_recorded[swap_image_index] = 1;
            printf("Recorded command buffer %d\n", swap_image_index);
        }

#if DEBUG_APP == 1
        presented_frame_ids[swap_image_index] = frame_id;
        start_time_per_slot[swap_image_index] = frame_start_time;
        frame_id_per_slot[swap_image_index] = frame_id;
        // Calibrated CPU+GPU now
        uint64_t ts[2], maxDev = 0;
        if (vkGetCalibratedTimestampsEXT) {
            VkCalibratedTimestampInfoEXT infos[2] = {
                { .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT },
                #ifdef _WIN32
                { .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, .timeDomain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT }
                #else
                { .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, .timeDomain = VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT }
                #endif
            };
            vkGetCalibratedTimestampsEXT(machine.device, 2, infos, ts, &maxDev);
            uint64_t gpu_now_ticks = ts[0];
            uint64_t cpu_now_ns    = ts[1]; // on Linux MONOTONIC is ns
        }
#endif

        // submit
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphore waits[]   = { acquire_image_semaphore }; // binary from acquire
        VkSemaphore signals[] = { swapchain.present_ready_per_image[swap_image_index], render_semaphore };
        // Bump our CPU-side notion of the timeline
        timeline_value++;
        // Values for each signal semaphore; binaries ignore their value (can be 0)
        uint64_t signal_values[] = { 0, timeline_value };
        VkTimelineSemaphoreSubmitInfo tssi = {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .signalSemaphoreValueCount = 2,
            .pSignalSemaphoreValues    = signal_values,
            .waitSemaphoreValueCount   = 0,    // (we’re not waiting on a timeline in this submit)
            .pWaitSemaphoreValues      = NULL
        };
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &tssi,
            .waitSemaphoreCount = 1, .pWaitSemaphores = waits, .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1, .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 2, .pSignalSemaphores = signals
        };
        VK_CHECK(vkQueueSubmit(machine.queue_graphics, 1, &submit_info, VK_NULL_HANDLE));

        // (E) Present: wait on render-finished
        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1, .pWaitSemaphores = &swapchain.present_ready_per_image[swap_image_index],
            .swapchainCount = 1, .pSwapchains = &swapchain.swapchain, .pImageIndices = &swap_image_index
        };
        #if DEBUG_APP == 1
        if (vkWaitForPresentKHR) {
            VkPresentIdKHR present_id_info = {
                .sType          = VK_STRUCTURE_TYPE_PRESENT_ID_KHR,
                .pNext          = NULL,
                .swapchainCount = 1,
                .pPresentIds    = (uint64_t *)&frame_id,
            };
            present_info.pNext = &present_id_info;
        }
        #endif
        VkResult present_res = vkQueuePresentKHR(machine.queue_present, &present_info);
        if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
            recreate_swapchain(&machine, &swapchain, format.format, format.colorSpace);
            continue;
        } else if (present_res != VK_SUCCESS) {
            printf("vkQueuePresentKHR failed: %d\n", present_res);
            break;
        }

        float frame_end_time = (float) (pf_ns_now() - pf_ns_start()) / 1e6;
        #if DEBUG_CPU == 1
        printf("[%llu] cpu time %.3fms - %.3fms [%.3fms]\n", frame_id, frame_start_time, frame_end_time, frame_end_time - frame_start_time);
        printf("----------------------------------------\n");
        #endif
        frame_id++;
    }

    return 0;
}
