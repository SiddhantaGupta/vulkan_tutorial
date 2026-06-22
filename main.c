#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vulkan/vulkan_core.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
    uint32_t frame_index;
} Vulkan_Context;

// NOTE: This should not be global but we'll keep it as global for ease.
Vulkan_Context ctx = { 0 };

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
            suppports_required_extensions = false; // Missing a required extension!
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

    int supports_required_features = features_11.shaderDrawParameters &&
        features_13.dynamicRendering && features_ext.extendedDynamicState;

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

void create_image_views() {
    ctx.swapchain_image_views = malloc(ctx.swapchain_image_count * sizeof(VkImageView));

    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = ctx.swapchain_surface_format.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    for (uint32_t i = 0; i < ctx.swapchain_image_count; ++i) {
        image_view_create_info.image = ctx.swapchain_images[i];
        VkResult vk_result = vkCreateImageView(ctx.device, &image_view_create_info,
                NULL, &ctx.swapchain_image_views[i]);

        if (vk_result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create image view at index %u! Error: %d\n", i, vk_result);
        }
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

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL
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
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
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
        .setLayoutCount = 0,
        .pSetLayouts = NULL,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    vk_result = vkCreatePipelineLayout(ctx.device, &pipeline_layout_info, NULL, &ctx.pipeline_layout);

    if (vk_result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline layout! Error code: %d\n", vk_result);
    }

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = NULL,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &ctx.swapchain_surface_format.format,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
    };


    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_create_info,
        .flags = 0,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState   = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state_create_info,
        .pRasterizationState = &rasterizer_create_info,
        .pMultisampleState   = &multisampling_create_info,
        .pColorBlendState    = &color_blend_state_create_info,
        .pDynamicState       = &dynamic_state_create_info,
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

void transition_image_layout( uint32_t image_index, VkImageLayout old_layout,
    VkImageLayout new_layout, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask) {

    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask        = src_stage_mask,
        .srcAccessMask       = src_access_mask,
        .dstStageMask        = dst_stage_mask,
        .dstAccessMask       = dst_access_mask,
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = ctx.swapchain_images[image_index],
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .dependencyFlags         = 0,
        .memoryBarrierCount      = 0,
        .pMemoryBarriers         = NULL,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers   = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier
    };

    vkCmdPipelineBarrier2(ctx.command_buffers[ctx.frame_index], &dependency_info);
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

    transition_image_layout(image_index,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

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
        .pDepthAttachment = NULL,
        .pStencilAttachment = NULL
    };

    vkCmdBeginRendering(ctx.command_buffers[ctx.frame_index], &rendering_info);
    vkCmdBindPipeline(ctx.command_buffers[ctx.frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.graphics_pipeline);
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
    vkCmdDraw(ctx.command_buffers[ctx.frame_index], 3, 1, 0, 0);
    vkCmdEndRendering(ctx.command_buffers[ctx.frame_index]);

    transition_image_layout(image_index,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

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
}

void init_vulkan() {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
    create_graphics_pipeline();
    create_command_pool();
    create_command_buffers();
    create_sync_objects();
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
        exit(EXIT_FAILURE);
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
