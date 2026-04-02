#pragma once
#include "header.h"

extern const unsigned char BODY[];
extern const unsigned char BODY_end[];
#define BODY_len ((__SIZE_TYPE__)(BODY_end - BODY))

extern const unsigned char HEAD[];
extern const unsigned char HEAD_end[];
#define HEAD_len ((__SIZE_TYPE__)(HEAD_end - HEAD))

#define PLANE_MESHES     5
#define LOD_LEVELS       5  // hardcoded to have easy offsets in shader code
#define MAX_ANIMATIONS   8  // not hardcoded
#define ANIMATION_FRAMES 4  // hardcoded to have easy offsets in shader code

// New layout: animation -> frame -> LOD

struct MeshFrameLod {
    const unsigned int *positions;  // [num_vertices for this LOD]
    const unsigned int *normals;    // [num_vertices for this LOD]
};

struct MeshFrame {
    struct MeshFrameLod lods[LOD_LEVELS];
};

struct MeshAnimation {
    struct MeshFrame    frames[ANIMATION_FRAMES];
};

struct MeshLod {
    unsigned int       num_vertices;
    unsigned int       num_indices;
    const unsigned short *indices;    // [num_indices]
    const unsigned int *uvs;        // [num_vertices]
};

struct Mesh {
    const float    *bbox_min;   // [3] (X,Z,Y) (currently last LOD's data)
    const float    *bbox_max;   // [3]
    float    radius;     // [1]
    unsigned int        num_animations;     // <= MAX_ANIMATIONS
    struct MeshLod  lods[LOD_LEVELS];   // static per-LOD data
    struct MeshAnimation animations[MAX_ANIMATIONS]; // animation-major
};

static int load_mesh_blob(const void *data, __SIZE_TYPE__ size, struct Mesh *mesh) {
    if (!data || !mesh) return 0;

    const char *ptr = (const char *)data;
    const char *end = ptr + size;

#define NEED(bytes)                                \
    do {                                           \
        if ((__SIZE_TYPE__)(end - ptr) < (__SIZE_TYPE__)(bytes)) \
            return 0;                              \
    } while (0)

    // -----------------------------
    // 1. Header: "VAML", version, num_lods
    // -----------------------------
    NEED(4 + 4 + 4);
    const char *magic = (const char *)ptr;
    if (magic[0] != 'V' || magic[1] != 'A' || magic[2] != 'M' || magic[3] != 'L')
        return 0;
    ptr += 4;

    unsigned int version  = *(const unsigned int *)ptr; ptr += 4;
    unsigned int num_lods = *(const unsigned int *)ptr; ptr += 4;

    if (version != 1)            return 0;
    if (num_lods != LOD_LEVELS) return 0;

    // -----------------------------
    // 2. Metadata: per LOD
    // -----------------------------
    for (unsigned int lo = 0; lo < LOD_LEVELS; ++lo)
    {
        NEED(3 * sizeof(unsigned int));

        struct MeshLod *lod = &mesh->lods[lo];

        lod->num_vertices = *(const unsigned int *)ptr; ptr += 4;
        lod->num_indices  = *(const unsigned int *)ptr; ptr += 4;
        unsigned int na       = *(const unsigned int *)ptr; ptr += 4; // num_animations for this LOD

        if (na > MAX_ANIMATIONS)
            return 0;

        if (lo == 0) {
            mesh->num_animations = na;
        } else {
            if (na != mesh->num_animations)
                return 0; // all LODs must agree
        }
    }

    // -----------------------------
    // 3. Per-LOD static data:
    //    indices, uvs, bbox_min/max, radius
    // -----------------------------
    for (unsigned int lo = 0; lo < LOD_LEVELS; ++lo)
    {
        struct MeshLod *lod = &mesh->lods[lo];

        unsigned int nv = lod->num_vertices;
        unsigned int ni = lod->num_indices;

        // Indices
        NEED((__SIZE_TYPE__)ni * sizeof(unsigned short));
        lod->indices = (const unsigned short *)ptr;
        ptr += (__SIZE_TYPE__)ni * sizeof(unsigned short);

        // UVs
        NEED((__SIZE_TYPE__)nv * sizeof(unsigned int));
        lod->uvs = (const unsigned int *)ptr;
        ptr += (__SIZE_TYPE__)nv * sizeof(unsigned int);

        // Per-LOD bbox + radius
        // (Mesh keeps only one pointer set; this will point to the last LOD's AABB/radius,
        //  same behaviour as your previous code.)
        NEED(3 * sizeof(float));
        mesh->bbox_min = (const float *)ptr;
        ptr += 3 * sizeof(float);

        NEED(3 * sizeof(float));
        mesh->bbox_max = (const float *)ptr;
        ptr += 3 * sizeof(float);

        NEED(sizeof(float));
        mesh->radius = *(const float *)ptr;
        ptr += sizeof(float);
    }

    // -----------------------------
    // 4. Animation data:
    //
    //    for each animation:
    //      uint32 frame_numbers[4]
    //      for each frame:
    //        for each LOD:
    //          positions[num_vertices_lod]
    //          normals[num_vertices_lod]
    // -----------------------------
    unsigned int na = mesh->num_animations;

    for (unsigned int ai = 0; ai < na; ++ai)
    {
        struct MeshAnimation *anim = &mesh->animations[ai];

        // frame_numbers[4]
        NEED(ANIMATION_FRAMES * sizeof(unsigned int));
        for (unsigned int fi = 0; fi < ANIMATION_FRAMES; ++fi) {
            ptr += sizeof(unsigned int);
        }

        // For each frame in this animation
        for (unsigned int fi = 0; fi < ANIMATION_FRAMES; ++fi)
        {
            struct MeshFrame *frame = &anim->frames[fi];

            // For each LOD in order
            for (unsigned int lo = 0; lo < LOD_LEVELS; ++lo)
            {
                struct MeshLod    *lod      = &mesh->lods[lo];
                struct MeshFrameLod *frameLod = &frame->lods[lo];

                unsigned int nv = lod->num_vertices;

                // positions
                NEED((__SIZE_TYPE__)nv * sizeof(unsigned int));
                frameLod->positions = (const unsigned int *)ptr;
                ptr += (__SIZE_TYPE__)nv * sizeof(unsigned int);

                // normals
                NEED((__SIZE_TYPE__)nv * sizeof(unsigned int));
                frameLod->normals = (const unsigned int *)ptr;
                ptr += (__SIZE_TYPE__)nv * sizeof(unsigned int);
            }
        }
    }

#undef NEED
    return 1;
}
