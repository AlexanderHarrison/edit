#ifndef COMMON_H_
#define COMMON_H_

// add filetype macros in dirent
#define _DEFAULT_SOURCE

#include <math.h>
#include <errno.h>
#include <tools.h>

typedef enum FontSize {
    FontSize_9, FontSize_13, FontSize_21,
    FontSize_Count,
} FontSize;

static const F32 font_size_px[FontSize_Count] = {
    9.f, 13.f, 21.f
};

static const F32 font_height_px[FontSize_Count] = {
    12.f, 17.f, 26.f
};

static inline bool is(U64 held, U64 mask) {
    return (held & mask) == mask;
}

static inline U32 my_strlen(const U8 *str) {
    U32 i = 0;
    while (*str) {
        str++;
        i++;
    }
    return i;
}

U8 *copy_cstr(Arena *arena, const U8 *str);
U8 *copy_str(Arena *arena, const U8 *str, U32 str_len);

// Config -----------------------------

#define CODE_FONT_SIZE FontSize_13
#define CODE_SMALL_FONT_SIZE FontSize_9
#define MODE_FONT_SIZE FontSize_21
#define CODE_SCROLL_SPEED_SLOW 6.f
#define CODE_SCROLL_SPEED_FAST 15.f

// Percentage to travel to target per frame, assuming 60hz.
// Will be translated in w->anim_exp_factor for other refresh rates.
#define ANIM_EXP_FACTOR 0.25f

#define BAR_SIZE 2.f
#define FILETREE_WIDTH 200.f
#define FILETREE_INDENTATION_WIDTH 10.f
#define MODE_INFO_HEIGHT 26.f
#define MODE_INFO_Y_OFFSET 60.f
#define MODE_INFO_PADDING 5.f
#define EDITOR_FOCUSED_PANEL_WEIGHT 1.5f
#define EDITOR_SYNTAX_GROUP_SIZE 2

#define COLOUR_WHITE    { 200, 200, 200, 255 }
#define COLOUR_RED      { 230, 100, 100, 255 }
#define COLOUR_ORANGE   { 160, 100,  20, 255 }
#define COLOUR_GREEN    { 100, 160, 100, 255 }
#define COLOUR_BLUE     { 100, 100, 230, 255 }
#define COLOUR_PURPLE   { 150, 50, 110, 255 }

#define COLOUR_BACKGROUND           { 4, 4, 4, 255 }
#define COLOUR_FOREGROUND           { 175, 170, 165, 255 }
#define COLOUR_MODE_INFO            { 10, 10, 10, 200 }
#define COLOUR_FILE_INFO            { 10, 10, 10, 255 }
#define COLOUR_SELECT               { 30, 30, 30, 255 }
#define COLOUR_SEARCH               { 80, 50, 10, 255 }
#define COLOUR_SEARCH_SHOWN         { 80, 10, 80, 255 }
#define COLOUR_DIRECTORY_OPEN       {100, 100, 255, 255}
#define COLOUR_DIRECTORY_CLOSED     {100, 100, 100, 255}
#define COLOUR_COMMENT              COLOUR_RED
#define COLOUR_STRING               COLOUR_GREEN


// Limits -----------------------------

#define MODE_INPUT_TEXT_MAX 512
#define MODE_TEXT_MAX_LENGTH 8096
#define TEXT_MAX_LENGTH (1ull << 28)
#define SEARCH_MAX_LENGTH (64ul*MB)
#define MAX_LINE_LOOKUP_SIZE (64ul*MB)
#define MAX_SYNTAX_LOOKUP_SIZE (64ul*MB)

#define UNDO_STACK_SIZE (64ul*MB)
#define UNDO_TEXT_SIZE (64ul*MB)
#define UNDO_MAX (UNDO_STACK_SIZE / sizeof(UndoElem))

#define EVENTS_MAX 128

#define FILETREE_MAX_ENTRY_SIZE (64ul*MB)
#define FILETREE_MAX_TEXT_SIZE (64ul*MB)
#define FILETREE_MAX_ROW_SIZE (64ul*MB)
#define FILETREE_MAX_ENTRY_COUNT (FILETREE_MAX_ENTRY_SIZE / sizeof(Entry))
#define FILETREE_MAX_ROW_COUNT (FILETREE_MAX_ROW_SIZE / sizeof(FileTreeRow))
#define FILETREE_MAX_SEARCH_SIZE (4ul*KB)

#define JUMPLIST_MAX_POINT_SIZE (64ul*MB)
#define JUMPLIST_MAX_POINT_COUNT (JUMPLIST_MAX_POINT_SIZE / sizeof(JumpPoint))

#define UI_MAX_PANEL_SIZE (64ul*MB)
#define UI_MAX_PANEL_COUNT (UI_MAX_PANEL_SIZE / sizeof(Panel))
#define UI_MAX_OP_QUEUE_SIZE (64ul*MB)
#define UI_MAX_OP_QUEUE_COUNT (UI_MAX_OP_QUEUE_SIZE / sizeof(UIOp))

#define MAX_GLYPHS 8192
#define MAX_GLYPHS_SIZE (MAX_GLYPHS*sizeof(Glyph))

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// vulkan validation messes with gdb, comment out when debugging
//#define VALIDATION

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
        printf("^^^^^^^^^^ %fms\n", elapsed);
        free(syms);
    }
}

void timer_split(Timer *timer) {
    timer_print(timer);
    *timer = timer_start();
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

typedef struct W {
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
    bool force_update;
    F32 refresh_rate;
    F32 exp_factor;
} W;

extern W *w;

VkResult
gpu_alloc_buffer(
    VkBuffer buffer, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
);

VkResult
gpu_alloc_image(
    VkImage image, 
    VkMemoryPropertyFlags props, 
    VkDeviceMemory *mem
);

void
gpu_free(VkDeviceMemory mem);

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
