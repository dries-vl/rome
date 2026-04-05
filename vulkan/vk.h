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
