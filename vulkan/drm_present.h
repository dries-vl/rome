#pragma once
#include "../platform/platform.h"
#if defined(__linux__) && USE_DRM_KMS == 1

#include <errno.h>
#include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gbm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdint.h>
#include <gbm.h>
#include <xf86drmMode.h>
#include <vulkan/vulkan.h>

struct ImportedScanoutImage {
    struct gbm_bo *bo;
    uint32_t fb_id;

    uint32_t width;
    uint32_t height;
    uint32_t drm_format;
    uint64_t modifier;

    int num_planes;
    int dma_buf_fd[4];
    uint32_t stride[4];
    uint32_t offset[4];

    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;

    int in_flight;
};

struct DrmPresent {
    int drm_fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;

    struct gbm_device *gbm;
    struct ImportedScanoutImage images[2];

    int current_front;
    int pending_flip;
    int first_modeset_done;
};

static VkFormat drm_format_to_vk_format(uint32_t drm_format) {
    switch (drm_format) {
        case DRM_FORMAT_XRGB8888: return VK_FORMAT_B8G8R8A8_UNORM;
        case DRM_FORMAT_ARGB8888: return VK_FORMAT_B8G8R8A8_UNORM;
        case DRM_FORMAT_XBGR8888: return VK_FORMAT_R8G8B8A8_UNORM;
        case DRM_FORMAT_ABGR8888: return VK_FORMAT_R8G8B8A8_UNORM;
        default: return VK_FORMAT_UNDEFINED;
    }
}

static uint32_t find_memory_type(
    VkPhysicalDevice phys,
    uint32_t type_bits,
    VkMemoryPropertyFlags required)
{
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }
    return UINT32_MAX;
}

static int create_fb_for_bo(int drm_fd, struct gbm_bo *bo, uint32_t *out_fb_id) {
    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};

    int planes = gbm_bo_get_plane_count(bo);
    if (planes <= 0 || planes > 4) return 0;

    for (int i = 0; i < planes; ++i) {
        union gbm_bo_handle h = gbm_bo_get_handle_for_plane(bo, i);
        handles[i] = h.u32;
        strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        offsets[i] = gbm_bo_get_offset(bo, i);
        modifiers[i] = gbm_bo_get_modifier(bo);
    }

    uint32_t fb_id = 0;
    int r = drmModeAddFB2WithModifiers(
        drm_fd,
        gbm_bo_get_width(bo),
        gbm_bo_get_height(bo),
        gbm_bo_get_format(bo),
        handles,
        strides,
        offsets,
        modifiers,
        &fb_id,
        DRM_MODE_FB_MODIFIERS
    );
    if (r != 0) {
        r = drmModeAddFB2(
            drm_fd,
            gbm_bo_get_width(bo),
            gbm_bo_get_height(bo),
            gbm_bo_get_format(bo),
            handles,
            strides,
            offsets,
            &fb_id,
            0
        );
        if (r != 0) return 0;
    }

    *out_fb_id = fb_id;
    return 1;
}

static int import_bo_into_vulkan(
    struct Machine *m,
    struct gbm_bo *bo,
    struct ImportedScanoutImage *out)
{
    out->bo = bo;
    out->width = gbm_bo_get_width(bo);
    out->height = gbm_bo_get_height(bo);
    out->drm_format = gbm_bo_get_format(bo);
    out->modifier = gbm_bo_get_modifier(bo);
    out->num_planes = gbm_bo_get_plane_count(bo);

    if (out->num_planes <= 0 || out->num_planes > 4) return 0;

    for (int i = 0; i < 4; ++i) {
        out->dma_buf_fd[i] = -1;
        out->stride[i] = 0;
        out->offset[i] = 0;
    }

    VkFormat vk_format = drm_format_to_vk_format(out->drm_format);
    if (vk_format == VK_FORMAT_UNDEFINED) {
        printf("Unsupported DRM format 0x%x\n", out->drm_format);
        return 0;
    }

    for (int i = 0; i < out->num_planes; ++i) {
        out->dma_buf_fd[i] = gbm_bo_get_fd_for_plane(bo, i);
        out->stride[i] = gbm_bo_get_stride_for_plane(bo, i);
        out->offset[i] = gbm_bo_get_offset(bo, i);
        if (out->dma_buf_fd[i] < 0) {
            printf("gbm_bo_get_fd_for_plane failed for plane %d\n", i);
            return 0;
        }
    }

    VkSubresourceLayout plane_layouts[4];
    memset(plane_layouts, 0, sizeof(plane_layouts));
    for (int i = 0; i < out->num_planes; ++i) {
        plane_layouts[i].offset = out->offset[i];
        plane_layouts[i].rowPitch = out->stride[i];
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT mod_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
        .drmFormatModifier = out->modifier,
        .drmFormatModifierPlaneCount = (uint32_t)out->num_planes,
        .pPlaneLayouts = plane_layouts
    };

    VkExternalMemoryImageCreateInfo ext_img_ci = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = &mod_ci,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT
    };

    VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &ext_img_ci,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = vk_format,
        .extent = { out->width, out->height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VK_CHECK(vkCreateImage(m->device, &ici, NULL, &out->image));

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(m->device, out->image, &mr);

    VkImportMemoryFdInfoKHR import_fd = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        .fd = out->dma_buf_fd[0]
    };

    VkMemoryDedicatedAllocateInfo dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &import_fd,
        .image = out->image
    };

    uint32_t mt = find_memory_type(m->physical_device, mr.memoryTypeBits, 0);
    if (mt == UINT32_MAX) {
        printf("No suitable memory type for imported image\n");
        return 0;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated,
        .allocationSize = mr.size,
        .memoryTypeIndex = mt
    };

    VK_CHECK(vkAllocateMemory(m->device, &mai, NULL, &out->memory));
    VK_CHECK(vkBindImageMemory(m->device, out->image, out->memory, 0));

    VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = out->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = vk_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(m->device, &ivci, NULL, &out->view));
    return 1;
}

static void destroy_imported_image(struct Machine *m, struct ImportedScanoutImage *img) {
    if (img->view) vkDestroyImageView(m->device, img->view, NULL);
    if (img->image) vkDestroyImage(m->device, img->image, NULL);
    if (img->memory) vkFreeMemory(m->device, img->memory, NULL);

    for (int i = 0; i < img->num_planes; ++i) {
        if (img->dma_buf_fd[i] >= 0) close(img->dma_buf_fd[i]);
        img->dma_buf_fd[i] = -1;
    }

    if (img->fb_id) drmModeRmFB(pf_drm_fd(), img->fb_id);
    if (img->bo) gbm_bo_destroy(img->bo);

    memset(img, 0, sizeof(*img));
}

int drm_present_init(struct DrmPresent *p, struct Machine *m) {
    memset(p, 0, sizeof(*p));
    p->drm_fd = pf_drm_fd();
    p->connector_id = pf_drm_connector_id();
    p->crtc_id = pf_drm_crtc_id();
    p->mode = *(const drmModeModeInfo *)pf_drm_mode();
    p->current_front = -1;

    p->gbm = gbm_create_device(p->drm_fd);
    if (!p->gbm) {
        printf("gbm_create_device failed\n");
        return 0;
    }

    const uint32_t width = (uint32_t)p->mode.hdisplay;
    const uint32_t height = (uint32_t)p->mode.vdisplay;
    const uint32_t format = DRM_FORMAT_XRGB8888;

    uint64_t modifiers[] = { DRM_FORMAT_MOD_LINEAR };

    for (int i = 0; i < 2; ++i) {
        memset(&p->images[i], 0, sizeof(p->images[i]));
        for (int j = 0; j < 4; ++j) p->images[i].dma_buf_fd[j] = -1;

        struct gbm_bo *bo = gbm_bo_create_with_modifiers2(
            p->gbm,
            width,
            height,
            format,
            modifiers,
            1,
            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
        );

        if (!bo) {
            bo = gbm_bo_create(
                p->gbm,
                width,
                height,
                format,
                GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
            );
        }

        if (!bo) {
            printf("Failed to allocate GBM BO %d\n", i);
            return 0;
        }

        p->images[i].bo = bo;

        if (!create_fb_for_bo(p->drm_fd, bo, &p->images[i].fb_id)) {
            printf("Failed to create FB for GBM BO %d\n", i);
            gbm_bo_destroy(bo);
            p->images[i].bo = NULL;
            return 0;
        }

        if (!import_bo_into_vulkan(m, bo, &p->images[i])) {
            printf("Failed to import GBM BO into Vulkan %d\n", i);
            drmModeRmFB(p->drm_fd, p->images[i].fb_id);
            p->images[i].fb_id = 0;
            gbm_bo_destroy(bo);
            p->images[i].bo = NULL;
            return 0;
        }

        printf("Initialized scanout image %d: fb=%u %ux%u format=0x%x modifier=0x%llx\n",
               i,
               p->images[i].fb_id,
               p->images[i].width,
               p->images[i].height,
               p->images[i].drm_format,
               (unsigned long long)p->images[i].modifier);
    }

    return 1;
}

struct ImportedScanoutImage *drm_present_acquire_backbuffer(struct DrmPresent *p) {
    for (int i = 0; i < 2; ++i) {
        if (i == p->current_front) continue;
        if (!p->images[i].in_flight) return &p->images[i];
    }
    return NULL;
}

static struct DrmPresent *g_present_for_pageflip = NULL;

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
    (void)fd; (void)frame; (void)sec; (void)usec;
    struct ImportedScanoutImage *img = (struct ImportedScanoutImage *)data;
    img->in_flight = 0;
    if (g_present_for_pageflip) g_present_for_pageflip->pending_flip = 0;
}

int drm_present_queue_flip(struct DrmPresent *p, struct ImportedScanoutImage *img) {
    if (!img || img->fb_id == 0) {
        printf("drm_present_queue_flip: invalid framebuffer id\n");
        return 0;
    }

    if (!p->first_modeset_done) {
        printf("drmModeSetCrtc(crtc=%u, fb=%u, conn=%u, mode=%s %dx%d)\n",
               p->crtc_id, img->fb_id, p->connector_id,
               p->mode.name, p->mode.hdisplay, p->mode.vdisplay);

        int r = drmModeSetCrtc(
            p->drm_fd,
            p->crtc_id,
            img->fb_id,
            0, 0,
            &p->connector_id,
            1,
            &p->mode
        );
        if (r != 0) {
            perror("drmModeSetCrtc");
            return 0;
        }

        p->first_modeset_done = 1;
        p->current_front = (int)(img - p->images);
        return 1;
    }

    img->in_flight = 1;
    p->pending_flip = 1;
    g_present_for_pageflip = p;

    int r = drmModePageFlip(
        p->drm_fd,
        p->crtc_id,
        img->fb_id,
        DRM_MODE_PAGE_FLIP_EVENT,
        img
    );
    if (r != 0) {
        perror("drmModePageFlip");
        img->in_flight = 0;
        p->pending_flip = 0;
        return 0;
    }

    p->current_front = (int)(img - p->images);
    return 1;
}

void drm_present_handle_drm_event(struct DrmPresent *p) {
    drmEventContext ev = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .page_flip_handler = page_flip_handler
    };
    drmHandleEvent(p->drm_fd, &ev);
}

void drm_present_destroy(struct DrmPresent *p, struct Machine *m) {
    for (int i = 0; i < 2; ++i) {
        destroy_imported_image(m, &p->images[i]);
    }
    if (p->gbm) gbm_device_destroy(p->gbm);
    memset(p, 0, sizeof(*p));
}

static inline int drm_present_image_index(struct DrmPresent *p, struct ImportedScanoutImage *img) {
    (void)p;
    return (int)(img - &p->images[0]);
}
#endif
