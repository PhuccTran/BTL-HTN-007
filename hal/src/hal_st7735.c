#include "hal_st7735.h"
#include "hal_spi.h"
#include "hal_gpio.h"
#include "bsp_display.h"
#include "semphr.h"
#include "task.h"
#include <stddef.h>

enum { SWRESET=0x01, SLPOUT=0x11, NORON=0x13, INVOFF=0x20, INVON=0x21,
       DISPON=0x29, CASET=0x2A, RASET=0x2B, RAMWR=0x2C, MADCTL=0x36, COLMOD=0x3A };
static SemaphoreHandle_t s_mutex;
static bool s_ready;
static uint8_t s_pixels[256];

/* Mỗi byte font là một hàng, bit 4 ở bên trái. */
static const uint8_t s_digits[10][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
static const uint8_t s_letters[26][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

static TickType_t prvTicks(uint32_t ms)
{
    TickType_t t = pdMS_TO_TICKS(ms);
    return t == 0U ? 1U : t;
}

static TickType_t prvLeft(TickType_t start, TickType_t budget)
{
    TickType_t elapsed = xTaskGetTickCount() - start;
    return elapsed >= budget ? 0U : budget - elapsed;
}

static hal_st7735_status_t prvBegin(bool require_ready)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || s_mutex == NULL)
    { return HAL_ST7735_NOT_READY; }
    if (xSemaphoreTake(s_mutex, prvTicks(200U)) != pdPASS) { return HAL_ST7735_TIMEOUT; }
    if (require_ready && !s_ready)
    {
        (void)xSemaphoreGive(s_mutex);
        return HAL_ST7735_NOT_READY;
    }
    hal_gpio_display_cs(true);
    return HAL_ST7735_OK;
}

static hal_st7735_status_t prvEnd(hal_st7735_status_t status)
{
    hal_gpio_display_cs(false);
    (void)xSemaphoreGive(s_mutex);
    return status;
}

static hal_st7735_status_t prvSend(bool data, const uint8_t *bytes, size_t n,
                                  TickType_t start, TickType_t budget)
{
    TickType_t left = prvLeft(start, budget);
    if (left == 0U) { return HAL_ST7735_TIMEOUT; }
    hal_gpio_display_dc(data);
    hal_spi_status_t result = hal_spi_display_write(bytes, n, left);
    if (result == HAL_SPI_OK) { return HAL_ST7735_OK; }
    return result == HAL_SPI_TIMEOUT ? HAL_ST7735_TIMEOUT : HAL_ST7735_ERROR;
}

static hal_st7735_status_t prvCommand(uint8_t command, const uint8_t *args, size_t n,
                                     TickType_t start, TickType_t budget)
{
    hal_st7735_status_t s = prvSend(false, &command, 1U, start, budget);
    if (s == HAL_ST7735_OK && n != 0U) { s = prvSend(true, args, n, start, budget); }
    return s;
}

static hal_st7735_status_t prvPanel(TickType_t start,TickType_t budget)
{
#if BSP_DISPLAY_PANEL_PROFILE
    /* Profile frame/power/gamma: STMicroelectronics/stm32-st7735, ST7735_Init. */
    static const struct { uint8_t command,n,args[16]; } profile[]={
        {0xB1,3,{0x01,0x2C,0x2D}}, {0xB2,3,{0x01,0x2C,0x2D}},
        {0xB3,6,{0x01,0x2C,0x2D,0x01,0x2C,0x2D}}, {0xB4,1,{0x07}},
        {0xC0,3,{0xA2,0x02,0x84}}, {0xC1,1,{0xC5}}, {0xC2,2,{0x0A,0x00}},
        {0xC3,2,{0x8A,0x2A}}, {0xC4,2,{0x8A,0xEE}}, {0xC5,1,{0x0E}},
        {0xE0,16,{0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10}},
        {0xE1,16,{0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10}}
    };
    for (size_t i=0U;i<sizeof(profile)/sizeof(profile[0]);++i)
    {
        hal_st7735_status_t s=prvCommand(profile[i].command,profile[i].args,profile[i].n,start,budget);
        if (s!=HAL_ST7735_OK) { return s; }
    }
#else
    (void)start; (void)budget;
#endif
    return HAL_ST7735_OK;
}

static hal_st7735_status_t prvWindow(int32_t x, int32_t y, int32_t w, int32_t h,
                                    TickType_t start, TickType_t budget)
{
    uint16_t x0 = (uint16_t)(x + BSP_DISPLAY_X_OFFSET);
    uint16_t y0 = (uint16_t)(y + BSP_DISPLAY_Y_OFFSET);
    uint16_t x1 = (uint16_t)(x0 + w - 1);
    uint16_t y1 = (uint16_t)(y0 + h - 1);
    uint8_t a[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    hal_st7735_status_t s = prvCommand(CASET, a, 4U, start, budget);
    a[0]=(uint8_t)(y0>>8); a[1]=(uint8_t)y0; a[2]=(uint8_t)(y1>>8); a[3]=(uint8_t)y1;
    if (s == HAL_ST7735_OK) { s = prvCommand(RASET, a, 4U, start, budget); }
    if (s == HAL_ST7735_OK) { s = prvCommand(RAMWR, NULL, 0U, start, budget); }
    return s;
}

static bool prvClip(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > HAL_ST7735_WIDTH) { *w = HAL_ST7735_WIDTH - *x; }
    if (*y + *h > HAL_ST7735_HEIGHT) { *h = HAL_ST7735_HEIGHT - *y; }
    return *w > 0 && *h > 0;
}

static hal_st7735_status_t prvRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color,
                                  TickType_t start, TickType_t budget)
{
    if (!prvClip(&x, &y, &w, &h)) { return HAL_ST7735_OK; }
    hal_st7735_status_t s = prvWindow(x,y,w,h,start,budget);
    for (size_t i=0; i<sizeof(s_pixels); i+=2U)
    { s_pixels[i]=(uint8_t)(color>>8); s_pixels[i+1U]=(uint8_t)color; }
    uint32_t remaining = (uint32_t)(w*h)*2U;
    while (s == HAL_ST7735_OK && remaining != 0U)
    {
        size_t n = remaining > sizeof(s_pixels) ? sizeof(s_pixels) : remaining;
        s = prvSend(true,s_pixels,n,start,budget);
        remaining -= (uint32_t)n;
    }
    return s;
}

static uint8_t prvGlyph(char c, uint32_t row)
{
    static const uint8_t question[7]={14,17,1,2,4,0,4};
    if (row >= 7U || c == ' ') { return 0U; }
    if (c >= 'a' && c <= 'z') { c = (char)(c-'a'+'A'); }
    if (c >= 'A' && c <= 'Z') { return s_letters[(uint8_t)(c-'A')][row]; }
    if (c >= '0' && c <= '9') { return s_digits[(uint8_t)(c-'0')][row]; }
    if (c == '-') { return row == 3U ? 14U : 0U; }
    if (c == ':') { return row == 2U || row == 5U ? 4U : 0U; }
    if (c == '.') { return row == 6U ? 4U : 0U; }
    if (c == '/') { return (uint8_t)(1U << (row*4U/6U)); }
    return question[row];
}

static hal_st7735_status_t prvChar(int32_t x, int32_t y, char c, uint16_t fg, uint16_t bg,
                                  TickType_t start, TickType_t budget)
{
    int32_t cx=x, cy=y, w=HAL_ST7735_FONT_W, h=HAL_ST7735_FONT_H;
    if (!prvClip(&cx,&cy,&w,&h)) { return HAL_ST7735_OK; }
    size_t n=0U;
    for (int32_t row=cy-y; row<cy-y+h; ++row)
    {
        uint8_t bits = prvGlyph(c,(uint32_t)row);
        for (int32_t col=cx-x; col<cx-x+w; ++col)
        {
            uint16_t color = col<5 && (bits & (1U << (4-col))) != 0U ? fg : bg;
            s_pixels[n++]=(uint8_t)(color>>8); s_pixels[n++]=(uint8_t)color;
        }
    }
    hal_st7735_status_t s=prvWindow(cx,cy,w,h,start,budget);
    return s == HAL_ST7735_OK ? prvSend(true,s_pixels,n,start,budget) : s;
}

BaseType_t hal_st7735_prepare(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { return pdFAIL; }
    if (s_mutex != NULL) { return pdPASS; }
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) { return pdFAIL; }
    hal_gpio_display_init();
    if (hal_spi_display_prepare() != pdPASS)
    {
        vSemaphoreDelete(s_mutex); s_mutex=NULL;
        return pdFAIL;
    }
    return pdPASS;
}

hal_st7735_status_t hal_st7735_init(void)
{
    hal_st7735_status_t s=prvBegin(false);
    if (s != HAL_ST7735_OK) { return s; }
    s_ready=false;
    TickType_t start=xTaskGetTickCount(), budget=prvTicks(1800U);
    (void)hal_gpio_display_backlight(false);
    hal_gpio_display_reset(true); vTaskDelay(prvTicks(20U));
    hal_gpio_display_reset(false); vTaskDelay(prvTicks(150U));
    s=prvCommand(SWRESET,NULL,0U,start,budget);
    if (s == HAL_ST7735_OK) { vTaskDelay(prvTicks(150U)); s=prvCommand(SLPOUT,NULL,0U,start,budget); }
    if (s == HAL_ST7735_OK) { vTaskDelay(prvTicks(150U)); }
    if (s == HAL_ST7735_OK) { s=prvPanel(start,budget); }
    uint8_t arg=0x05U;
    if (s == HAL_ST7735_OK) { s=prvCommand(COLMOD,&arg,1U,start,budget); }
    arg=BSP_DISPLAY_MADCTL;
    if (s == HAL_ST7735_OK) { s=prvCommand(MADCTL,&arg,1U,start,budget); }
    if (s == HAL_ST7735_OK) { s=prvCommand(BSP_DISPLAY_INVERT ? INVON : INVOFF,NULL,0U,start,budget); }
    if (s == HAL_ST7735_OK) { s=prvCommand(NORON,NULL,0U,start,budget); }

    if (s == HAL_ST7735_OK) { s=prvRect(0,0,128,160,HAL_ST7735_WHITE,start,budget); }
    if (s == HAL_ST7735_OK) { s=prvCommand(DISPON,NULL,0U,start,budget); }
    if (s == HAL_ST7735_OK)
    { vTaskDelay(prvTicks(100U)); s_ready=true; (void)hal_gpio_display_backlight(true); }
    return prvEnd(s);
}

hal_st7735_status_t hal_st7735_fill_rect(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color)
{
    hal_st7735_status_t s=prvBegin(true);
    if (s != HAL_ST7735_OK) { return s; }
    return prvEnd(prvRect(x,y,w,h,color,xTaskGetTickCount(),prvTicks(500U)));
}
hal_st7735_status_t hal_st7735_draw_pixel(int16_t x,int16_t y,uint16_t c)
{ return hal_st7735_fill_rect(x,y,1U,1U,c); }
hal_st7735_status_t hal_st7735_draw_hline(int16_t x,int16_t y,uint16_t n,uint16_t c)
{ return hal_st7735_fill_rect(x,y,n,1U,c); }
hal_st7735_status_t hal_st7735_draw_vline(int16_t x,int16_t y,uint16_t n,uint16_t c)
{ return hal_st7735_fill_rect(x,y,1U,n,c); }

hal_st7735_status_t hal_st7735_draw_circle_filled(int16_t x,int16_t y,uint16_t radius,uint16_t c)
{
    if (radius > 160U) { return HAL_ST7735_INVALID; }
    hal_st7735_status_t s=prvBegin(true);
    if (s != HAL_ST7735_OK) { return s; }
    TickType_t start=xTaskGetTickCount(), budget=prvTicks(500U);
    int32_t r=radius;
    for (int32_t dy=-r; dy<=r && s==HAL_ST7735_OK; ++dy)
    {
        int32_t dx=r;
        while (dx*dx+dy*dy>r*r) { --dx; }
        s=prvRect((int32_t)x-dx,(int32_t)y+dy,2*dx+1,1,c,start,budget);
    }
    return prvEnd(s);
}

hal_st7735_status_t hal_st7735_draw_text(int16_t x,int16_t y,const char *text,uint16_t fg,uint16_t bg)
{
    if (text == NULL) { return HAL_ST7735_INVALID; }
    size_t n=0U;
    while (n<65U && text[n]!='\0') { ++n; }
    if (n>64U) { return HAL_ST7735_INVALID; }
    hal_st7735_status_t s=prvBegin(true);
    if (s != HAL_ST7735_OK) { return s; }
    TickType_t start=xTaskGetTickCount(), budget=prvTicks(500U);
    for (size_t i=0U; i<n && s==HAL_ST7735_OK; ++i)
    { s=prvChar((int32_t)x+(int32_t)i*6,y,text[i],fg,bg,start,budget); }
    return prvEnd(s);
}
hal_st7735_status_t hal_st7735_draw_char(int16_t x,int16_t y,char c,uint16_t fg,uint16_t bg)
{
    char text[2]={c,'\0'};
    return hal_st7735_draw_text(x,y,text,fg,bg);
}
hal_st7735_status_t hal_st7735_set_backlight(bool on)
{
    hal_st7735_status_t s=prvBegin(true);
    if (s != HAL_ST7735_OK) { return s; }
    return prvEnd(hal_gpio_display_backlight(on) ? HAL_ST7735_OK : HAL_ST7735_UNSUPPORTED);
}
