#include "display_render.h"
#include "star_maze_levels.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define BLACK HAL_ST7735_BLACK
#define WHITE HAL_ST7735_WHITE
#define DRAW(call) do { hal_st7735_status_t draw_s=(call); if (draw_s!=HAL_ST7735_OK) { return draw_s; } } while (0)
_Static_assert(APP_DISPLAY_LEVEL_COUNT==STAR_MAZE_NUM_LEVELS && APP_DISPLAY_ROWS==STAR_MAZE_ROWS &&
               APP_DISPLAY_COLS==STAR_MAZE_COLS,"Display grid must match supplied maps");
_Static_assert((int)DISPLAY_TILE_START==(int)TILE_START && (int)DISPLAY_TILE_GATE==(int)TILE_GATE &&
               (int)DISPLAY_TILE_STAR==(int)TILE_STAR && (int)DISPLAY_TILE_TRAP==(int)TILE_TRAP,
               "IPC tile IDs must match supplied STAR MAZE data");

static unsigned prvPopCount(uint16_t mask)
{
    unsigned count=0U;
    while (mask!=0U) { count+=mask&1U; mask>>=1; }
    return count;
}

static bool prvValidUpdate(const DisplayUpdate_t *u)
{
    return u->ball_x>=APP_DISPLAY_BALL_RADIUS && u->ball_x<APP_DISPLAY_MAP_SIZE-APP_DISPLAY_BALL_RADIUS &&
           u->ball_y>=APP_DISPLAY_BALL_RADIUS && u->ball_y<APP_DISPLAY_MAP_SIZE-APP_DISPLAY_BALL_RADIUS &&
           u->lives<=APP_DISPLAY_MAX_LIVES && u->gate_open<=1U &&
           u->collected_stars==prvPopCount(u->collected_star_mask);
}

static bool prvMatchesScene(const DisplayUpdate_t *u,uint8_t total)
{
    uint16_t allowed=(uint16_t)((1UL<<total)-1UL);
    return (u->collected_star_mask & (uint16_t)~allowed)==0U &&
           (u->gate_open==0U || u->collected_stars==total);
}

bool task_display_render_validate(const DisplayMsg_t *m)
{
    if (m==NULL) { return false; }
    switch (m->type)
    {
    case DISPLAY_MSG_SCENE:
    {
        const DisplayScene_t *s=&m->data.scene;
        if (s->level<1U || s->level>APP_DISPLAY_LEVEL_COUNT || s->total_stars>APP_DISPLAY_MAX_STARS ||
            !prvValidUpdate(&s->hud) || !prvMatchesScene(&s->hud,s->total_stars)) { return false; }
        unsigned stars=0U,starts=0U,gates=0U;
        for (size_t y=0U;y<APP_DISPLAY_ROWS;++y)
        { for (size_t x=0U;x<APP_DISPLAY_COLS;++x)
          {
              uint8_t tile=s->tiles[y][x];
              if (tile>DISPLAY_TILE_TRAP) { return false; }
              stars+=tile==DISPLAY_TILE_STAR?1U:0U;
              starts+=tile==DISPLAY_TILE_START?1U:0U;
              gates+=tile==DISPLAY_TILE_GATE?1U:0U;
          } }
        return stars==s->total_stars && starts==1U && gates==1U;
    }
    case DISPLAY_MSG_UPDATE: return prvValidUpdate(&m->data.update);
    case DISPLAY_MSG_NOTICE: return (unsigned)m->data.notice<=(unsigned)DISPLAY_NOTICE_GAME_OVER;
    case DISPLAY_MSG_RESUME: return true;
    default: return false;
    }
}

void task_display_render_reset(DisplayRender_t *r)
{ memset(r,0,sizeof(*r)); r->dirty=true; }
void task_display_render_invalidate(DisplayRender_t *r)
{ r->dirty=true; r->painted_valid=false; }

static void prvIndexStars(DisplayRender_t *r)
{
    unsigned n=0U;
    for (unsigned y=0U;y<APP_DISPLAY_ROWS;++y)
    { for (unsigned x=0U;x<APP_DISPLAY_COLS;++x)
      { if (r->scene.tiles[y][x]==DISPLAY_TILE_STAR)
        { r->star_rows[n]=(uint8_t)y; r->star_cols[n]=(uint8_t)x; ++n; } } }
}

/* Nền tại một pixel: tường trắng, đường đen, sao trắng, bẫy X,
 * cổng vuông đặc khi khóa và vuông rỗng khi mở. Chỉ tái hiện trạng thái GAME
 * gửi, không tự nhặt sao/trừ tim/mở cổng hay kiểm tra va chạm.
 */
static uint16_t prvBackground(const DisplayRender_t *r,int x,int y)
{
    static const uint8_t star[7]={0x08,0x08,0x7F,0x3E,0x1C,0x36,0x22};
    unsigned row=(unsigned)y/APP_DISPLAY_TILE_PX,col=(unsigned)x/APP_DISPLAY_TILE_PX;
    unsigned px=(unsigned)x%APP_DISPLAY_TILE_PX,py=(unsigned)y%APP_DISPLAY_TILE_PX;
    uint8_t tile=r->scene.tiles[row][col];
    if (tile==DISPLAY_TILE_WALL) { return WHITE; }
    if (tile==DISPLAY_TILE_GATE)
    {
        bool square=px>=2U && px<=7U && py>=2U && py<=7U;
        bool edge=px==2U || px==7U || py==2U || py==7U;
        return square && (r->scene.hud.gate_open==0U || edge)?WHITE:BLACK;
    }
    if (tile==DISPLAY_TILE_STAR)
    {
        for (unsigned i=0U;i<r->scene.total_stars;++i)
        {
            if (r->star_rows[i]==row && r->star_cols[i]==col &&
                (r->scene.hud.collected_star_mask&(1U<<i))!=0U) { return BLACK; }
        }
        if (px>=2U && px<=8U && py>=2U && py<=8U && (star[py-2U]&(1U<<(8U-px)))!=0U) { return WHITE; }
    }
    if (tile==DISPLAY_TILE_TRAP && px>=2U && px<=7U && py>=2U && py<=7U &&
        (px==py || px+py==9U)) { return WHITE; }
    return BLACK;
}

static hal_st7735_status_t prvRegion(const DisplayRender_t *r,int x0,int y0,int x1,int y1)
{
    for (int y=y0;y<=y1;++y)
    {
        int x=x0;
        while (x<=x1)
        {
            int end=x;
            uint16_t color=prvBackground(r,x,y);
            while (end<x1 && prvBackground(r,end+1,y)==color) { ++end; }
            DRAW(hal_st7735_draw_hline((int16_t)(APP_DISPLAY_MAP_X+x),(int16_t)(APP_DISPLAY_MAP_Y+y),
                                       (uint16_t)(end-x+1),color));
            x=end+1;
        }
    }
    return HAL_ST7735_OK;
}

static hal_st7735_status_t prvTile(const DisplayRender_t *r,unsigned row,unsigned col)
{
    int x=(int)(col*APP_DISPLAY_TILE_PX),y=(int)(row*APP_DISPLAY_TILE_PX);
    uint8_t tile=r->scene.tiles[row][col];
    if (tile==DISPLAY_TILE_WALL || tile==DISPLAY_TILE_FLOOR || tile==DISPLAY_TILE_START)
    { return hal_st7735_fill_rect((int16_t)(APP_DISPLAY_MAP_X+x),(int16_t)(APP_DISPLAY_MAP_Y+y),
                                  APP_DISPLAY_TILE_PX,APP_DISPLAY_TILE_PX,tile==DISPLAY_TILE_WALL?WHITE:BLACK); }
    return prvRegion(r,x,y,x+(int)APP_DISPLAY_TILE_PX-1,y+(int)APP_DISPLAY_TILE_PX-1);
}

static hal_st7735_status_t prvBall(const DisplayUpdate_t *u)
{
    int16_t x=(int16_t)(APP_DISPLAY_MAP_X+u->ball_x),y=(int16_t)(APP_DISPLAY_MAP_Y+u->ball_y);
    DRAW(hal_st7735_draw_circle_filled(x,y,APP_DISPLAY_BALL_RADIUS,WHITE));
    return hal_st7735_draw_circle_filled(x,y,APP_DISPLAY_BALL_RADIUS-1U,BLACK);
}

static hal_st7735_status_t prvHeader(const DisplayScene_t *scene)
{
    char text[16];
    DRAW(hal_st7735_fill_rect(0,0,128U,25U,BLACK));
    (void)snprintf(text,sizeof(text),"LEVEL%u",(unsigned)scene->level);
    DRAW(hal_st7735_draw_text(4,8,text,WHITE,BLACK));
    (void)snprintf(text,sizeof(text),"STAR%u/%u",(unsigned)scene->hud.collected_stars,(unsigned)scene->total_stars);
    DRAW(hal_st7735_draw_text(68,8,text,WHITE,BLACK));
    return hal_st7735_draw_hline(4,22,120U,WHITE);
}

static hal_st7735_status_t prvFooter(const DisplayUpdate_t *u)
{
    static const uint8_t heart[7]={0x36,0x7F,0x7F,0x3E,0x1C,0x08,0};
    DRAW(hal_st7735_fill_rect(0,145,128U,15U,BLACK));
    DRAW(hal_st7735_draw_hline(4,146,120U,WHITE));
    DRAW(hal_st7735_draw_text(4,150,"HP",WHITE,BLACK));
    for (unsigned i=0U;i<u->lives;++i)
    { for (unsigned y=0U;y<7U;++y)
      { for (unsigned x=0U;x<7U;++x)
        { if ((heart[y]&(1U<<(6U-x)))!=0U)
          { DRAW(hal_st7735_draw_pixel((int16_t)(25U+i*10U+x),(int16_t)(150U+y),WHITE)); } } } }
    return HAL_ST7735_OK;
}

static hal_st7735_status_t prvFull(DisplayRender_t *r)
{
    DRAW(hal_st7735_fill_rect(0,0,128U,160U,BLACK));
    DRAW(prvHeader(&r->scene));
    for (unsigned y=0U;y<APP_DISPLAY_ROWS;++y)
    { for (unsigned x=0U;x<APP_DISPLAY_COLS;++x) { DRAW(prvTile(r,y,x)); } }
    DRAW(prvBall(&r->scene.hud));
    return prvFooter(&r->scene.hud);
}

static hal_st7735_status_t prvOverlay(const DisplayRender_t *r)
{
    const char *title="RESTART",*subtitle="PLEASE WAIT";
    switch (r->notice)
    {
    case DISPLAY_NOTICE_COMPLETE: title="LEVEL COMPLETE"; break;
    case DISPLAY_NOTICE_TRAP: title="TRAP -1 HP"; break;
    case DISPLAY_NOTICE_YOU_WIN: title="YOU WIN"; subtitle="5 LEVELS CLEAR"; break;
    case DISPLAY_NOTICE_GAME_OVER: title="GAME OVER"; subtitle="NO HEARTS LEFT"; break;
    default: break;
    }
    DRAW(hal_st7735_fill_rect(4,25,120U,120U,BLACK));
    DRAW(hal_st7735_draw_hline(8,65,112U,WHITE));
    DRAW(hal_st7735_draw_hline(8,100,112U,WHITE));
    DRAW(hal_st7735_draw_text((int16_t)((128U-strlen(title)*6U)/2U),77,title,WHITE,BLACK));
    return hal_st7735_draw_text((int16_t)((128U-strlen(subtitle)*6U)/2U),89,subtitle,WHITE,BLACK);
}

hal_st7735_status_t task_display_render_refresh(DisplayRender_t *r)
{
    if (!r->have_scene || !r->dirty) { return HAL_ST7735_OK; }
    hal_st7735_status_t s=r->overlay?prvOverlay(r):prvFull(r);
    if (s==HAL_ST7735_OK)
    { r->dirty=false; r->painted_valid=!r->overlay; r->painted=r->scene.hud; }
    return s;
}

static hal_st7735_status_t prvChangedTiles(DisplayRender_t *r)
{
    uint16_t changed=r->painted.collected_star_mask^r->scene.hud.collected_star_mask;
    for (unsigned i=0U;i<r->scene.total_stars;++i)
    { if ((changed&(1U<<i))!=0U) { DRAW(prvTile(r,r->star_rows[i],r->star_cols[i])); } }
    if (r->painted.gate_open!=r->scene.hud.gate_open)
    {
        for (unsigned y=0U;y<APP_DISPLAY_ROWS;++y)
        { for (unsigned x=0U;x<APP_DISPLAY_COLS;++x)
          { if (r->scene.tiles[y][x]==DISPLAY_TILE_GATE) { DRAW(prvTile(r,y,x)); } } }
    }
    return HAL_ST7735_OK;
}

hal_st7735_status_t task_display_render_apply(DisplayRender_t *r,const DisplayMsg_t *m)
{
    if (!task_display_render_validate(m)) { return HAL_ST7735_INVALID; }
    if (m->type==DISPLAY_MSG_SCENE)
    {
        /* SCENE do một producer gửi FIFO. ID là token so khớp, hỗ trợ wrap. */
        r->scene=m->data.scene; r->scene_id=m->scene_id; r->have_scene=true;
        prvIndexStars(r);
        if (!r->overlay) { task_display_render_invalidate(r); }
        return task_display_render_refresh(r);
    }
    if (!r->have_scene || m->scene_id!=r->scene_id) { return HAL_ST7735_OK; }
    if (m->type==DISPLAY_MSG_NOTICE)
    { r->notice=m->data.notice; r->overlay=true; task_display_render_invalidate(r); return task_display_render_refresh(r); }
    if (m->type==DISPLAY_MSG_RESUME)
    { r->overlay=false; task_display_render_invalidate(r); return task_display_render_refresh(r); }
    if (!prvMatchesScene(&m->data.update,r->scene.total_stars)) { return HAL_ST7735_INVALID; }
    r->scene.hud=m->data.update;
    if (r->overlay) { return task_display_render_refresh(r); }
    if (r->dirty || !r->painted_valid) { return task_display_render_refresh(r); }
    bool moved=r->painted.ball_x!=r->scene.hud.ball_x || r->painted.ball_y!=r->scene.hud.ball_y;
    bool tiles_changed=r->painted.collected_star_mask!=r->scene.hud.collected_star_mask ||
                       r->painted.gate_open!=r->scene.hud.gate_open;
    hal_st7735_status_t s=HAL_ST7735_OK;
    if (moved || tiles_changed)
    {
        int x=r->painted.ball_x,y=r->painted.ball_y,radius=APP_DISPLAY_BALL_RADIUS;
        s=prvRegion(r,x-radius,y-radius,x+radius,y+radius);
        if (s==HAL_ST7735_OK && tiles_changed) { s=prvChangedTiles(r); }
        /* Vẽ bi sau tile để sao/cổng vừa cập nhật không đè bi đang đứng yên. */
        if (s==HAL_ST7735_OK) { s=prvBall(&r->scene.hud); }
    }
    if (s==HAL_ST7735_OK && r->painted.collected_stars!=r->scene.hud.collected_stars) { s=prvHeader(&r->scene); }
    if (s==HAL_ST7735_OK && r->painted.lives!=r->scene.hud.lives) { s=prvFooter(&r->scene.hud); }
    if (s!=HAL_ST7735_OK) { task_display_render_invalidate(r); }
    else { r->painted=r->scene.hud; }
    return s;
}
