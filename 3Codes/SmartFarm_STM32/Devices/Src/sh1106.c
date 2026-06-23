/**
 * @file    SH1106.c
 * @brief   Implementation of SH1106 OLED driver.
 * @note    All I2C functions return ErrorCode_t; pointer parameters are checked.
 */

#include "sh1106.h"
#include "app_types.h"
#include "i2c.h"

/*------------------ External I2C handle ------------------*/
extern I2C_HandleTypeDef hi2c1;
#define SH1106_I2C_HANDLE   &hi2c1

/*------------------ Macros for command/data write (return ErrorCode_t) ---------*/
#define SH1106_WRITECOMMAND(cmd)   sh1106_i2c_write(SH1106_I2C_ADDR, 0x00U, (cmd))
#define SH1106_WRITEDATA(data)     sh1106_i2c_write(SH1106_I2C_ADDR, 0x40U, (data))

/*------------------ Private constants ------------------*/
#define SH1106_NORMALDISPLAY   0xA6U
#define SH1106_INVERTDISPLAY   0xA7U

/*------------------ Frame buffer (static) ------------------*/
static uint8_t sh1106_buffer[SH1106_WIDTH * SH1106_HEIGHT / 8U];

/*------------------ Private context structure ------------------*/
typedef struct {
    uint16_t current_x;
    uint16_t current_y;
    uint8_t  inverted;
    uint8_t  initialized;
} Sh1106Context_t;

static Sh1106Context_t sh1106_ctx;

/*------------------ Helper: absolute value ------------------*/
#define ABS(x)   ((x) > 0 ? (x) : -(x))

/*==============================================================================
 * Public API
 *============================================================================*/

ErrorCode_t sh1106_init(void)
{
    /* Check if LCD is present on I2C bus */
    if (HAL_I2C_IsDeviceReady(SH1106_I2C_HANDLE, SH1106_I2C_ADDR, 1U,
                              SH1106_I2C_TIMEOUT) != HAL_OK)
    {
        return ERR_HW_INIT_FAIL;
    }

    /* Small delay (approx. 2.5ms at 72MHz) */
    uint32_t delay = 2500U;
    while (delay > 0U) { delay--; }

    /* Send initialization command sequence */
    ErrorCode_t err = ERR_NONE;

    err = SH1106_WRITECOMMAND(0xAEU);  /* display off */
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xB0U | 0x00U); /* set page start */
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0x81U);  /* contrast control */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0xFFU);  /* contrast value */
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xA1U);  /* segment re-map */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0xA6U);  /* normal display */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0xA8U);  /* multiplex ratio */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x3FU);  /* 64 lines */
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xADU);  /* pump mode */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x8BU);  /* pump ON */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x30U | 0x02U); /* pump voltage 8.0V */
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xC8U);  /* COM output scan direction */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0xD3U);  /* display offset */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x00U);  /* no offset */
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xD5U);  /* clock divide ratio */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x80U);
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xD9U);  /* pre-charge period */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x1FU);
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xDAU);  /* COM pins hardware config */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x12U);
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xDBU);  /* vcomh */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x40U);
    if (err != ERR_NONE) return err;

    err = SH1106_WRITECOMMAND(0xAFU);  /* display on */
    if (err != ERR_NONE) return err;

    /* Clear screen and update */
    sh1106_fill(SH1106_COLOR_BLACK);
    err = sh1106_update_screen();
    if (err != ERR_NONE) return err;

    sh1106_ctx.current_x = 0U;
    sh1106_ctx.current_y = 0U;
    sh1106_ctx.inverted = 0U;
    sh1106_ctx.initialized = 1U;

    return ERR_NONE;
}

ErrorCode_t sh1106_update_screen(void)
{
    ErrorCode_t err = ERR_NONE;

    /* Column offset: low nibble and high nibble (with 0x10 added) */
    uint8_t col_low  = (SH1106_X_OFFSET & 0x0FU);
    uint8_t col_high = 0x10U | ((SH1106_X_OFFSET >> 4U) & 0x0FU);

    for (uint8_t page = 0U; page < 8U; page++)
    {
        /* Set page address */
        err = SH1106_WRITECOMMAND(0xB0U + page);
        if (err != ERR_NONE) return err;

        /* Set column start with offset */
        err = SH1106_WRITECOMMAND(col_low);
        if (err != ERR_NONE) return err;
        err = SH1106_WRITECOMMAND(col_high);
        if (err != ERR_NONE) return err;

        /* Send one page (SH1106_WIDTH bytes) */
        err = sh1106_i2c_write_multi(SH1106_I2C_ADDR, 0x40U,
                                     &sh1106_buffer[SH1106_WIDTH * page],
                                     SH1106_WIDTH);
        if (err != ERR_NONE) return err;
    }

    return ERR_NONE;
}

void sh1106_toggle_invert(void)
{
    sh1106_ctx.inverted = (uint8_t)(sh1106_ctx.inverted ? 0U : 1U);

    for (uint16_t i = 0U; i < sizeof(sh1106_buffer); i++)
    {
        sh1106_buffer[i] = ~sh1106_buffer[i];
    }
}

void sh1106_fill(Sh1106Color_t colour)
{
    uint8_t val = (colour == SH1106_COLOR_BLACK) ? 0x00U : 0xFFU;
    (void)memset(sh1106_buffer, val, sizeof(sh1106_buffer));
}

void sh1106_draw_pixel(uint16_t x, uint16_t y, Sh1106Color_t colour)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT)
        return;

    if (sh1106_ctx.inverted)
        colour = (Sh1106Color_t)(!colour);

    uint16_t byte_index = x + (y / 8U) * SH1106_WIDTH;
    uint8_t bit_mask = 1U << (y % 8U);

    if (colour == SH1106_COLOR_WHITE)
        sh1106_buffer[byte_index] |= bit_mask;
    else
        sh1106_buffer[byte_index] &= ~bit_mask;
}

void sh1106_goto_xy(uint16_t x, uint16_t y)
{
    sh1106_ctx.current_x = x;
    sh1106_ctx.current_y = y;
}

ErrorCode_t sh1106_put_char(char ch, FontDef_t *font, Sh1106Color_t colour)
{
    if (font == NULL)
        return ERR_INVALID_PARAM;

    if (SH1106_WIDTH <= (sh1106_ctx.current_x + font->width) ||
        SH1106_HEIGHT <= (sh1106_ctx.current_y + font->height))
    {
        return ERR_INVALID_PARAM;
    }

    uint32_t font_height = font->height;
    uint32_t font_width  = font->width;

    for (uint32_t row = 0U; row < font_height; row++)
    {
        uint16_t data = font->data[(ch - 32U) * font_height + row];
        for (uint32_t col = 0U; col < font_width; col++)
        {
            if ((data << col) & 0x8000U)
                sh1106_draw_pixel(sh1106_ctx.current_x + col,
                                  sh1106_ctx.current_y + row,
                                  colour);
            else
                sh1106_draw_pixel(sh1106_ctx.current_x + col,
                                  sh1106_ctx.current_y + row,
                                  (Sh1106Color_t)!colour);
        }
    }

    sh1106_ctx.current_x += font_width;
    return ERR_NONE;
}

ErrorCode_t sh1106_puts(const char *str, FontDef_t *font, Sh1106Color_t colour)
{
    if (str == NULL || font == NULL)
        return ERR_INVALID_PARAM;

    while (*str != '\0')
    {
        ErrorCode_t err = sh1106_put_char(*str, font, colour);
        if (err != ERR_NONE)
            return err;
        str++;
    }
    return ERR_NONE;
}

void sh1106_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                      Sh1106Color_t colour)
{
    /* Clamp coordinates */
    if (x0 >= SH1106_WIDTH) x0 = SH1106_WIDTH - 1U;
    if (x1 >= SH1106_WIDTH) x1 = SH1106_WIDTH - 1U;
    if (y0 >= SH1106_HEIGHT) y0 = SH1106_HEIGHT - 1U;
    if (y1 >= SH1106_HEIGHT) y1 = SH1106_HEIGHT - 1U;

    int16_t dx = (x0 < x1) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t dy = (y0 < y1) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = ((dx > dy) ? dx : -dy) / 2;

    if (dx == 0)
    {
        if (y1 < y0) { int16_t tmp = y1; y1 = y0; y0 = tmp; }
        if (x1 < x0) { int16_t tmp = x1; x1 = x0; x0 = tmp; }
        for (int16_t y = y0; y <= y1; y++)
            sh1106_draw_pixel((uint16_t)x0, (uint16_t)y, colour);
        return;
    }

    if (dy == 0)
    {
        if (y1 < y0) { int16_t tmp = y1; y1 = y0; y0 = tmp; }
        if (x1 < x0) { int16_t tmp = x1; x1 = x0; x0 = tmp; }
        for (int16_t x = x0; x <= x1; x++)
            sh1106_draw_pixel((uint16_t)x, (uint16_t)y0, colour);
        return;
    }

    while (1)
    {
        sh1106_draw_pixel((uint16_t)x0, (uint16_t)y0, colour);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy)  { err += dx; y0 += sy; }
    }
}

void sh1106_draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           Sh1106Color_t colour)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return;
    if (x + w >= SH1106_WIDTH) w = SH1106_WIDTH - x;
    if (y + h >= SH1106_HEIGHT) h = SH1106_HEIGHT - y;

    sh1106_draw_line(x, y, x + w, y, colour);
    sh1106_draw_line(x, y + h, x + w, y + h, colour);
    sh1106_draw_line(x, y, x, y + h, colour);
    sh1106_draw_line(x + w, y, x + w, y + h, colour);
}

void sh1106_draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                  Sh1106Color_t colour)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return;
    if (x + w >= SH1106_WIDTH) w = SH1106_WIDTH - x;
    if (y + h >= SH1106_HEIGHT) h = SH1106_HEIGHT - y;

    for (uint16_t i = 0U; i <= h; i++)
        sh1106_draw_line(x, y + i, x + w, y + i, colour);
}

void sh1106_draw_triangle(uint16_t x1, uint16_t y1,
                          uint16_t x2, uint16_t y2,
                          uint16_t x3, uint16_t y3,
                          Sh1106Color_t colour)
{
    sh1106_draw_line(x1, y1, x2, y2, colour);
    sh1106_draw_line(x2, y2, x3, y3, colour);
    sh1106_draw_line(x3, y3, x1, y1, colour);
}

void sh1106_draw_circle(int16_t x0, int16_t y0, int16_t r, Sh1106Color_t colour)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    sh1106_draw_pixel((uint16_t)x0, (uint16_t)(y0 + r), colour);
    sh1106_draw_pixel((uint16_t)x0, (uint16_t)(y0 - r), colour);
    sh1106_draw_pixel((uint16_t)(x0 + r), (uint16_t)y0, colour);
    sh1106_draw_pixel((uint16_t)(x0 - r), (uint16_t)y0, colour);

    while (x < y)
    {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++;
        ddF_x += 2;
        f += ddF_x;

        sh1106_draw_pixel((uint16_t)(x0 + x), (uint16_t)(y0 + y), colour);
        sh1106_draw_pixel((uint16_t)(x0 - x), (uint16_t)(y0 + y), colour);
        sh1106_draw_pixel((uint16_t)(x0 + x), (uint16_t)(y0 - y), colour);
        sh1106_draw_pixel((uint16_t)(x0 - x), (uint16_t)(y0 - y), colour);
        sh1106_draw_pixel((uint16_t)(x0 + y), (uint16_t)(y0 + x), colour);
        sh1106_draw_pixel((uint16_t)(x0 - y), (uint16_t)(y0 + x), colour);
        sh1106_draw_pixel((uint16_t)(x0 + y), (uint16_t)(y0 - x), colour);
        sh1106_draw_pixel((uint16_t)(x0 - y), (uint16_t)(y0 - x), colour);
    }
}

void sh1106_draw_filled_circle(int16_t x0, int16_t y0, int16_t r,
                               Sh1106Color_t colour)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    sh1106_draw_pixel((uint16_t)x0, (uint16_t)(y0 + r), colour);
    sh1106_draw_pixel((uint16_t)x0, (uint16_t)(y0 - r), colour);
    sh1106_draw_pixel((uint16_t)(x0 + r), (uint16_t)y0, colour);
    sh1106_draw_pixel((uint16_t)(x0 - r), (uint16_t)y0, colour);
    sh1106_draw_line((uint16_t)(x0 - r), (uint16_t)y0,
                     (uint16_t)(x0 + r), (uint16_t)y0, colour);

    while (x < y)
    {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++;
        ddF_x += 2;
        f += ddF_x;

        sh1106_draw_line((uint16_t)(x0 - x), (uint16_t)(y0 + y),
                         (uint16_t)(x0 + x), (uint16_t)(y0 + y), colour);
        sh1106_draw_line((uint16_t)(x0 + x), (uint16_t)(y0 - y),
                         (uint16_t)(x0 - x), (uint16_t)(y0 - y), colour);
        sh1106_draw_line((uint16_t)(x0 + y), (uint16_t)(y0 + x),
                         (uint16_t)(x0 - y), (uint16_t)(y0 + x), colour);
        sh1106_draw_line((uint16_t)(x0 + y), (uint16_t)(y0 - x),
                         (uint16_t)(x0 - y), (uint16_t)(y0 - x), colour);
    }
}

ErrorCode_t sh1106_draw_bitmap(int16_t x, int16_t y,
                               const unsigned char *bitmap,
                               int16_t width, int16_t height,
                               uint16_t colour)
{
    if (bitmap == NULL)
        return ERR_INVALID_PARAM;
    if (x < 0 || y < 0 || (x + width) > SH1106_WIDTH || (y + height) > SH1106_HEIGHT)
        return ERR_INVALID_PARAM;

    int16_t byte_width = (width + 7) / 8;
    for (int16_t row = 0; row < height; row++)
    {
        for (int16_t col = 0; col < width; col++)
        {
            uint8_t byte = bitmap[row * byte_width + col / 8];
            if (col & 7)
                byte <<= 1U;
            if (byte & 0x80U)
                sh1106_draw_pixel((uint16_t)(x + col), (uint16_t)(y + row),
                                  (Sh1106Color_t)colour);
        }
    }
    return ERR_NONE;
}

ErrorCode_t sh1106_clear(void)
{
    sh1106_fill(SH1106_COLOR_BLACK);
    return sh1106_update_screen();
}

ErrorCode_t sh1106_on(void)
{
    ErrorCode_t err = SH1106_WRITECOMMAND(0x8DU);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x14U);
    if (err != ERR_NONE) return err;
    return SH1106_WRITECOMMAND(0xAFU);
}

ErrorCode_t sh1106_off(void)
{
    ErrorCode_t err = SH1106_WRITECOMMAND(0x8DU);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x10U);
    if (err != ERR_NONE) return err;
    return SH1106_WRITECOMMAND(0xAEU);
}

ErrorCode_t sh1106_set_invert(int invert)
{
    uint8_t cmd = invert ? SH1106_INVERTDISPLAY : SH1106_NORMALDISPLAY;
    return SH1106_WRITECOMMAND(cmd);
}

/*------------------ Scrolling functions ------------------*/

static ErrorCode_t sh1106_scroll_setup(uint8_t cmd, uint8_t start_row, uint8_t end_row)
{
    if (start_row > 7U || end_row > 7U || start_row > end_row)
        return ERR_INVALID_PARAM;

    ErrorCode_t err = SH1106_WRITECOMMAND(cmd);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x00U);  /* dummy byte */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(start_row);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(end_row);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x00U);  /* dummy */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0xFFU);  /* dummy */
    if (err != ERR_NONE) return err;
    return SH1106_WRITECOMMAND(0x2FU); /* activate scroll */
}

ErrorCode_t sh1106_scroll_right(uint8_t start_row, uint8_t end_row)
{
    return sh1106_scroll_setup(0x26U, start_row, end_row);
}

ErrorCode_t sh1106_scroll_left(uint8_t start_row, uint8_t end_row)
{
    return sh1106_scroll_setup(0x27U, start_row, end_row);
}

ErrorCode_t sh1106_scroll_diag_right(uint8_t start_row, uint8_t end_row)
{
    return sh1106_scroll_setup(0x29U, start_row, end_row);
}

ErrorCode_t sh1106_scroll_diag_left(uint8_t start_row, uint8_t end_row)
{
    return sh1106_scroll_setup(0x2AU, start_row, end_row);
}

ErrorCode_t sh1106_scroll_down(uint8_t start_row, uint8_t end_row)
{
    if (start_row > 7U || end_row > 7U || start_row > end_row)
        return ERR_INVALID_PARAM;

    ErrorCode_t err = SH1106_WRITECOMMAND(0x29U); /* vertical scroll (same as diag) */
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x00U);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(start_row);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(end_row);
    if (err != ERR_NONE) return err;
    err = SH1106_WRITECOMMAND(0x01U); /* vertical offset */
    if (err != ERR_NONE) return err;
    return SH1106_WRITECOMMAND(0x2FU);
}

ErrorCode_t sh1106_stop_scroll(void)
{
    return SH1106_WRITECOMMAND(0x2EU);
}

/*------------------ Low‑level I2C ------------------*/

ErrorCode_t sh1106_i2c_init(void)
{
    /* Nothing to do; HAL handle is already initialised */
    return ERR_NONE;
}

ErrorCode_t sh1106_i2c_write(uint8_t address, uint8_t reg, uint8_t data)
{
    uint8_t tx_buf[2] = { reg, data };
    if (HAL_I2C_Master_Transmit(SH1106_I2C_HANDLE, address, tx_buf, 2U,
                                SH1106_I2C_TIMEOUT) != HAL_OK)
    {
        return ERR_I2C_TX_FAIL;
    }
    return ERR_NONE;
}

ErrorCode_t sh1106_i2c_write_multi(uint8_t address, uint8_t reg,
                                   const uint8_t *data, uint16_t count)
{
    if (data == NULL)
        return ERR_INVALID_PARAM;

    uint8_t tx_buf[256]; /* sufficient for SH1106_WIDTH (128) */
    tx_buf[0] = reg;
    for (uint16_t i = 0U; i < count; i++)
        tx_buf[i + 1U] = data[i];

    if (HAL_I2C_Master_Transmit(SH1106_I2C_HANDLE, address, tx_buf, count + 1U,
                                SH1106_I2C_TIMEOUT) != HAL_OK)
    {
        return ERR_I2C_TX_FAIL;
    }
    return ERR_NONE;
}
