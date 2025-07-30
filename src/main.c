#include "common.h"
#include "font.h"
#include "font.c"

#include "ui.h"
#include "filetree.h"
#include "editor.h"
#include "jumplist.h"
#include "mass.h"

#include "ui.c"
#include "filetree.c"
#include "editor.c"
#include "jumplist.c"
#include "mass.c"

#include "../build/main_vert.h"
#include "../build/main_frag.h"

#define INITIAL_WIDTH 1200
#define INITIAL_HEIGHT 800

#define PROJECT_NAME "Editor"

#define STAGING_BUFFER_SIZE (64*MB)
#define SURFACE_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define SURFACE_COLOUR_SPACE VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define SURFACE_PRESENT_MODE VK_PRESENT_MODE_IMMEDIATE_KHR

W *w;

void
window_create(Arena *arena);

void
window_destroy(void);

VkDescriptorSet
descriptor_set_glyphs_create(FontAtlas *font_atlas, VkBuffer glyphs_buffer);

void
descriptor_set_destroy(VkDescriptorSet descriptor_set);

Swapchain *
swapchain_create(
    VkDevice device,
    VkPhysicalDevice phy_device,
    VkSurfaceKHR surface,
    VkRenderPass pass,
    Arena *arena
);

void
swapchain_destroy(VkDevice device, Swapchain *sc);

void glfw_callback_key          (GLFWwindow *window, int key, int scan, int action, int mods);
void glfw_callback_char         (GLFWwindow *window, unsigned int codepoint);
void glfw_callback_mouse_pos    (GLFWwindow *window, double x, double y);
void glfw_callback_mouse_button (GLFWwindow *window, int button, int action, int mods);
void glfw_callback_scroll       (GLFWwindow *window, double x, double y);

GLFWmonitor*
glfw_get_current_monitor(GLFWwindow *window);

// WINDOWING FUNCTION #####################################################

void window_create(Arena *arena) { TRACE    
    // GLFW --------------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(INITIAL_WIDTH, INITIAL_HEIGHT, PROJECT_NAME, NULL, NULL);

    U32 glfw_ext_count;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    // VALIDATION --------------------------------------------------------------------------------

#ifdef VALIDATION
    const char *validation_layer = "VK_LAYER_KHRONOS_validation";
    SCOPE_TRACE {
        ArenaResetPoint reset = arena_reset_point(arena);
        U32 layer_count;
        VK_ASSERT(vkEnumerateInstanceLayerProperties(&layer_count, NULL));
        VkLayerProperties *props = ARENA_ALLOC_ARRAY(arena, *props, layer_count);
        VK_ASSERT(vkEnumerateInstanceLayerProperties(&layer_count, props));
        bool validation_found = false;
        for (U64 i = 0; i < layer_count; ++i) {
            if (strcmp(props[i].layerName, validation_layer) == 0) {
                validation_found = true;
                break;
            }
        }
        expect(validation_found);
        arena_reset(arena, &reset);
    }
#endif

    // INSTANCES --------------------------------------------------------------------------------
    
    VkInstance instance;
    SCOPE_TRACE {
        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = PROJECT_NAME,
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };

        VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .enabledExtensionCount = glfw_ext_count,
            .ppEnabledExtensionNames = glfw_exts,
#ifdef VALIDATION
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = &validation_layer,
#endif
        };

        VK_ASSERT(vkCreateInstance(&create_info, NULL, &instance));
    }

    // SURFACE -----------------------------------------------------------------------------

    VkSurfaceKHR surface;
    VK_ASSERT(glfwCreateWindowSurface(instance, window, NULL, &surface));

    // PHYSICAL DEVICES --------------------------------------------------------------------

    const char *required_dev_exts[] = {
        "VK_KHR_swapchain",
    };

    VkPhysicalDevice phy_device = {0};
    SCOPE_TRACE {
        ArenaResetPoint reset = arena_reset_point(arena);
        U32 device_count;
        VK_ASSERT(vkEnumeratePhysicalDevices(instance, &device_count, NULL));
        expect(device_count != 0);
        VkPhysicalDevice *devices = ARENA_ALLOC_ARRAY(arena, *devices, device_count);
        VK_ASSERT(vkEnumeratePhysicalDevices(instance, &device_count, devices));

        typedef enum {
            SWAPCHAIN = (1u << 0),

            ALL = SWAPCHAIN,
        } Extensions;

        bool found_dev = false;
        for (U32 d = 0; d < device_count; ++d) {
            VkPhysicalDevice dev = devices[d];

            U32 dev_ext_count;
            VK_ASSERT(vkEnumerateDeviceExtensionProperties(dev, NULL, &dev_ext_count, NULL));
            VkExtensionProperties *dev_props = ARENA_ALLOC_ARRAY(arena, *dev_props, dev_ext_count);
            VK_ASSERT(vkEnumerateDeviceExtensionProperties(dev, NULL, &dev_ext_count, dev_props));

            Extensions found_exts = 0;
            for (U32 i = 0; i < dev_ext_count; ++i) {
                if (strcmp(dev_props[i].extensionName, "VK_KHR_swapchain") == 0)
                    found_exts |= SWAPCHAIN;
            }

            if ((found_exts & ALL) == ALL) {
                found_dev = true;
                phy_device = dev;
                break;
            }
        }

        expect(found_dev);
        arena_reset(arena, &reset);
    }

    VkPhysicalDeviceMemoryProperties phy_mem_props;
    vkGetPhysicalDeviceMemoryProperties(phy_device, &phy_mem_props);

    // QUEUES --------------------------------------------------------------------

    U32 queue_family_idx = ~0u;
    SCOPE_TRACE {
        ArenaResetPoint reset = arena_reset_point(arena);
        U32 family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &family_count, NULL);
        expect(family_count != 0);
        VkQueueFamilyProperties *families = ARENA_ALLOC_ARRAY(arena, VkQueueFamilyProperties, family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &family_count, families);

        bool family_found = false;
        for (U32 i = 0; i < family_count; ++i) {
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) 
                continue;

            VkBool32 present_support;
            VK_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(phy_device, i, surface, &present_support));
            if (present_support != VK_TRUE)
                continue;

            queue_family_idx = i;
            family_found = true;
            break;
        }

        expect(family_found);
        arena_reset(arena, &reset);
    }

    // LOGICAL DEVICES --------------------------------------------------------------------

    VkDevice device;
    SCOPE_TRACE {
        F32 queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_family_idx,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };

        VkPhysicalDeviceFeatures features = {0};

        VkDeviceCreateInfo device_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue_create_info,
            .pEnabledFeatures = &features,
            .enabledExtensionCount = countof(required_dev_exts),
            .ppEnabledExtensionNames = required_dev_exts,
#ifdef VALIDATION
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = &validation_layer,
#endif
        };

        VK_ASSERT(vkCreateDevice(phy_device, &device_info, NULL, &device));
    }

    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_idx, 0, &queue);

    // SURFACE STUFF ------------------------------------------------------------------------

    VkSurfaceCapabilitiesKHR surface_cap;
    SCOPE_TRACE {
        ArenaResetPoint reset = arena_reset_point(arena);
        VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &surface_cap));

        // expect present mode 
        U32 present_mode_count;
        VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &present_mode_count, NULL));
        expect(present_mode_count != 0);
        VkPresentModeKHR *present_modes = ARENA_ALLOC_ARRAY(arena, VkPresentModeKHR, present_mode_count);
        VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &present_mode_count, present_modes));
        bool mode_found = false;
        for (U32 i = 0; i < present_mode_count; ++i) {
            if (present_modes[i] == SURFACE_PRESENT_MODE) {
                mode_found = true;
                break;
            }
        }
        expect(mode_found);

        // expect format
        U32 format_count;
        VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &format_count, NULL));
        VkSurfaceFormatKHR *formats = ARENA_ALLOC_ARRAY(arena, VkSurfaceFormatKHR, format_count);
        VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &format_count, formats));
        bool format_found = false;
        for (U32 i = 0; i < format_count; ++i) {
            if (formats[i].format == SURFACE_FORMAT && formats[i].colorSpace == SURFACE_COLOUR_SPACE) {
                format_found = true;
                break;
            }
        }
        expect(format_found);
        arena_reset(arena, &reset);
    }

    // DESCRIPTOR SET POOL ---------------------------------------------------------------

    VkDescriptorPool descriptor_pool;
    SCOPE_TRACE {
        VkDescriptorPoolSize pools[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 },
        };

        VkDescriptorPoolCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = countof(pools),
            .pPoolSizes = pools,
            .maxSets = 16,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        };

        VK_ASSERT(vkCreateDescriptorPool(device, &info, NULL, &descriptor_pool))
    }

    // DESCRIPTOR SET LAYOUTS ---------------------------------------------------------------

    VkDescriptorSetLayout descriptor_set_layout_glyphs;
    SCOPE_TRACE {
        static const VkDescriptorSetLayoutBinding bindings[] = {
            // atlas
            {
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
            },

            // glyph lookup table
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
            },

            // glyphs to draw
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
            },

            // static data uniform
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
            }
        };

        // glyphs to draw
        VkDescriptorSetLayoutCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = countof(bindings),
            .pBindings = bindings,
        };

        VK_ASSERT(vkCreateDescriptorSetLayout(device, &info, NULL, &descriptor_set_layout_glyphs));
    }

    // UNIFORMS ------------------------------------------------------------------------------

    StaticDataUniform static_data_uniform = {0}; // diffed then filled each frame
    VkBuffer static_data_uniform_buffer;
    VkDeviceMemory static_data_uniform_buffer_memory = {0}; // allocated later
    SCOPE_TRACE {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(StaticDataUniform),
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        };
        VK_ASSERT(vkCreateBuffer(device, &info, NULL, &static_data_uniform_buffer));
    }

    // COMMAND POOL ----------------------------------------------------------------------

    VkCommandPool cmd_pool;
    SCOPE_TRACE {
        VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queue_family_idx,
        };
        VK_ASSERT(vkCreateCommandPool(device, &pool_info, NULL, &cmd_pool));
    }

    // COMMAND BUFFER ----------------------------------------------------------------------

    VkCommandBuffer cmd_buffer;
    SCOPE_TRACE {
        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_ASSERT(vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer));
    }

    // FRAME SYNC -------------------------------------------------------------------------

    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;
    SCOPE_TRACE {
        VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fence_info = { 
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VK_ASSERT(vkCreateSemaphore(device, &sem_info, NULL, &image_available));
        VK_ASSERT(vkCreateSemaphore(device, &sem_info, NULL, &render_finished));
        VK_ASSERT(vkCreateFence(device, &fence_info, NULL, &in_flight));
    }

    // PER FRAME ARENA -------------------------------------------------------------------

    Arena frame_arena = arena_create_sized(1ull << 30);

    // STAGING BUFFER --------------------------------------------------------------------

    StagingBuffer staging = {0};
    SCOPE_TRACE {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = STAGING_BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VK_ASSERT(vkCreateBuffer(device, &info, NULL, &staging.buffer));

        // staging.buffer_memory will be allocated later
    }

    // SHADERS -------------------------------------------------------

    VkShaderModule vert_module, frag_module;
    SCOPE_TRACE {
        #pragma GCC diagnostic ignored "-Wcast-align"
        VkShaderModuleCreateInfo vert_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = build_main_vert_spv_len,
            .pCode = (const U32*)build_main_vert_spv,
        };

        #pragma GCC diagnostic ignored "-Wcast-align"
        VkShaderModuleCreateInfo frag_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = build_main_frag_spv_len,
            .pCode = (const U32*)build_main_frag_spv,
        };
        VK_ASSERT(vkCreateShaderModule(device, &vert_info, NULL, &vert_module));
        VK_ASSERT(vkCreateShaderModule(device, &frag_info, NULL, &frag_module));
    }

    // RENDER PASS -------------------------------------------------------

    VkRenderPass pass;
    SCOPE_TRACE {
        VkAttachmentDescription colour_attach = {
            .format = SURFACE_FORMAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };

        VkAttachmentReference colour_attach_ref = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colour_attach_ref,
        };

        VkSubpassDependency dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        };

        VkRenderPassCreateInfo pass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &colour_attach,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };

        VK_ASSERT(vkCreateRenderPass(device, &pass_info, NULL, &pass));
    }
    // SWAPCHAIN -----------------------------------------------------------------------------

    Arena sc_arena = arena_create_sized(8ul*KB);
    Swapchain *sc = swapchain_create(device, phy_device, surface, pass, &sc_arena);
    U32 width_i = sc->width;
    U32 height_i = sc->height;

    // PIPELINE -----------------------------------------------------------------

    VkPipelineLayout pl_layout;
    SCOPE_TRACE {
        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptor_set_layout_glyphs,
        };

        VK_ASSERT(vkCreatePipelineLayout(device, &layout_info, NULL, &pl_layout));
    }

    VkPipeline pl;
    SCOPE_TRACE {
        VkPipelineShaderStageCreateInfo vert_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        };

        VkPipelineShaderStageCreateInfo frag_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        };

        VkPipelineVertexInputStateCreateInfo vert_input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = NULL,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = NULL,
        };

        VkPipelineInputAssemblyStateCreateInfo primitive_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
        };

        VkPipelineMultisampleStateCreateInfo ms = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState colour_blend_attachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable = VK_TRUE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        };

        VkPipelineColorBlendStateCreateInfo colour_blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colour_blend_attachment,
        };

        const VkPipelineShaderStageCreateInfo stages[] = {vert_info, frag_info};

        VkDynamicState dyn_state[] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dyn_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = countof(dyn_state),
            .pDynamicStates = dyn_state,
        };

        VkPipelineViewportStateCreateInfo viewport_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &(VkViewport){
                .x = 0.f,
                .y = 0.f,
                .width = (F32)width_i,
                .height = (F32)height_i,
                .minDepth = 0.f,
                .maxDepth = 1.f,
            },
            .scissorCount = 1,
            .pScissors = &(VkRect2D){
                { 0, 0 },
                { (U32)width_i, (U32)height_i },
            },
        };
        
        VkGraphicsPipelineCreateInfo pl_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &vert_input_info,
            .pInputAssemblyState = &primitive_info,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &ms,
            .pDepthStencilState = NULL,
            .pColorBlendState = &colour_blend_state,
            .pDynamicState = &dyn_info,
            .pViewportState = &viewport_info,
            .layout = pl_layout,
            .renderPass = pass,
            .subpass = 0,
        };

        VK_ASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pl_info, NULL, &pl));
    }

    vkDestroyShaderModule(device, vert_module, NULL);
    vkDestroyShaderModule(device, frag_module, NULL);

    Inputs inputs = {
        .char_events = ARENA_ALLOC_ARRAY(arena, CharEvent, EVENTS_MAX),
        .key_events = ARENA_ALLOC_ARRAY(arena, KeyEvent, EVENTS_MAX),
    };

    // CREATE W -------------------------------------------------------------------------------

    w = ARENA_ALLOC(arena, *w);
    *w = (W) { 
        window, NULL,
        instance, phy_device, phy_mem_props,
        device, queue,
        cmd_pool, cmd_buffer,
        surface, sc_arena, sc,
        image_available, render_finished, in_flight,
        pass, pl_layout, pl,
        descriptor_pool, descriptor_set_layout_glyphs,
        static_data_uniform, static_data_uniform_buffer, static_data_uniform_buffer_memory,

        inputs, frame_arena, staging, 
        false, false, 60.f, 0.2f
    };

    // ALLOC GPU BUFFERS ---------------------------------------------------------------------

    // staging buffer
    SCOPE_TRACE {
        VK_ASSERT(gpu_alloc_buffer(
            w->staging_buffer.buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &w->staging_buffer.buffer_memory
        ));
        VK_ASSERT(vkMapMemory(w->device, w->staging_buffer.buffer_memory, 0, STAGING_BUFFER_SIZE, 0, (void**) &w->staging_buffer.mapped_ptr));
        staging_buffer_reset(&w->staging_buffer);
    }

    // static uniform buffer
    VK_ASSERT(gpu_alloc_buffer(
        w->static_data_uniform_buffer,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &w->static_data_uniform_buffer_memory
    ));

    // END -----------------------------------------------------------------------------------
}

// HELDER FUNCTION DEFINITIONS #####################################################

void window_destroy(void) { TRACE
    VkDevice device = w->device;
    VkInstance instance = w->instance;

    swapchain_destroy(device, w->sc);
    arena_destroy(&w->sc_arena);

    vkDestroyDescriptorSetLayout(device, w->descriptor_set_layout_glyphs, NULL);
    vkDestroyRenderPass(device, w->pass, NULL);
    vkDestroyPipelineLayout(device, w->pl_layout, NULL);
    vkDestroyPipeline(device, w->pl, NULL);

    vkDestroyDescriptorPool(device, w->descriptor_pool, NULL);

    gpu_free(w->staging_buffer.buffer_memory);
    vkDestroyBuffer(device, w->staging_buffer.buffer, NULL);

    vkDestroySemaphore(device, w->render_finished, NULL);
    vkDestroySemaphore(device, w->image_available, NULL);
    vkDestroyFence(device, w->in_flight, NULL);

    vkDestroySurfaceKHR(instance, w->surface, NULL);
    vkDestroyCommandPool(device, w->cmd_pool, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(w->window);
    glfwTerminate();
}

void gpu_free(VkDeviceMemory mem) { TRACE
    vkFreeMemory(w->device, mem, NULL);
}

static VkResult gpu_alloc(
    VkMemoryRequirements *mem_req,
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
) { TRACE
    U32 mem_idx;
    bool found_mem = false;
    for (U32 i = 0; i < w->phy_mem_props.memoryTypeCount; ++i) {
        bool filter_match = (mem_req->memoryTypeBits & (1u << i)) != 0;
        bool props_match = (w->phy_mem_props.memoryTypes[i].propertyFlags & props) == props;
        if (filter_match & props_match) {
            mem_idx = i;
            found_mem = true;
            break;
        }
    }

    expect(found_mem);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req->size,
        .memoryTypeIndex = mem_idx,
    };
    VkResult ret = vkAllocateMemory(w->device, &alloc_info, NULL, mem);
    if (ret != VK_SUCCESS) return ret;

    return VK_SUCCESS;
}

VkResult gpu_alloc_buffer(
    VkBuffer buffer, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
) { TRACE
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(w->device, buffer, &mem_req);
    VkResult res = gpu_alloc(&mem_req, props, mem);
    if (res != VK_SUCCESS) return res;
    return vkBindBufferMemory(w->device, buffer, *mem, 0);
}

VkResult gpu_alloc_image(
    VkImage image, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
) { TRACE
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(w->device, image, &mem_req);
    VkResult res = gpu_alloc(&mem_req, props, mem);
    if (res != VK_SUCCESS) return res;
    return vkBindImageMemory(w->device, image, *mem, 0);
}

// returns an offset into the staging buffer
U8 *staging_buffer_alloc(StagingBuffer *staging_buffer, U64 size, U64 alignment) {
    U8 *alloc_start = ALIGN_UP(staging_buffer->staging_head, alignment);
    U8 *alloc_end = alloc_start + size;
    expect(alloc_end - staging_buffer->mapped_ptr <= (I64)STAGING_BUFFER_SIZE);
    staging_buffer->staging_head = alloc_end;
    return alloc_start;
}

void staging_buffer_cmd_copy_to_buffer(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkBuffer target,
    U32 copy_count,
    VkBufferCopy *buffer_copies // MUST POINT TO FRAME ARENA
) { TRACE
    StagingCopyBufferList *prev = staging_buffer->staging_copies_buffer;
    StagingCopyBufferList *new = ARENA_ALLOC(arena, *new);
    *new = (StagingCopyBufferList) { prev, target, copy_count, buffer_copies };
    staging_buffer->staging_copies_buffer = new;
}

void staging_buffer_cmd_copy_to_image(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkImage target,
    U32 copy_count,
    VkBufferImageCopy *buffer_image_copies // MUST POINT TO FRAME ARENA
) { TRACE
    StagingCopyImageList *prev = staging_buffer->staging_copies_image;
    StagingCopyImageList *new = ARENA_ALLOC(arena, *new);
    *new = (StagingCopyImageList) { prev, target, copy_count, buffer_image_copies };
    staging_buffer->staging_copies_image = new;
}

void staging_buffer_push_image_transition(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkImage target,
    VkImageLayout old_layout,
    VkImageLayout new_layout
) { TRACE
    ImageTransitionList *prev = staging_buffer->image_transitions;
    ImageTransitionList *new = ARENA_ALLOC(arena, *new);
    *new = (ImageTransitionList) { prev, target, old_layout, new_layout};
    staging_buffer->image_transitions = new;
}

void staging_buffer_reset(StagingBuffer *staging_buffer) { TRACE
    staging_buffer->staging_head = staging_buffer->mapped_ptr;
    staging_buffer->staging_copies_buffer = NULL;
    staging_buffer->staging_copies_image = NULL;
    staging_buffer->image_transitions = NULL;
}

VkDescriptorSet descriptor_set_glyphs_create(
    FontAtlas *font_atlas, VkBuffer glyphs_buffer
) { TRACE
    VkDescriptorSet descriptor_set;
    {
        VkDescriptorSetAllocateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = w->descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &w->descriptor_set_layout_glyphs,
        };

        VK_ASSERT(vkAllocateDescriptorSets(w->device, &info, &descriptor_set));
    }

    {
        VkWriteDescriptorSet descriptor_writes[] = {
            // atlas
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .pImageInfo = &(VkDescriptorImageInfo) {
                    .imageView = font_atlas->atlas_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }
            },

            // glyph locations
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = font_atlas->glyph_lookup_buffer,
                    .offset = 0,
                    .range = GLYPH_LOOKUP_BUFFER_SIZE,
                }
            },

            // glyphs to draw
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = glyphs_buffer,
                    .offset = 0,
                    .range = MAX_GLYPHS_SIZE,
                }
            },

            // static data
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = w->static_data_uniform_buffer,
                    .offset = 0,
                    .range = sizeof(StaticDataUniform),
                }
            },
        };

        U32 write_count = countof(descriptor_writes);
        vkUpdateDescriptorSets(w->device, write_count, descriptor_writes, 0, NULL);
    }

    return descriptor_set;
}

void descriptor_set_destroy(VkDescriptorSet descriptor_set) { TRACE
    vkFreeDescriptorSets(w->device, w->descriptor_pool, 1, &descriptor_set);
}


// MAIN #####################################################

void bench_main(void) {
    Arena static_arena = arena_create_sized(1ull*GB);
    window_create(&static_arena);
    const char *ttf_path = "/usr/share/fonts/truetype/roboto/mono/RobotoMono-Medium.ttf";
    FontAtlas *font_atlas = font_atlas_create(&static_arena, ttf_path);
    UI *ui = ui_create(font_atlas, &static_arena);
    
    U64 runs = 128;
    double ms_total = 0.f;
    for (U64 i = 0; i < runs; ++i) {
        Panel *massp = mass_create(ui, NULL);
        Mass *mass = massp->data;
        memcpy(mass->search, "test", 1);
        mass->search_len = 1;
        
        Timer t = timer_start();
        mass_search(mass);
        ms_total += timer_elapsed_ms(&t);
        
        ((unsigned char volatile *)mass->files->path)[0] = mass->files->path[0];
        panel_destroy(massp);
    }
    
    printf("ave %f ms\n", ms_total / (double)runs);
    fflush(stdout);
    exit(0);
}

int main(int argc, char *argv[]) { INIT_TRACE
    //bench_main();

    Arena static_arena = arena_create_sized(1ull*GB);

    // gltf window and vulkan -------------------------------------------

    window_create(&static_arena);
    glfwSetCharCallback(w->window, glfw_callback_char);
    glfwSetKeyCallback(w->window, glfw_callback_key);
    glfwSetCursorPosCallback(w->window, glfw_callback_mouse_pos);
    glfwSetMouseButtonCallback(w->window, glfw_callback_mouse_button);
    glfwSetScrollCallback(w->window, glfw_callback_scroll);

    // font atlas -------------------------------------------------------

    const char *ttf_path = "/usr/share/fonts/truetype/roboto/mono/RobotoMono-Medium.ttf";
    //const char *ttf_path = "/usr/share/fonts/TTF/IosevkaFixed-Regular.ttf";
    FontAtlas *font_atlas = font_atlas_create(&static_arena, ttf_path);

    // UI ---------------------------------------------------------------

    UI *ui = ui_create(font_atlas, &static_arena);

    // Editor -----------------------------------------------------------

    const char *file = NULL;
    if (argc > 1) file = argv[1];
    Panel *vsplit = panel_create(ui);
    vsplit->flags |= PanelMode_VSplit;
    Panel *editor_panel = editor_create(ui, (const U8*)file);
    ui->root = vsplit;
    panel_add_child(ui->root, editor_panel);
    panel_focus(editor_panel);
    
    Panel *jl_panel = jumplist_create(ui);
    panel_add_child(ui->root, jl_panel);

    // glyph draw buffer ------------------------------------------------

    VkBuffer glyph_draw_buffer;
    VkDeviceMemory glyph_draw_buffer_memory;
    {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_GLYPHS_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        };
        VK_ASSERT(vkCreateBuffer(w->device, &info, NULL, &glyph_draw_buffer));
        VK_ASSERT(gpu_alloc_buffer(
            glyph_draw_buffer,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &glyph_draw_buffer_memory
        ));
    }

    // alloc descriptor sets --------------------------------------------

    VkDescriptorSet descriptor_set_glyphs = descriptor_set_glyphs_create(font_atlas, glyph_draw_buffer);

    F32 frame = 10.0;
    while (!glfwWindowShouldClose(w->window)) {
        bool has_ops = ui->op_count != 0;
        ui_flush_ops(ui);

        // INPUTS -------------------------------------------------------------

        w->inputs.char_event_count = 0;
        w->inputs.key_event_count = 0;
        w->inputs.mouse_held_prev = w->inputs.mouse_held;
        w->inputs.scroll = 0.f;
        w->inputs.key_held_prev = w->inputs.key_held;
        w->inputs.key_repeating = 0;
        w->inputs.key_special_pressed = 0;
        w->inputs.key_special_repeating = 0;
        if (!has_ops && !w->force_update)
            glfwWaitEventsTimeout(0.1);
        else
            glfwPollEvents();
        w->inputs.mouse_in_window = glfwGetWindowAttrib(w->window, GLFW_HOVERED) != 0;
        w->inputs.mouse_pressed = w->inputs.mouse_held & ~w->inputs.mouse_held_prev;
        w->inputs.mouse_released = w->inputs.mouse_held_prev & ~w->inputs.mouse_held;
        w->inputs.key_pressed = w->inputs.key_held & ~w->inputs.key_held_prev;
        w->inputs.key_released = w->inputs.key_held_prev & ~w->inputs.key_held;
        w->force_update = false;

        if (w->inputs.key_special_pressed & special_mask(GLFW_KEY_F11)) {
            if (w->monitor == NULL) {
                w->monitor = glfw_get_current_monitor(w->window);
                const GLFWvidmode *mode = glfwGetVideoMode(w->monitor);
                glfwSetWindowMonitor(
                    w->window, w->monitor,
                    0, 0, mode->width, mode->height,
                    mode->refreshRate
                );
            } else {
                w->monitor = NULL;
                glfwSetWindowMonitor(
                    w->window, NULL,
                    0, 0, INITIAL_WIDTH, INITIAL_HEIGHT,
                    GLFW_DONT_CARE
                );
            }
        }

        GLFWmonitor *monitor = glfw_get_current_monitor(w->window);
        if (monitor) {        
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            if (mode != NULL) {
                F32 refresh_rate = (F32)mode->refreshRate;
                w->refresh_rate = refresh_rate;
                w->exp_factor = 1.f - (F32)pow(1.f - ANIM_EXP_FACTOR, 60.f/refresh_rate);
            }
        }

        // STATIC DATA --------------------------------------------------------

        F32 width, height;
        U32 width_i, height_i;
        {
            width_i = w->sc->width;
            height_i = w->sc->height;
            width = (F32)width_i;
            height = (F32)height_i;
        }

        bool x_match = w->static_data_uniform.viewport_size[0] == width;
        bool y_match = w->static_data_uniform.viewport_size[1] == height;
        if (!x_match || !y_match) {
            w->static_data_uniform.viewport_size[0] = width;
            w->static_data_uniform.viewport_size[1] = height;
            StaticDataUniform *uniform = (StaticDataUniform *)staging_buffer_alloc(&w->staging_buffer, sizeof(StaticDataUniform), 16);
            *uniform = w->static_data_uniform;

            VkBufferCopy *copy = ARENA_ALLOC(&w->frame_arena, *copy);
            *copy = (VkBufferCopy) {
                .srcOffset = (U64)((U8*)uniform - w->staging_buffer.mapped_ptr),
                .dstOffset = 0,
                .size = sizeof(StaticDataUniform),
            };
            staging_buffer_cmd_copy_to_buffer(
                &w->staging_buffer,
                &w->frame_arena,
                w->static_data_uniform_buffer,
                1,
                copy
            );
        }

        // UPDATE ----------------------------------------------------------------

        Rect viewport = { 0.f, 0.f, width, height };
        ui_update(ui, &viewport);
        U64 glyphs_size = ui->glyph_count * sizeof(Glyph);

        if (w->should_close) glfwSetWindowShouldClose(w->window, GLFW_TRUE);

        // write glyphs buffer

        if (glyphs_size) {
            Glyph* staging_glyph_draws = (Glyph*)staging_buffer_alloc(&w->staging_buffer, glyphs_size, 16);
            memcpy(staging_glyph_draws, ui->glyphs, glyphs_size);

            VkBufferCopy *buffer_copy = ARENA_ALLOC(&w->frame_arena, *buffer_copy);
            *buffer_copy = (VkBufferCopy) {
                .srcOffset = (U64)((U8*)staging_glyph_draws - w->staging_buffer.mapped_ptr),
                .dstOffset = 0,
                .size = glyphs_size,
            };

            staging_buffer_cmd_copy_to_buffer(
                &w->staging_buffer,
                &w->frame_arena,
                glyph_draw_buffer,
                1,
                buffer_copy
            );
        }

        // WAIT FOR NEXT FRAME ---------------------------------------------------

        U32 sc_image_idx;
        SCOPE_TRACE {
            VK_ASSERT(vkWaitForFences(w->device, 1, &w->in_flight, VK_TRUE, UINT64_MAX));
            VkResult res = vkAcquireNextImageKHR(w->device, w->sc->sc, UINT64_MAX, w->image_available, VK_NULL_HANDLE, &sc_image_idx);

            if (res != VK_SUCCESS) {
                if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                    while (width_i == 0 || height_i == 0) {
                        glfwWaitEvents();
                        glfwGetFramebufferSize(w->window, (int*)&width_i, (int*)&height_i);
                    }

                    vkDeviceWaitIdle(w->device);

                    swapchain_destroy(w->device, w->sc);
                    arena_clear(&w->sc_arena);
                    w->sc = swapchain_create(w->device, w->phy_device, w->surface, w->pass, &w->sc_arena);

                    vkDestroySemaphore(w->device, w->image_available, NULL);
                    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                    VK_ASSERT(vkCreateSemaphore(w->device, &sem_info, NULL, &w->image_available));

                    continue;
                } else {
                    expect(false);
                }
            }

            VK_ASSERT(vkResetFences(w->device, 1, &w->in_flight));
        }

        // START RECORDING -------------------------------------------------------

        SCOPE_TRACE {
            VK_ASSERT(vkResetCommandBuffer(w->cmd_buffer, 0));
            VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,
                .pInheritanceInfo = NULL,
            };
            VK_ASSERT(vkBeginCommandBuffer(w->cmd_buffer, &begin_info));
        }

        // WRITE TRANSFER COMMANDS ------------------------------------------------

        SCOPE_TRACE {
            U64 transition_count = 0;
            VkImageMemoryBarrier *image_barriers = arena_prealign(&w->frame_arena, alignof(VkImageMemoryBarrier));

            ImageTransitionList *transition = w->staging_buffer.image_transitions;
            for (; transition != NULL; transition = transition->next) {
                VkImageLayout old_layout = transition->old_layout;
                VkImageLayout new_layout = transition->new_layout;

                if (old_layout == new_layout) continue;

                transition_count++;

                VkImageMemoryBarrier *barrier = ARENA_ALLOC(&w->frame_arena, *barrier);
                *barrier = (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .oldLayout = transition->old_layout,
                    .newLayout = transition->new_layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = transition->target_image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },

                    // idk about these for writes outside of initialization (LAYOUT_UNDEFINED -> ...)
                    .srcAccessMask = 0,                           
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,                           
                };
            }

            if (transition_count) {
                vkCmdPipelineBarrier(
                    w->cmd_buffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // srcStageMask
                    VK_PIPELINE_STAGE_TRANSFER_BIT,       // dstStageMask
                    0,                                    // dependencyFlags
                    0,                                    // memoryBarrierCount
                    NULL,                                 // pMemoryBarriers
                    0,                                    // bufferBarrierCount  
                    NULL,                                 // pBufferBarriers
                    (U32)transition_count,                // imageBarrierCount
                    image_barriers                        // pImageBarriers
                );
            }

            U64 copy_count = 0;

            StagingCopyBufferList *copy_b = w->staging_buffer.staging_copies_buffer;
            for (; copy_b != NULL; copy_b = copy_b->next) {
                if (copy_b->copy_count == 0) continue;
                copy_count += copy_b->copy_count;

                vkCmdCopyBuffer(
                    w->cmd_buffer,
                    w->staging_buffer.buffer,
                    copy_b->target_buffer,
                    copy_b->copy_count,
                    copy_b->buffer_copies
                );
            }

            StagingCopyImageList *copy_i = w->staging_buffer.staging_copies_image;
            for (; copy_i != NULL; copy_i = copy_i->next) {
                if (copy_i->copy_count == 0) continue;
                copy_count += copy_i->copy_count;

                vkCmdCopyBufferToImage(
                    w->cmd_buffer,
                    w->staging_buffer.buffer,
                    copy_i->target_image,
                    VK_IMAGE_LAYOUT_GENERAL,
                    copy_i->copy_count,
                    copy_i->buffer_image_copies
                );
            }

            if (copy_count | transition_count) {
                // unused for now as we require the COHERENT bit
                //_mm_sfence(); // flush cpu caches

                //VkMappedMemoryRange staging_range = {
                //    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                //    .memory = w->staging_buffer.buffer_memory,
                //    .offset = 0,
                //    .size = ALIGN_UP_OFFSET(w->staging_buffer.staging_head, w->phy_mem_props.nonCoherentAtomSize),
                //};
                //vkFlushMappedMemoryRanges(device, 1, &staging_range);

                VkMemoryBarrier staging_barrier = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,                           
                    .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                };
                  
                vkCmdPipelineBarrier(
                    w->cmd_buffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT ,      // srcStageMask
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,   // dstStageMask
                    0,                                    // dependencyFlags
                    1,                                    // memoryBarrierCount
                    &staging_barrier,                     // pMemoryBarriers
                    0,                                    // bufferBarrierCount  
                    NULL,                                 // pBufferBarriers
                    0,                                    // imageBarrierCount
                    NULL                                  // pImageBarriers
                );
            }
        }

        // START RENDER ----------------------------------------------------------

        SCOPE_TRACE {
            RGBA8 background = COLOUR_BACKGROUND;
            VkClearValue clear_colour = { .color = { .float32 = {
                (float)background.r / 255.f,
                (float)background.g / 255.f,
                (float)background.b / 255.f,
                (float)background.a / 255.f,
            }}};
            VkRenderPassBeginInfo pass_info = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = w->pass,
                .framebuffer = w->sc->framebuffers[sc_image_idx],
                .renderArea = {
                    .offset = {0, 0},
                    .extent = { width_i, height_i },
                },
                .clearValueCount = 1,
                .pClearValues = &clear_colour,
            };
            vkCmdBeginRenderPass(w->cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
        }

        SCOPE_TRACE {
            VkViewport v = {
                .x = 0.f,
                .y = 0.f,
                .width = width,
                .height = height,
                .minDepth = 0.f,
                .maxDepth = 1.f,
            };
            VkRect2D s = { { 0, 0 }, { width_i, height_i } };
            vkCmdBindPipeline(w->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, w->pl);
            vkCmdSetViewport(w->cmd_buffer, 0, 1, &v);
            vkCmdSetScissor(w->cmd_buffer, 0, 1, &s);

            vkCmdBindDescriptorSets(
                w->cmd_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                w->pl_layout,
                0,
                1,
                &descriptor_set_glyphs,
                0,
                NULL
            );

            if (glyphs_size) {
                expect(ui->glyph_count < MAX_GLYPHS);
                vkCmdDraw(w->cmd_buffer, 4, (U32)ui->glyph_count, 0, 0);
            }
        }

        SCOPE_TRACE {
            vkCmdEndRenderPass(w->cmd_buffer);
            VK_ASSERT(vkEndCommandBuffer(w->cmd_buffer));
        }

        SCOPE_TRACE {
            VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &w->image_available,
                .pWaitDstStageMask = &dst_stage_mask,
                .commandBufferCount = 1,
                .pCommandBuffers = &w->cmd_buffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &w->render_finished,
            };
            VK_ASSERT(vkQueueSubmit(w->queue, 1, &submit_info, w->in_flight));
        }

        SCOPE_TRACE {
            VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                //.pNext = &(VkSwapchainPresentScalingCreateInfoEXT) {
                //    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,
                //    .scalingBehavior = VK_PRESENT_SCALING_STRETCH_BIT_EXT,
                //},
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &w->render_finished,
                .swapchainCount = 1,
                .pSwapchains = &w->sc->sc,
                .pImageIndices = &sc_image_idx,
            };
            VkResult res = vkQueuePresentKHR(w->queue, &present_info);
            expect(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
        }

        // RESET STATE -----------------------------------------------------------

        arena_clear(&w->frame_arena);
        staging_buffer_reset(&w->staging_buffer);
        frame += 1.0f;
    }

    ui_destroy(ui);

    VK_ASSERT(vkWaitForFences(w->device, 1, &w->in_flight, VK_TRUE, UINT64_MAX));
    vkDeviceWaitIdle(w->device);

    gpu_free(w->static_data_uniform_buffer_memory);
    vkDestroyBuffer(w->device, w->static_data_uniform_buffer, NULL);
    gpu_free(glyph_draw_buffer_memory);
    vkDestroyBuffer(w->device, glyph_draw_buffer, NULL);

    descriptor_set_destroy(descriptor_set_glyphs);
    font_atlas_destroy(font_atlas);
    VK_ASSERT(vkWaitForFences(w->device, 1, &w->in_flight, VK_TRUE, UINT64_MAX));
    window_destroy();

    arena_destroy(&static_arena);
    return 0;
}

void glfw_callback_key(GLFWwindow *window, int key, int scan, int action, int mods) { TRACE
    (void)window;
    if (w->inputs.key_event_count == EVENTS_MAX) return;

    U64 kmask = key_mask(key);
    U32 mmask = mod_mask(key);
    U64 smask = special_mask(key);

    if (action == GLFW_PRESS) {
        w->inputs.key_events[w->inputs.key_event_count++] = (KeyEvent) { key, scan, action, mods };

        w->inputs.key_special_pressed |= smask;
        w->inputs.key_held |= kmask;
        w->inputs.modifiers |= mmask;
    } else if (action == GLFW_REPEAT) {
        w->inputs.key_special_repeating |= smask;
        w->inputs.key_repeating |= kmask;
    } else if (action == GLFW_RELEASE) {
        w->inputs.key_held &= ~kmask;
        w->inputs.modifiers &= ~mmask;
    }
}

void glfw_callback_char(GLFWwindow *window, unsigned int codepoint) { TRACE
    (void)window;
    if (w->inputs.char_event_count == EVENTS_MAX) return;
    w->inputs.char_events[w->inputs.char_event_count++] = (CharEvent) { codepoint };
}

void glfw_callback_mouse_pos(GLFWwindow *window, double x, double y) { TRACE
    (void)window;
    w->inputs.mouse_x = (F32)x;
    w->inputs.mouse_y = (F32)y;
}

void glfw_callback_mouse_button(GLFWwindow *window, int button, int action, int mods) { TRACE
    (void)mods;
    (void)window;
    if (action == GLFW_PRESS) {
        w->inputs.mouse_held |= (1u << button);
    } else if (action == GLFW_RELEASE) {
        w->inputs.mouse_held &= ~(1u << button);
    }
}

void glfw_callback_scroll(GLFWwindow *window, double x, double y) { TRACE
    (void)x;
    (void)window;
    w->inputs.scroll = (F32)y;
}

Swapchain *swapchain_create(
    VkDevice device,
    VkPhysicalDevice phy_device,
    VkSurfaceKHR surface,
    VkRenderPass pass,
    Arena *arena
) { TRACE
    Swapchain *sc = ARENA_ALLOC(arena, *sc);

    VkSurfaceCapabilitiesKHR surface_cap;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &surface_cap));

    // SWAPCHAIN ----------------------------------------------------------

    VkExtent2D sc_extent = surface_cap.currentExtent;
    U32 width = sc_extent.width;
    U32 height = sc_extent.height;
    sc->width = width;
    sc->height = height;

    U32 image_count = surface_cap.minImageCount + 1;
    if (surface_cap.maxImageCount != 0)
        expect(image_count <= surface_cap.maxImageCount);

    // swapchain creation
    VkSwapchainCreateInfoKHR sc_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = SURFACE_FORMAT,
        .imageColorSpace = SURFACE_COLOUR_SPACE,
        .imageExtent = sc_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_cap.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = SURFACE_PRESENT_MODE,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VK_ASSERT(vkCreateSwapchainKHR(device, &sc_info, NULL, &sc->sc));

    // swapchain images
    VK_ASSERT(vkGetSwapchainImagesKHR(device, sc->sc, &sc->image_count, NULL));
    sc->images = ARENA_ALLOC_ARRAY(arena, VkImage, sc->image_count);
    sc->image_views = ARENA_ALLOC_ARRAY(arena, VkImageView, sc->image_count);
    VK_ASSERT(vkGetSwapchainImagesKHR(device, sc->sc, &sc->image_count, sc->images));

    // swapchain image views
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SURFACE_FORMAT,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    for (U32 i = 0; i < sc->image_count; ++i) {
        view_info.image = sc->images[i];
        VK_ASSERT(vkCreateImageView(device, &view_info, NULL, &sc->image_views[i]));
    }

    // FRAMEBUFFERS ---------------------------------------------------------------

    sc->framebuffers = ARENA_ALLOC_ARRAY(arena, VkFramebuffer, sc->image_count);
    {
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pass,
            .attachmentCount = 1,
            .width = width,
            .height = height,
            .layers = 1,
        };

        for (U32 i = 0; i < sc->image_count; ++i) {
            VkImageView attachments[] = { sc->image_views[i] };
            fb_info.pAttachments = attachments;
            VK_ASSERT(vkCreateFramebuffer(device, &fb_info, NULL, &sc->framebuffers[i]));
        }
    }

    return sc;
}

void swapchain_destroy(VkDevice device, Swapchain *sc) { TRACE
    for (U32 i = 0; i < sc->image_count; ++i) {
        vkDestroyFramebuffer(device, sc->framebuffers[i], NULL);
        vkDestroyImageView(device, sc->image_views[i], NULL);
    }
    vkDestroySwapchainKHR(device, sc->sc, NULL);
}

static int min_i(int a, int b) { TRACE
    return a < b ? a : b;
}

static int max_i(int a, int b) { TRACE
    return a > b ? a : b;
}

U8 *copy_cstr(Arena *arena, const U8 *str) {
    U64 str_len = my_strlen(str);
    U8 *new_str = arena_alloc(arena, str_len + 1, 1);
    memcpy(new_str, str, str_len + 1);
    return new_str;
}

U8 *copy_str(Arena *arena, const U8 *str, U32 str_len) {
    U8 *new_str = arena_alloc(arena, str_len, 1);
    memcpy(new_str, str, str_len);
    return new_str;
}

U8 *path_join(Arena *arena, const U8 *a, const U8 *b) {
    size_t len_a = my_strlen(a);
    size_t len_b = my_strlen(b);
    bool add_separator = len_a != 0 && a[len_a-1] != '/';
    
    U8 *path = arena_alloc(arena, len_a + len_b + 1 + (size_t)add_separator, 1);
    
    U8 *cur = path;
    
    memcpy(cur, a, len_a);
    cur += len_a;
    
    if (add_separator) {
        *cur = '/';
        cur += 1;
    }
    
    memcpy(cur, b, len_b);
    cur += len_b;
    
    *cur = 0;
    
    return path;
}

// returns true if the text buffer was changed
bool write_inputs(U8 *text, U32 *text_len, U32 *cursor) {
    bool ret = false;

    if (*cursor > *text_len)
        *cursor = *text_len;
    
    for (I64 i = 0; i < w->inputs.char_event_count; ++i) {
        U32 codepoint = w->inputs.char_events[i].codepoint;
        // enforce ascii for now
        expect(codepoint < 128);

        U8 codepoint_as_char = (U8)codepoint;
        
        memmove(text + *cursor + 1, *text + cursor, *text_len - *cursor);
        text[*cursor] = codepoint_as_char;
        *text_len += 1;
        *cursor += 1;
        
        ret = true;
    }
    
    U64 special_pressed = w->inputs.key_special_pressed;
    U64 special_repeating = w->inputs.key_special_repeating;
    bool backspace = is(special_pressed | special_repeating, special_mask(GLFW_KEY_BACKSPACE));  
    if (*cursor > 0 && backspace) {
        memmove(text + *cursor - 1, *text + cursor, *text_len - *cursor);
        *text_len -= 1;
        *cursor -= 1;
        
        ret = true;
    }
    
    // ensure null terminated
    text[*text_len] = 0;
    
    return ret;
}

GLFWmonitor* glfw_get_current_monitor(GLFWwindow *window) { TRACE
    // adapted from https://stackoverflow->com/a/31526753
    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);

    int monitor_count;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);

    int best_overlap = 0;
    GLFWmonitor *best_monitor = NULL;
    for (int i = 0; i < monitor_count; i++) {
        const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);

        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        int mw = mode->width;
        int mh = mode->height;

        int overlap =
            max_i(0, min_i(wx + ww, mx + mw) - max_i(wx, mx)) *
            max_i(0, min_i(wy + wh, my + mh) - max_i(wy, my));

        if (best_overlap < overlap) {
            best_overlap = overlap;
            best_monitor = monitors[i];
        }
    }

    return best_monitor;
}
