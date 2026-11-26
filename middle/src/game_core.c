#include "game_core.h"
#include <stddef.h>
#include <string.h>

_Static_assert(STAR_MAZE_NUM_LEVELS == 5U, "Game requires five maps");
_Static_assert(GAME_X_SIGN == 1 || GAME_X_SIGN == -1, "Invalid X sign");
_Static_assert(GAME_Y_SIGN == 1 || GAME_Y_SIGN == -1, "Invalid Y sign");
_Static_assert(GAME_TILT_BOOST_START_MG > 0 &&
               GAME_TILT_INPUT_LIMIT_MG >= GAME_TILT_BOOST_START_MG, "Invalid tilt curve");

static int32_t prvAbs(int32_t value)
{ return value < 0 ? -value : value; }

const uint8_t (*game_core_map(const Game_t *game))[STAR_MAZE_COLS]
{ return star_maze_levels[game->level - 1U]; }

bool game_core_gate_open(const Game_t *game)
{ return game->collected == game->total_stars; }

uint8_t game_core_ball_x(const Game_t *game)
{ return (uint8_t)((game->x_q + GAME_Q / 2) / GAME_Q); }
uint8_t game_core_ball_y(const Game_t *game)
{ return (uint8_t)((game->y_q + GAME_Q / 2) / GAME_Q); }

void game_core_stop(Game_t *game)
{
    game->vx_q = 0; game->vy_q = 0;
    game->filtered_ax = 0; game->filtered_ay = 0;
}

static void prvRespawn(Game_t *game)
{
    game->x_q = ((int32_t)game->start_col * (int32_t)STAR_MAZE_CELL_PX + 5) * GAME_Q;
    game->y_q = ((int32_t)game->start_row * (int32_t)STAR_MAZE_CELL_PX + 5) * GAME_Q;
    game_core_stop(game);
}

static void prvLoadLevel(Game_t *game, uint8_t level)
{
    game->level = level;
    ++game->scene_id;
    if (game->scene_id == 0U) { game->scene_id = 1U; }
    game->collected_mask = 0U; game->collected = 0U; game->total_stars = 0U;
    const uint8_t (*map)[STAR_MAZE_COLS] = game_core_map(game);
    for (unsigned row = 0U; row < STAR_MAZE_ROWS; ++row) {
        for (unsigned col = 0U; col < STAR_MAZE_COLS; ++col) {
            if (map[row][col] == TILE_START) {
                game->start_row = (uint8_t)row; game->start_col = (uint8_t)col;
            }
            if (map[row][col] == TILE_STAR) { ++game->total_stars; }
        }
    }
    prvRespawn(game);
}

void game_core_init(Game_t *game, uint32_t now_ms)
{
    memset(game, 0, sizeof(*game));
    game->lives = 3U; game->state_since_ms = now_ms;
    prvLoadLevel(game, 1U); game->state = GAME_SPLASH;
}

void game_core_restart(Game_t *game, uint32_t now_ms)
{
    game->lives = 3U; prvLoadLevel(game, 1U);
    game->state = GAME_RESTART_NOTICE; game->state_since_ms = now_ms;
}

void game_core_next_level(Game_t *game, uint32_t now_ms)
{
    if (game->lives == 0U) { game->lives = 3U; }
    uint8_t next = game->level == 5U ? 1U : (uint8_t)(game->level + 1U);
    prvLoadLevel(game, next); game->state = GAME_LEVEL_NOTICE;
    game->state_since_ms = now_ms;
}

/* Circle versus closed rectangle. Coordinates are Q8. Tangency is contact. */
static bool prvRectContact(int32_t x, int32_t y, int left, int top, int size)
{
    int32_t x0 = left * GAME_Q, y0 = top * GAME_Q;
    int32_t x1 = (left + size) * GAME_Q, y1 = (top + size) * GAME_Q;
    int32_t dx = x < x0 ? x0 - x : (x > x1 ? x - x1 : 0);
    int32_t dy = y < y0 ? y0 - y : (y > y1 ? y - y1 : 0);
    const int32_t radius = GAME_BALL_RADIUS_PX * GAME_Q;
    /* Far rectangles are discarded before squaring, avoiding overflow. */
    if (dx > radius || dy > radius) { return false; }
    return dx * dx + dy * dy <= radius * radius;
}

static bool prvSolid(const Game_t *game, int32_t x, int32_t y)
{
    const int32_t radius = GAME_BALL_RADIUS_PX * GAME_Q;
    const int32_t size = STAR_MAZE_COLS * STAR_MAZE_CELL_PX * GAME_Q;
    if (x <= radius || y <= radius || x >= size - radius || y >= size - radius) {
        return true;
    }
    const uint8_t (*map)[STAR_MAZE_COLS] = game_core_map(game);
    for (unsigned row = 0U; row < STAR_MAZE_ROWS; ++row) {
        for (unsigned col = 0U; col < STAR_MAZE_COLS; ++col) {
            int left = (int)col * (int)STAR_MAZE_CELL_PX;
            int top = (int)row * (int)STAR_MAZE_CELL_PX;
            if (map[row][col] == TILE_WALL && prvRectContact(x, y, left, top, 10)) {
                return true;
            }
            if (map[row][col] == TILE_GATE && !game_core_gate_open(game) &&
                prvRectContact(x, y, left + 2, top + 2, 6)) { return true; }
        }
    }
    return false;
}

static bool prvTrapContact(const Game_t *game, int left, int top)
{
    for (int i = 2; i <= 7; ++i) {
        if (prvRectContact(game->x_q, game->y_q, left + i, top + i, 1) ||
            prvRectContact(game->x_q, game->y_q, left + i, top + 9 - i, 1)) {
            return true;
        }
    }
    return false;
}

static bool prvStarContact(const Game_t *game, int left, int top)
{
    static const uint8_t pixels[7] = {0x08,0x08,0x7F,0x3E,0x1C,0x36,0x22};
    for (unsigned y = 0U; y < 7U; ++y) {
        for (unsigned x = 0U; x < 7U; ++x) {
            if ((pixels[y] & (1U << (6U - x))) != 0U &&
                prvRectContact(game->x_q, game->y_q, left + 2 + (int)x,
                               top + 2 + (int)y, 1)) { return true; }
        }
    }
    return false;
}

static void prvContact(Game_t *game, uint32_t now_ms)
{
    const uint8_t (*map)[STAR_MAZE_COLS] = game_core_map(game);
    /* A trap has precedence over a simultaneous pickup. Never subtract twice. */
    for (unsigned row = 0U; row < STAR_MAZE_ROWS; ++row) {
        for (unsigned col = 0U; col < STAR_MAZE_COLS; ++col) {
            if (map[row][col] == TILE_TRAP &&
                prvTrapContact(game, (int)col * 10, (int)row * 10)) {
                if (game->lives != 0U) { --game->lives; }
                prvRespawn(game);
                game->state = game->lives == 0U ? GAME_OVER : GAME_TRAP_NOTICE;
                game->state_since_ms = now_ms; return;
            }
        }
    }
    unsigned star_index = 0U;
    for (unsigned row = 0U; row < STAR_MAZE_ROWS; ++row) {
        for (unsigned col = 0U; col < STAR_MAZE_COLS; ++col) {
            if (map[row][col] != TILE_STAR) { continue; }
            uint16_t bit = (uint16_t)(1U << star_index);
            ++star_index;
            if ((game->collected_mask & bit) == 0U &&
                prvStarContact(game, (int)col * 10, (int)row * 10)) {
                game->collected_mask |= bit; ++game->collected;
            }
        }
    }
    if (!game_core_gate_open(game)) { return; }
    for (unsigned row = 0U; row < STAR_MAZE_ROWS; ++row) {
        for (unsigned col = 0U; col < STAR_MAZE_COLS; ++col) {
            if (map[row][col] == TILE_GATE &&
                prvRectContact(game->x_q, game->y_q, (int)col * 10 + 2,
                               (int)row * 10 + 2, 6)) {
                if (game->level == 5U) { game->state = GAME_WON; }
                else {
                    prvLoadLevel(game, (uint8_t)(game->level + 1U));
                    game->state = GAME_LEVEL_NOTICE;
                }
                game->state_since_ms = now_ms; game_core_stop(game); return;
            }
        }
    }
}

static int32_t prvDeadzone(int32_t value)
{
    if (value > GAME_DEADZONE_MG) { return value - GAME_DEADZONE_MG; }
    if (value < -GAME_DEADZONE_MG) { return value + GAME_DEADZONE_MG; }
    return 0;
}

static int32_t prvTiltDrive(int32_t value)
{
    int32_t drive = prvDeadzone(value);
    int32_t magnitude = prvAbs(drive);
    if (magnitude > GAME_TILT_INPUT_LIMIT_MG) { magnitude = GAME_TILT_INPUT_LIMIT_MG; }
    if (magnitude > GAME_TILT_BOOST_START_MG) {
        magnitude += (magnitude - GAME_TILT_BOOST_START_MG) / 2;
    }
    return drive < 0 ? -magnitude : magnitude;
}

static bool prvAdvanceNotice(Game_t *game, uint32_t now_ms)
{
    uint32_t elapsed = now_ms - game->state_since_ms;
    switch (game->state) {
    case GAME_SPLASH:
        if (elapsed >= GAME_SPLASH_MS) {
            game->state = GAME_LEVEL_NOTICE; game->state_since_ms = now_ms;
        }
        break;
    case GAME_TRAP_NOTICE:
        if (elapsed >= GAME_TRAP_NOTICE_MS) { game->state = GAME_PLAYING; }
        break;
    case GAME_RESTART_NOTICE:
        if (elapsed >= GAME_RESTART_NOTICE_MS) { game->state = GAME_PLAYING; }
        break;
    case GAME_LEVEL_NOTICE:
        if (elapsed >= GAME_LEVEL_NOTICE_MS) {
            game->state = GAME_PLAYING;
        }
        break;
    case GAME_OVER:
    case GAME_WON:
    case GAME_PLAYING:
    default: break;
    }
    /* A transition frame never moves the ball immediately after respawn. */
    return game->state == GAME_PLAYING;
}

void game_core_step(Game_t *game, int16_t ax_mg, int16_t ay_mg, uint32_t now_ms)
{
    if (game->state != GAME_PLAYING) { (void)prvAdvanceNotice(game, now_ms); return; }
    int32_t ax = ax_mg, ay = ay_mg;
#if GAME_SWAP_AXES
    int32_t swap = ax; ax = ay; ay = swap;
#endif
    ax *= GAME_X_SIGN; ay *= GAME_Y_SIGN;
    game->filtered_ax += (ax - game->filtered_ax) / 4;
    game->filtered_ay += (ay - game->filtered_ay) / 4;
    ax = prvTiltDrive(game->filtered_ax); ay = prvTiltDrive(game->filtered_ay);
    /* a = 0.3 * shaped tilt drive px/s^2; friction = 0.92 per 20 ms. */
    game->vx_q = game->vx_q * 92 / 100 + ax * GAME_Q * 3 * (int32_t)GAME_STEP_MS / 10000;
    game->vy_q = game->vy_q * 92 / 100 + ay * GAME_Q * 3 * (int32_t)GAME_STEP_MS / 10000;
    if (ax == 0 && prvAbs(game->vx_q) < GAME_Q) { game->vx_q = 0; }
    if (ay == 0 && prvAbs(game->vy_q) < GAME_Q) { game->vy_q = 0; }
    /* L1 bound also keeps Euclidean speed within the configured limit. */
    int32_t speed = prvAbs(game->vx_q) + prvAbs(game->vy_q);
    const int32_t limit = GAME_MAX_SPEED_PX_S * GAME_Q;
    if (speed > limit) {
        game->vx_q = game->vx_q * limit / speed;
        game->vy_q = game->vy_q * limit / speed;
    }
    int32_t dx = game->vx_q * (int32_t)GAME_STEP_MS / 1000;
    int32_t dy = game->vy_q * (int32_t)GAME_STEP_MS / 1000;
    int32_t longest = prvAbs(dx) > prvAbs(dy) ? prvAbs(dx) : prvAbs(dy);
    int32_t steps = longest / (GAME_Q / 2) + 1;
    for (int32_t i = 1; i <= steps; ++i) {
        int32_t sx = dx * i / steps - dx * (i - 1) / steps;
        int32_t sy = dy * i / steps - dy * (i - 1) / steps;
        if (!prvSolid(game, game->x_q + sx, game->y_q)) { game->x_q += sx; }
        else { game->vx_q = 0; }
        prvContact(game, now_ms);
        if (game->state != GAME_PLAYING) { return; }
        if (!prvSolid(game, game->x_q, game->y_q + sy)) { game->y_q += sy; }
        else { game->vy_q = 0; }
        prvContact(game, now_ms);
        if (game->state != GAME_PLAYING) { return; }
    }
}
