/**
 * @file    SH1106.h
 * @brief   SH1106 OLED driver (I2C) for STM32 HAL.
 * @note    Adapted from Tilen Majerle and ControllersTech.
 *          Fully compliant with MISRA-C 2025 and project coding style.
 *
 * @details All functions that communicate over I2C return an ErrorCode_t
 *          defined in app_types.h. Pointer parameters are checked for NULL.
 *          The internal frame buffer is updated only when sh1106_update_screen()
 *          is called.
 *
 * @warning The I2C handle (e.g., hi2c1) must be defined in the application
 *          and is referenced externally in the .c file.
 */

#ifndef SH1106_H_
#define SH1106_H_

#ifdef __cplusplus
extern "C" {
#endif

/*------------------ Standard includes ------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*------------------ Project includes ------------------*/
#include "app_types.h"          /* ErrorCode_t, etc. */
#include "fonts.h"              /* FontDef_t */

/*------------------ Configuration macros (user overridable) ------------------*/
#ifndef SH1106_I2C_ADDR
/** I2C slave address of the SH1106 (7‑bit, left‑aligned). */
#define SH1106_I2C_ADDR              (0x3C << 1U)
#endif

/* SH1106 settings */
/*------------------ X‑offset configuration ----------------*/
#ifndef SH1106_X_OFFSET
/** X offset (columns) to center 128‑pixel display on 132‑column RAM.
 *  Typical value: 2 (since (132‑128)/2 = 2). */
#define SH1106_X_OFFSET   2U
#endif

#ifndef SH1106_WIDTH
/** Display width in pixels. */
#define SH1106_WIDTH                 128U
#endif

#ifndef SH1106_HEIGHT
/** Display height in pixels. */
#define SH1106_HEIGHT                64U
#endif

#ifndef SH1106_I2C_TIMEOUT
/** I2C timeout in milliseconds (HAL tick). */
#define SH1106_I2C_TIMEOUT           20000U
#endif

/*------------------ Type definitions ------------------*/

/**
 * @brief   Color selection for drawing operations.
 */
typedef enum {
    SH1106_COLOR_BLACK = 0x00,    /**< Pixel off (background). */
    SH1106_COLOR_WHITE = 0x01     /**< Pixel on (foreground).  */
} Sh1106Color_t;

/*------------------ Public API ------------------*/

/**
 * @brief   Initialise the SH1106 display.
 * @note    Performs a hardware reset sequence and clears the screen.
 * @return  ERR_NONE on success, ERR_HW_INIT_FAIL if device not detected,
 *          or ERR_I2C_TX_FAIL if a command write fails.
 */
ErrorCode_t sh1106_init(void);

/**
 * @brief   Transfer the internal frame buffer to the display.
 * @note    Must be called after any drawing operation to update the screen.
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if I2C transmission fails.
 */
ErrorCode_t sh1106_update_screen(void);

/**
 * @brief   Toggle the invert state of all pixels (RAM only).
 * @note    Call @ref sh1106_update_screen afterwards to see the change.
 */
void sh1106_toggle_invert(void);

/**
 * @brief   Fill the entire frame buffer with a colour.
 * @param   colour  Colour to fill (BLACK or WHITE).
 * @note    Call @ref sh1106_update_screen to apply.
 */
void sh1106_fill(Sh1106Color_t colour);

/**
 * @brief   Draw a single pixel.
 * @param   x       X coordinate (0 .. SH1106_WIDTH-1).
 * @param   y       Y coordinate (0 .. SH1106_HEIGHT-1).
 * @param   colour  Colour to draw.
 * @note    Call @ref sh1106_update_screen to see changes.
 */
void sh1106_draw_pixel(uint16_t x, uint16_t y, Sh1106Color_t colour);

/**
 * @brief   Set the cursor position for subsequent text output.
 * @param   x       X coordinate (0 .. SH1106_WIDTH-1).
 * @param   y       Y coordinate (0 .. SH1106_HEIGHT-1).
 */
void sh1106_goto_xy(uint16_t x, uint16_t y);

/**
 * @brief   Write a character using the selected font.
 * @param   ch      Character to write (ASCII 32..127).
 * @param   font    Pointer to the font definition (must not be NULL).
 * @param   colour  Colour for the character pixels.
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if font is NULL or
 *          the cursor is out of bounds.
 * @note    Call @ref sh1106_update_screen to apply.
 */
ErrorCode_t sh1106_put_char(char ch, FontDef_t *font, Sh1106Color_t colour);

/**
 * @brief   Write a null‑terminated string using the selected font.
 * @param   str     Pointer to the string (must not be NULL).
 * @param   font    Pointer to the font definition (must not be NULL).
 * @param   colour  Colour for the character pixels.
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if str or font is NULL,
 *          or the error code from the failing character.
 * @note    Call @ref sh1106_update_screen to apply.
 */
ErrorCode_t sh1106_puts(const char *str, FontDef_t *font, Sh1106Color_t colour);

/**
 * @brief   Draw a line between two points.
 * @param   x0      Start X coordinate.
 * @param   y0      Start Y coordinate.
 * @param   x1      End X coordinate.
 * @param   y1      End Y coordinate.
 * @param   colour  Colour of the line.
 * @note    Call @ref sh1106_update_screen to see the line.
 */
void sh1106_draw_line(uint16_t x0, uint16_t y0,
                      uint16_t x1, uint16_t y1,
                      Sh1106Color_t colour);

/**
 * @brief   Draw an unfilled rectangle.
 * @param   x       Top‑left X coordinate.
 * @param   y       Top‑left Y coordinate.
 * @param   width   Rectangle width in pixels.
 * @param   height  Rectangle height in pixels.
 * @param   colour  Colour of the outline.
 */
void sh1106_draw_rectangle(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           Sh1106Color_t colour);

/**
 * @brief   Draw a filled rectangle.
 * @param   x       Top‑left X coordinate.
 * @param   y       Top‑left Y coordinate.
 * @param   width   Rectangle width in pixels.
 * @param   height  Rectangle height in pixels.
 * @param   colour  Fill colour.
 */
void sh1106_draw_filled_rectangle(uint16_t x, uint16_t y,
                                  uint16_t width, uint16_t height,
                                  Sh1106Color_t colour);

/**
 * @brief   Draw a triangle outline.
 * @param   x1,y1   First vertex.
 * @param   x2,y2   Second vertex.
 * @param   x3,y3   Third vertex.
 * @param   colour  Outline colour.
 */
void sh1106_draw_triangle(uint16_t x1, uint16_t y1,
                          uint16_t x2, uint16_t y2,
                          uint16_t x3, uint16_t y3,
                          Sh1106Color_t colour);

/**
 * @brief   Draw a circle outline.
 * @param   x0      Center X coordinate.
 * @param   y0      Center Y coordinate.
 * @param   radius  Circle radius in pixels.
 * @param   colour  Outline colour.
 */
void sh1106_draw_circle(int16_t x0, int16_t y0,
                        int16_t radius, Sh1106Color_t colour);

/**
 * @brief   Draw a filled circle.
 * @param   x0      Center X coordinate.
 * @param   y0      Center Y coordinate.
 * @param   radius  Circle radius in pixels.
 * @param   colour  Fill colour.
 */
void sh1106_draw_filled_circle(int16_t x0, int16_t y0,
                               int16_t radius, Sh1106Color_t colour);

/**
 * @brief   Draw a monochrome bitmap at the given position.
 * @param   x       Top‑left X coordinate.
 * @param   y       Top‑left Y coordinate.
 * @param   bitmap  Pointer to the bitmap data (must not be NULL).
 * @param   width   Bitmap width in pixels.
 * @param   height  Bitmap height in pixels.
 * @param   colour  Colour for the set bits (WHITE or BLACK).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if bitmap is NULL or
 *          the drawing area exceeds the screen boundaries.
 * @note    The bitmap is stored as 1‑bit per pixel, row‑major, LSB first.
 */
ErrorCode_t sh1106_draw_bitmap(int16_t x, int16_t y,
                               const unsigned char *bitmap,
                               int16_t width, int16_t height,
                               uint16_t colour);

/**
 * @brief   Clear the display (fill with BLACK) and update the screen.
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if the update fails.
 */
ErrorCode_t sh1106_clear(void);

/**
 * @brief   Turn the display on.
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_on(void);

/**
 * @brief   Turn the display off (power save).
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_off(void);

/**
 * @brief   Invert all pixels on the display (hardware command).
 * @param   invert  Non‑zero to invert, zero for normal.
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_set_invert(int invert);

/*------------------ Scrolling functions ------------------*/

/**
 * @brief   Scroll the display content to the right.
 * @param   start_row   Start page (0..7).
 * @param   end_row     End page (0..7, must be >= start_row).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if rows out of range,
 *          or ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_scroll_right(uint8_t start_row, uint8_t end_row);

/**
 * @brief   Scroll the display content to the left.
 * @param   start_row   Start page (0..7).
 * @param   end_row     End page (0..7, must be >= start_row).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if rows out of range,
 *          or ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_scroll_left(uint8_t start_row, uint8_t end_row);

/**
 * @brief   Scroll the display content diagonally right.
 * @param   start_row   Start page (0..7).
 * @param   end_row     End page (0..7, must be >= start_row).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if rows out of range,
 *          or ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_scroll_diag_right(uint8_t start_row, uint8_t end_row);

/**
 * @brief   Scroll the display content diagonally left.
 * @param   start_row   Start page (0..7).
 * @param   end_row     End page (0..7, must be >= start_row).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if rows out of range,
 *          or ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_scroll_diag_left(uint8_t start_row, uint8_t end_row);

/**
 * @brief   Stop any ongoing scrolling.
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_stop_scroll(void);

/**
 * @brief   Scroll the display content vertically (down).
 * @param   start_row   Start page (0..7).
 * @param   end_row     End page (0..7, must be >= start_row).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if rows out of range,
 *          or ERR_I2C_TX_FAIL if command fails.
 */
ErrorCode_t sh1106_scroll_down(uint8_t start_row, uint8_t end_row);

/*------------------ Low‑level I2C helpers ------------------*/

/**
 * @brief   Initialise the I2C peripheral (called internally).
 * @note    This function is exposed only for advanced use.
 * @return  ERR_NONE (always succeeds on current implementation).
 */
ErrorCode_t sh1106_i2c_init(void);

/**
 * @brief   Write a single byte to a register.
 * @param   address  I2C slave address (7‑bit, left‑aligned).
 * @param   reg      Register address (0x00 for command, 0x40 for data).
 * @param   data     Byte to write.
 * @return  ERR_NONE on success, ERR_I2C_TX_FAIL if HAL fails.
 * @note    Uses the global I2C handle defined in the application.
 */
ErrorCode_t sh1106_i2c_write(uint8_t address, uint8_t reg, uint8_t data);

/**
 * @brief   Write multiple bytes to a register.
 * @param   address  I2C slave address.
 * @param   reg      Register address.
 * @param   data     Pointer to data buffer (must not be NULL).
 * @param   count    Number of bytes to write.
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if data is NULL,
 *          or ERR_I2C_TX_FAIL if HAL fails.
 * @note    Uses the global I2C handle.
 */
ErrorCode_t sh1106_i2c_write_multi(uint8_t address, uint8_t reg,
                                   const uint8_t *data, uint16_t count);

#ifdef __cplusplus
}
#endif

#endif /* SH1106_H_ */
