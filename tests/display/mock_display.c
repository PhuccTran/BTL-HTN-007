/* CHỈ DÙNG TEST: phát kịch bản dựng sẵn của 5 bản đồ. Không đọc BMI, không
 * kiểm tra va chạm hoặc xử lý nút. Sao/tim/cổng dưới đây đều là dữ liệu giả.
 */
#include "mock_display.h"
#include "task_display.h"
#include "star_maze_levels.h"
#include "star_maze_mock_paths.h"
#include <stdbool.h>
#include <string.h>

static TaskHandle_t s_mock_task;
volatile DisplayStats_t g_display_test_stats;
volatile uint32_t g_display_mock_send_failures;
volatile uint32_t g_display_mock_aborted;
volatile uint32_t g_display_mock_ipc_version;
volatile UBaseType_t g_display_mock_stack_free;

static void prvStats(void)
{
    DisplayStats_t stats;
    task_display_get_stats(&stats); g_display_test_stats=stats;
    g_display_mock_stack_free=uxTaskGetStackHighWaterMark(NULL);
}

static BaseType_t prvPost(const DisplayMsg_t *m)
{
    for (unsigned tries=0U;tries<20U;++tries)
    {
        if (task_display_post(m,pdMS_TO_TICKS(100U))==pdPASS) { prvStats(); return pdPASS; }
        ++g_display_mock_send_failures;
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
    return pdFAIL;
}

static void prvMask(DisplayUpdate_t *hud,uint16_t mask,uint8_t total)
{
    hud->collected_star_mask=mask; hud->collected_stars=0U;
    for (uint16_t bits=mask;bits!=0U;bits>>=1) { hud->collected_stars+=(uint8_t)(bits&1U); }
    /* Chỉ dữ liệu giả theo kịch bản. Renderer không tự tính điều kiện này. */
    hud->gate_open=hud->collected_stars==total?1U:0U;
}

static uint8_t prvCellX(uint8_t cell) { return (uint8_t)((cell%12U)*10U+5U); }
static uint8_t prvCellY(uint8_t cell) { return (uint8_t)((cell/12U)*10U+5U); }

static BaseType_t prvUpdate(DisplayMsg_t *m,const DisplayUpdate_t *hud)
{ m->type=DISPLAY_MSG_UPDATE; m->data.update=*hud; return prvPost(m); }
static BaseType_t prvNotice(DisplayMsg_t *m,DisplayNotice_t notice)
{ m->type=DISPLAY_MSG_NOTICE; m->data.notice=notice; return prvPost(m); }
static BaseType_t prvResume(DisplayMsg_t *m)
{ m->type=DISPLAY_MSG_RESUME; return prvPost(m); }

static BaseType_t prvScene(DisplayMsg_t *m,uint8_t level,uint32_t id,DisplayUpdate_t *hud,bool resume)
{
    const StarMockPath_t *path=&s_star_mock_paths[level-1U];
    *hud=(DisplayUpdate_t){.ball_x=prvCellX(path->points[0].cell),.ball_y=prvCellY(path->points[0].cell),.lives=hud->lives};
    memset(m,0,sizeof(*m)); m->type=DISPLAY_MSG_SCENE; m->scene_id=id;
    m->data.scene.level=level; m->data.scene.hud=*hud;
    m->data.scene.total_stars=star_maze_star_count[level-1U];
    memcpy(m->data.scene.tiles,star_maze_levels[level-1U],sizeof(m->data.scene.tiles));
    if (prvPost(m)!=pdPASS) { return pdFAIL; }
    if (resume)
    {
        /* SCENE tới trong overlay phải được giữ lại, không ghi đè notice. */
        vTaskDelay(pdMS_TO_TICKS(700U));
        if (prvResume(m)!=pdPASS) { return pdFAIL; }
    }
    vTaskDelay(pdMS_TO_TICKS(300U));
    return pdPASS;
}

static BaseType_t prvTrap(DisplayMsg_t *m,DisplayUpdate_t *hud,uint8_t lives)
{
    /* Điểm bẫy của màn 2 là (hàng 6,cột 8). Đây là teleport trong mock để test
     * hiển thị, không phải đường điều khiển thật hoặc thuật toán va chạm. */
    hud->ball_x=85U; hud->ball_y=65U;
    if (prvUpdate(m,hud)!=pdPASS) { return pdFAIL; }
    vTaskDelay(pdMS_TO_TICKS(250U));
    hud->lives=lives;
    if (prvUpdate(m,hud)!=pdPASS ||
        prvNotice(m,lives==0U?DISPLAY_NOTICE_GAME_OVER:DISPLAY_NOTICE_TRAP)!=pdPASS) { return pdFAIL; }
    vTaskDelay(pdMS_TO_TICKS(lives==0U?1500U:600U));
    if (lives!=0U)
    {
        hud->ball_x=15U; hud->ball_y=15U; /* Giữ nguyên mask sao và trạng thái cổng. */
        if (prvUpdate(m,hud)!=pdPASS) { return pdFAIL; }
        vTaskDelay(pdMS_TO_TICKS(300U));
        if (prvResume(m)!=pdPASS) { return pdFAIL; }
    }
    return pdPASS;
}

static BaseType_t prvWalk(DisplayMsg_t *m,uint8_t level,DisplayUpdate_t *hud)
{
    const StarMockPath_t *path=&s_star_mock_paths[level-1U];
    bool trap_shown=false;
    TickType_t wake=xTaskGetTickCount();
    unsigned k=1U;
    while (k<path->count)
    {
        int x=prvCellX(path->points[k-1U].cell),y=prvCellY(path->points[k-1U].cell);
        int dx=(int)prvCellX(path->points[k].cell)-x,dy=(int)prvCellY(path->points[k].cell)-y;
        for (int frame=1;frame<=5;++frame)
        {
            hud->ball_x=(uint8_t)(x+dx*frame/5); hud->ball_y=(uint8_t)(y+dy*frame/5);
            if (frame==5)
            { prvMask(hud,(uint16_t)(hud->collected_star_mask|path->points[k].mask),star_maze_star_count[level-1U]); }
            if (prvUpdate(m,hud)!=pdPASS) { return pdFAIL; }
            (void)xTaskDelayUntil(&wake,pdMS_TO_TICKS(APP_DISPLAY_MOCK_PERIOD_MS));
        }
        if (level==2U && !trap_shown && hud->collected_stars!=0U)
        {
            /* Mất một tim sau sao đầu; sau reset đi lại đường cũ nhưng sao
             * đã nhặt không xuất hiện lại. Hai tim còn lại dùng cho màn 3..5. */
            if (prvTrap(m,hud,2U)!=pdPASS) { return pdFAIL; }
            trap_shown=true; k=1U; wake=xTaskGetTickCount();
            continue;
        }
        ++k;
    }
    return pdPASS;
}

static void prvMockTask(void *argument)
{
    (void)argument;
    g_display_mock_ipc_version=DISPLAY_IPC_VERSION;
    DisplayMsg_t m;
    DisplayUpdate_t hud={0};
    DisplayStats_t stats;
    /* Chờ init hữu hạn để không dồn Queue trong lúc màn đang reset. */
    bool ready=false;
    for (unsigned tries=0U;tries<60U;++tries)
    {
        task_display_get_stats(&stats);
        if (stats.ready==pdTRUE) { ready=true; break; }
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
    if (!ready) { ++g_display_mock_aborted; vTaskDelete(NULL); return; }
    uint32_t id=0U;
    bool resume=false;
    for (;;)
    {
        hud.lives=3U;
        for (uint8_t level=1U;level<=APP_DISPLAY_LEVEL_COUNT;++level)
        {
            if (prvScene(&m,level,++id,&hud,resume)!=pdPASS || prvWalk(&m,level,&hud)!=pdPASS) { goto abort_mock; }
            vTaskDelay(pdMS_TO_TICKS(400U));
            if (prvNotice(&m,level==APP_DISPLAY_LEVEL_COUNT?DISPLAY_NOTICE_YOU_WIN:DISPLAY_NOTICE_COMPLETE)!=pdPASS) { goto abort_mock; }
            vTaskDelay(pdMS_TO_TICKS(level==APP_DISPLAY_LEVEL_COUNT?1500U:600U));
            resume=true;
        }
        /* Kịch bản TEST riêng: màn 2, ba lần bẫy để thấy GAME OVER.
         * Không phải quy tắc bắt đầu game ở màn 2; lượt đầy đủ vẫn bắt đầu màn 1. */
        hud.lives=3U;
        if (prvScene(&m,2U,++id,&hud,true)!=pdPASS) { goto abort_mock; }
        for (unsigned loss=0U;loss<3U;++loss)
        { if (prvTrap(&m,&hud,(uint8_t)(2U-loss))!=pdPASS) { goto abort_mock; } }
        /* Vòng kế: SCENE 1 reset sao/tim dưới overlay GAME OVER, rồi RESUME. */
    }
abort_mock:
    ++g_display_mock_aborted;
    vTaskDelete(NULL);
}

BaseType_t task_display_mock_create(void)
{
    if (s_mock_task!=NULL) { return pdPASS; }
    return xTaskCreate(prvMockTask,"DISPLAY_MOCK",APP_DISPLAY_MOCK_STACK_WORDS,NULL,
                       APP_DISPLAY_MOCK_PRIORITY,&s_mock_task);
}
