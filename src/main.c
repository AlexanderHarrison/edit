#include "common.h"

#include "font.h"
#include "font.c"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define WIDTH 800
#define HEIGHT 800

#define PROJECT_NAME "Editor"

#define STAGING_BUFFER_SIZE (64*MB)
#define SURFACE_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define SURFACE_COLOUR_SPACE VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define SURFACE_PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR
#define MAX_GLYPHS 1024
#define MAX_GLYPHS_SIZE (1024*sizeof(Glyph))

W
window_create(Arena *arena);

void
window_destroy(W *w);

VkDescriptorSet
descriptor_set_glyphs_create(W *w, FontAtlas *font_atlas, VkBuffer glyphs_buffer);

void
descriptor_set_destroy(W *w, VkDescriptorSet descriptor_set);

void glfw_callback_key          (GLFWwindow *window, int key, int scan, int action, int mods);
void glfw_callback_char         (GLFWwindow *window, unsigned int codepoint);
void glfw_callback_mouse_pos    (GLFWwindow *window, double x, double y);
void glfw_callback_mouse_button (GLFWwindow *window, int button, int action, int mods);
void glfw_callback_scroll       (GLFWwindow *window, double x, double y);

int
main(void);

// WINDOWING FUNCTION #####################################################

W window_create(Arena *arena) {
    // GLFW --------------------------------------------------------------------------------

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, PROJECT_NAME, NULL, NULL);

    U32 glfw_ext_count;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    // VALIDATION --------------------------------------------------------------------------------

    const char *validation_layer = "VK_LAYER_KHRONOS_validation";
    {
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
        assert(validation_found);
        arena_reset(arena, &reset);
    }

    // INSTANCES --------------------------------------------------------------------------------
    
    VkInstance instance;
    {
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
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = &validation_layer,
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
    {
        ArenaResetPoint reset = arena_reset_point(arena);
        U32 device_count;
        VK_ASSERT(vkEnumeratePhysicalDevices(instance, &device_count, NULL));
        assert(device_count != 0);
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

        assert(found_dev);
        arena_reset(arena, &reset);
    }

    VkPhysicalDeviceMemoryProperties phy_mem_props;
    vkGetPhysicalDeviceMemoryProperties(phy_device, &phy_mem_props);

    // QUEUES --------------------------------------------------------------------

    U32 queue_family_idx = ~0u;
    {
        ArenaResetPoint reset = arena_reset_point(arena);
        U32 family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &family_count, NULL);
        assert(family_count != 0);
        VkQueueFamilyProperties *families = ARENA_ALLOC_ARRAY(arena, sizeof(VkQueueFamilyProperties), family_count);
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

        assert(family_found);
        arena_reset(arena, &reset);
    }

    // LOGICAL DEVICES --------------------------------------------------------------------

    VkDevice device;
    {
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
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = &validation_layer,
        };

        VK_ASSERT(vkCreateDevice(phy_device, &device_info, NULL, &device));
    }

    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_idx, 0, &queue);

    // SURFACE STUFF ------------------------------------------------------------------------

    VkSurfaceCapabilitiesKHR surface_cap;
    {
        ArenaResetPoint reset = arena_reset_point(arena);
        VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &surface_cap));

        // assert present mode 
        U32 present_mode_count;
        VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &present_mode_count, NULL));
        assert(present_mode_count != 0);
        VkPresentModeKHR *present_modes = ARENA_ALLOC_ARRAY(arena, sizeof(VkPresentModeKHR), present_mode_count);
        VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &present_mode_count, present_modes));
        bool mode_found = false;
        for (U32 i = 0; i < present_mode_count; ++i) {
            if (present_modes[i] == SURFACE_PRESENT_MODE) {
                mode_found = true;
                break;
            }
        }
        assert(mode_found);

        // assert format
        U32 format_count;
        VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &format_count, NULL));
        VkSurfaceFormatKHR *formats = ARENA_ALLOC_ARRAY(arena, sizeof(VkSurfaceFormatKHR), format_count);
        VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &format_count, formats));
        bool format_found = false;
        for (U32 i = 0; i < format_count; ++i) {
            if (formats[i].format == SURFACE_FORMAT && formats[i].colorSpace == SURFACE_COLOUR_SPACE) {
                format_found = true;
                break;
            }
        }
        assert(format_found);
        arena_reset(arena, &reset);
    }

    // SWAPCHAIN -----------------------------------------------------------------------------
    
    VkSwapchainKHR sc;
    VkExtent2D sc_extent;
    VkImage *sc_images;
    VkImageView *sc_image_views;
    U32 sc_image_count;
    {
        // swapchain size
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        sc_extent = (VkExtent2D) { (U32)width, (U32)height };
        U32 image_count = surface_cap.minImageCount + 1;
        if (surface_cap.maxImageCount != 0)
            assert(image_count <= surface_cap.maxImageCount);

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

        VK_ASSERT(vkCreateSwapchainKHR(device, &sc_info, NULL, &sc));

        // swapchain images
        VK_ASSERT(vkGetSwapchainImagesKHR(device, sc, &sc_image_count, NULL));
        sc_images = ARENA_ALLOC_ARRAY(arena, sizeof(VkImage), sc_image_count);
        sc_image_views = ARENA_ALLOC_ARRAY(arena, sizeof(VkImageView), sc_image_count);
        VK_ASSERT(vkGetSwapchainImagesKHR(device, sc, &sc_image_count, sc_images));

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

        for (U32 i = 0; i < sc_image_count; ++i) {
            view_info.image = sc_images[i];
            VK_ASSERT(vkCreateImageView(device, &view_info, NULL, &sc_image_views[i]));
        }
    }

    // DESCRIPTOR SET POOL ---------------------------------------------------------------

    VkDescriptorPool descriptor_pool;
    {
        VkDescriptorPoolSize pools[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 },
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
    {
        static const VkDescriptorSetLayoutBinding bindings[] = {
            // atlas
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
            },

            // glyph lookup table
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
            },

            // glyphs to draw
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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

    // COMMAND POOL ----------------------------------------------------------------------

    VkCommandPool cmd_pool;
    {
        VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queue_family_idx,
        };
        VK_ASSERT(vkCreateCommandPool(device, &pool_info, NULL, &cmd_pool));
    }

    // COMMAND BUFFER ----------------------------------------------------------------------

    VkCommandBuffer cmd_buffer;
    {
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
    {
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
    {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = STAGING_BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VK_ASSERT(vkCreateBuffer(device, &info, NULL, &staging.buffer));

        // staging.buffer_memory will be allocated later
    }

    // SHADERS -------------------------------------------------------

    Bytes vert_source = read_file_in("build/main_vert.spv", arena);
    Bytes frag_source = read_file_in("build/main_frag.spv", arena);

    assert(vert_source.ptr != NULL);
    assert(frag_source.ptr != NULL);

    VkShaderModule vert_module, frag_module;
    {
        #pragma GCC diagnostic ignored "-Wcast-align"
        VkShaderModuleCreateInfo vert_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = vert_source.len,
            .pCode = (const U32*)vert_source.ptr,
        };

        #pragma GCC diagnostic ignored "-Wcast-align"
        VkShaderModuleCreateInfo frag_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = frag_source.len,
            .pCode = (const U32*)frag_source.ptr,
        };
        VK_ASSERT(vkCreateShaderModule(device, &vert_info, NULL, &vert_module));
        VK_ASSERT(vkCreateShaderModule(device, &frag_info, NULL, &frag_module));
    }

    // RENDER PASS -------------------------------------------------------

    VkRenderPass pass;
    {
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

    // PIPELINE -----------------------------------------------------------------

    VkPipelineLayout pl_layout;
    {
        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptor_set_layout_glyphs,
        };

        VK_ASSERT(vkCreatePipelineLayout(device, &layout_info, NULL, &pl_layout));
    }

    VkPipeline pl;
    {
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

        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (F32) sc_extent.width,
            .height = (F32) sc_extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = sc_extent,
        };

        VkPipelineViewportStateCreateInfo viewport_state_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
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
            .blendEnable = VK_FALSE,
        };

        VkPipelineColorBlendStateCreateInfo colour_blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colour_blend_attachment,
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        const VkPipelineShaderStageCreateInfo stages[] = {vert_info, frag_info};
        
        VkGraphicsPipelineCreateInfo pl_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &vert_input_info,
            .pInputAssemblyState = &primitive_info,
            .pViewportState = &viewport_state_info,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &ms,
            .pDepthStencilState = NULL,
            .pColorBlendState = &colour_blend_state,
            .pDynamicState = NULL,
            .layout = pl_layout,
            .renderPass = pass,
            .subpass = 0,
        };

        VK_ASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pl_info, NULL, &pl));
    }

    vkDestroyShaderModule(device, vert_module, NULL);
    vkDestroyShaderModule(device, frag_module, NULL);

    // FRAMEBUFFERS ---------------------------------------------------------------

    VkFramebuffer *sc_framebuffers = ARENA_ALLOC_ARRAY(arena, sizeof(VkFramebuffer), sc_image_count);
    {
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pass,
            .attachmentCount = 1,
            .width = sc_extent.width,
            .height = sc_extent.height,
            .layers = 1,
        };

        for (U32 i = 0; i < sc_image_count; ++i) {
            VkImageView attachments[] = { sc_image_views[i] };
            fb_info.pAttachments = attachments;
            VK_ASSERT(vkCreateFramebuffer(device, &fb_info, NULL, &sc_framebuffers[i]));
        }
    }

    Inputs inputs = {
        .char_events = ARENA_ALLOC_ARRAY(arena, CharEvent, MAX_EVENTS),
    };

    // CREATE W -------------------------------------------------------------------------------

    W w = { 
        window, instance, phy_device, phy_mem_props, device, queue, cmd_pool, cmd_buffer,
        surface, sc, sc_extent, sc_images, sc_image_views, sc_image_count,
        image_available, render_finished, in_flight,
        pass, sc_framebuffers, pl_layout, pl,
        descriptor_pool, descriptor_set_layout_glyphs,

        inputs, frame_arena, staging
    };

    // ALLOC GPU BUFFERS ---------------------------------------------------------------------

    {
        VK_ASSERT(gpu_alloc_buffer(
            &w, 
            w.staging_buffer.buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &w.staging_buffer.buffer_memory
        ));
        VK_ASSERT(vkMapMemory(w.device, w.staging_buffer.buffer_memory, 0, STAGING_BUFFER_SIZE, 0, (void**) &w.staging_buffer.mapped_ptr));
        staging_buffer_reset(&w.staging_buffer);
    }

    // END -----------------------------------------------------------------------------------

    return w;
}

// HELDER FUNCTION DEFINITIONS #####################################################

void window_destroy(W *w) {
    VkDevice device = w->device;
    VkInstance instance = w->instance;

    vkDestroyDescriptorSetLayout(device, w->descriptor_set_layout_glyphs, NULL);

    for (U32 i = 0; i < w->sc_image_count; ++i)
        vkDestroyFramebuffer(device, w->sc_framebuffers[i], NULL);

    vkDestroyRenderPass(device, w->pass, NULL);
    vkDestroyPipelineLayout(device, w->pl_layout, NULL);
    vkDestroyPipeline(device, w->pl, NULL);

    vkDestroyDescriptorPool(device, w->descriptor_pool, NULL);

    gpu_free(w, w->staging_buffer.buffer_memory);
    vkDestroyBuffer(device, w->staging_buffer.buffer, NULL);

    vkDestroySemaphore(device, w->image_available, NULL);
    vkDestroySemaphore(device, w->render_finished, NULL);
    vkDestroyFence(device, w->in_flight, NULL);

    for (U32 i = 0; i < w->sc_image_count; ++i)
        vkDestroyImageView(device, w->sc_image_views[i], NULL);

    vkDestroySwapchainKHR(device, w->sc, NULL);
    vkDestroySurfaceKHR(instance, w->surface, NULL);
    vkDestroyCommandPool(device, w->cmd_pool, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(w->window);
    glfwTerminate();
}

void gpu_free(W *w, VkDeviceMemory mem) {
    vkFreeMemory(w->device, mem, NULL);
}

static VkResult gpu_alloc(
    W *w,
    VkMemoryRequirements *mem_req,
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
) {
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

    assert(found_mem);

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
    W *w,
    VkBuffer buffer, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
) {
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(w->device, buffer, &mem_req);
    VkResult res = gpu_alloc(w, &mem_req, props, mem);
    if (res != VK_SUCCESS) return res;
    return vkBindBufferMemory(w->device, buffer, *mem, 0);
}

VkResult gpu_alloc_image(
    W *w,
    VkImage image, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
) {
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(w->device, image, &mem_req);
    VkResult res = gpu_alloc(w, &mem_req, props, mem);
    if (res != VK_SUCCESS) return res;
    return vkBindImageMemory(w->device, image, *mem, 0);
}

// returns an offset into the staging buffer
U8 *staging_buffer_alloc(StagingBuffer *staging_buffer, U64 size, U64 alignment) {
    U8 *alloc_start = ALIGN_UP(staging_buffer->staging_head, alignment);
    U8 *alloc_end = alloc_start + size;
    assert(alloc_end - staging_buffer->mapped_ptr <= (I64)STAGING_BUFFER_SIZE);
    staging_buffer->staging_head = alloc_end;
    return alloc_start;
}

void staging_buffer_cmd_copy_to_buffer(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkBuffer target,
    U32 copy_count,
    VkBufferCopy *buffer_copies // MUST POINT TO FRAME ARENA
) {
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
) {
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
) {
    ImageTransitionList *prev = staging_buffer->image_transitions;
    ImageTransitionList *new = ARENA_ALLOC(arena, *new);
    *new = (ImageTransitionList) { prev, target, old_layout, new_layout};
    staging_buffer->image_transitions = new;
}

void staging_buffer_reset(StagingBuffer *staging_buffer) {
    staging_buffer->staging_head = staging_buffer->mapped_ptr;
    staging_buffer->staging_copies_buffer = NULL;
    staging_buffer->staging_copies_image = NULL;
    staging_buffer->image_transitions = NULL;
}

VkDescriptorSet descriptor_set_glyphs_create(
    W *w, FontAtlas *font_atlas, VkBuffer glyphs_buffer
) {
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
        };

        U32 write_count = countof(descriptor_writes);
        vkUpdateDescriptorSets(w->device, write_count, descriptor_writes, 0, NULL);
    }

    return descriptor_set;
}

void descriptor_set_destroy(W *w, VkDescriptorSet descriptor_set) {
    vkFreeDescriptorSets(w->device, w->descriptor_pool, 1, &descriptor_set);
}


// MAIN #####################################################

#define SIZE_X FontSize_Count
#define SIZE_Y 5*2

int main(void) {
    Arena static_arena = arena_create_sized(1ull << 30); // 1 GB, virtual allocated

    // gltf window and vulkan -------------------------------------------

    W w = window_create(&static_arena);

    // font atlas -------------------------------------------------------

    const char *ttf_path = "/usr/share/fonts/TTF/RobotoMono-Medium.ttf";
    FontAtlas *font_atlas = font_atlas_create(&w, &static_arena, ttf_path);

    // Editor -----------------------------------------------------------

    glfwSetWindowUserPointer(w.window, &w);
    glfwSetCharCallback(w.window, glfw_callback_char);
    glfwSetKeyCallback(w.window, glfw_callback_key);
    glfwSetCursorPosCallback(w.window, glfw_callback_mouse_pos);
    glfwSetMouseButtonCallback(w.window, glfw_callback_mouse_button);
    glfwSetScrollCallback(w.window, glfw_callback_scroll);

    //Editor editor = editor_create(&w, &static_arena, font_atlas);

    // glyph draw buffer ------------------------------------------------

    VkBuffer glyph_draw_buffer;
    VkDeviceMemory glyph_draw_buffer_memory;
    {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = MAX_GLYPHS_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        };
        VK_ASSERT(vkCreateBuffer(w.device, &info, NULL, &glyph_draw_buffer));
        VK_ASSERT(gpu_alloc_buffer(
            &w,
            glyph_draw_buffer,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &glyph_draw_buffer_memory
        ));
    }

    // alloc descriptor sets --------------------------------------------

    VkDescriptorSet descriptor_set_glyphs = descriptor_set_glyphs_create(&w, font_atlas, glyph_draw_buffer);

    F32 frame = 0.0;
    while (!glfwWindowShouldClose(w.window)) {
        w.inputs.char_event_count = 0;
        w.inputs.mouse_held_prev = w.inputs.mouse_held;
        w.inputs.mouse_held = 0;
        w.inputs.scroll = 0.f;
        glfwPollEvents();
        w.inputs.mouse_in_window = glfwGetWindowAttrib(w.window, GLFW_HOVERED) != 0;
        w.inputs.mouse_pressed = w.inputs.mouse_held & ~w.inputs.mouse_held_prev;
        w.inputs.mouse_released = w.inputs.mouse_held_prev & ~w.inputs.mouse_held;

        // UPDATE ----------------------------------------------------------------

        GlyphSlice glyphs = { NULL, 0 };
        //GlyphSlice glyphs = editor_update(&w);
        U64 glyphs_size = glyphs.count * sizeof(Glyph);

        // write glyphs buffer

        //const char *text = "Hello, World!";
        //U64 glyphs_count = strlen(text);
        //U64 glyphs_size = glyphs_count * sizeof(Glyph);
        //Glyph *glyphs = ARENA_ALLOC_ARRAY(&w.frame_arena, *glyphs, glyphs_count);

        //F32 pen_x = 50.f;
        //for (U64 ch_i = 0; ch_i < glyphs_count; ++ch_i) {
        //    char ch = text[ch_i];
        //    U32 glyph_idx = glyph_lookup_idx(FontSize_16, ch);
        //    GlyphInfo info = font_atlas->glyph_info[glyph_idx];

        //    glyphs[ch_i] = (Glyph) {
        //        .x = pen_x + info.offset_x,
        //        .y = 400.f + info.offset_y,
        //        .glyph_idx = glyph_idx,
        //        .colour = { 255, 255, 255, 255 },
        //    };

        //    pen_x += info.advance_width;
        //}

        if (glyphs.count) {
            Glyph* staging_glyph_draws = (Glyph*)staging_buffer_alloc(&w.staging_buffer, glyphs_size, 16);
            memcpy(staging_glyph_draws, glyphs.ptr, glyphs_size);

            VkBufferCopy *buffer_copy = ARENA_ALLOC(&w.frame_arena, *buffer_copy);
            *buffer_copy = (VkBufferCopy) {
                .srcOffset = (U64)((U8*)staging_glyph_draws - w.staging_buffer.mapped_ptr),
                .dstOffset = 0,
                .size = glyphs_size,
            };

            staging_buffer_cmd_copy_to_buffer(
                &w.staging_buffer,
                &w.frame_arena,
                glyph_draw_buffer,
                1,
                buffer_copy
            );
        }

        // WAIT FOR NEXT FRAME ---------------------------------------------------

        U32 sc_image_idx;
        {
            VK_ASSERT(vkWaitForFences(w.device, 1, &w.in_flight, VK_TRUE, UINT64_MAX));
            VK_ASSERT(vkResetFences(w.device, 1, &w.in_flight));
            VK_ASSERT(vkAcquireNextImageKHR(w.device, w.sc, UINT64_MAX, w.image_available, VK_NULL_HANDLE, &sc_image_idx));
        }

        // START RECORDING -------------------------------------------------------

        {
            VK_ASSERT(vkResetCommandBuffer(w.cmd_buffer, 0));
            VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,
                .pInheritanceInfo = NULL,
            };
            VK_ASSERT(vkBeginCommandBuffer(w.cmd_buffer, &begin_info));
        }

        // WRITE TRANSFER COMMANDS ------------------------------------------------

        {
            U64 transition_count = 0;
            VkImageMemoryBarrier *image_barriers = arena_prealign(&w.frame_arena, alignof(VkImageMemoryBarrier));

            ImageTransitionList *transition = w.staging_buffer.image_transitions;
            for (; transition != NULL; transition = transition->next) {
                VkImageLayout old_layout = transition->old_layout;
                VkImageLayout new_layout = transition->new_layout;

                if (old_layout == new_layout) continue;

                transition_count++;

                VkImageMemoryBarrier *barrier = ARENA_ALLOC(&w.frame_arena, *barrier);
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
                    w.cmd_buffer,
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

            StagingCopyBufferList *copy_b = w.staging_buffer.staging_copies_buffer;
            for (; copy_b != NULL; copy_b = copy_b->next) {
                if (copy_b->copy_count == 0) continue;
                copy_count += copy_b->copy_count;

                vkCmdCopyBuffer(
                    w.cmd_buffer,
                    w.staging_buffer.buffer,
                    copy_b->target_buffer,
                    copy_b->copy_count,
                    copy_b->buffer_copies
                );
            }

            StagingCopyImageList *copy_i = w.staging_buffer.staging_copies_image;
            for (; copy_i != NULL; copy_i = copy_i->next) {
                if (copy_i->copy_count == 0) continue;
                copy_count += copy_i->copy_count;

                vkCmdCopyBufferToImage(
                    w.cmd_buffer,
                    w.staging_buffer.buffer,
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
                //    .memory = w.staging_buffer.buffer_memory,
                //    .offset = 0,
                //    .size = ALIGN_UP_OFFSET(w.staging_buffer.staging_head, w.phy_mem_props.nonCoherentAtomSize),
                //};
                //vkFlushMappedMemoryRanges(device, 1, &staging_range);

                VkMemoryBarrier staging_barrier = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,                           
                    .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                };
                  
                vkCmdPipelineBarrier(
                    w.cmd_buffer,
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

        {
            VkClearValue clear_colour = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            VkRenderPassBeginInfo pass_info = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = w.pass,
                .framebuffer = w.sc_framebuffers[sc_image_idx],
                .renderArea = {
                    .offset = {0, 0},
                    .extent = w.sc_extent,
                },
                .clearValueCount = 1,
                .pClearValues = &clear_colour,
            };
            vkCmdBeginRenderPass(w.cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
        }

        {
            vkCmdBindPipeline(w.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, w.pl);

            vkCmdBindDescriptorSets(
                w.cmd_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                w.pl_layout,
                0,
                1,
                &descriptor_set_glyphs,
                0,
                NULL
            );

            if (glyphs.count) {
                assert(glyphs.count < MAX_GLYPHS);
                vkCmdDraw(w.cmd_buffer, 4, (U32)glyphs.count, 0, 0);
            }
        }

        {
            vkCmdEndRenderPass(w.cmd_buffer);
            VK_ASSERT(vkEndCommandBuffer(w.cmd_buffer));
        }

        {
            VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &w.image_available,
                .pWaitDstStageMask = &dst_stage_mask,
                .commandBufferCount = 1,
                .pCommandBuffers = &w.cmd_buffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &w.render_finished,
            };
            VK_ASSERT(vkQueueSubmit(w.queue, 1, &submit_info, w.in_flight));
        }

        {
            VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &w.render_finished,
                .swapchainCount = 1,
                .pSwapchains = &w.sc,
                .pImageIndices = &sc_image_idx,
            };
            VK_ASSERT(vkQueuePresentKHR(w.queue, &present_info));
        }

        // RESET STATE -----------------------------------------------------------

        arena_clear(&w.frame_arena);
        staging_buffer_reset(&w.staging_buffer);
        frame += 1.0f;
    }

    VK_ASSERT(vkWaitForFences(w.device, 1, &w.in_flight, VK_TRUE, UINT64_MAX));

    gpu_free(&w, glyph_draw_buffer_memory);
    vkDestroyBuffer(w.device, glyph_draw_buffer, NULL);

    descriptor_set_destroy(&w, descriptor_set_glyphs);
    font_atlas_destroy(&w, font_atlas);
    window_destroy(&w);
    arena_destroy(&static_arena);
    return 0;
}

void glfw_callback_key(GLFWwindow *window, int key, int scan, int action, int mods) {
    // TODO
    (void)key;
    (void)window;
    (void)scan;
    (void)action;
    (void)mods;
    //if (action != GLFW_
    //W *w = glfwGetWindowUserPointer(window);
    //w->inputs.
}

void glfw_callback_char(GLFWwindow *window, unsigned int codepoint) {
    W *w = glfwGetWindowUserPointer(window);
    
    // if we encounter too many events, replace the last event.
    if (w->inputs.char_event_count == MAX_EVENTS) w->inputs.char_event_count--;

    w->inputs.char_events[w->inputs.char_event_count++] = (CharEvent) { codepoint };
}

void glfw_callback_mouse_pos(GLFWwindow *window, double x, double y) {
    W *w = glfwGetWindowUserPointer(window);
    w->inputs.mouse_x = (F32)x;
    w->inputs.mouse_y = (F32)y;
}

void glfw_callback_mouse_button(GLFWwindow *window, int button, int action, int mods) {
    (void)mods;
    W *w = glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS) {
        w->inputs.mouse_held |= (1u << button);
    } else if (action == GLFW_RELEASE) {
        w->inputs.mouse_held &= ~(1u << button);
    }
}

void glfw_callback_scroll(GLFWwindow *window, double x, double y) {
    (void)y;
    W *w = glfwGetWindowUserPointer(window);
    w->inputs.scroll = (F32)x;
}
