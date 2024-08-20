/*****************************************************************************
* | File      	:   main.c
* | Function    :   test Demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2024-01-09
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
******************************************************************************/

// -----------------------------------------------------------------------------
// Code Sections (ctrl/cmc + f these titles)
//   - Color Mask Mode Switch Functions 
//   - Pixel Processing Mode Functions
//   - Main Camera Process Function
//   - Color Mask Mode Switch Functions (Button 3)
//   - Camera Fidelity Mode Switch Functions (Button 4)
//   - Button Debounce Function
//   - Entrypoint Function
// =============================================================================

#include <stdio.h>
#include <tusb.h>
#include "pico/stdlib.h"
#include "main.h"

#include "pico/multicore.h"
#include "hardware/vreg.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "cam.h"
#include "ImageData.h"
#include "LCD_1in14_V2.h" 
#include "GUI_Paint.h"

uint8_t image_buf[IMAGE_BUFF_WIDTH * IMAGE_BUFF_WIDTH];
uint8_t displayBuf[DISPLAY_WIDTH * DISPLAY_HEIGHT * 2];
uint8_t header[2] = {0x55,0xAA};

#define FLAG_VALUE 666

// Global variables to store the last debounce time for each button
static uint8_t debounceIncrementorKey3 = 0;
static uint8_t debounceIncrementorKey4 = 0;

uint8_t imageReady = 0;

// Global variable to store the current camera mode
CameraMode currentMode = FULL_RANGE_CAMERA;


// Global variable to store the current color mask
ColorMask currentColorMask = COLOR_MASK_BLACK;


// ------------------------------
// ---- Color Mask Functions ----
// ==============================
// Function to apply color mask to a pixel
uint16_t applyColorMask(uint16_t pixel, ColorMask mask) {
    uint8_t r = (pixel >> 11) & 0x1F;
    uint8_t g = (pixel >> 5) & 0x3F;
    uint8_t b = pixel & 0x1F;

    switch (mask) {
        case COLOR_MASK_BLACK:
            return pixel;
        case COLOR_MASK_RED:
            return (r << 11);
        case COLOR_MASK_PINK:
            return (r << 11) | (b << 0);
        case COLOR_MASK_GREEN:
            return (g << 5);
        case COLOR_MASK_BLUE:
            return (b << 0);
        case COLOR_MASK_YELLOW:
            return (r << 11) | (g << 5);
        default:
            return pixel;
    }
}

// Function to apply color mask to a black and white pixel
uint16_t applyColorMaskToBW(uint8_t bwPixel, ColorMask mask) {
    uint8_t r = 0, g = 0, b = 0;

    switch (mask) {
        case COLOR_MASK_BLACK:
            r = g = b = bwPixel; // Don't scale
            break;
        case COLOR_MASK_RED:
            r = bwPixel >> 3; // Scale to 5 bits
            break;
        case COLOR_MASK_PINK:
            r = bwPixel >> 3; // Scale to 5 bits
            b = bwPixel >> 3; // Scale to 5 bits
            break;
        case COLOR_MASK_GREEN:
            g = bwPixel >> 2; // Scale to 6 bits
            break;
        case COLOR_MASK_BLUE:
            b = bwPixel >> 3; // Scale to 5 bits
            break;
        case COLOR_MASK_YELLOW:
            r = bwPixel >> 3; // Scale to 5 bits
            g = bwPixel >> 2; // Scale to 6 bits
            break;
    }

    return (r << 11) | (g << 5) | b;
}

// -----------------------------------------
// ---- Pixel Processing Mode Functions ----
// =========================================
void process_pixel_bw(int x, int y, int index) {
    // Get current pixel from the image buffer
    uint16_t c = image_buf[(y) * IMAGE_BUFF_WIDTH + (x)];
    
    // Convert the RGB color from the sensor to a black and white pixel
    uint8_t bwPixel = (c > BW_THRESHOLD) ? 0xFF : 0x00;
    uint16_t maskedPixel = applyColorMaskToBW(bwPixel, currentColorMask);
    
    // Set the black and white color in the display buffer
    displayBuf[index++] = (maskedPixel >> 8) & 0xFF;
    displayBuf[index++] = maskedPixel & 0xFF;
}


void process_pixel_grayscale(int x, int y, int index) {
    // Get current pixel from the image buffer
    uint16_t c = image_buf[(y) * IMAGE_BUFF_WIDTH + (x)];
    
    uint8_t grayPixel;
    if (c < GRAY_LEVEL_1) {
        grayPixel = 0x00; // Black
    } else if (c < GRAY_LEVEL_2) {
        grayPixel = 0x55; // Dark gray
    } else if (c < GRAY_LEVEL_3) {
        grayPixel = 0xAA; // Light gray
    } else {
        grayPixel = 0xFF; // White
    }
    
    uint16_t maskedPixel = applyColorMaskToBW(grayPixel, currentColorMask);
    
    // Set the black and white color in the display buffer
    displayBuf[index++] = (maskedPixel >> 8) & 0xFF;
    displayBuf[index++] = maskedPixel & 0xFF;
}

void process_pixel_full_range(int x, int y, int index) {
    // Get current pixel from the image buffer
    uint16_t c = image_buf[(y) * IMAGE_BUFF_WIDTH + (x)];
    
    // RED (Most significant 5 bits.)
    // (c & 0xF8) << 8:
    //      c & 0xF8: Masks the most significant 5 bits of c (the red component in 5-6-5 RGB format).
    //      << 8: Shifts these 5 bits to the left by 8 positions, placing them in the most significant 5 bits of the 16-bit imageRGB.
    // GREEN (Middle 6 bits.)
    // (c & 0xFC) << 3:
    //      c & 0xFC: Masks the most significant 6 bits of c (the green component in 5-6-5 RGB format).
    //      << 3: Shifts these 6 bits to the left by 3 positions, placing them in the middle 6 bits of the 16-bit imageRGB.
    // BLUE (Least significant 5 bits)
    // (c & 0xF8) >> 3:
    //      c & 0xF8: Masks the most significant 5 bits of c (the blue component in 5-6-5 RGB format).
    //      >> 3: Shifts these 5 bits to the right by 3 positions, placing them in the least significant 5 bits of the 16-bit imageRGB.

    // Get the RGB color from the pixel
    uint16_t imageRGB = (((c & 0xF8) << 8) | ((c & 0xFC) << 3) | ((c & 0xF8) >> 3));
    
    imageRGB = applyColorMask(imageRGB, currentColorMask);
    // Set the RGB color in the display buffer
    displayBuf[index++] = (uint16_t)(imageRGB >> 8) & 0xFF;
    displayBuf[index++] = (uint16_t)(imageRGB) & 0xFF;

}

void process_pixel(int x, int y, int index){
    // Call the appropriate camera mode function
    switch (currentMode) {
        case FULL_RANGE_CAMERA:
            process_pixel_full_range(x, y, index);
            break;
        case BW_CAMERA:
            process_pixel_bw(x, y, index);
            break;
        case GRAYSCALE_CAMERA:
            process_pixel_grayscale(x, y, index);
            break;
    }

    // Set the imageReady flag to indicate the image is ready for display
    imageReady = 1;
}


// --------------------------------------
// ---- Main Camera Process Function ----
// ======================================
void run_camera() {

    // Notify core 0 that core 1 is ready
    multicore_fifo_push_blocking(FLAG_VALUE);

    // Wait for acknowledgment from core 0
    uint32_t ack  = multicore_fifo_pop_blocking();
    if (ack != FLAG_VALUE)
    	printf("Error: Core 1 failed to receive acknowledgment from core 0!\n");
	else
		printf("Success: Core 1 Received acknowledgment from core 0!\n");

    // LCD Init
    DEV_Module_Init();
    LCD_1IN14_V2_Init(HORIZONTAL);
    LCD_1IN14_V2_Clear(BLACK);
    UDOUBLE Imagesize = LCD_1IN14_V2_HEIGHT * LCD_1IN14_V2_WIDTH * 2;
    UWORD *BlackImage;
    if ((BlackImage = (UWORD *)malloc(Imagesize)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }
    
    // Create a new image cache and draw on the image
    // Paint_NewImage((UBYTE *)BlackImage, LCD_1IN14_V2.WIDTH, LCD_1IN14_V2.HEIGHT, 0, WHITE);
    // Paint_SetScale(65);
    // Paint_SetRotate(ROTATE_0);
    // Paint_DrawImage(gImage_waveshare, 0, 0, 240, 135);
    // LCD_1IN14_V2_Display(BlackImage);
    // DEV_Delay_ms(500);

    // CAM Init
    struct cam_config config;
    cam_config_struct(&config);
    cam_init(&config);

    // Image Processing
    while (true) {
        cam_capture_frame(&config);

        uint16_t index = 0;
        for (int y = DISPLAY_HEIGHT; y > 0; y--) {
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                process_pixel(x, y, index);
                index += 2;
            }
        }

        // Set the imageReady flag to indicate the image is ready for display
        imageReady = 1;
    }
}

// ------------------------------------------
// ---- Color Mask Mode Switch Functions ----
// ==========================================
// Function to handle KEY3_PIN press
void handleKey3Press() {
    // Rotate through the color masks
    currentColorMask = (currentColorMask + 1) % 6;
    switch (currentColorMask) {
        case COLOR_MASK_BLACK:
            printf("Color mask set to Black\n");
            break;
        case COLOR_MASK_RED:
            printf("Color mask set to Red\n");
            break;
        case COLOR_MASK_PINK:
            printf("Color mask set to Pink\n");
            break;
        case COLOR_MASK_GREEN:
            printf("Color mask set to Green\n");
            break;
        case COLOR_MASK_BLUE:
            printf("Color mask set to Blue\n");
            break;
        case COLOR_MASK_YELLOW:
            printf("Color mask set to Yellow\n");
            break;
    }
}

// Function to check if KEY3_PIN is pressed
bool isKey3Pressed() {
    return gpio_get(KEY3_PIN) == 0; // Active low
}
// Function to check if the button press is debounced for KEY3_PIN
bool isDebouncedKey3Pressed() {
    if (isKey3Pressed()) {
        if (debounceIncrementorKey3 == 0) {
            debounceIncrementorKey3 = 1;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------
// ---- Camera Fidelity Mode Switch Functions ----
// ===============================================
// Function to handle KEY4_PIN press
void handleKey4Press() {
    // Cycle through the camera modes
    if (currentMode == FULL_RANGE_CAMERA) {
        currentMode = BW_CAMERA;
        printf("Switched to BW Camera mode\n");
    } else if (currentMode == BW_CAMERA) {
        currentMode = GRAYSCALE_CAMERA;
        printf("Switched to Grayscale Camera mode\n");
    } else {
        currentMode = FULL_RANGE_CAMERA;
        printf("Switched to Full Range Camera mode\n");
    }
}

bool isKey4Pressed() {
    return gpio_get(KEY4_PIN) == 0; // Active low
}
bool isDebouncedKey4Pressed() {
    if (isKey4Pressed()) {
        if (debounceIncrementorKey4 == 0) {
            debounceIncrementorKey4 = 1;
            return true;
        }
    }
    return false;
}

// ----------------------------------
// ---- Button Debounce Function ----
// ==================================
void incrementDebounceCounters() {
    if (debounceIncrementorKey3 > 0) {
        debounceIncrementorKey3++;
        if (debounceIncrementorKey3 > DEBOUNCE_DELAY) {
            debounceIncrementorKey3 = 0;
        }
    }
    if (debounceIncrementorKey4 > 0) {
        debounceIncrementorKey4++;
        if (debounceIncrementorKey4 > DEBOUNCE_DELAY) {
            debounceIncrementorKey4 = 0;
        }
    }
}

// -----------------------------
// ---- Entrypoint Function ----
// =============================
int main() {

    int loops = 20;
    stdio_init_all();
    while (!tud_cdc_connected()) {
        DEV_Delay_ms(100);
        if (--loops == 0)
            break;
    }

    printf("tud_cdc_connected(%d)\n", tud_cdc_connected() ? 1 : 0);
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    set_sys_clock_khz(250000, true);

    // Run the camera on core 1 and display the image on core 0
    // Loop and wait for the next image (below)
    multicore_launch_core1(run_camera);

    // Wait for acknowledgment from core 1
    uint32_t ack = multicore_fifo_pop_blocking();
    if (ack != FLAG_VALUE)
        printf("Error: Core 0 failed to receive acknowledgment from core 1!\n");
    else {
        multicore_fifo_push_blocking(FLAG_VALUE);
        printf("Success: Core 0 Received acknowledgment from core 1!\n");
    }

    // Show Image
    while (1) {
        if (imageReady == 1) {
            LCD_1IN14_V2_Display((uint16_t*)displayBuf);
            // Reset the imageReady flag after displaying the image
            imageReady = 0;
        }
		DEV_Delay_ms(1);

        // Button handling
        // Could do interrupts but this is simple
        if(isDebouncedKey3Pressed()) {
            handleKey3Press();
        }
        if(isDebouncedKey4Pressed()) {
            handleKey4Press();
        }
        incrementDebounceCounters();
    }
    tight_loop_contents();
}
