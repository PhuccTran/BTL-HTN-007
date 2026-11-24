/* Thực thi task mock thật với RTOS giả; validate và renderer vẫn dùng code App
 * thật. Test riêng ảnh pixel nằm trong unit_display.c, không nhân đôi ở đây.
 */
#include "mock_display.h"
#include "task_display.h"
#include "display_render.h"
#include "star_maze_levels.h"
#include <string.h>

volatile uint32_t test_checks,test_failure,test_messages,test_wins,test_game_overs,test_traps;
static DisplayRender_t s_render;
static void (*s_task)(void *);
static TickType_t s_tick;
static unsigned s_tries,s_levels;
static bool s_respawn;
static uint16_t s_saved_mask;
__attribute__((noinline,noreturn)) void test_done(void) { for(;;) { __asm volatile("nop"); } }
#define CHECK(e) do { ++test_checks; if(!(e)) { test_failure=__LINE__; test_done(); } } while(0)

BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *handle)
{
    (void)name; (void)arg;
    CHECK(stack==APP_DISPLAY_MOCK_STACK_WORDS && priority==APP_DISPLAY_MOCK_PRIORITY);
    s_task=fn; *handle=(void *)1; return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return s_tick; }
void vTaskDelay(TickType_t ticks) { CHECK(ticks!=0U && ticks!=portMAX_DELAY); s_tick+=ticks; }
BaseType_t xTaskDelayUntil(TickType_t *wake,TickType_t period)
{ CHECK(period==50U); *wake+=period; s_tick=*wake; return pdTRUE; }
void vTaskDelete(TaskHandle_t task) { (void)task; CHECK(false); }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task; return 128U; }
void task_display_get_stats(DisplayStats_t *stats) { memset(stats,0,sizeof(*stats)); stats->ready=pdTRUE; }

BaseType_t task_display_post(const DisplayMsg_t *m,TickType_t timeout)
{
    CHECK(timeout<=100U);
    if ((++s_tries%37U)==0U) { return pdFAIL; } /* Ép retry một lần, không mất sự kiện. */
    CHECK(task_display_render_validate(m));
    if (m->type==DISPLAY_MSG_SCENE)
    {
        unsigned level=m->data.scene.level;
        CHECK(memcmp(m->data.scene.tiles,star_maze_levels[level-1U],144U)==0);
        CHECK(m->data.scene.total_stars==star_maze_star_count[level-1U]);
        CHECK(m->data.scene.hud.collected_star_mask==0U && m->data.scene.hud.gate_open==0U);
        if (test_game_overs!=0U && level==1U)
        {
            CHECK(test_wins==1U && test_game_overs==1U && test_traps==3U && s_levels==31U);
            CHECK(s_render.scene.hud.lives==0U && m->data.scene.hud.lives==3U);
            test_done();
        }
        if (test_wins==0U)
        {
            s_levels|=1U<<(level-1U);
            CHECK(m->data.scene.hud.lives==(level<=2U?3U:2U));
        }
    }
    else { CHECK(m->scene_id==s_render.scene_id); }
    if (s_respawn && m->type==DISPLAY_MSG_UPDATE)
    {
        CHECK(m->data.update.ball_x==15U && m->data.update.ball_y==15U);
        CHECK(m->data.update.collected_star_mask==s_saved_mask);
        s_respawn=false;
    }
    CHECK(task_display_render_apply(&s_render,m)==HAL_ST7735_OK);
    if (m->type==DISPLAY_MSG_NOTICE)
    {
        switch(m->data.notice)
        {
        case DISPLAY_NOTICE_YOU_WIN:
            ++test_wins;
            CHECK(s_render.scene.level==5U && s_render.scene.hud.lives==2U && s_render.scene.hud.gate_open==1U);
            break;
        case DISPLAY_NOTICE_GAME_OVER:
            ++test_game_overs; CHECK(s_render.scene.hud.lives==0U); break;
        case DISPLAY_NOTICE_TRAP:
            ++test_traps; s_saved_mask=s_render.scene.hud.collected_star_mask; s_respawn=true; break;
        case DISPLAY_NOTICE_COMPLETE:
            CHECK(s_render.scene.hud.gate_open==1U); break;
        default: CHECK(false); break;
        }
    }
    ++test_messages;
    return pdPASS;
}

hal_st7735_status_t hal_st7735_fill_rect(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color)
{ (void)x;(void)y;(void)w;(void)h; CHECK(color==0U || color==65535U); return HAL_ST7735_OK; }
hal_st7735_status_t hal_st7735_draw_hline(int16_t x,int16_t y,uint16_t n,uint16_t color)
{ return hal_st7735_fill_rect(x,y,n,1U,color); }
hal_st7735_status_t hal_st7735_draw_pixel(int16_t x,int16_t y,uint16_t color)
{ return hal_st7735_fill_rect(x,y,1U,1U,color); }
hal_st7735_status_t hal_st7735_draw_circle_filled(int16_t x,int16_t y,uint16_t r,uint16_t color)
{ CHECK(r<=3U); return hal_st7735_fill_rect(x,y,1U,1U,color); }
hal_st7735_status_t hal_st7735_draw_text(int16_t x,int16_t y,const char *t,uint16_t fg,uint16_t bg)
{ CHECK(t!=NULL && strlen(t)<=21U); (void)bg; return hal_st7735_fill_rect(x,y,1U,1U,fg); }

void test_entry(void)
{
    task_display_render_reset(&s_render);
    CHECK(task_display_mock_create()==pdPASS && task_display_mock_create()==pdPASS);
    s_task(NULL);
    CHECK(false);
}
