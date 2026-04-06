#pragma once

struct CommonTargets {
    VkExtent2D extent;
    // DEPTH
    VkFormat depth_format;
    VkImage depth_image;
    VkDeviceMemory depth_memory;
    VkImageView depth_view;
    // OBJECT IDS
    VkImage pick_image;
    VkDeviceMemory pick_image_memory;
    VkImageView pick_image_view;
#if DEBUG_APP == 1
    VkQueryPool query_pool;
#endif
};


#if DEBUG_APP == 1
    enum {
        Q_BEGIN = 0, // start of frame (already have)
        Q_CHUNKS, // after vkCmdFillBuffer
        Q_OBJECTS, // after compute build visible
        Q_PREPARE, // after prepare indirect
        Q_SCATTER, // after compute->graphics barrier
        Q_BUCKETS, // just before vkCmdBeginRenderPass
        Q_RENDER, // just after vkCmdEndRenderPass
        Q_BLIT, // after vkCmdBlitImage2 + transition to PRESENT
        Q_END, // end of cmdbuf (already have)
        QUERIES_PER_IMAGE // nr of queries
    };
#endif

struct CommonTargets create_targets(struct Machine *machine) {
    struct CommonTargets targets;

    // EXTENT
#if defined(__linux__) && USE_DRM_KMS == 1
    targets.extent.width  = (uint32_t)pf_window_width();
    targets.extent.height = (uint32_t)pf_window_height();
#else
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        machine->physical_device,
        machine->surface,
        &capabilities));

    VkExtent2D desired_extent = capabilities.currentExtent;
    if (desired_extent.width == UINT32_MAX) {
        desired_extent.width  = pf_window_width();
        desired_extent.height = pf_window_height();
    }
    targets.extent = desired_extent;
#endif

    // DEPTH
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_X8_D24_UNORM_PACK32,  // optional on some platforms
        VK_FORMAT_D16_UNORM
    };
    for (uint32_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        VkFormatProperties p; vkGetPhysicalDeviceFormatProperties(machine->physical_device, candidates[i], &p);
        if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            targets.depth_format = candidates[i];
            break;
        }
    }
    if (!targets.depth_format) targets.depth_format = VK_FORMAT_D16_UNORM;
    VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format    = targets.depth_format,
        .extent    = { targets.extent.width, targets.extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples   = VK_SAMPLE_COUNT_1_BIT,
        .tiling    = VK_IMAGE_TILING_OPTIMAL,
        .usage     = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(machine->device, &ici, NULL, &targets.depth_image));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(machine->device, targets.depth_image, &req);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = find_memory_type_index(machine->physical_device, req.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(machine->device, &mai, NULL, &targets.depth_memory));
    VK_CHECK(vkBindImageMemory(machine->device, targets.depth_image, targets.depth_memory, 0));
    VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = targets.depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = targets.depth_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1
        }
    };
    VK_CHECK(vkCreateImageView(machine->device, &ivci, NULL, &targets.depth_view));

    // OBJECT IDS
    VkImageCreateInfo img = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width  = targets.extent.width,
            .height = targets.extent.height,
            .depth  = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R32_UINT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VK_CHECK(vkCreateImage(machine->device, &img, NULL, &targets.pick_image));

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(machine->device, targets.pick_image, &memReq);

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = find_memory_type_index(
            machine->physical_device,
            memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    };
    VK_CHECK(vkAllocateMemory(machine->device, &alloc, NULL, &targets.pick_image_memory));
    VK_CHECK(vkBindImageMemory(machine->device, targets.pick_image, targets.pick_image_memory, 0));

    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = targets.pick_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32_UINT,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VK_CHECK(vkCreateImageView(machine->device, &view, NULL, &targets.pick_image_view));

#if DEBUG_APP == 1
    // QUERY POOL
    VkQueryPoolCreateInfo qpci = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = MAX_SWAPCHAIN_IMAGES * QUERIES_PER_IMAGE
    };
    VK_CHECK(vkCreateQueryPool(machine->device, &qpci, NULL, &targets.query_pool));
    uint32_t total_queries = MAX_SWAPCHAIN_IMAGES * QUERIES_PER_IMAGE;
    vkResetQueryPool(machine->device, targets.query_pool, 0, total_queries);
#endif

    return targets;
}

#if USE_DRM_KMS == 0
struct Swapchain {
    // swapchain
    VkSwapchainKHR            swapchain;
    VkExtent2D                swapchain_extent;
    uint32_t                  swapchain_image_count;
    // for each image in the swapchain
    VkImage                   swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkImageView               swapchain_views[MAX_SWAPCHAIN_IMAGES];
    VkImageLayout             image_layouts[MAX_SWAPCHAIN_IMAGES];
    VkSemaphore               present_ready_per_image[MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer             framebuffers[MAX_SWAPCHAIN_IMAGES];
};

struct Swapchain create_swapchain(const struct Machine *machine, VkFormat format, VkColorSpaceKHR colorspace) {
    struct Swapchain swapchain = {0};

    // Surface capabilities and formats
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(machine->physical_device, machine->surface, &capabilities));

    VkExtent2D desired_extent = capabilities.currentExtent;
    if (desired_extent.width == UINT32_MAX) {
        // we can essentially just pick the size on wayland
        desired_extent.width  = pf_window_width();
        desired_extent.height = pf_window_height();
    }
    swapchain.swapchain_extent = desired_extent;

    uint32_t min_image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && min_image_count > capabilities.maxImageCount)
        min_image_count = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = machine->surface,
        .minImageCount    = min_image_count,
        .imageFormat      = format,
        .imageColorSpace  = colorspace,
        .imageExtent      = swapchain.swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform     = capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
#if defined(_WIN32)
        .presentMode      = VK_PRESENT_MODE_IMMEDIATE_KHR,
#else
        .presentMode      = VK_PRESENT_MODE_MAILBOX_KHR,
#endif
        .clipped          = VK_TRUE
    };

    uint32_t queue_indices[3] = { machine->queue_family_graphics, machine->queue_family_present, machine->queue_family_compute };
    if (machine->queue_family_graphics != machine->queue_family_present) {
        swapchain_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices   = queue_indices;
    } else {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(machine->device, &swapchain_info, NULL, &swapchain.swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(machine->device, swapchain.swapchain, &min_image_count, NULL));
    if(min_image_count > MAX_SWAPCHAIN_IMAGES) {printf("Too many swapchain images; can handle %d but got %d\n", MAX_SWAPCHAIN_IMAGES, min_image_count); exit(0);};
    swapchain.swapchain_image_count = min_image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(machine->device, swapchain.swapchain, &min_image_count, swapchain.swapchain_images));
    for (uint32_t i = 0; i < min_image_count; ++i) {
        swapchain.image_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    for (uint32_t i = 0; i < min_image_count; ++i) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain.swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        };
        VK_CHECK(vkCreateImageView(machine->device, &view_info, NULL, &swapchain.swapchain_views[i]));
    }

    // create per-image present semaphores
    VkSemaphoreCreateInfo si = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (uint32_t i = 0; i < swapchain.swapchain_image_count; ++i) {
        VK_CHECK(vkCreateSemaphore(machine->device, &si, NULL, &swapchain.present_ready_per_image[i]));
    }

    printf("swapchain has %d images\n", swapchain.swapchain_image_count);
    pf_timestamp("Swapchain created");
    return swapchain;
}

void destroy_swapchain(struct Machine* machine, struct Swapchain* swapchain) {
    // Present semaphores depend on image count — free them here
    if (swapchain->present_ready_per_image) {
        for (uint32_t i = 0; i < swapchain->swapchain_image_count; ++i)
            vkDestroySemaphore(machine->device, swapchain->present_ready_per_image[i], NULL);
        memset(swapchain->present_ready_per_image, 0, sizeof swapchain->present_ready_per_image);
    }
    if (swapchain->image_layouts) { memset(swapchain->image_layouts, 0, sizeof swapchain->image_layouts); }
    if (swapchain->swapchain_views) {
        for (uint32_t i = 0; i < swapchain->swapchain_image_count; ++i)
            vkDestroyImageView(machine->device, swapchain->swapchain_views[i], NULL);
        memset(swapchain->swapchain_views, 0, sizeof swapchain->swapchain_views);
    }
    // Free the heap array of VkImage handles (the images are owned by the swapchain; no vkDestroyImage)
    if (swapchain->swapchain_images) {
        memset(swapchain->swapchain_images, 0, sizeof swapchain->swapchain_images);
    }
    // swapchain
    if (swapchain->swapchain) {
        vkDestroySwapchainKHR(machine->device, swapchain->swapchain, NULL);
        swapchain->swapchain = VK_NULL_HANDLE;
    }
    swapchain->swapchain_image_count = 0;
}

static void recreate_swapchain(struct Machine* machine, struct Swapchain* swapchain, VkFormat format, VkColorSpaceKHR colorspace) {
    // Wait for GPU to finish anything that might use old swapchain resources
    vkDeviceWaitIdle(machine->device);
    destroy_swapchain(machine, swapchain);
    *swapchain = create_swapchain(machine, format, colorspace);
}
#endif
