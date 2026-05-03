#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#else
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#include <vulkan/vulkan.h>
#undef VkResult
#define VkResult int
#undef VkBool32
#define VkBool32 unsigned

#define MAX_SWAPCHAIN_IMAGES 5

struct Machine {
    VkInstance instance;
#if !defined(__linux__) || USE_DRM_KMS == 0
    VkSurfaceKHR surface;
#endif
    VkPhysicalDevice physical_device;
    VkDevice device;
#if DEBUG_VULKAN == 1
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    uint32_t queue_family_graphics;
    uint32_t queue_family_compute;
#if !defined(__linux__) || USE_DRM_KMS == 0
    uint32_t queue_family_present;
#endif
    VkQueue queue_graphics;
    VkQueue queue_compute;
#if !defined(__linux__) || USE_DRM_KMS == 0
    VkQueue queue_present;
#endif
};
struct Machine create_machine(char *app_name, int use_discrete_gpu);

static const char* vk_result_str(VkResult r) {
    switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    default: return "VK_RESULT_UNKNOWN";
    }
}

extern void exit(int);
extern int printf(const char*,...); // todo: just don't use
extern void *memset(void*,int,__SIZE_TYPE__);
extern int strcmp(const char*,const char*);
#define VK_CHECK(call) do{ VkResult _r=(call); if(_r!=VK_SUCCESS){ printf("vulkan error: %s on line @%s:%d\n",vk_result_str(_r),__FILE__,__LINE__); exit(1);} }while(0)
