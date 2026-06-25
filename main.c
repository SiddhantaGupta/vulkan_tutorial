#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <cglm/cglm.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/affine-pre.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#include "vulkan/vulkan_core.h"

#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
constexpr uint32_t validation_layer_count = sizeof(validation_layers) / sizeof(validation_layers[0]);

#ifdef NDEBUG
constexpr bool enable_validation_layers = false;
#else
constexpr bool enable_validation_layers = true;
#endif

typedef struct Vulkan_Context {
    GLFWwindow *window;
    bool frame_buffer_resized;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkPhysicalDeviceFeatures device_features;
    int graphics_queue_index;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    uint32_t swapchain_image_count;
    VkImage *swapchain_images;
    uint32_t swapchain_image_view_count;
    VkImageView *swapchain_image_views;
    VkSurfaceFormatKHR swapchain_surface_format;
    VkExtent2D swapchain_extent;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore present_complete_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore *render_finished_semaphores;
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniform_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void *uniform_buffers_mapped[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];
    VkImage texture_image;
    VkDeviceMemory texture_image_memory;
    uint32_t texture_image_view_count;
    VkImageView texture_image_view;
    VkSampler texture_sampler;
    uint32_t frame_index;
    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
    VkFormat depth_format;
} Vulkan_Context;

typedef struct UniformBufferObject {
    alignas(16) mat4 model;
    alignas(16) mat4 view;
    alignas(16) mat4 proj;
} UniformBufferObject;

Vulkan_Context ctx = { 0 };

typedef struct Vertex {
    vec3 pos;
    vec2 tex_coord;
} Vertex;

const Vertex vertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}}
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    ctx.frame_buffer_resized = true;
}

void init_window() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ctx.window = glfwCreateWindow(WIDTH , HEIGHT, "Vulkan", NULL, NULL);
    glfwSetFramebufferSizeCallback(ctx.window, framebufferResizeCallback);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    fprintf(stderr, "validation layer: type %d msg: %s\n", messageTypes, pCallbackData->pMessage);
    return VK_FALSE;
}

void setup_debug_messenger() {
    if (!enable_validation_layers) return;

    VkDebugUtilsMessageSeverityFlagsEXT severity_flags =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    VkDebugUtilsMessageTypeFlagsEXT message_type_flags =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

    VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = NULL,
        .messageSeverity = severity_flags,
        .messageType = message_type_flags,
        .pfnUserCallback = debug_callback,
    };

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(ctx.instance, "vkCreateDebugUtilsMessengerEXT");

    VkResult r = func(ctx.instance, &messenger_info, NULL, &ctx.debug_messenger);

    if (r != VK_SUCCESS) {
        fprintf(stderr, "Failed to set up debug messenger!\n");
    }
}

void create_surface() {
    VkResult result = glfwCreateWindowSurface(ctx.instance, ctx.window, NULL, &ctx.surface);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create window surface");
    }
}

bool is_device_suitable() {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(ctx.physical_device, &properties);
    bool supports_vk_1_3 = properties.apiVersion >= VK_API_VERSION_1_3;

    uint32_t q_family_property_count;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &q_family_property_count, NULL);
    VkQueueFamilyProperties q_family_properties[q_family_property_count];
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &q_family_property_count,
            q_family_properties);

    int supports_graphics = 0;
    for (uint32_t i = 0; i < q_family_property_count; ++i) {
        if (q_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            supports_graphics = 1;
            break;
        }
    }

    char *required_device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    uint32_t required_extension_count = sizeof(required_device_extensions) / sizeof(required_device_extensions[0]);

    uint32_t available_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(ctx.physical_device, NULL,
            &available_extension_count, NULL);

    VkExtensionProperties available_extensions[available_extension_count];
    vkEnumerateDeviceExtensionProperties(ctx.physical_device, NULL,
            &available_extension_count, available_extensions);

    bool suppports_required_extensions = true;

    for (uint32_t i = 0; i < required_extension_count; ++i) {
        int found = 0;
        for (uint32_t j = 0; j < available_extension_count; ++j) {
            if (strcmp(required_device_extensions[i], available_extensions[j].extensionName) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            suppports_required_extensions = false;
            printf("Required device extension not supported: %s\n", required_device_extensions[i]);
            break;
        }
    }

    VkPhysicalDeviceVulkan11Features features_11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = NULL
    };

    VkPhysicalDeviceVulkan13Features features_13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features_11
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT features_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &features_13
    };

    VkPhysicalDeviceFeatures2 features_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features_ext
    };

    vkGetPhysicalDeviceFeatures2(ctx.physical_device, &features_2);

    int supports_required_features = features_2.features.samplerAnisotropy &&
        features_11.shaderDrawParameters && features_13.dynamicRendering &&
        features_ext.extendedDynamicState;

    return supports_vk_1_3 && supports_graphics && suppports_required_extensions && supports_required_features;
}

void pick_physical_device() {
    uint32_t physical_device_count;
    vkEnumeratePhysicalDevices(ctx.instance, &physical_device_count, NULL);

    VkPhysicalDevice *physical_device = malloc(physical_device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx.instance, &physical_device_count, physical_device);
    ctx.physical_device = physical_device[0];
    free(physical_device);

    if (!is_device_suitable()) {
        fprintf(stderr, "No Suitable devies found");
    }
}

void create_logical_device() {
    uint32_t q_family_property_count;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &q_family_property_count, NULL);
    VkQueueFamilyProperties q_family_properties[q_family_property_count];
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &q_family_property_count,
            q_family_properties);

    ctx.graphics_queue_index = -1;
    for (uint32_t i = 0; i < q_family_property_count; ++i) {
        VkBool32 supports_present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physical_device, i, ctx.surface, &supports_present);
        if ((q_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_present) {
            ctx.graphics_queue_index = i;
            break;
        }
    }

    if (ctx.graphics_queue_index == -1) {
        fprintf(stderr, "Failed to find a queue family that supports graphics!\n");
        return;
    }

    float queue_priority = 0.5f;
    VkDeviceQueueCreateInfo device_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = ctx.graphics_queue_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    VkPhysicalDeviceVulkan11Features features_11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = NULL,
        .shaderDrawParameters = VK_TRUE
    };

    VkPhysicalDeviceVulkan13Features features_13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features_11,
        .dynamicRendering = VK_TRUE,
        .synchronization2 = VK_TRUE
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT features_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &features_13,
        .extendedDynamicState = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = {
            .samplerAnisotropy = VK_TRUE
        },
        .pNext = &features_ext
    };

    const char *required_device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    uint32_t required_extension_count = sizeof(required_device_extensions) / sizeof(required_device_extensions[0]);

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features_2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &device_queue_create_info,
        .enabledExtensionCount = required_extension_count,
        .ppEnabledExtensionNames = required_device_extensions,
        .pEnabledFeatures = NULL,
    };
    VkResult vk_result = vkCreateDevice(ctx.physical_device, &device_create_info, NULL, &ctx.device);
    if (vk_result != VK_SUCCESS) {
        printf("Failed to create logical device ERR code: %d\n", vk_result);
    }

    vkGetDeviceQueue(ctx.device, ctx.graphics_queue_index, 0, &ctx.graphics_queue);
}

void create_swapchain() {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VkResult vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &surface_capabilities);
    if (vk_result) {
        fprintf(stderr, "Failed to get physical device surface capabilities! Error code: %d\n", vk_result);
    }
    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        ctx.swapchain_extent = surface_capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(ctx.window, &width, &height);
        ctx.swapchain_extent.width = CLAMP((uint32_t)width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        ctx.swapchain_extent.height = CLAMP((uint32_t)height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, NULL);
    VkSurfaceFormatKHR available_formats[format_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, available_formats);

    if (format_count > 0) {
        ctx.swapchain_surface_format = available_formats[0];
    }
    for (uint32_t i = 0; i < format_count; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
                available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            ctx.swapchain_surface_format = available_formats[i];
            break;
        }
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_mode_count, NULL);
    VkPresentModeKHR available_present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_mode_count, available_present_modes);

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    uint32_t image_count = surface_capabilities.minImageCount > 3u ? surface_capabilities.minImageCount : 3u;
    if ((0 < surface_capabilities.maxImageCount) && (surface_capabilities.maxImageCount < image_count)) {
        image_count = surface_capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = ctx.surface,
        .minImageCount = image_count,
        .imageFormat = ctx.swapchain_surface_format.format,
        .imageColorSpace = ctx.swapchain_surface_format.colorSpace,
        .imageExtent = ctx.swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    vk_result = vkCreateSwapchainKHR(ctx.device, &swapchain_create_info, NULL, &ctx.swapchain);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain! Error code: %d\n", vk_result);
    }

    vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &ctx.swapchain_image_count, NULL);

    ctx.swapchain_images = malloc(ctx.swapchain_image_count * sizeof(VkImage));
    if (ctx.swapchain_images == NULL) {
        fprintf(stderr, "Failed to allocate memory for swapchain images!\n");
    }

    vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &ctx.swapchain_image_count, ctx.swapchain_images);
}

int create_instance() {
    uint32_t required_layer_count = 0;
    if (enable_validation_layers) {
        required_layer_count = validation_layer_count;
    }

    const char **required_layers = NULL;
    if (required_layer_count > 0) {
        required_layers = malloc(required_layer_count * sizeof(char *));
    }

    for (int i = 0; i < required_layer_count; ++i) {
        required_layers[i] = validation_layers[i];
    }

    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties *layer_properties = malloc(layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);

    for (uint32_t i = 0; i < required_layer_count; ++i) {
        int found = 0;
        for (uint32_t j = 0; j < layer_count; ++j) {
            if (strcmp(required_layers[i], layer_properties[j].layerName) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Required validation layer not supported: %s\n", required_layers[i]);
            free(required_layers);
            free(layer_properties);
        }
    }

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    uint32_t total_extension_count = 0;
    total_extension_count += glfw_extension_count;
    if (enable_validation_layers) {
        total_extension_count++;
    }
    const char *required_extensions[total_extension_count];

    memcpy(required_extensions, glfw_extensions, sizeof(char *) * glfw_extension_count);
    if (enable_validation_layers) {
        required_extensions[glfw_extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    uint32_t vk_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &vk_extension_count, NULL);
    VkExtensionProperties *vk_extension_properties = malloc(vk_extension_count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &vk_extension_count, vk_extension_properties);

    for (uint32_t i = 0; i < glfw_extension_count; ++i) {
        int found = 0;
        for (uint32_t j = 0; j < vk_extension_count; ++j) {
            if (strcmp(glfw_extensions[i], vk_extension_properties[j].extensionName) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Required extension not supported: %s\n", glfw_extensions[i]);
            free(vk_extension_properties);
        }
    }

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = required_layer_count,
        .ppEnabledLayerNames = required_layers,
        .enabledExtensionCount = total_extension_count,
        .ppEnabledExtensionNames = required_extensions,
    };

    int vk_result = vkCreateInstance(&create_info, NULL, &ctx.instance);
    if (vk_result != VK_SUCCESS) {
        printf("Failed to create VK Instance ERR code: %d", vk_result);
    }


    free(required_layers);
    free(layer_properties);
    free(vk_extension_properties);
    return 0;
}

VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlagBits aspect_flags) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkImageView image_view;
    if (vkCreateImageView(ctx.device, &view_info, NULL, &image_view) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan image view!\n");
    }

    return image_view;
}

void create_image_views() {
    ctx.swapchain_image_views = malloc(ctx.swapchain_image_count * sizeof(VkImageView));
    for (uint32_t i = 0; i < ctx.swapchain_image_count; ++i) {
        ctx.swapchain_image_views[i] = create_image_view(ctx.swapchain_images[i],
            ctx.swapchain_surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

char* read_file(const char* filename, size_t* out_size) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory for file buffer: %s\n", filename);
        fclose(file);
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Warning: Did not read the entire file block for %s\n", filename);
    }

    fclose(file);
    *out_size = file_size;

    return buffer;
}

void create_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = NULL
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        }
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = sizeof(bindings) / sizeof(bindings[0]),
        .pBindings = bindings
    };

    if (vkCreateDescriptorSetLayout(ctx.device, &layout_info, NULL, &ctx.descriptor_set_layout) != VK_SUCCESS) {
        fprintf(stderr, "failed to create descriptor set layout!\n");
    }
}

void create_graphics_pipeline() {
    size_t out_size = 0;
    char *shader_code = read_file("shaders/slang.spv", &out_size);

    VkShaderModuleCreateInfo shader_module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = out_size,
        .pCode = (uint32_t *)shader_code,
    };

    VkShaderModule shader_module;
    VkResult vk_result = vkCreateShaderModule(ctx.device, &shader_module_create_info, NULL, &shader_module);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create shader module! Error code: %d\n", vk_result);
    }

    free(shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shader_module,
        .pName = "vert_main",
        .pSpecializationInfo = NULL,
    };
    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shader_module,
        .pName = "frag_main",
        .pSpecializationInfo = NULL,
    };
    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    VkVertexInputBindingDescription vertex_input_binding_desc = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    uint32_t vertex_input_attribute_count = 2;
    VkVertexInputAttributeDescription vertex_input_attribute_desc[vertex_input_attribute_count] = {};
    vertex_input_attribute_desc[0] = (VkVertexInputAttributeDescription){
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, pos)
    };
    vertex_input_attribute_desc[1] = (VkVertexInputAttributeDescription){
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, tex_coord)
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_input_binding_desc,
        .vertexAttributeDescriptionCount = vertex_input_attribute_count,
        .pVertexAttributeDescriptions = vertex_input_attribute_desc
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    uint32_t dynamic_state_count = sizeof(dynamic_states) / sizeof(dynamic_states[0]);

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = dynamic_state_count,
        .pDynamicStates = dynamic_states
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = NULL,
        .scissorCount = 1,
        .pScissors = NULL
    };

    VkPipelineRasterizationStateCreateInfo rasterizer_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx.descriptor_set_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    vk_result = vkCreatePipelineLayout(ctx.device, &pipeline_layout_info, NULL, &ctx.pipeline_layout);

    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline layout! Error code: %d\n", vk_result);
    }

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {0},
        .back = {0},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = NULL,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &ctx.swapchain_surface_format.format,
        .depthAttachmentFormat = ctx.depth_format,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
    };

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_create_info,
        .flags = 0,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterizer_create_info,
        .pMultisampleState = &multisampling_create_info,
        .pColorBlendState = &color_blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .pDepthStencilState = &depth_stencil,
        .layout = ctx.pipeline_layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    vk_result = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1,
        &pipeline_create_info, NULL, &ctx.graphics_pipeline);

    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create graphics pipeline! Error code: %d\n", vk_result);
    }

    vkDestroyShaderModule(ctx.device, shader_module, NULL);
}

void create_command_pool() {
    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx.graphics_queue_index,
    };

    VkResult vk_result = vkCreateCommandPool(ctx.device, &command_pool_info, NULL, &ctx.command_pool);

    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool! Error code: %d\n", vk_result);
    }
}

VkFormat find_supported_format(const VkFormat* candidates, uint32_t candidates_count,
        VkImageTiling tiling, VkFormatFeatureFlags features) {

    for (uint32_t i = 0; i < candidates_count; i++) {
        VkFormat format = candidates[i];
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(ctx.physical_device, format, &props);

        if ((tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) ||
            (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (props.optimalTilingFeatures & features) == features)) {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

int find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx.physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((memory_type_bits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    fprintf(stderr, "Failed to find suitable memory\n");
    return -1;
}

void create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *out_image,
        VkDeviceMemory *out_image_memory) {

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent.width = width,
        .extent.height = height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(ctx.device, &image_info, NULL, out_image) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan image object!\n");
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(ctx.device, *out_image, &mem_req);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties),
    };

    if (vkAllocateMemory(ctx.device, &alloc_info, NULL, out_image_memory) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate memory for vulkan image!\n");
    }

    if (vkBindImageMemory(ctx.device, *out_image, *out_image_memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "Failed to bind memory to vulkan image!\n");
    }
}

void create_depth_resources() {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    ctx.depth_format = find_supported_format(candidates, sizeof(candidates) / sizeof(candidates[0]),
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    create_image(ctx.swapchain_extent.width, ctx.swapchain_extent.height, ctx.depth_format,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ctx.depth_image, &ctx.depth_image_memory);

    ctx.depth_image_view = create_image_view(ctx.depth_image, ctx.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer *buffer, VkDeviceMemory *buffer_memory) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    vkCreateBuffer(ctx.device, &buffer_info, NULL, buffer);

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx.device, *buffer, &mem_req);

    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties),
    };

    if (vkAllocateMemory(ctx.device, &mem_alloc_info, NULL, buffer_memory) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate memory for buffer");
    }

    if (vkBindBufferMemory(ctx.device, *buffer, *buffer_memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "Failed to bind memory to buffer");
    }
}

VkCommandBuffer begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = ctx.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(ctx.device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate single-time command buffer!\n");
        exit(EXIT_FAILURE);
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        fprintf(stderr, "Failed to begin recording single-time command buffer!\n");
        exit(EXIT_FAILURE);
    }

    return command_buffer;
}

void end_single_time_commands(VkCommandBuffer command_buffer) {
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to end recording single-time command buffer!\n");
        exit(EXIT_FAILURE);
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL
    };

    if (vkQueueSubmit(ctx.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit single-time command buffer to queue!\n");
        exit(EXIT_FAILURE);
    }

    vkQueueWaitIdle(ctx.graphics_queue);
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &command_buffer);
}

void copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size) {
    VkCommandBuffer copy_command_buffer = begin_single_time_commands();
    VkBufferCopy copy_region = { .srcOffset = 0, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(copy_command_buffer, src_buf, dst_buf, 1, &copy_region);
    end_single_time_commands(copy_command_buffer);
}

void transition_image_layout(VkCommandBuffer command_buffer, VkImage image,
        VkImageLayout old_layout, VkImageLayout new_layout,
        VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
        VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask,
        VkImageAspectFlags image_aspect_flags) {
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = image_aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = NULL,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}

void copy_buffer_to_image(VkCommandBuffer command_buffer, VkBuffer buffer,
    VkImage image, uint32_t width, uint32_t height) {
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = { .x = 0, .y = 0, .z = 0 },
        .imageExtent = { .width = width, .height = height, .depth = 1 }
    };

    vkCmdCopyBufferToImage(command_buffer, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void create_texture_image() {
    int tex_width, tex_height, tex_channels;
    stbi_uc *pixels    = stbi_load("textures/texture.jpg", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    VkDeviceSize image_size = tex_width * tex_height * 4;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging_buffer, &staging_buffer_memory);

    void *data;
    if (vkMapMemory(ctx.device, staging_buffer_memory, 0, image_size, 0, &data) != VK_SUCCESS) {
        fprintf(stderr, "Failed to map staging buffer memory!\n");
    }
    memcpy(data, pixels, image_size);
    vkUnmapMemory(ctx.device, staging_buffer_memory);
    stbi_image_free(pixels);

    create_image(tex_width, tex_height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ctx.texture_image, &ctx.texture_image_memory);

    VkCommandBuffer command_buffer = begin_single_time_commands();

    transition_image_layout(command_buffer, ctx.texture_image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    copy_buffer_to_image(command_buffer, staging_buffer, ctx.texture_image, tex_width, tex_height);

    transition_image_layout(command_buffer, ctx.texture_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);

    end_single_time_commands(command_buffer);

    if (staging_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, staging_buffer_memory, NULL);
    }
    if (staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device, staging_buffer, NULL);
    }
}

void create_texture_image_view() {
    ctx.texture_image_view = create_image_view(ctx.texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}


void create_texture_sampler() {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(ctx.physical_device, &properties);
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    if (vkCreateSampler(ctx.device, &sampler_info, NULL, &ctx.texture_sampler) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create texture sampler!\n");
    }
}

void create_vertex_buffer() {
    VkDeviceSize buffer_size = sizeof(vertices);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging_buffer, &staging_buffer_memory);

    void *data;
    vkMapMemory(ctx.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices, buffer_size);
    vkUnmapMemory(ctx.device, staging_buffer_memory);

    create_buffer(buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ctx.vertex_buffer, &ctx.vertex_buffer_memory);

    copy_buffer(staging_buffer, ctx.vertex_buffer, buffer_size);
    if (staging_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, staging_buffer_memory, NULL);
    }
    if (staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device, staging_buffer, NULL);
    }
}

void create_index_buffer() {
    VkDeviceSize buffer_size = sizeof(indices);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging_buffer, &staging_buffer_memory);

    void *data;
    vkMapMemory(ctx.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, indices, buffer_size);
    vkUnmapMemory(ctx.device, staging_buffer_memory);

    create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ctx.index_buffer, &ctx.index_buffer_memory);

    copy_buffer(staging_buffer, ctx.index_buffer, buffer_size);
    if (staging_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, staging_buffer_memory, NULL);
    }
    if (staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device, staging_buffer, NULL);
    }
}

void create_uniform_buffer() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        create_buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &ctx.uniform_buffers[i], &ctx.uniform_buffers_memory[i]);

        if (vkMapMemory(ctx.device, ctx.uniform_buffers_memory[i], 0, bufferSize, 0, &ctx.uniform_buffers_mapped[i]) != VK_SUCCESS) {
            fprintf(stderr, "failed to map uniform buffer memory!\n");
        }
    }
}

void create_descriptor_pool() {
    VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT
        }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    if (vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &ctx.descriptor_pool) != VK_SUCCESS) {
        fprintf(stderr, "failed to create descriptor pool!\n");
    }
}

void create_descriptor_sets() {
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = ctx.descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = ctx.descriptor_pool,
        .descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(ctx.device, &alloc_info, ctx.descriptor_sets) != VK_SUCCESS) {
        fprintf(stderr, "failed to allocate descriptor sets!\n");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = ctx.uniform_buffers[i],
            .offset = 0,
            .range  = sizeof(UniformBufferObject),
        };

        VkDescriptorImageInfo image_info = {
            .sampler = ctx.texture_sampler,
            .imageView = ctx.texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet descriptor_writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = ctx.descriptor_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
                .pImageInfo = NULL,
                .pTexelBufferView = NULL
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = ctx.descriptor_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pBufferInfo = NULL,
                .pImageInfo = &image_info,
                .pTexelBufferView = NULL
            }
        };

        vkUpdateDescriptorSets(ctx.device, sizeof(descriptor_writes) / sizeof(descriptor_writes[0]),
                descriptor_writes, 0, NULL);
    }
}

void record_command_buffer(uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };
    VkResult vk_result = vkBeginCommandBuffer(ctx.command_buffers[ctx.frame_index], &begin_info);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to begin recording command buffer! Error code: %d\n", vk_result);
    }

    transition_image_layout(ctx.command_buffers[ctx.frame_index], ctx.swapchain_images[image_index],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);

    transition_image_layout(ctx.command_buffers[ctx.frame_index], ctx.depth_image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);

    VkClearValue clear_color = {
        .color = { { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = ctx.swapchain_image_views[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_color,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkClearValue clear_depth = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0
        }
    };
    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = ctx.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = clear_depth
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = ctx.swapchain_extent
        },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = &depth_attachment_info,
        .pStencilAttachment = NULL
    };

    vkCmdBeginRendering(ctx.command_buffers[ctx.frame_index], &rendering_info);
    vkCmdBindPipeline(ctx.command_buffers[ctx.frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.graphics_pipeline);
    vkCmdBindVertexBuffers(ctx.command_buffers[ctx.frame_index], 0, 1, &ctx.vertex_buffer, (VkDeviceSize[]){0});
    vkCmdBindIndexBuffer(ctx.command_buffers[ctx.frame_index], ctx.index_buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(ctx.command_buffers[ctx.frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx.pipeline_layout, 0, 1, &ctx.descriptor_sets[ctx.frame_index], 0, NULL);
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)ctx.swapchain_extent.width,
        .height = (float)ctx.swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(ctx.command_buffers[ctx.frame_index], 0, 1, &viewport);
    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = ctx.swapchain_extent
    };
    vkCmdSetScissor(ctx.command_buffers[ctx.frame_index], 0, 1, &scissor);
    vkCmdDrawIndexed(ctx.command_buffers[ctx.frame_index], sizeof(indices) / sizeof(indices[0]), 1, 0, 0, 0);
    vkCmdEndRendering(ctx.command_buffers[ctx.frame_index]);

    transition_image_layout(ctx.command_buffers[ctx.frame_index], ctx.swapchain_images[image_index],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);

    vk_result = vkEndCommandBuffer(ctx.command_buffers[ctx.frame_index]);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to end recording command buffer! Error code: %d\n", vk_result);
    }
}

void create_command_buffers() {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = ctx.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };
    VkResult vk_result = vkAllocateCommandBuffers(ctx.device, &alloc_info, ctx.command_buffers);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate command buffer! Error code: %d\n", vk_result);
    }
}

void create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    ctx.render_finished_semaphores = malloc(ctx.swapchain_image_count * sizeof(VkSemaphore));

    for (uint32_t i = 0; i < ctx.swapchain_image_count; i++) {
        if (vkCreateSemaphore(ctx.device, &semaphore_info, NULL, &ctx.render_finished_semaphores[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create render finished semaphore at index %d!\n", i);
        }
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(ctx.device, &semaphore_info, NULL, &ctx.present_complete_semaphores[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create present complete semaphore at index %d!\n", i);
        }

        if (vkCreateFence(ctx.device, &fence_info, NULL, &ctx.in_flight_fences[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create frame fence at index %d!\n", i);
        }
    }

    ctx.frame_index = 0;
}

void cleanup_swapchain() {
    if (ctx.depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.device, ctx.depth_image_view, NULL);
    }
    if (ctx.depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(ctx.device, ctx.depth_image, NULL);
    }
    if (ctx.depth_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, ctx.depth_image_memory, NULL);
    }
    if (ctx.swapchain_image_views != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < ctx.swapchain_image_count; i++) {
            vkDestroyImageView(ctx.device, ctx.swapchain_image_views[i], NULL);
        }
        free(ctx.swapchain_image_views);
        ctx.swapchain_image_views = NULL;
    }
    if (ctx.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx.device, ctx.swapchain, NULL);
    }
    if (ctx.swapchain_images != NULL) {
        free(ctx.swapchain_images);
        ctx.swapchain_images = NULL;
    }
}

void recreate_swap_chain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(ctx.window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(ctx.device);
    cleanup_swapchain();

    create_swapchain();
    create_image_views();
    create_depth_resources();
}

void init_vulkan() {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
    create_descriptor_set_layout();
    create_depth_resources();
    create_graphics_pipeline();
    create_command_pool();
    create_texture_image();
    create_texture_image_view();
    create_texture_sampler();
    create_vertex_buffer();
    create_index_buffer();
    create_uniform_buffer();
    create_descriptor_pool();
    create_descriptor_sets();
    create_command_buffers();
    create_sync_objects();
}

void update_uniform_buffer(uint32_t current_image) {
    static struct timespec startTime;
    static int initialized = 0;
    if (!initialized) {
        timespec_get(&startTime, TIME_UTC);
        initialized = 1;
    }
    struct timespec currentTime;
    timespec_get(&currentTime, TIME_UTC);
    float time = (float)(currentTime.tv_sec - startTime.tv_sec) +
                 (float)(currentTime.tv_nsec - startTime.tv_nsec) / 1000000000.0f;

    UniformBufferObject ubo = {0};

    glm_mat4_identity(ubo.model);
    vec3 rotation_axis = {0.0f, 0.0f, 1.0f};
    float angle_in_rad = time * glm_rad(90.0f);
    glm_rotate(ubo.model, angle_in_rad, rotation_axis);

    vec3 eye    = {2.0f, 2.0f, 2.0f};
    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up     = {0.0f, 0.0f, 1.0f};
    glm_lookat(eye, center, up, ubo.view);

    float fov          = glm_rad(45.0f);
    float aspectRatio  = (float)ctx.swapchain_extent.width / (float)ctx.swapchain_extent.height;
    float nearPlane    = 0.1f;
    float farPlane     = 10.0f;
    glm_perspective(fov, aspectRatio, nearPlane, farPlane, ubo.proj);
    ubo.proj[1][1] *= -1.0f;

    memcpy(ctx.uniform_buffers_mapped[current_image], &ubo, sizeof(UniformBufferObject));
}

void draw_frame() {
    VkResult vk_result = vkWaitForFences(ctx.device, 1, &ctx.in_flight_fences[ctx.frame_index], VK_TRUE, UINT64_MAX);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to wait for draw fence! Error code: %d\n", vk_result);
    }

    uint32_t image_index = 0;

    vk_result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
        ctx.present_complete_semaphores[ctx.frame_index], VK_NULL_HANDLE, &image_index);

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swap_chain();
        return;
    } else if (vk_result != VK_SUCCESS && vk_result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to acquire next swapchain image! Error code: %d\n", vk_result);
    }

    vk_result = vkResetFences(ctx.device, 1, &ctx.in_flight_fences[ctx.frame_index]);
    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to reset draw fence! Error code: %d\n", vk_result);
    }

    record_command_buffer(image_index);

    update_uniform_buffer(ctx.frame_index);

    VkSemaphoreSubmitInfo wait_semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
        .semaphore = ctx.present_complete_semaphores[ctx.frame_index],
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .deviceIndex = 0
    };

    VkCommandBufferSubmitInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = NULL,
        .commandBuffer = ctx.command_buffers[ctx.frame_index],
        .deviceMask = 0
    };

    VkSemaphoreSubmitInfo signal_semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
        .semaphore = ctx.render_finished_semaphores[image_index],
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .deviceIndex = 0
    };

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = NULL,
        .flags = 0,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_semaphore_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &command_buffer_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_semaphore_info
    };

    vk_result = vkQueueSubmit2(ctx.graphics_queue, 1, &submit_info, ctx.in_flight_fences[ctx.frame_index]);

    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw commands to graphics queue! Error code: %d\n", vk_result);
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx.render_finished_semaphores[image_index],
        .swapchainCount = 1,
        .pSwapchains = &ctx.swapchain,
        .pImageIndices = &image_index,
        .pResults = NULL
    };

    vk_result = vkQueuePresentKHR(ctx.graphics_queue, &present_info);

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR || ctx.frame_buffer_resized) {
        ctx.frame_buffer_resized = false;
        recreate_swap_chain();
    } else if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to present swapchain image! Error code: %d\n", vk_result);
    }

    ctx.frame_index = (ctx.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

void main_loop() {
    while(!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();
        draw_frame();
    }
    vkDeviceWaitIdle(ctx.device);
}

void cleanup() {
    if (ctx.texture_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(ctx.device, ctx.texture_sampler, NULL);
    }
    if (ctx.texture_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.device, ctx.texture_image_view, NULL);
    }
    if (ctx.texture_image != VK_NULL_HANDLE) {
        vkDestroyImage(ctx.device, ctx.texture_image, NULL);
    }
    if (ctx.texture_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, ctx.texture_image_memory, NULL);
    }
    if (ctx.descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device, ctx.descriptor_pool, NULL);
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkUnmapMemory(ctx.device, ctx.uniform_buffers_memory[i]);
        vkDestroyBuffer(ctx.device, ctx.uniform_buffers[i], NULL);
        vkFreeMemory(ctx.device, ctx.uniform_buffers_memory[i], NULL);
    }
    if (ctx.index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, ctx.index_buffer_memory, NULL);
    }
    if (ctx.index_buffer  != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device, ctx.index_buffer, NULL);
    }
    if (ctx.vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device, ctx.vertex_buffer_memory, NULL);
    }
    if (ctx.vertex_buffer  != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device, ctx.vertex_buffer, NULL);
    }
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (ctx.present_complete_semaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device, ctx.present_complete_semaphores[i], NULL);
        }
        if (ctx.in_flight_fences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(ctx.device, ctx.in_flight_fences[i], NULL);
        }
    }
    if (ctx.render_finished_semaphores != NULL) {
        for (uint32_t i = 0; i < ctx.swapchain_image_count; i++) {
            if (ctx.render_finished_semaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(ctx.device, ctx.render_finished_semaphores[i], NULL);
            }
        }
        free(ctx.render_finished_semaphores);
        ctx.render_finished_semaphores = NULL;
    }
    if (ctx.graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx.device, ctx.graphics_pipeline, NULL);
    }
    if (ctx.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layout, NULL);
    }
    if (ctx.command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx.device, ctx.command_pool, NULL);
    }
    cleanup_swapchain();
    if (ctx.descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device, ctx.descriptor_set_layout, NULL);
    }
    if (ctx.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, NULL);
    }
    if (ctx.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL) {
            func(ctx.instance, ctx.debug_messenger, NULL);
        }
    }
    if (ctx.device != VK_NULL_HANDLE) {
        vkDestroyDevice(ctx.device, NULL);
    }
    if (ctx.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx.instance, NULL);
    }
    glfwDestroyWindow(ctx.window);
    glfwTerminate();
}

void run() {
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
}

int main() {
    run();
    return 0;
}
