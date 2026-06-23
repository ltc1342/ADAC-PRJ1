/**
 * @file    fonts.h
 * @brief   Font definitions for monochrome OLED/LCD displays.
 * @author  Adapted from Tilen Majerle & Alexander Lutsai
 * @date    2026-07-01
 *
 * @details Provides font structures for 7x10, 11x18 and 16x26 pixel fonts.
 *          All functions return an ErrorCode_t (defined in app_types.h) and
 *          perform NULL pointer checking.
 *
 * @note    This header does not depend on any specific STM32 HAL version.
 *          It uses only standard C types (stdint, stdbool, size_t).
 */

#ifndef FONTS_H
#define FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*------------------ Standard includes ------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*------------------ Project includes ------------------*/
#include "app_types.h"          /* ErrorCode_t */

/*==============================================================================
 *  Types
 *============================================================================*/

/**
 * @brief   Font definition structure.
 * @note    The `data` array contains the pixel map for each character.
 *          Each character's bitmap is stored column‑major, LSB first.
 */
typedef struct {
    uint8_t width;          /**< Font width in pixels. */
    uint8_t height;         /**< Font height in pixels. */
    const uint16_t *data;   /**< Pointer to the font bitmap array. */
} FontDef_t;

/**
 * @brief   Size structure returned by @ref fonts_get_string_size.
 */
typedef struct {
    uint16_t length;        /**< String width in pixels. */
    uint16_t height;        /**< String height in pixels (same as font height). */
} FontSize_t;

/*==============================================================================
 *  External font variables (defined in fonts.c)
 *============================================================================*/

/** @brief  7×10 pixel font (ASCII 32..127). */
extern FontDef_t font_7x10;

/** @brief  11×18 pixel font (ASCII 32..127). */
extern FontDef_t font_11x18;

/** @brief  16×26 pixel font (ASCII 32..127). */
extern FontDef_t font_16x26;

/*==============================================================================
 *  Public functions
 *============================================================================*/

/**
 * @brief   Calculate the width and height of a string when rendered with a given font.
 * @param   str         Pointer to the null‑terminated string (must not be NULL).
 * @param   size_out    Pointer to a @ref FontSize_t structure to receive the result.
 * @param   font        Pointer to the @ref FontDef_t font to use (must not be NULL).
 * @return  ERR_NONE on success, ERR_INVALID_PARAM if any pointer is NULL.
 *
 * @note    The `height` field is always set to the font's height.
 *          The `length` field is the sum of the widths of all printable characters.
 *          Non‑printable characters (outside 32..127) are skipped with a warning.
 */
ErrorCode_t fonts_get_string_size(const char *str, FontSize_t *size_out,
                                  const FontDef_t *font);

#ifdef __cplusplus
}
#endif

#endif /* FONTS_H */
