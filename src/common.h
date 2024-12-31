#ifndef COMMON_H_
#define COMMON_H_

#include <math.h>
#include <tools.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define CODE_FONT_SIZE FontSize_14
#define CODE_LINE_SPACING 15.f
#define CODE_SCROLL_SPEED_SLOW 0.2f
#define CODE_SCROLL_SPEED_FAST 0.6f
#define ANIM_EXP_FACTOR 0.2f

#define BACKGROUND { 4, 4, 4, 255 }
#define FOREGROUND { 170, 170, 170, 255 }
#define SELECT { 40, 40, 40, 255 }
#define COMMENT { 230, 100, 100, 255 }
#define STRING { 100, 160, 100, 255 }

#define MAX_GLYPHS 4096
#define MAX_GLYPHS_SIZE (MAX_GLYPHS*sizeof(Glyph))

#define KB (1ull << 10)
#define MB (1ull << 20)
#define GB (1ull << 30)

#define countof(A) (sizeof(A)/sizeof(*A))
#define VK_ASSERT(expr) assert(expr == VK_SUCCESS);

#ifndef FASTMODE
#define arena_reset(A, B) arena_reset_safe(A, B)
#endif

#define MAX_EVENTS 128

typedef struct CharEvent {
    U32 codepoint;
} CharEvent;

typedef struct KeyEvent {
    int key, scan, action, mods;
} KeyEvent;

typedef struct Inputs {
    CharEvent *char_events;
    KeyEvent *key_events;
    U32 char_event_count;
    U32 key_event_count;

    // starting from GLFW_KEY_SPACE
    U64 key_held;
    U64 key_held_prev;
    U64 key_pressed;
    U64 key_released;
    U64 key_repeating;
    // starting from GLFW_KEY_ESCAPE
    U64 key_special_pressed;
    U32 modifiers;

    // mouse
    bool mouse_in_window;
    F32 mouse_x, mouse_y;
    U32 mouse_held;
    U32 mouse_held_prev;
    U32 mouse_pressed;
    U32 mouse_released;
    F32 scroll;
} Inputs;

static inline U64 key_mask(int glfw_key) {
    if (glfw_key < GLFW_KEY_SPACE || GLFW_KEY_SPACE + 64 <= glfw_key)
        return 0;
    else
        return 1ul << (glfw_key - GLFW_KEY_SPACE);
}

static inline U32 mod_mask(int glfw_key) {
    switch (glfw_key) {
        case GLFW_KEY_LEFT_SHIFT:
        case GLFW_KEY_RIGHT_SHIFT:
            return GLFW_MOD_SHIFT;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            return GLFW_MOD_CONTROL;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT:
            return GLFW_MOD_ALT;
        case GLFW_KEY_LEFT_SUPER:
        case GLFW_KEY_RIGHT_SUPER:
            return GLFW_MOD_SUPER;
        default:
            return 0;
    }
}

static inline U64 special_mask(int glfw_key) {
    if (glfw_key < GLFW_KEY_ESCAPE || GLFW_KEY_ESCAPE + 64 <= glfw_key)
        return 0;
    else
        return 1ul << (glfw_key - GLFW_KEY_ESCAPE);
}

typedef struct Rect {
    F32 x, y, w, h;
} Rect;

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

    Inputs inputs;
    Arena frame_arena;
    StagingBuffer staging_buffer;

    bool should_close;
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
