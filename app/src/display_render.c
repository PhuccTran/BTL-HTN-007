/* ============================================================================
 * display_render.c - Bộ vẽ (renderer) của module màn hình ST7735.
 *
 * Nhiệm vụ:
 *   1. Kiểm tra message từ game có hợp lệ không.
 *   2. Nhớ trạng thái màn chơi và trạng thái ĐÃ VẼ lên LCD.
 *   3. Quyết định vẽ lại toàn bộ hay chỉ cập nhật phần thay đổi.
 *
 * Lưu ý: file này CHỈ tái hiện trạng thái game gửi sang. Nó không tự nhặt sao,
 * không trừ tim, không mở cổng, không kiểm tra va chạm.
 * Mọi hàm ở đây chỉ được gọi từ task Display (nên không cần khóa mutex riêng).
 * ========================================================================== */
#include "display_render.h"
#include "star_maze_levels.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define BLACK HAL_ST7735_BLACK
#define WHITE HAL_ST7735_WHITE

/* Gọi một hàm vẽ; nếu hàm trả lỗi thì thoát ngay và báo lỗi lên trên.
 * Tránh phải viết "if (lỗi) return" sau mỗi lần vẽ. */
#define DRAW(call) do { hal_st7735_status_t draw_s=(call); if (draw_s!=HAL_ST7735_OK) { return draw_s; } } while (0)

/* Kiểm tra lúc BIÊN DỊCH: kích thước lưới và số màn phải khớp dữ liệu màn chơi. */
_Static_assert(APP_DISPLAY_LEVEL_COUNT==STAR_MAZE_NUM_LEVELS && APP_DISPLAY_ROWS==STAR_MAZE_ROWS &&
               APP_DISPLAY_COLS==STAR_MAZE_COLS,"Display grid must match supplied maps");
/* Mã các loại ô bên IPC phải trùng mã trong dữ liệu màn chơi. */
_Static_assert((int)DISPLAY_TILE_START==(int)TILE_START && (int)DISPLAY_TILE_GATE==(int)TILE_GATE &&
               (int)DISPLAY_TILE_STAR==(int)TILE_STAR && (int)DISPLAY_TILE_TRAP==(int)TILE_TRAP,
               "IPC tile IDs must match supplied STAR MAZE data");

/* ------------------------------ 1. KIỂM TRA ------------------------------ */

/* Đếm số bit 1 trong mặt nạ sao (mỗi bit = một sao đã nhặt). */
static unsigned prvPopCount(uint16_t mask)
{
    unsigned count=0U;
    while (mask!=0U) { count+=mask&1U; mask>>=1; }
    return count;
}

/* Kiểm tra phần thông số (HUD) tự thân có hợp lý không:
 *  - tâm bi nằm trong 3..116 px (bi bán kính 3, bản đồ 120 px) theo cả hai chiều;
 *  - số tim không vượt tối đa, gate_open chỉ là 0 hoặc 1;
 *  - số sao đã nhặt khớp với số bit trong mặt nạ. */
static bool prvValidUpdate(const DisplayUpdate_t *u)
{
    return u->ball_x>=APP_DISPLAY_BALL_RADIUS && u->ball_x<APP_DISPLAY_MAP_SIZE-APP_DISPLAY_BALL_RADIUS &&
           u->ball_y>=APP_DISPLAY_BALL_RADIUS && u->ball_y<APP_DISPLAY_MAP_SIZE-APP_DISPLAY_BALL_RADIUS &&
           u->lives<=APP_DISPLAY_MAX_LIVES && u->gate_open<=1U &&
           u->collected_stars==prvPopCount(u->collected_star_mask);
}

/* Kiểm tra thông số có khớp với màn chơi (tổng số sao = total):
 *  - mặt nạ không được có bit của sao "không tồn tại";
 *  - cổng chỉ được mở khi đã nhặt đủ sao. */
static bool prvMatchesScene(const DisplayUpdate_t *u,uint8_t total)
{
    uint16_t allowed=(uint16_t)((1UL<<total)-1UL);
    return (u->collected_star_mask & (uint16_t)~allowed)==0U &&
           (u->gate_open==0U || u->collected_stars==total);
}

/* Kiểm tra cả message. Cũng được task_display_post() gọi để chặn message xấu
 * ngay từ cửa, trước khi vào queue. */
bool task_display_render_validate(const DisplayMsg_t *m)
{
    if (m==NULL) { return false; }
    switch (m->type)
    {
    case DISPLAY_MSG_SCENE:
    {
        const DisplayScene_t *s=&m->data.scene;
        /* Level 1..5, số sao không quá tối đa, HUD hợp lệ và khớp màn. */
        if (s->level<1U || s->level>APP_DISPLAY_LEVEL_COUNT || s->total_stars>APP_DISPLAY_MAX_STARS ||
            !prvValidUpdate(&s->hud) || !prvMatchesScene(&s->hud,s->total_stars)) { return false; }
        unsigned stars=0U,starts=0U,gates=0U;
        /* Quét 144 ô: mã ô hợp lệ, đồng thời đếm sao / START / GATE. */
        for (size_t y=0U;y<APP_DISPLAY_ROWS;++y)
        { for (size_t x=0U;x<APP_DISPLAY_COLS;++x)
          {
              uint8_t tile=s->tiles[y][x];
              if (tile>DISPLAY_TILE_TRAP) { return false; }
              stars+=tile==DISPLAY_TILE_STAR?1U:0U;
              starts+=tile==DISPLAY_TILE_START?1U:0U;
              gates+=tile==DISPLAY_TILE_GATE?1U:0U;
          } }
        /* Số sao trên bản đồ phải bằng total_stars; đúng 1 START và 1 GATE. */
        return stars==s->total_stars && starts==1U && gates==1U;
    }
    case DISPLAY_MSG_UPDATE: return prvValidUpdate(&m->data.update);
    case DISPLAY_MSG_NOTICE: return (unsigned)m->data.notice<=(unsigned)DISPLAY_NOTICE_GAME_OVER;
    case DISPLAY_MSG_RESUME: return true;
    default: return false;
    }
}

/* ------------------------- QUẢN LÝ TRẠNG THÁI --------------------------- */

/* Xóa sạch về 0, đặt dirty = true để lần refresh đầu tiên sẽ vẽ. */
void task_display_render_reset(DisplayRender_t *r)
{ memset(r,0,sizeof(*r)); r->dirty=true; }

/* Đánh dấu "màn hình thật không còn đáng tin": phải vẽ lại toàn bộ.
 * painted_valid = false cấm kiểu cập nhật từng phần. */
void task_display_render_invalidate(DisplayRender_t *r)
{ r->dirty=true; r->painted_valid=false; }

/* Lập danh sách vị trí các sao theo thứ tự hàng rồi cột.
 * Sao thứ i trong danh sách ứng với bit thứ i của collected_star_mask. */
static void prvIndexStars(DisplayRender_t *r)
{
    unsigned n=0U;
    for (unsigned y=0U;y<APP_DISPLAY_ROWS;++y)
    { for (unsigned x=0U;x<APP_DISPLAY_COLS;++x)
      { if (r->scene.tiles[y][x]==DISPLAY_TILE_STAR)
        { r->star_rows[n]=(uint8_t)y; r->star_cols[n]=(uint8_t)x; ++n; } } }
}

/* ---------------------- 2. MÀU CỦA MỘT ĐIỂM ẢNH ------------------------- */

/* Nền tại một pixel: tường trắng, đường đen, sao trắng, bẫy X,
 * cổng vuông đặc khi khóa và vuông rỗng khi mở. Chỉ tái hiện trạng thái GAME
 * gửi, không tự nhặt sao/trừ tim/mở cổng hay kiểm tra va chạm.
 *
 * (x, y) tính theo pixel TRONG bản đồ (0..119), chưa cộng offset màn hình.
 * Đây là "nguồn sự thật" về nền trông thế nào, dùng cả khi vẽ ô lẫn khi xóa
 * vết bi.
 */
static uint16_t prvBackground(const DisplayRender_t *r,int x,int y)
{
    /* Bitmap ngôi sao 7x7: mỗi byte là một hàng, bit cao ở bên trái. */
    static const uint8_t star[7]={0x08,0x08,0x7F,0x3E,0x1C,0x36,0x22};
    /* row/col: ô nào; px/py: điểm nào trong ô 10x10. */
    unsigned row=(unsigned)y/APP_DISPLAY_TILE_PX,col=(unsigned)x/APP_DISPLAY_TILE_PX;
    unsigned px=(unsigned)x%APP_DISPLAY_TILE_PX,py=(unsigned)y%APP_DISPLAY_TILE_PX;
    uint8_t tile=r->scene.tiles[row][col];
    if (tile==DISPLAY_TILE_WALL) { return WHITE; }
    if (tile==DISPLAY_TILE_GATE)
    {
        /* Hình vuông 6x6 (px, py từ 2 đến 7). Khóa: tô đặc. Mở: chỉ còn viền. */
        bool square=px>=2U && px<=7U && py>=2U && py<=7U;
        bool edge=px==2U || px==7U || py==2U || py==7U;
        return square && (r->scene.hud.gate_open==0U || edge)?WHITE:BLACK;
    }
    if (tile==DISPLAY_TILE_STAR)
    {
        /* Tìm sao này là sao thứ mấy; nếu bit tương ứng đã bật (đã nhặt) thì ô đen. */
        for (unsigned i=0U;i<r->scene.total_stars;++i)
        {
            if (r->star_rows[i]==row && r->star_cols[i]==col &&
                (r->scene.hud.collected_star_mask&(1U<<i))!=0U) { return BLACK; }
        }
        /* Chưa nhặt: vẽ ngôi sao từ bitmap trong vùng 7x7 (px, py từ 2 đến 8). */
        if (px>=2U && px<=8U && py>=2U && py<=8U && (star[py-2U]&(1U<<(8U-px)))!=0U) { return WHITE; }
    }
    /* Bẫy: hai đường chéo trong vùng 6x6 tạo thành chữ X. */
    if (tile==DISPLAY_TILE_TRAP && px>=2U && px<=7U && py>=2U && py<=7U &&
        (px==py || px+py==9U)) { return WHITE; }
    return BLACK;
}

/* ------------------------- 3. CÁC HÀM VẼ CỤ THỂ ------------------------- */

/* Vẽ lại một vùng chữ nhật của bản đồ (x0..x1, y0..y1, tọa độ trong bản đồ)
 * đúng theo nền. Mỗi hàng được gộp các điểm liền nhau cùng màu thành một đoạn
 * ngang rồi vẽ một lần, để giảm số lần gọi xuống LCD. */
static hal_st7735_status_t prvRegion(const DisplayRender_t *r,int x0,int y0,int x1,int y1)
{
    for (int y=y0;y<=y1;++y)
    {
        int x=x0;
        while (x<=x1)
        {
            int end=x;
            uint16_t color=prvBackground(r,x,y);
            /* Kéo dài đoạn chừng nào điểm kế tiếp còn cùng màu. */
            while (end<x1 && prvBackground(r,end+1,y)==color) { ++end; }
            DRAW(hal_st7735_draw_hline((int16_t)(APP_DISPLAY_MAP_X+x),(int16_t)(APP_DISPLAY_MAP_Y+y),
                                       (uint16_t)(end-x+1),color));
            x=end+1;
        }
    }
    return HAL_ST7735_OK;
}

/* Vẽ một ô (row, col). Tường / sàn / start chỉ là một khối đặc nên tô thẳng.
 * Sao / cổng / bẫy có hoa văn nên đi qua prvRegion. */
static hal_st7735_status_t prvTile(const DisplayRender_t *r,unsigned row,unsigned col)
{
    int x=(int)(col*APP_DISPLAY_TILE_PX),y=(int)(row*APP_DISPLAY_TILE_PX);
    uint8_t tile=r->scene.tiles[row][col];
    if (tile==DISPLAY_TILE_WALL || tile==DISPLAY_TILE_FLOOR || tile==DISPLAY_TILE_START)
    { return hal_st7735_fill_rect((int16_t)(APP_DISPLAY_MAP_X+x),(int16_t)(APP_DISPLAY_MAP_Y+y),
                                  APP_DISPLAY_TILE_PX,APP_DISPLAY_TILE_PX,tile==DISPLAY_TILE_WALL?WHITE:BLACK); }
    return prvRegion(r,x,y,x+(int)APP_DISPLAY_TILE_PX-1,y+(int)APP_DISPLAY_TILE_PX-1);
}

/* Vẽ bi: vòng tròn trắng bán kính 3, rồi chồng vòng tròn đen bán kính 2 lên
 * trên, còn lại một vòng viền trắng. */
static hal_st7735_status_t prvBall(const DisplayUpdate_t *u)
{
    int16_t x=(int16_t)(APP_DISPLAY_MAP_X+u->ball_x),y=(int16_t)(APP_DISPLAY_MAP_Y+u->ball_y);
    DRAW(hal_st7735_draw_circle_filled(x,y,APP_DISPLAY_BALL_RADIUS,WHITE));
    return hal_st7735_draw_circle_filled(x,y,APP_DISPLAY_BALL_RADIUS-1U,BLACK);
}

/* Dải trên cùng (y 0..24): "LEVELn" bên trái, "STARa/b" bên phải, một đường kẻ. */
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

/* Dải dưới cùng (y 145..159): đường kẻ, chữ "HP", rồi mỗi mạng một trái tim. */
static hal_st7735_status_t prvFooter(const DisplayUpdate_t *u)
{
    /* Bitmap trái tim 7x7, bit cao ở bên trái. */
    static const uint8_t heart[7]={0x36,0x7F,0x7F,0x3E,0x1C,0x08,0};
    DRAW(hal_st7735_fill_rect(0,145,128U,15U,BLACK));
    DRAW(hal_st7735_draw_hline(4,146,120U,WHITE));
    DRAW(hal_st7735_draw_text(4,150,"HP",WHITE,BLACK));
    /* Trái tim thứ i bắt đầu ở x = 25 + i*10; vẽ từng điểm sáng của bitmap. */
    for (unsigned i=0U;i<u->lives;++i)
    { for (unsigned y=0U;y<7U;++y)
      { for (unsigned x=0U;x<7U;++x)
        { if ((heart[y]&(1U<<(6U-x)))!=0U)
          { DRAW(hal_st7735_draw_pixel((int16_t)(25U+i*10U+x),(int16_t)(150U+y),WHITE)); } } } }
    return HAL_ST7735_OK;
}

/* Vẽ lại TOÀN BỘ: tô đen cả màn, tiêu đề, 144 ô, bi, chân trang. Chậm nhất. */
static hal_st7735_status_t prvFull(DisplayRender_t *r)
{
    DRAW(hal_st7735_fill_rect(0,0,128U,160U,BLACK));
    DRAW(prvHeader(&r->scene));
    for (unsigned y=0U;y<APP_DISPLAY_ROWS;++y)
    { for (unsigned x=0U;x<APP_DISPLAY_COLS;++x) { DRAW(prvTile(r,y,x)); } }
    DRAW(prvBall(&r->scene.hud));
    return prvFooter(&r->scene.hud);
}

/* Vẽ bảng thông báo đè lên vùng bản đồ: chọn chữ theo loại thông báo
 * (mặc định là RESTART / PLEASE WAIT), căn giữa theo chiều ngang. */
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
    DRAW(hal_st7735_fill_rect(4,25,120U,120U,BLACK));   /* xóa vùng bản đồ */
    DRAW(hal_st7735_draw_hline(8,65,112U,WHITE));       /* đường kẻ trên */
    DRAW(hal_st7735_draw_hline(8,100,112U,WHITE));      /* đường kẻ dưới */
    /* Căn giữa: x = (128 - số ký tự * 6) / 2, vì mỗi ký tự rộng 6 px. */
    DRAW(hal_st7735_draw_text((int16_t)((128U-strlen(title)*6U)/2U),77,title,WHITE,BLACK));
    return hal_st7735_draw_text((int16_t)((128U-strlen(subtitle)*6U)/2U),89,subtitle,WHITE,BLACK);
}

/* ---------------------- 4. QUYẾT ĐỊNH VẼ GÌ, KHI NÀO --------------------- */

/* Nếu có màn và màn hình đang lệch (dirty) thì vẽ lại toàn bộ (hoặc bảng thông
 * báo nếu đang overlay). Ngược lại không làm gì.
 * Task Display gọi hàm này khi hết thời gian chờ queue, nhờ đó tự sửa màn hình
 * sau một lần vẽ lỗi. */
hal_st7735_status_t task_display_render_refresh(DisplayRender_t *r)
{
    if (!r->have_scene || !r->dirty) { return HAL_ST7735_OK; }
    hal_st7735_status_t s=r->overlay?prvOverlay(r):prvFull(r);
    if (s==HAL_ST7735_OK)
    {
        /* Vẽ xong: hết lệch; chụp lại trạng thái đã vẽ. Khi đang hiện bảng
         * thông báo thì màn hình không phải bản đồ nên painted_valid = false. */
        r->dirty=false; r->painted_valid=!r->overlay; r->painted=r->scene.hud;
    }
    return s;
}

/* Vẽ lại các ô thay đổi: những sao vừa được nhặt (so mặt nạ cũ và mới bằng XOR)
 * và ô cổng nếu trạng thái mở/khóa đổi. */
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

/* Xử lý một message từ queue. Đọc từ trên xuống, gặp nhánh nào khớp thì dừng. */
hal_st7735_status_t task_display_render_apply(DisplayRender_t *r,const DisplayMsg_t *m)
{
    /* (1) Message không hợp lệ: không vẽ gì. */
    if (!task_display_render_validate(m)) { return HAL_ST7735_INVALID; }

    /* (2) SCENE: màn mới. Lưu toàn bộ rồi vẽ lại. */
    if (m->type==DISPLAY_MSG_SCENE)
    {
        /* SCENE do một producer gửi FIFO. ID là token so khớp, hỗ trợ wrap. */
        r->scene=m->data.scene; r->scene_id=m->scene_id; r->have_scene=true;
        prvIndexStars(r);
        /* Đang hiện thông báo thì chưa đụng màn hình; chờ RESUME mới vẽ. */
        if (!r->overlay) { task_display_render_invalidate(r); }
        return task_display_render_refresh(r);
    }

    /* (3) Chưa có màn, hoặc message thuộc màn cũ (scene_id khác): bỏ qua. */
    if (!r->have_scene || m->scene_id!=r->scene_id) { return HAL_ST7735_OK; }

    /* (4) NOTICE: bật bảng thông báo và vẽ ngay. */
    if (m->type==DISPLAY_MSG_NOTICE)
    { r->notice=m->data.notice; r->overlay=true; task_display_render_invalidate(r); return task_display_render_refresh(r); }

    /* (5) RESUME: tắt bảng thông báo, vẽ lại toàn bộ bản đồ. */
    if (m->type==DISPLAY_MSG_RESUME)
    { r->overlay=false; task_display_render_invalidate(r); return task_display_render_refresh(r); }

    /* (6) Còn lại là UPDATE. Thông số phải khớp màn hiện tại. */
    if (!prvMatchesScene(&m->data.update,r->scene.total_stars)) { return HAL_ST7735_INVALID; }
    r->scene.hud=m->data.update;                 /* luôn lưu trạng thái mới nhất */

    /* Đang hiện thông báo: chỉ lưu, không đổi hình (refresh không làm gì vì không dirty). */
    if (r->overlay) { return task_display_render_refresh(r); }
    /* Màn hình chưa đáng tin: vẽ lại toàn bộ thay vì cập nhật từng phần. */
    if (r->dirty || !r->painted_valid) { return task_display_render_refresh(r); }

    /* --- Cập nhật TỪNG PHẦN: so thông số mới với cái đã vẽ (painted) --- */
    bool moved=r->painted.ball_x!=r->scene.hud.ball_x || r->painted.ball_y!=r->scene.hud.ball_y;
    bool tiles_changed=r->painted.collected_star_mask!=r->scene.hud.collected_star_mask ||
                       r->painted.gate_open!=r->scene.hud.gate_open;
    hal_st7735_status_t s=HAL_ST7735_OK;
    if (moved || tiles_changed)
    {
        /* Bước 1: xóa vết bi cũ - vẽ lại vùng 7x7 quanh vị trí cũ theo nền. */
        int x=r->painted.ball_x,y=r->painted.ball_y,radius=APP_DISPLAY_BALL_RADIUS;
        s=prvRegion(r,x-radius,y-radius,x+radius,y+radius);
        /* Bước 2: vẽ lại sao vừa nhặt / cổng vừa đổi. */
        if (s==HAL_ST7735_OK && tiles_changed) { s=prvChangedTiles(r); }
        /* Vẽ bi sau tile để sao/cổng vừa cập nhật không đè bi đang đứng yên. */
        /* Bước 3: vẽ bi ở vị trí mới. */
        if (s==HAL_ST7735_OK) { s=prvBall(&r->scene.hud); }
    }
    /* Tiêu đề chỉ vẽ lại khi số sao đổi; chân trang chỉ khi số tim đổi. */
    if (s==HAL_ST7735_OK && r->painted.collected_stars!=r->scene.hud.collected_stars) { s=prvHeader(&r->scene); }
    if (s==HAL_ST7735_OK && r->painted.lives!=r->scene.hud.lives) { s=prvFooter(&r->scene.hud); }
    /* Lỗi giữa chừng: màn hình có thể đang vẽ dở, nên đánh dấu vẽ lại toàn bộ.
     * Thành công: ghi nhớ trạng thái vừa vẽ. */
    if (s!=HAL_ST7735_OK) { task_display_render_invalidate(r); }
    else { r->painted=r->scene.hud; }
    return s;
}
