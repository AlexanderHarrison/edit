#ifndef COMMON_H_
#define COMMON_H_

#include <math.h>
#include <tools.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define KB (1ull << 10)
#define MB (1ull << 20)
#define GB (1ull << 30)

#define countof(A) (sizeof(A)/sizeof(*A))
#define VK_ASSERT(expr) assert(expr == VK_SUCCESS);

#ifndef FASTMODE
#define arena_reset(A, B) arena_reset_safe(A, B)
#endif

typedef struct RGBA8 {
    U8 r, g, b, a;
} RGBA8;

typedef struct StagingCopyBufferList {
    struct StagingCopyBufferList *next;
    VkBuffer target_buffer;
    U32 copy_count;
    VkBufferCopy *buffer_copies;
} StagingCopyBufferList;

typedef struct StagingCopyImageList {
    struct StagingCopyImageList *next;
    VkImage target_image;
    U32 copy_count;
    VkBufferImageCopy *buffer_image_copies;
} StagingCopyImageList;

typedef struct ImageTransitionList {
    struct ImageTransitionList *next;
    VkImage target_image;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
} ImageTransitionList;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory buffer_memory;
    U8 *mapped_ptr;
    U8 *staging_head;

    StagingCopyBufferList *staging_copies_buffer;
    StagingCopyImageList *staging_copies_image;
    ImageTransitionList *image_transitions;
} StagingBuffer;

typedef struct {
    // STATIC DATA -------------------------------

    GLFWwindow *window;
    VkInstance instance;
    VkPhysicalDevice phy_device;
    VkPhysicalDeviceMemoryProperties phy_mem_props;

    VkDevice device;
    VkQueue queue;
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    VkSurfaceKHR surface;

    VkSwapchainKHR sc;
    VkExtent2D sc_extent;
    VkImage *sc_images;
    VkImageView *sc_image_views;
    U64 sc_image_count;

    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    VkRenderPass pass;
    VkFramebuffer* sc_framebuffers;
    VkPipelineLayout pl_layout;
    VkPipeline pl;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout_glyphs;

    // PER FRAME DATA -------------------------------

    Arena frame_arena;
    StagingBuffer staging_buffer;
} W;

VkResult
gpu_alloc_buffer(
    W *w,
    VkBuffer buffer, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
);

VkResult
gpu_alloc_image(
    W *w,
    VkImage image, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
);

void
gpu_free(W *w, VkDeviceMemory mem);

U8 *
staging_buffer_alloc(StagingBuffer *staging_buffer, U64 size, U64 alignment);

void
staging_buffer_cmd_copy_to_buffer(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkBuffer target,
    U32 copy_count,
    VkBufferCopy *buffer_copies // MUST POINT TO FRAME ARENA
);

void staging_buffer_cmd_copy_to_image(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkImage target,
    U32 copy_count,
    VkBufferImageCopy *buffer_image_copies // MUST POINT TO FRAME ARENA
);

void
staging_buffer_push_image_transition(
    StagingBuffer *staging_buffer,
    Arena *arena,
    VkImage target,
    VkImageLayout old_layout,
    VkImageLayout new_layout
);

void
staging_buffer_reset(StagingBuffer *staging_buffer);

#endif
