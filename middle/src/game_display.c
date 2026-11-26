/* 1. Prepare queue messages in task_game. No LCD writes in this part. */
#include "game_display.h"
#include <string.h>

_Static_assert(APP_DISPLAY_BALL_RADIUS == GAME_BALL_RADIUS_PX, "Radius mismatch");
_Static_assert(APP_DISPLAY_ROWS == STAR_MAZE_ROWS && APP_DISPLAY_COLS == STAR_MAZE_COLS,
               "Map mismatch");

static DisplayUpdate_t prvHud(const Game_t *game)
{
    DisplayUpdate_t update = {0};
    update.ball_x = game_core_ball_x(game); update.ball_y = game_core_ball_y(game);
    update.lives = game->lives; update.collected_star_mask = game->collected_mask;
    update.collected_stars = game->collected;
    update.gate_open = game_core_gate_open(game) ? 1U : 0U;
    return update;
}

void game_present_init(GamePresent_t *present)
{ memset(present, 0, sizeof(*present)); }

static void prvAdd(GamePresent_t *present, DisplayMsgType_t type, uint32_t id)
{
    DisplayMsg_t *msg = &present->pending[present->count++];
    memset(msg, 0, sizeof(*msg)); msg->type = type; msg->scene_id = id;
}

void game_present_prepare(GamePresent_t *present, const Game_t *game)
{
    /* Caller must finish an old transaction or explicitly reinitialize on restart. */
    if (present->next < present->count) { return; }
    present->count = 0U; present->next = 0U; present->has_notice = false;
    bool new_scene = !present->valid || present->scene_id != game->scene_id;
    bool state_change = !present->valid || present->state != game->state;
    bool was_overlay = present->valid && present->state != GAME_PLAYING;
    if (new_scene) {
        prvAdd(present, DISPLAY_MSG_SCENE, game->scene_id);
        DisplayScene_t *scene = &present->pending[0].data.scene;
        scene->hud = prvHud(game); scene->level = game->level;
        scene->total_stars = game->total_stars;
        memcpy(scene->tiles, game_core_map(game), sizeof(scene->tiles));
    } else {
        prvAdd(present, DISPLAY_MSG_UPDATE, game->scene_id);
        present->pending[0].data.update = prvHud(game);
    }
    if ((state_change || new_scene) && game->state != GAME_PLAYING) {
        DisplayNotice_t notice = DISPLAY_NOTICE_RESTART;
        switch (game->state) {
        case GAME_TRAP_NOTICE: notice = DISPLAY_NOTICE_TRAP; break;
        case GAME_LEVEL_NOTICE: notice = DISPLAY_NOTICE_COMPLETE; break;
        case GAME_RESTART_NOTICE: notice = DISPLAY_NOTICE_COMPLETE; break;
        case GAME_OVER: notice = DISPLAY_NOTICE_GAME_OVER; break;
        case GAME_WON: notice = DISPLAY_NOTICE_YOU_WIN; break;
        case GAME_SPLASH:
        case GAME_PLAYING:
        default: break;
        }
        prvAdd(present, DISPLAY_MSG_NOTICE, game->scene_id);
        present->pending[present->count - 1U].data.notice = notice;
        present->has_notice = true;
    } else if (game->state == GAME_PLAYING && (was_overlay || new_scene)) {
        /* RESUME also clears an old terminal overlay after a forced restart. */
        prvAdd(present, DISPLAY_MSG_RESUME, game->scene_id);
    } else { /* An ordinary update has no control message. */ }
    present->scene_id = game->scene_id; present->state = game->state;
    present->valid = true;
}

const DisplayMsg_t *game_present_peek(const GamePresent_t *present)
{ return present->next < present->count ? &present->pending[present->next] : NULL; }
void game_present_accept(GamePresent_t *present)
{ if (present->next < present->count) { ++present->next; } }

/* 2. Draw only from task_display. Reuse the supplied maze renderer. */
#include "display_render.h"
#include <stddef.h>
#include <string.h>

/* Game-only presentation layer. Supplied renderer/driver/IPC stay unchanged.
 * Only task_display draws; its original queue and context remain the owner. */
hal_st7735_status_t game_ui_base_apply(DisplayRender_t *r, const DisplayMsg_t *msg);
hal_st7735_status_t game_ui_base_refresh(DisplayRender_t *r);

#define WHITE HAL_ST7735_WHITE
#define BLACK HAL_ST7735_BLACK
#define DRAW(call) do { hal_st7735_status_t s_ = (call); if (s_ != HAL_ST7735_OK) { return s_; } } while (0)

/* Compact 5x7 block alphabet, matching the game's existing pixel style. */
static const uint8_t letters[26][7] = {
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
static const uint8_t digits[5][7] = {
 {4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2},{31,16,16,30,1,1,30}
};

static hal_st7735_status_t text_center(const char *text, int y, unsigned scale)
{
    size_t length = strlen(text);
    int width = (int)(length * 6U * scale - scale);
    if (length == 0U || width > HAL_ST7735_WIDTH || y < 0 ||
        y + (int)(7U * scale) > HAL_ST7735_HEIGHT) { return HAL_ST7735_INVALID; }
    int left = (HAL_ST7735_WIDTH - width) / 2;
    for (size_t n = 0U; n < length; ++n) {
        char c = text[n];
        const uint8_t *glyph = c >= 'A' && c <= 'Z' ? letters[c - 'A'] :
                              (c >= '1' && c <= '5' ? digits[c - '1'] : NULL);
        if (glyph == NULL) { continue; }
        for (unsigned row = 0U; row < 7U; ++row) {
            for (unsigned col = 0U; col < 5U;) {
                if ((glyph[row] & (1U << (4U - col))) == 0U) { ++col; continue; }
                unsigned start = col++;
                while (col < 5U && (glyph[row] & (1U << (4U - col))) != 0U) { ++col; }
                DRAW(hal_st7735_fill_rect((int16_t)(left + (int)((n * 6U + start) * scale)),
                     (int16_t)(y + (int)(row * scale)), (uint16_t)((col - start) * scale),
                     (uint16_t)scale, WHITE));
            }
        }
    }
    return HAL_ST7735_OK;
}

static hal_st7735_status_t star(int cx, int cy)
{
    static const int8_t points[10][2] = {
        {0,-14},{4,-4},{14,-4},{6,3},{9,13},{0,7},{-9,13},{-6,3},{-14,-4},{-4,-4}
    };
    /* Scanline polygon, bounded to a 29x28 icon; no framebuffer allocation. */
    for (int y = -14; y < 14; ++y) {
        int crossings[10]; unsigned count = 0U;
        for (unsigned i = 0U; i < 10U; ++i) {
            unsigned j = (i + 1U) % 10U;
            int x0 = points[i][0], y0 = points[i][1], x1 = points[j][0], y1 = points[j][1];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                crossings[count++] = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
            }
        }
        for (unsigned i = 1U; i < count; ++i) {
            int x = crossings[i]; unsigned j = i;
            while (j > 0U && crossings[j - 1U] > x) { crossings[j] = crossings[j - 1U]; --j; }
            crossings[j] = x;
        }
        for (unsigned i = 0U; i + 1U < count; i += 2U) {
            DRAW(hal_st7735_draw_hline((int16_t)(cx + crossings[i]), (int16_t)(cy + y),
                                      (uint16_t)(crossings[i + 1U] - crossings[i] + 1), WHITE));
        }
    }
    return HAL_ST7735_OK;
}

static hal_st7735_status_t broken_heart(void)
{
    /* User's pixel icon: 18x14 bits, enlarged 3x to 54x42 pixels.
     * Centered on 128x160; only white icon on the cleared black screen. */
    static const uint32_t rows[14] = {
        0x07838U,0x0FC7CU,0x1FE7EU,0x3FCFFU,0x3F9FFU,0x3FCFFU,0x3FE7FU,
        0x1FE7EU,0x0FCFCU,0x039F0U,0x01C60U,0x01E40U,0x00C80U,0x00500U
    };
    for (unsigned y = 0U; y < 14U; ++y) {
        for (unsigned x = 0U; x < 18U;) {
            if ((rows[y] & (1UL << (17U - x))) == 0U) { ++x; continue; }
            unsigned start = x++;
            while (x < 18U && (rows[y] & (1UL << (17U - x))) != 0U) { ++x; }
            DRAW(hal_st7735_fill_rect((int16_t)(37U + start * 3U),
                                     (int16_t)(59U + y * 3U),
                                     (uint16_t)((x - start) * 3U), 3U, WHITE));
        }
    }
    return HAL_ST7735_OK;
}

static bool is_card(DisplayNotice_t notice)
{
    return notice == DISPLAY_NOTICE_RESTART || notice == DISPLAY_NOTICE_COMPLETE ||
           notice == DISPLAY_NOTICE_YOU_WIN || notice == DISPLAY_NOTICE_GAME_OVER ||
           notice == DISPLAY_NOTICE_TRAP;
}

static hal_st7735_status_t card(const DisplayRender_t *r)
{
    DRAW(hal_st7735_fill_rect(0, 0, 128U, 160U, BLACK));
    if (r->notice == DISPLAY_NOTICE_TRAP) { return broken_heart(); }
    if (r->notice == DISPLAY_NOTICE_RESTART) {
        DRAW(star(64, 59));
        return text_center("STAR MAZE", 94, 2U);
    }
    if (r->notice == DISPLAY_NOTICE_COMPLETE) {
        char title[] = "LEVEL 1";
        title[6] = (char)('0' + r->scene.level);
        return text_center(title, 73, 2U);
    }
    if (r->notice == DISPLAY_NOTICE_YOU_WIN) {
        DRAW(star(64, 59));
        return text_center("YOU WIN", 94, 2U);
    }
    return text_center("GAME OVER", 73, 2U);
}

hal_st7735_status_t task_display_render_refresh(DisplayRender_t *r)
{
    if (!r->have_scene || !r->dirty) { return HAL_ST7735_OK; }
    if (!r->overlay || !is_card(r->notice)) { return game_ui_base_refresh(r); }
    hal_st7735_status_t status = card(r);
    if (status == HAL_ST7735_OK) { r->dirty = false; r->painted_valid = false; }
    return status;
}

hal_st7735_status_t task_display_render_apply(DisplayRender_t *r, const DisplayMsg_t *msg)
{
    if (!task_display_render_validate(msg)) { return HAL_ST7735_INVALID; }
    if (msg->type == DISPLAY_MSG_SCENE) {
        /* Cache/index a scene without flashing the maze before its title.
         * Presenter always follows SCENE with NOTICE or RESUME, including
         * forced level changes while another title is already visible. */
        r->overlay = true; r->dirty = false;
        hal_st7735_status_t status = game_ui_base_apply(r, msg);
        r->painted_valid = false;
        return status;
    }
    if (!r->have_scene || r->scene_id != msg->scene_id) { return HAL_ST7735_OK; }
    if (msg->type == DISPLAY_MSG_NOTICE && is_card(msg->data.notice)) {
        r->overlay = true; r->notice = msg->data.notice;
        task_display_render_invalidate(r);
        return task_display_render_refresh(r);
    }
    if (msg->type == DISPLAY_MSG_UPDATE && r->overlay && is_card(r->notice)) {
        bool dirty = r->dirty; r->dirty = false;
        hal_st7735_status_t status = game_ui_base_apply(r, msg);
        r->dirty = dirty;
        return status == HAL_ST7735_OK ? task_display_render_refresh(r) : status;
    }
    return game_ui_base_apply(r, msg);
}
