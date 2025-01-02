#ifndef COMMON_H_
#define COMMON_H_

// add filetype macros in dirent
#define _DEFAULT_SOURCE

#include <math.h>
#include <tools.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// timing information
#if 0
#include <execinfo.h>
static U8 backtrace_buffer[1024];
void timer_print(Timer *timer) {
    double elapsed = timer_elapsed_ms(timer);
    if (elapsed > 1.0) {
        int frames = backtrace((void*) &backtrace_buffer, 4);
        char **syms = backtrace_symbols((void*) &backtrace_buffer, frames);
        for (int i = frames-1; i > 0; --i) {
            printf("%s:\n", syms[i]);
        }
        printf("--------- %fms\n", elapsed);
        free(syms);
    }
}


#define INIT_TRACE
#define TRACE Timer __timer __attribute__((__cleanup__(timer_print))) = timer_start();
#define SCOPE_TRACE
// function call information
#elif 0
U32 global_depth = 1;
void depth_decrement(int *__unused) {
    (void)__unused;
    global_depth--;
}
#define INIT_TRACE
#define TRACE  printf("%*c%s\n", global_depth++, ' ', __func__); int __unused __attribute__((__cleanup__(depth_decrement))) = 0;
#define SCOPE_TRACE
// no tracing
#else
#define INIT_TRACE
#define TRACE
#define SCOPE_TRACE
#endif

#define CODE_FONT_SIZE FontSize_13
#define CODE_LINE_SPACING 17.f
#define CODE_SCROLL_SPEED_SLOW 0.2f
#define CODE_SCROLL_SPEED_FAST 0.6f
#define ANIM_EXP_FACTOR 1.0f

#define SELECTION_GROUP_BAR_WIDTH 2.f
#define FILETREE_WIDTH 200.f
#define FILETREE_INDENTATION_WIDTH 10.f

#define COLOUR_RED      { 230, 100, 100, 255 }
#define COLOUR_ORANGE   { 160, 100,  20, 255 }
#define COLOUR_GREEN    { 100, 160, 100, 255 }
#define COLOUR_BLUE     { 100, 100, 230, 255 }
#define COLOUR_PURPLE   { 150, 50, 110, 255 }

#define COLOUR_BACKGROUND   { 4, 4, 4, 255 }
#define COLOUR_FOREGROUND   { 170, 170, 170, 255 }
#define COLOUR_SELECT       { 40, 40, 40, 255 }
#define COLOUR_COMMENT      COLOUR_RED
#define COLOUR_STRING       COLOUR_GREEN

#define MAX_GLYPHS 8192
#define MAX_GLYPHS_SIZE (MAX_GLYPHS*sizeof(Glyph))

#define KB (1ull << 10)
#define MB (1ull << 20)
#define GB (1ull << 30)

#define countof(A) (sizeof(A)/sizeof(*A))
#define VK_ASSERT(expr) expect(expr == VK_SUCCESS);

#ifndef FASTMODE
#define arena_reset(A, B) arena_reset_safe(A, B)
#define expect(A) do { if(A) {} else { printf("assertion failed %s:%i\n", __FILE__, __LINE__); exit(1); } } while(0)
#elif
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
    U64 key_special_repeating;
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

typedef struct StaticDataUniform {
    F32 viewport_size[2];
} StaticDataUniform;

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

typedef struct Swapchain {
    VkSwapchainKHR sc;
    VkImage *images;
    VkImageView *image_views;
    VkFramebuffer* framebuffers;
    U32 image_count;
    U32 width;
    U32 height;
} Swapchain;

typedef struct {
    // STATIC DATA -------------------------------

    GLFWwindow *window;
    GLFWmonitor *monitor;
    VkInstance instance;
    VkPhysicalDevice phy_device;
    VkPhysicalDeviceMemoryProperties phy_mem_props;

    VkDevice device;
    VkQueue queue;
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    VkSurfaceKHR surface;

    Arena sc_arena;
    Swapchain *sc;

    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    VkRenderPass pass;
    VkPipelineLayout pl_layout;
    VkPipeline pl;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout_glyphs;

    StaticDataUniform static_data_uniform;
    VkBuffer static_data_uniform_buffer;
    VkDeviceMemory static_data_uniform_buffer_memory;

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
