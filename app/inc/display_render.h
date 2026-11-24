#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H
#include <stdbool.h>
#include "app_ipc.h"
#include "hal_st7735.h"

/* Context chỉ do task_display sở hữu. Không truy cập context từ task khác. */
typedef struct {
    DisplayScene_t scene;
    DisplayUpdate_t painted;
    uint32_t scene_id;
    DisplayNotice_t notice;
    uint8_t star_rows[APP_DISPLAY_MAX_STARS], star_cols[APP_DISPLAY_MAX_STARS];
    bool have_scene, overlay, dirty, painted_valid;
} DisplayRender_t;
void task_display_render_reset(DisplayRender_t *r);
bool task_display_render_validate(const DisplayMsg_t *msg);
hal_st7735_status_t task_display_render_apply(DisplayRender_t *r,const DisplayMsg_t *msg);
hal_st7735_status_t task_display_render_refresh(DisplayRender_t *r);
void task_display_render_invalidate(DisplayRender_t *r);
#endif
