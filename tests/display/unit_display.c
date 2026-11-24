

#include "hal_st7735.h"
#include "hal_spi.h"
#include "hal_gpio.h"
#include "display_render.h"
#include "task_display.h"
#include "star_maze_levels.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

volatile uint32_t test_failure, test_checks;
uint32_t test_ipsr;
uint16_t test_frame[160][128];
uint16_t test_snapshots[8][160][128];
static uint16_t s_saved[160][128];
static uint32_t s_pixels_written, s_calls;
static bool s_selected,s_data,s_force_busy,s_fail_next;
static int s_scheduler;
static TickType_t s_tick;
static unsigned s_mutex_token;
static bool s_mutex_held;
static uint8_t s_command, s_commands[64];
static unsigned s_command_count;
static uint16_t s_x0,s_x1,s_y0,s_y1,s_x,s_y;
static size_t s_heap=8192U;
static bool s_lazy_heap;
static uint8_t s_queue[APP_DISPLAY_QUEUE_LENGTH][sizeof(DisplayMsg_t)];
static unsigned s_head,s_tail,s_count;
static uint32_t s_task_stack,s_task_priority;

__attribute__((noinline,noreturn)) void test_done(void)
{ for (;;) { __asm volatile("nop"); } }
#define CHECK(expr) do { ++test_checks; if (!(expr)) { test_failure=__LINE__; test_done(); } } while(0)

BaseType_t xTaskGetSchedulerState(void) { return s_scheduler; }
TickType_t xTaskGetTickCount(void) { return s_tick; }
void vTaskDelay(TickType_t t) { CHECK(t!=0U && t!=portMAX_DELAY); s_tick+=t; }
size_t xPortGetFreeHeapSize(void) { return s_heap; }
void vPortGetHeapStats(HeapStats_t *out)
{ out->xAvailableHeapSpaceInBytes=s_lazy_heap?0U:s_heap; out->xNumberOfSuccessfulAllocations=s_lazy_heap?0U:1U; }
void *pvPortMalloc(size_t n) { CHECK(n==1U); s_lazy_heap=false; s_heap=8192U; return &s_heap; }
void vPortFree(void *p) { CHECK(p==&s_heap); }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t t) { (void)t; return 200U; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *h)
{ (void)fn; (void)name; (void)arg; s_task_stack=stack; s_task_priority=priority; *h=(void *)1; return pdPASS; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &s_mutex_token; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t m,TickType_t t)
{ CHECK(m==&s_mutex_token && t!=portMAX_DELAY); if (s_force_busy||s_mutex_held) { s_tick+=t; return pdFAIL; } s_mutex_held=true; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t m) { CHECK(m==&s_mutex_token && s_mutex_held); s_mutex_held=false; return pdPASS; }
void vSemaphoreDelete(SemaphoreHandle_t m) { (void)m; }
QueueHandle_t xQueueCreate(UBaseType_t n,UBaseType_t item)
{ CHECK(n==APP_DISPLAY_QUEUE_LENGTH && item==sizeof(DisplayMsg_t)); return s_queue; }
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t timeout)
{
    CHECK(q==s_queue && timeout<=APP_DISPLAY_POST_MAX_MS);
    if (s_count==APP_DISPLAY_QUEUE_LENGTH) { return pdFAIL; }
    memcpy(s_queue[s_tail],item,sizeof(DisplayMsg_t)); s_tail=(s_tail+1U)%APP_DISPLAY_QUEUE_LENGTH; ++s_count; return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t timeout)
{ CHECK(q==s_queue && timeout!=portMAX_DELAY); if(s_count==0U) { return pdFAIL; }
  memcpy(item,s_queue[s_head],sizeof(DisplayMsg_t)); s_head=(s_head+1U)%APP_DISPLAY_QUEUE_LENGTH; --s_count; return pdPASS; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
void hal_gpio_display_init(void) { }
void hal_gpio_display_cs(bool selected) { s_selected=selected; }
void hal_gpio_display_dc(bool data) { s_data=data; }
void hal_gpio_display_reset(bool asserted) { (void)asserted; }
bool hal_gpio_display_backlight(bool on) { (void)on; return false; }
BaseType_t hal_spi_display_prepare(void) { return pdPASS; }

hal_spi_status_t hal_spi_display_write(const uint8_t *bytes,size_t n,TickType_t timeout)
{
    CHECK(s_selected && s_mutex_held && n!=0U && timeout!=0U && timeout!=portMAX_DELAY);
    ++s_calls; ++s_tick;
    if(s_fail_next) { s_fail_next=false; return HAL_SPI_TIMEOUT; }
    if (!s_data)
    {
        CHECK(n==1U); s_command=bytes[0];
        if(s_command_count<sizeof(s_commands)) { s_commands[s_command_count++]=s_command; }
        if(s_command==0x2C) { s_x=s_x0; s_y=s_y0; }
    }
    else if(s_command==0x2A || s_command==0x2B)
    {
        CHECK(n==4U);
        uint16_t a=(uint16_t)((bytes[0]<<8)|bytes[1]),b=(uint16_t)((bytes[2]<<8)|bytes[3]);
        CHECK(a<=b);
        if(s_command==0x2A) { CHECK(b<128U); s_x0=a; s_x1=b; }
        else { CHECK(b<160U); s_y0=a; s_y1=b; }
    }
    else if(s_command==0x2C)
    {
        CHECK(n%2U==0U);
        for(size_t i=0;i<n;i+=2U)
        {
            CHECK(s_x<128U && s_y<160U);
            test_frame[s_y][s_x]=(uint16_t)((bytes[i]<<8)|bytes[i+1U]); ++s_pixels_written;
            if(++s_x>s_x1) { s_x=s_x0; if(++s_y>s_y1) { s_y=s_y0; } }
        }
    }
    return HAL_SPI_OK;
}

static DisplayMsg_t prvScene(void)
{
    DisplayMsg_t m={.type=DISPLAY_MSG_SCENE,.scene_id=1};
    m.data.scene.level=1; m.data.scene.total_stars=2;
    m.data.scene.hud=(DisplayUpdate_t){.ball_x=15,.ball_y=15,.lives=3};
    for(unsigned y=0;y<12;++y) { for(unsigned x=0;x<12;++x)
      { m.data.scene.tiles[y][x]=(x==0||x==11||y==0||y==11)?DISPLAY_TILE_WALL:DISPLAY_TILE_FLOOR; } }
    m.data.scene.tiles[1][1]=DISPLAY_TILE_START;
    m.data.scene.tiles[3][3]=DISPLAY_TILE_STAR;
    m.data.scene.tiles[4][6]=DISPLAY_TILE_STAR;
    m.data.scene.tiles[5][5]=DISPLAY_TILE_WALL;
    m.data.scene.tiles[6][5]=DISPLAY_TILE_TRAP;
    m.data.scene.tiles[10][10]=DISPLAY_TILE_GATE;
    return m;
}

void test_entry(void)
{
    CHECK(hal_st7735_init()==HAL_ST7735_NOT_READY);
    CHECK(hal_st7735_prepare()==pdPASS);
    CHECK(hal_st7735_prepare()==pdPASS);
    s_scheduler=taskSCHEDULER_RUNNING;
    CHECK(hal_st7735_init()==HAL_ST7735_OK);
    CHECK(!s_selected && !s_mutex_held);
    CHECK(s_commands[0]==0x01 && s_commands[1]==0x11 && s_commands[2]==0xB1 &&
          s_commands[14]==0x3A && s_commands[15]==0x36 && s_commands[16]==0x20 && s_commands[17]==0x13);
    CHECK(s_pixels_written==128U*160U);
    CHECK(test_frame[159][127]==HAL_ST7735_WHITE);
    CHECK(hal_st7735_draw_pixel(127,159,0x1234U)==HAL_ST7735_OK);
    CHECK(test_frame[159][127]==0x1234U);
    uint32_t before=s_pixels_written;
    CHECK(hal_st7735_fill_rect(-3,-2,5,4,HAL_ST7735_BLACK)==HAL_ST7735_OK);
    CHECK(s_pixels_written-before==4U && test_frame[1][1]==HAL_ST7735_BLACK);
    before=s_calls;
    CHECK(hal_st7735_fill_rect(200,200,2,2,0)==HAL_ST7735_OK && s_calls==before);
    CHECK(hal_st7735_fill_rect(0,0,0,10,0)==HAL_ST7735_OK && s_calls==before);
    CHECK(hal_st7735_draw_circle_filled(0,0,2,0)==HAL_ST7735_OK);
    CHECK(hal_st7735_draw_circle_filled(0,0,161,0)==HAL_ST7735_INVALID);
    CHECK(hal_st7735_draw_text(-2,-2,"A0?",0,65535)==HAL_ST7735_OK);
    CHECK(hal_st7735_draw_text(0,0,NULL,0,0)==HAL_ST7735_INVALID);
    CHECK(hal_st7735_draw_text(0,0,"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLM",0,0)==HAL_ST7735_INVALID);
    s_force_busy=true; CHECK(hal_st7735_draw_pixel(0,0,0)==HAL_ST7735_TIMEOUT); s_force_busy=false;
    CHECK(!s_mutex_held && !s_selected);
    s_fail_next=true; CHECK(hal_st7735_fill_rect(0,0,5,5,0)==HAL_ST7735_TIMEOUT);
    CHECK(!s_mutex_held && !s_selected);
    CHECK(hal_st7735_fill_rect(0,0,128,160,65535)==HAL_ST7735_OK);
    s_tick=UINT32_MAX-100U; CHECK(hal_st7735_init()==HAL_ST7735_OK);
    test_ipsr=1; CHECK(hal_st7735_draw_pixel(0,0,0)==HAL_ST7735_NOT_READY); test_ipsr=0;
    CHECK(hal_st7735_set_backlight(false)==HAL_ST7735_UNSUPPORTED);

    DisplayRender_t r; task_display_render_reset(&r);
    DisplayMsg_t m=prvScene();
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    memcpy(test_snapshots[0],test_frame,sizeof(test_frame));
    memcpy(s_saved,test_frame,sizeof(test_frame));

    CHECK(test_frame[25][4]==65535U && test_frame[65][25]==0U);
    CHECK(test_frame[129][108]==65535U);
    m.type=DISPLAY_MSG_UPDATE;
    m.data.update=(DisplayUpdate_t){.ball_x=35,.ball_y=35,.lives=3};
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    m.data.update.ball_x=65;
    before=s_pixels_written;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    CHECK(s_pixels_written-before<150U);

    for(unsigned y=32;y<=38;++y) { for(unsigned x=32;x<=38;++x)
      { CHECK(test_frame[y+25][x+4]==s_saved[y+25][x+4]); } }
    for(unsigned y=0;y<160;++y) { for(unsigned x=0;x<128;++x)
      { if(y<25U||y>=145U||x<4U||x>=124U) { CHECK(test_frame[y][x]==s_saved[y][x]); } } }
    before=s_calls; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && s_calls==before);

    m.data.update.gate_open=1;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_INVALID && s_calls==before);
    m.data.update.gate_open=0;
    m.scene_id=999; m.data.update.ball_x=15;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && r.scene.hud.ball_x==65);
    m.scene_id=1;

    m.data.update.ball_x=35; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    m.data.update.collected_star_mask=1; m.data.update.collected_stars=1;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    CHECK(test_frame[57][39]==65535U && test_frame[60][39]==0U);
    m.data.update.ball_x=45; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    for(unsigned y=30;y<40;++y) { for(unsigned x=30;x<40;++x) { CHECK(test_frame[y+25][x+4]==0U); } }

    memcpy(s_saved,test_frame,sizeof(test_frame));
    m.data.update.ball_x=55; m.data.update.ball_y=65; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    m.data.update.ball_x=85; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    for(unsigned y=62;y<=68;++y) { for(unsigned x=52;x<=58;++x)
      { CHECK(test_frame[y+25][x+4]==s_saved[y+25][x+4]); } }

    m.type=DISPLAY_MSG_NOTICE; m.data.notice=DISPLAY_NOTICE_TRAP;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    memcpy(s_saved,test_frame,sizeof(test_frame));
    m.type=DISPLAY_MSG_UPDATE;
    m.data.update=(DisplayUpdate_t){.ball_x=15,.ball_y=15,.lives=2,.collected_star_mask=1,.collected_stars=1};
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && memcmp(test_frame,s_saved,sizeof(test_frame))==0);
    m.type=DISPLAY_MSG_RESUME; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    CHECK(r.scene.hud.collected_star_mask==1 && r.scene.hud.lives==2);
    for(unsigned y=30;y<40;++y) { for(unsigned x=30;x<40;++x) { CHECK(test_frame[y+25][x+4]==0U); } }
    m.type=DISPLAY_MSG_UPDATE;
    m.data.update=(DisplayUpdate_t){.ball_x=15,.ball_y=15,.lives=2,.collected_star_mask=3,.collected_stars=2,.gate_open=1};
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    CHECK(test_frame[129][108]==0U && test_frame[127][106]==65535U);
    m.type=DISPLAY_MSG_NOTICE; m.data.notice=DISPLAY_NOTICE_YOU_WIN;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    memcpy(test_snapshots[1],test_frame,sizeof(test_frame));
    memcpy(s_saved,test_frame,sizeof(test_frame));

    m=prvScene(); m.scene_id=2; m.data.scene.level=5;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    m.type=DISPLAY_MSG_UPDATE; m.data.update=(DisplayUpdate_t){.ball_x=55,.ball_y=55,.lives=1};
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    CHECK(memcmp(test_frame,s_saved,sizeof(test_frame))==0);
    m.type=DISPLAY_MSG_RESUME; m.scene_id=999;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && r.overlay);
    m.scene_id=2; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && !r.overlay);
    memcpy(test_snapshots[2],test_frame,sizeof(test_frame));
    m.type=DISPLAY_MSG_UPDATE; m.data.update=(DisplayUpdate_t){.ball_x=85,.ball_y=55,.lives=1};
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && test_frame[80][59]==65535U);
    s_fail_next=true; m.data.update.ball_x=95;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_TIMEOUT && r.dirty);
    CHECK(task_display_render_refresh(&r)==HAL_ST7735_OK && !r.dirty);
    m.data.update.ball_y=0; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.tiles[0][0]=99; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.level=6; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.total_stars=3; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.tiles[1][1]=DISPLAY_TILE_FLOOR; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.hud.lives=4; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.hud.gate_open=2; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.hud.collected_star_mask=1; CHECK(!task_display_render_validate(&m));
    m=prvScene(); m.data.scene.hud.collected_star_mask=4; m.data.scene.hud.collected_stars=1;
    CHECK(!task_display_render_validate(&m));
    m.type=DISPLAY_MSG_NOTICE; m.data.notice=(DisplayNotice_t)-1; CHECK(!task_display_render_validate(&m));
    m.scene_id=r.scene_id;
    m.data.notice=DISPLAY_NOTICE_GAME_OVER;
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK && r.overlay);

    m=prvScene(); m.scene_id=20U;
    m.data.scene.tiles[3][3]=DISPLAY_TILE_FLOOR; m.data.scene.tiles[4][6]=DISPLAY_TILE_FLOOR;
    for(unsigned n=0U;n<16U;++n) { m.data.scene.tiles[2U+n/10U][1U+n%10U]=DISPLAY_TILE_STAR; }
    m.data.scene.total_stars=16U;
    m.data.scene.hud.collected_star_mask=65535U; m.data.scene.hud.collected_stars=16U; m.data.scene.hud.gate_open=1U;
    CHECK(task_display_render_validate(&m));
    CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
    m.type=DISPLAY_MSG_RESUME; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);

    for(unsigned level=1;level<=5;++level)
    {
        memset(&m,0,sizeof(m)); m.type=DISPLAY_MSG_SCENE; m.scene_id=100+level;
        m.data.scene.level=(uint8_t)level; m.data.scene.total_stars=star_maze_star_count[level-1];
        m.data.scene.hud=(DisplayUpdate_t){.ball_x=15,.ball_y=15,.lives=3};
        memcpy(m.data.scene.tiles,star_maze_levels[level-1],sizeof(m.data.scene.tiles));
        CHECK(task_display_render_validate(&m));
        CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK);
        if(r.overlay) { m.type=DISPLAY_MSG_RESUME; CHECK(task_display_render_apply(&r,&m)==HAL_ST7735_OK); }
        memcpy(test_snapshots[2+level],test_frame,sizeof(test_frame));
    }
    for(unsigned y=0;y<160;++y) { for(unsigned x=0;x<128;++x)
      { CHECK(test_frame[y][x]==0U||test_frame[y][x]==65535U); } }

    s_scheduler=taskSCHEDULER_NOT_STARTED; s_heap=100;
    CHECK(task_display_create()==pdFAIL); s_heap=0; s_lazy_heap=true;
    CHECK(task_display_create()==pdPASS && task_display_create()==pdPASS);
    CHECK(s_task_stack==512U && s_task_priority==1U);
    s_scheduler=taskSCHEDULER_RUNNING;
    m=prvScene(); CHECK(task_display_post(&m,0)==pdPASS);
    m.data.scene.level=2; CHECK(task_display_post(&m,100)==pdPASS);
    CHECK(task_display_post(&m,1)==pdFAIL);
    DisplayMsg_t out;
    CHECK(xQueueReceive(s_queue,&out,0)==pdPASS && out.data.scene.level==1);
    CHECK(xQueueReceive(s_queue,&out,0)==pdPASS && out.data.scene.level==2);
    CHECK(xQueueReceive(s_queue,&out,50)==pdFAIL);
    CHECK(task_display_post(&m,101)==pdFAIL && task_display_post(&m,portMAX_DELAY)==pdFAIL);
    test_ipsr=1; CHECK(task_display_post(&m,0)==pdFAIL); test_ipsr=0;
    DisplayStats_t stats; task_display_get_stats(&stats); CHECK(stats.rejected==1U);
    test_done();
}
