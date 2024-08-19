#include "stdint.h"

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 135
#define IMAGE_BUFF_WIDTH 324

// Define debounce delay iterations
#define DEBOUNCE_DELAY 200

// Define thresholds for 2 levels of grayscale for BW images
#define BW_THRESHOLD 128

// Define thresholds for 4 levels of grayscale
#define GRAY_LEVEL_1 64
#define GRAY_LEVEL_2 128
#define GRAY_LEVEL_3 192

typedef enum {
    FULL_RANGE_CAMERA,
    BW_CAMERA,
    GRAYSCALE_CAMERA
} CameraMode;

// Enum for color mask states
typedef enum{
    COLOR_MASK_BLACK,
    COLOR_MASK_RED,
    COLOR_MASK_PINK,
    COLOR_MASK_GREEN,
    COLOR_MASK_BLUE,
    COLOR_MASK_YELLOW,
} ColorMask;


uint16_t applyColorMask(uint16_t pixel, ColorMask mask);