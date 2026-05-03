#pragma once

static int has_instance_layer(const char *name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, NULL) != VK_SUCCESS)
        return 0;

    VkLayerProperties props[64];
    if (count > 64) count = 64;
    if (vkEnumerateInstanceLayerProperties(&count, props) != VK_SUCCESS)
        return 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(props[i].layerName, name) == 0)
            return 1;
    }
    return 0;
}

static int has_instance_extension(const char *name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, NULL) != VK_SUCCESS)
        return 0;

    VkExtensionProperties props[128];
    if (count > 128) count = 128;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, props) != VK_SUCCESS)
        return 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(props[i].extensionName, name) == 0)
            return 1;
    }
    return 0;
}

static int has_device_extension(VkPhysicalDevice dev, const char *name) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(dev, NULL, &count, NULL) != VK_SUCCESS)
        return 0;

    VkExtensionProperties props[256];
    if (count > 256) count = 256;
    if (vkEnumerateDeviceExtensionProperties(dev, NULL, &count, props) != VK_SUCCESS)
        return 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(props[i].extensionName, name) == 0)
            return 1;
    }
    return 0;
}

static const char *device_type_str(VkPhysicalDeviceType t) {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "cpu";
    default:                                     return "other";
    }
}

static int rate_device_type(VkPhysicalDeviceType t) {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 400;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 300;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 200;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return 100;
    default:                                     return 0;
    }
}

static int prefer_device_type_bonus(VkPhysicalDeviceType t, int use_discrete_gpu) {
    if (use_discrete_gpu) {
        return (t == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 0;
    } else {
        return (t == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? 10000 : 0;
    }
}

#if defined(__linux__) && USE_DRM_KMS == 1
static int drm_fd_matches_physical_device(VkPhysicalDevice dev, int drm_fd) {
    struct stat st;
    if (fstat(drm_fd, &st) != 0) {
        printf("fstat(drm_fd)\n");
        return 0;
    }

    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    VkPhysicalDeviceDrmPropertiesEXT drm_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT
    };
    props2.pNext = &drm_props;

    vkGetPhysicalDeviceProperties2(dev, &props2);

    if (drm_props.hasPrimary &&
        major(st.st_rdev) == (dev_t)drm_props.primaryMajor &&
        minor(st.st_rdev) == (dev_t)drm_props.primaryMinor) {
        return 1;
    }

    if (drm_props.hasRender &&
        major(st.st_rdev) == (dev_t)drm_props.renderMajor &&
        minor(st.st_rdev) == (dev_t)drm_props.renderMinor) {
        return 1;
    }

    return 0;
}
#endif

struct Machine create_machine(char *app_name, int use_discrete_gpu) {
    struct Machine machine;
    memset(&machine, 0, sizeof(machine));

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name,
        .pEngineName = app_name,
        .apiVersion = VK_API_VERSION_1_3
    };

    const char *instance_extensions[16];
    uint32_t instance_extension_count = 0;

#ifdef _WIN32
    instance_extensions[instance_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
    instance_extensions[instance_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#else
#if USE_DRM_KMS == 1
    instance_extensions[instance_extension_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
#else
    instance_extensions[instance_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
    instance_extensions[instance_extension_count++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif
#endif

#if DEBUG_VULKAN == 1
    const char *validation_layer = "VK_LAYER_KHRONOS_validation";
    const char *instance_layers[1];
    uint32_t instance_layer_count = 0;

    if (has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        instance_extensions[instance_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    } else {
        printf("Warning: %s not available; debug utils disabled.\n",
               VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (has_instance_layer(validation_layer)) {
        instance_layers[instance_layer_count++] = validation_layer;
    } else {
        printf("Warning: validation layer %s not available.\n", validation_layer);
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vk_debug_cb,
        .pUserData = NULL
    };
#else
    const char **instance_layers = NULL;
    uint32_t instance_layer_count = 0;
#endif

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = instance_extension_count,
        .ppEnabledExtensionNames = instance_extensions,
        .enabledLayerCount = instance_layer_count,
        .ppEnabledLayerNames = instance_layers,
#if DEBUG_VULKAN == 1
        .pNext = has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) ? &debug_ci : NULL
#else
        .pNext = NULL
#endif
    };

    VK_CHECK(vkCreateInstance(&instance_info, NULL, &machine.instance));

#if DEBUG_VULKAN == 1
    machine.debug_messenger = VK_NULL_HANDLE;
    if (has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        VK_CHECK(CreateDebugUtilsMessengerEXT(
            machine.instance, &debug_ci, NULL, &machine.debug_messenger));
    }
#endif

    pf_timestamp("Vulkan instance created");

#ifdef _WIN32
    {
        VkWin32SurfaceCreateInfoKHR surface_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hwnd = (HWND)pf_surface_or_hwnd(),
            .hinstance = (HINSTANCE)pf_display_or_instance()
        };
        VK_CHECK(vkCreateWin32SurfaceKHR(machine.instance, &surface_info, NULL, &machine.surface));
        pf_timestamp("Win32 surface created");
    }
#endif

#if defined(__linux__) && USE_DRM_KMS == 0
    {
        VkWaylandSurfaceCreateInfoKHR surface_info = {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = pf_display_or_instance(),
            .surface = pf_surface_or_hwnd()
        };
        VK_CHECK(vkCreateWaylandSurfaceKHR(machine.instance, &surface_info, NULL, &machine.surface));
        pf_timestamp("Wayland surface created");
    }
#endif

    uint32_t device_count = 0;
    VkResult res = vkEnumeratePhysicalDevices(machine.instance, &device_count, NULL);
    if (res != VK_SUCCESS || device_count == 0) {
        printf("No Vulkan physical devices found (res=%d, count=%u)\n", res, device_count);
        exit(1);
    }

    VkPhysicalDevice devices[16];
    if (device_count > 16) device_count = 16;
    VK_CHECK(vkEnumeratePhysicalDevices(machine.instance, &device_count, devices));

    int best_score = -1;

#if defined(__linux__) && USE_DRM_KMS == 1
    static const char *required_device_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
    };
#else
    static const char *required_device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#if DEBUG_APP == 1
        VK_KHR_PRESENT_ID_EXTENSION_NAME,
        VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
#endif
    };
#endif

    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDevice dev = devices[i];

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        printf("Found GPU: %s (%s), API %u.%u.%u\n",
               props.deviceName,
               device_type_str(props.deviceType),
               VK_API_VERSION_MAJOR(props.apiVersion),
               VK_API_VERSION_MINOR(props.apiVersion),
               VK_API_VERSION_PATCH(props.apiVersion));

        if (VK_API_VERSION_MAJOR(props.apiVersion) < 1 ||
            (VK_API_VERSION_MAJOR(props.apiVersion) == 1 &&
             VK_API_VERSION_MINOR(props.apiVersion) < 3)) {
            printf("  Rejected: Vulkan 1.3 not supported.\n");
            continue;
        }

        int missing_extension = 0;
        for (uint32_t e = 0;
             e < (uint32_t)(sizeof(required_device_extensions) / sizeof(required_device_extensions[0]));
             ++e) {
            if (!has_device_extension(dev, required_device_extensions[e])) {
                printf("  Rejected: missing device extension %s\n", required_device_extensions[e]);
                missing_extension = 1;
                break;
            }
        }
        if (missing_extension)
            continue;

#if defined(__linux__) && USE_DRM_KMS == 1
        if (!drm_fd_matches_physical_device(dev, drm_fd())) {
            printf("  Rejected: Vulkan physical device does not match selected DRM node.\n");
            continue;
        }
#endif

        uint32_t qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, NULL);
        if (qf_count == 0) {
            printf("  Rejected: no queue families.\n");
            continue;
        }

        VkQueueFamilyProperties qprops[32];
        if (qf_count > 32) qf_count = 32;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qprops);

        uint32_t q_graphics = UINT32_MAX;
        uint32_t q_compute = UINT32_MAX;
#if !defined(__linux__) || USE_DRM_KMS == 0
        uint32_t q_present = UINT32_MAX;
#endif
        uint32_t q_transfer = UINT32_MAX;

        for (uint32_t q = 0; q < qf_count; ++q) {
            if (qprops[q].queueCount == 0)
                continue;

            const VkQueueFlags flags = qprops[q].queueFlags;

            if (q_graphics == UINT32_MAX && (flags & VK_QUEUE_GRAPHICS_BIT)) {
                q_graphics = q;
            }

#if !defined(__linux__) || USE_DRM_KMS == 0
            {
                VkBool32 supports_present = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, machine.surface, &supports_present));
                if (q_present == UINT32_MAX && supports_present) {
                    q_present = q;
                }
            }
#endif

            if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT)) {
                if (q_compute == UINT32_MAX)
                    q_compute = q;
            }

            if ((flags & VK_QUEUE_TRANSFER_BIT) &&
                !(flags & VK_QUEUE_GRAPHICS_BIT) &&
                !(flags & VK_QUEUE_COMPUTE_BIT)) {
                if (q_transfer == UINT32_MAX)
                    q_transfer = q;
            }
        }

#if !defined(__linux__) || USE_DRM_KMS == 0
        if (q_graphics == UINT32_MAX || q_present == UINT32_MAX) {
            printf("  Rejected: missing graphics or present queue.\n");
            continue;
        }
#else
        if (q_graphics == UINT32_MAX) {
            printf("  Rejected: missing graphics queue.\n");
            continue;
        }
#endif

        if (q_compute == UINT32_MAX) {
            for (uint32_t q = 0; q < qf_count; ++q) {
                if (qprops[q].queueCount == 0)
                    continue;
                if (qprops[q].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    q_compute = q;
                    break;
                }
            }
        }
        if (q_compute == UINT32_MAX) {
            printf("  Rejected: missing compute queue.\n");
            continue;
        }

#if !defined(__linux__) || USE_DRM_KMS == 0
        {
            uint32_t fmt_count = 0;
            uint32_t pm_count = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(dev, machine.surface, &fmt_count, NULL));
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(dev, machine.surface, &pm_count, NULL));
            if (fmt_count == 0 || pm_count == 0) {
                printf("  Rejected: surface has no formats or present modes.\n");
                continue;
            }
        }
#endif

        VkPhysicalDeviceFeatures2 supported = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
        };
        VkPhysicalDeviceVulkan11Features supported11 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
        };
        VkPhysicalDeviceVulkan12Features supported12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
        };
        VkPhysicalDeviceVulkan13Features supported13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
        };

#if defined(__linux__) && USE_DRM_KMS == 1
        VkPhysicalDeviceDrmPropertiesEXT drm_props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT
        };
#endif

        supported.pNext = &supported11;
        supported11.pNext = &supported12;
        supported12.pNext = &supported13;
#if defined(__linux__) && USE_DRM_KMS == 1
        supported13.pNext = &drm_props;
#else
        supported13.pNext = NULL;
#endif

        vkGetPhysicalDeviceFeatures2(dev, &supported);

        if (!supported11.shaderDrawParameters) {
            printf("  Rejected: shaderDrawParameters unsupported.\n");
            continue;
        }
        if (!supported12.timelineSemaphore) {
            printf("  Rejected: timelineSemaphore unsupported.\n");
            continue;
        }
        if (!supported12.bufferDeviceAddress) {
            printf("  Rejected: bufferDeviceAddress unsupported.\n");
            continue;
        }
        if (!supported12.scalarBlockLayout) {
            printf("  Rejected: scalarBlockLayout unsupported.\n");
            continue;
        }
        if (!supported12.hostQueryReset) {
            printf("  Rejected: hostQueryReset unsupported.\n");
            continue;
        }
        if (!supported13.synchronization2) {
            printf("  Rejected: synchronization2 unsupported.\n");
            continue;
        }
        if (!supported13.dynamicRendering) {
            printf("  Rejected: dynamicRendering unsupported.\n");
            continue;
        }

        int score = 0;
        score += rate_device_type(props.deviceType);
        score += prefer_device_type_bonus(props.deviceType, use_discrete_gpu);

#if !defined(__linux__) || USE_DRM_KMS == 0
        if (q_graphics == q_present) score += 50;
#endif
        if (q_compute != q_graphics) score += 25;
        if (q_transfer != UINT32_MAX) score += 10;
        score += (int)(props.limits.maxImageDimension2D / 1024);

        printf("  Candidate score: %d\n", score);

        if (score > best_score) {
            best_score = score;
            machine.physical_device = dev;
            machine.queue_family_graphics = q_graphics;
            machine.queue_family_compute = q_compute;
#if !defined(__linux__) || USE_DRM_KMS == 0
            machine.queue_family_present = q_present;
#endif
        }
    }

    if (machine.physical_device == VK_NULL_HANDLE) {
        printf("No suitable GPU found after checking %u devices.\n", device_count);
        exit(1);
    }

    {
        VkPhysicalDeviceProperties chosen_props;
        vkGetPhysicalDeviceProperties(machine.physical_device, &chosen_props);
        printf("Selected GPU: %s (%s)\n",
               chosen_props.deviceName,
               device_type_str(chosen_props.deviceType));
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_infos[3];
    uint32_t queue_infos_count = 0;

#if !defined(__linux__) || USE_DRM_KMS == 0
    const uint32_t families[3] = {
        machine.queue_family_graphics,
        machine.queue_family_compute,
        machine.queue_family_present
    };
    const uint32_t family_count = 3;
#else
    const uint32_t families[2] = {
        machine.queue_family_graphics,
        machine.queue_family_compute
    };
    const uint32_t family_count = 2;
#endif

    for (uint32_t i = 0; i < family_count; ++i) {
        int already_added = 0;
        for (uint32_t j = 0; j < queue_infos_count; ++j) {
            if (queue_infos[j].queueFamilyIndex == families[i]) {
                already_added = 1;
                break;
            }
        }
        if (already_added)
            continue;

        queue_infos[queue_infos_count++] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
    }

    VkPhysicalDeviceFeatures2 supported = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
    };
    VkPhysicalDeviceVulkan11Features supported11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    };
    VkPhysicalDeviceVulkan12Features supported12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    VkPhysicalDeviceVulkan13Features supported13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };

    supported.pNext = &supported11;
    supported11.pNext = &supported12;
    supported12.pNext = &supported13;
    supported13.pNext = NULL;

#if DEBUG_APP == 1 && (!defined(__linux__) || USE_DRM_KMS == 0)
    VkPhysicalDevicePresentIdFeaturesKHR supported_present_id = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR
    };
    VkPhysicalDevicePresentWaitFeaturesKHR supported_present_wait = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR
    };

    supported13.pNext = &supported_present_id;
    supported_present_id.pNext = &supported_present_wait;
    supported_present_wait.pNext = NULL;
#endif

    vkGetPhysicalDeviceFeatures2(machine.physical_device, &supported);

    VkPhysicalDeviceFeatures2 enabled = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features.vertexPipelineStoresAndAtomics = supported.features.vertexPipelineStoresAndAtomics,
        .features.fragmentStoresAndAtomics = supported.features.fragmentStoresAndAtomics,
        .features.multiDrawIndirect = supported.features.multiDrawIndirect
    };
    VkPhysicalDeviceVulkan11Features enabled11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .shaderDrawParameters = supported11.shaderDrawParameters,
        .storageBuffer16BitAccess = supported11.storageBuffer16BitAccess,
    };
    VkPhysicalDeviceVulkan12Features enabled12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .timelineSemaphore = supported12.timelineSemaphore,
        .bufferDeviceAddress = supported12.bufferDeviceAddress,
        .scalarBlockLayout = supported12.scalarBlockLayout,
        .hostQueryReset = supported12.hostQueryReset,
        .storageBuffer8BitAccess = supported12.storageBuffer8BitAccess,
        .shaderFloat16 = supported12.shaderFloat16,
        .shaderInt8 = supported12.shaderInt8
    };
    VkPhysicalDeviceVulkan13Features enabled13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = supported13.synchronization2,
        .dynamicRendering = supported13.dynamicRendering
    };

    enabled.pNext = &enabled11;
    enabled11.pNext = &enabled12;
    enabled12.pNext = &enabled13;
    enabled13.pNext = NULL;

#if DEBUG_APP == 1 && (!defined(__linux__) || USE_DRM_KMS == 0)
    VkPhysicalDevicePresentIdFeaturesKHR enabled_present_id = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
        .presentId = supported_present_id.presentId
    };
    VkPhysicalDevicePresentWaitFeaturesKHR enabled_present_wait = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,
        .presentWait = supported_present_wait.presentWait
    };

    enabled13.pNext = &enabled_present_id;
    enabled_present_id.pNext = &enabled_present_wait;
    enabled_present_wait.pNext = NULL;
#endif

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled,
        .queueCreateInfoCount = queue_infos_count,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount = (uint32_t)(sizeof(required_device_extensions) / sizeof(required_device_extensions[0])),
        .ppEnabledExtensionNames = required_device_extensions,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL
    };

    VK_CHECK(vkCreateDevice(machine.physical_device, &device_info, NULL, &machine.device));

    vkGetDeviceQueue(machine.device, machine.queue_family_graphics, 0, &machine.queue_graphics);
    vkGetDeviceQueue(machine.device, machine.queue_family_compute, 0, &machine.queue_compute);

#if !defined(__linux__) || USE_DRM_KMS == 0
    vkGetDeviceQueue(machine.device, machine.queue_family_present, 0, &machine.queue_present);
#endif

    return machine;
}
