#include "game_core.h"
#include "task_button.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* Pure C tests: no GPIO, display, BMI or FreeRTOS scheduler on the host. */
static unsigned checks, frames;
static uint32_t now_ms;
static int32_t tilt_speeds_q[4];
#define REQUIRE(expr) do { ++checks; if (!(expr)) { \
    fprintf(stderr, "TEST FAILED line %d: %s\n", __LINE__, #expr); exit(1); } } while (0)

static void frame(Game_t *game, int16_t ax, int16_t ay)
{
    /* Rule/path fixtures express desired SCREEN acceleration. Convert to raw
     * sensor input so they also exercise the configured mounting in the core. */
    int32_t sx = (int32_t)ax * GAME_X_SIGN, sy = (int32_t)ay * GAME_Y_SIGN;
    if (sx > INT16_MAX) { sx = INT16_MAX; }
    if (sx < INT16_MIN) { sx = INT16_MIN; }
    if (sy > INT16_MAX) { sy = INT16_MAX; }
    if (sy < INT16_MIN) { sy = INT16_MIN; }
    int16_t raw_x = (int16_t)sx, raw_y = (int16_t)sy;
#if GAME_SWAP_AXES
    int16_t swap = raw_x; raw_x = raw_y; raw_y = swap;
#endif
    now_ms += GAME_STEP_MS;
    game_core_step(game, raw_x, raw_y, now_ms); ++frames;
}

static void pose(Game_t *game, int x, int y)
{ game->x_q = x * GAME_Q; game->y_q = y * GAME_Q; game_core_stop(game); }

static void init_playing(Game_t *game)
{
    game_core_init(game, now_ms);
    now_ms += GAME_SPLASH_MS; game_core_step(game, 0, 0, now_ms);
    now_ms += GAME_LEVEL_NOTICE_MS; game_core_step(game, 0, 0, now_ms);
    REQUIRE(game->state == GAME_PLAYING && game->level == 1U);
}

static void test_installed_sensor_orientation(void)
{
    /* Latest board feedback: vertical correct; reverse only horizontal.
     * Mounting inferred: lower right -> raw +Y, lower top -> raw +X.
     * Expected SCREEN directions
     * are literal here, independent of the mapping macros used by fixtures. */
    const int16_t raw[4][2] = {{0, 600}, {600, 0}, {0, -600}, {-600, 0}};
    const int expected[4][2] = {{1, 0}, {0, -1}, {-1, 0}, {0, 1}};
    for (unsigned i = 0U; i < 4U; ++i) {
        Game_t game; init_playing(&game); pose(&game, 55, 55);
        now_ms += GAME_STEP_MS;
        game_core_step(&game, raw[i][0], raw[i][1], now_ms); ++frames;
        int dx = (game.x_q > 55 * GAME_Q) - (game.x_q < 55 * GAME_Q);
        int dy = (game.y_q > 55 * GAME_Q) - (game.y_q < 55 * GAME_Q);
        REQUIRE(dx == expected[i][0]); REQUIRE(dy == expected[i][1]);
        REQUIRE(game.state == GAME_PLAYING && game.lives == 3U);
    }
}

static void test_tilt_strength(void)
{
    const int16_t tilts[4] = {100, 300, 600, 1000};
    for (unsigned axis = 0U; axis < 2U; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            int32_t previous_speed = 0, previous_distance = 0;
            for (unsigned strength = 0U; strength < 4U; ++strength) {
                Game_t game; init_playing(&game);
                int start_x = axis == 0U && sign < 0 ? 105 : 15;
                int start_y = axis == 1U && sign < 0 ? 105 : 15;
                pose(&game, start_x, start_y);
                for (unsigned i = 0U; i < 40U; ++i) {
                    frame(&game, axis == 0U ? sign * tilts[strength] : 0,
                                 axis == 1U ? sign * tilts[strength] : 0);
                    REQUIRE(abs(game.vx_q) + abs(game.vy_q) <= GAME_MAX_SPEED_PX_S * GAME_Q);
                }
                int32_t speed = abs(axis == 0U ? game.vx_q : game.vy_q);
                int32_t distance = axis == 0U ? abs(game.x_q - start_x * GAME_Q) :
                                                       abs(game.y_q - start_y * GAME_Q);
                REQUIRE(game.state == GAME_PLAYING && game.lives == 3U);
                REQUIRE(speed > previous_speed && distance > previous_distance);
                if (strength == 0U) { REQUIRE(speed >= 2 * GAME_Q && speed <= 6 * GAME_Q); }
                if (strength == 2U) { REQUIRE(speed > 40 * GAME_Q); }
                if (strength == 3U) { REQUIRE(speed >= 55 * GAME_Q); }
                previous_speed = speed; previous_distance = distance;
                if (axis == 0U && sign > 0) { tilt_speeds_q[strength] = speed; }
                /* Returning level must stop the ball without changing direction. */
                for (unsigned i = 0U; i < 60U; ++i) { frame(&game, 0, 0); }
                REQUIRE(game.vx_q == 0 && game.vy_q == 0);
                REQUIRE(game.state == GAME_PLAYING && game.lives == 3U);
            }
        }
    }
    Game_t game; init_playing(&game);
    for (unsigned i = 0U; i < 100U; ++i) { frame(&game, 30, -30); }
    REQUIRE(game.x_q == 15 * GAME_Q && game.y_q == 15 * GAME_Q);
    /* Full diagonal input is capped too, even for extreme sensor values. */
    for (unsigned i = 0U; i < 100U; ++i) {
        game.x_q = 55 * GAME_Q; game.y_q = 55 * GAME_Q;
        frame(&game, INT16_MAX, INT16_MIN);
        REQUIRE(abs(game.vx_q) + abs(game.vy_q) <= GAME_MAX_SPEED_PX_S * GAME_Q);
    }
    /* Fast downward motion must stop before the one-cell wall at row 6. */
    pose(&game, 55, 55);
    for (unsigned i = 0U; i < 200U; ++i) { frame(&game, 0, INT16_MAX); }
    REQUIRE(game.y_q > 56 * GAME_Q && game.y_q < 57 * GAME_Q);
    REQUIRE(game.vy_q == 0);
    /* Crossing a trap at high tilt still costs exactly one heart and respawns. */
    game_core_next_level(&game, now_ms);
    now_ms += GAME_LEVEL_NOTICE_MS; game_core_step(&game, 0, 0, now_ms); pose(&game, 65, 65);
    for (unsigned i = 0U; i < 100U && game.state == GAME_PLAYING; ++i) {
        frame(&game, 1000, 0);
    }
    REQUIRE(game.state == GAME_TRAP_NOTICE && game.lives == 2U);
    REQUIRE(game.x_q == 15 * GAME_Q && game.y_q == 15 * GAME_Q);
    REQUIRE(game.vx_q == 0 && game.vy_q == 0);
}

static void collect_all_fixture(Game_t *game)
{
    for (unsigned row = 0U; row < 12U; ++row) {
        for (unsigned col = 0U; col < 12U; ++col) {
            if (game_core_map(game)[row][col] == TILE_STAR) {
                pose(game, (int)col * 10 + 5, (int)row * 10 + 5); frame(game, 0, 0);
            }
        }
    }
    REQUIRE(game->collected == game->total_stars);
}

static void touch_gate_fixture(Game_t *game)
{
    for (unsigned row = 0U; row < 12U; ++row) {
        for (unsigned col = 0U; col < 12U; ++col) {
            if (game_core_map(game)[row][col] == TILE_GATE) {
                pose(game, (int)col * 10 + 5, (int)row * 10 + 5); frame(game, 0, 0); return;
            }
        }
    }
    REQUIRE(false);
}

static void test_rules(void)
{
    Game_t game;
    init_playing(&game);
    REQUIRE(game.level == 1U && game.lives == 3U && game.total_stars == 2U);
    REQUIRE(game_core_ball_x(&game) == 15U && game_core_ball_y(&game) == 15U);
    REQUIRE(!game_core_gate_open(&game));
    for (unsigned i = 0U; i < 700U; ++i) { frame(&game, INT16_MAX, 0); }
    REQUIRE(game.x_q < 107 * GAME_Q && game.x_q > 106 * GAME_Q);
    REQUIRE(game.vx_q <= GAME_MAX_SPEED_PX_S * GAME_Q);
    pose(&game, 15, 15);
    for (unsigned i = 0U; i < 500U; ++i) { frame(&game, INT16_MIN, 0); }
    REQUIRE(game.x_q > 13 * GAME_Q && game.x_q < 14 * GAME_Q);
    /* Closed gate blocks a circle before the white gate square. */
    pose(&game, 95, 105);
    for (unsigned i = 0U; i < 300U; ++i) { frame(&game, 1200, 0); }
    REQUIRE(game.level == 1U && game.state == GAME_PLAYING);
    REQUIRE(game.x_q < 99 * GAME_Q && game.x_q > 98 * GAME_Q);
    pose(&game, 25, 25); frame(&game, 0, 0);
    REQUIRE(game.collected == 1U && game.collected_mask == 1U);
    for (unsigned i = 0U; i < 40U; ++i) { frame(&game, 0, 0); }
    REQUIRE(game.collected == 1U);
    pose(&game, 85, 65); frame(&game, 0, 0);
    REQUIRE(game_core_gate_open(&game) && game.collected_mask == 3U);
    REQUIRE(game.level == 1U && game.state == GAME_PLAYING);

    pose(&game, 95, 105);
    for (unsigned i = 0U; i < 100U && game.state == GAME_PLAYING; ++i) { frame(&game, 500, 0); }
    REQUIRE(game.state == GAME_LEVEL_NOTICE);
    now_ms = game.state_since_ms + GAME_LEVEL_NOTICE_MS;
    game_core_step(&game, 0, 0, now_ms);
    REQUIRE(game.level == 2U && game.lives == 3U && game.collected == 0U);

    pose(&game, 25, 25); frame(&game, 0, 0);
    uint16_t saved = game.collected_mask;
    pose(&game, 85, 65); frame(&game, 0, 0);
    REQUIRE(game.lives == 2U && game.state == GAME_TRAP_NOTICE);
    REQUIRE(game.collected_mask == saved && game.collected == 1U);
    REQUIRE(game.x_q == 15 * GAME_Q && game.y_q == 15 * GAME_Q);
    REQUIRE(game.vx_q == 0 && game.vy_q == 0);

    for (unsigned i = 0U; i < 20U; ++i) { frame(&game, 2000, 2000); }
    REQUIRE(game.lives == 2U && game.state == GAME_TRAP_NOTICE);
    now_ms = game.state_since_ms + GAME_TRAP_NOTICE_MS;
    game_core_step(&game, 1000, 1000, now_ms);
    REQUIRE(game.state == GAME_PLAYING && game.collected_mask == saved);
    REQUIRE(game.x_q == 15 * GAME_Q);
    collect_all_fixture(&game);
    pose(&game, 85, 65); frame(&game, 0, 0);
    REQUIRE(game.lives == 1U && game_core_gate_open(&game));
    now_ms = game.state_since_ms + GAME_TRAP_NOTICE_MS; game_core_step(&game, 0, 0, now_ms);
    pose(&game, 85, 65); frame(&game, 0, 0);
    REQUIRE(game.lives == 0U && game.state == GAME_OVER);

    for (unsigned i = 0U; i < 100U; ++i) { frame(&game, 1000, 1000); }
    REQUIRE(game.lives == 0U && game.state == GAME_OVER);
    game_core_restart(&game, now_ms);
    REQUIRE(game.level == 1U && game.lives == 3U && game.collected_mask == 0U);
    REQUIRE(game.state == GAME_RESTART_NOTICE);
    now_ms += GAME_RESTART_NOTICE_MS; game_core_step(&game, 0, 0, now_ms);

    /* Completing every gate carries lives and reaches an indefinite YOU WIN. */
    game.lives = 2U;
    for (unsigned level = 1U; level <= 5U; ++level) {
        REQUIRE(game.level == level && game.lives == 2U);
        collect_all_fixture(&game); touch_gate_fixture(&game);
        if (level < 5U) {
            REQUIRE(game.state == GAME_LEVEL_NOTICE);
            now_ms += GAME_LEVEL_NOTICE_MS; game_core_step(&game, 0, 0, now_ms);
        }
    }
    REQUIRE(game.state == GAME_WON);
    now_ms += 60000U; game_core_step(&game, 2000, 2000, now_ms);
    REQUIRE(game.state == GAME_WON && game.level == 5U);
    game_core_restart(&game, now_ms);
    REQUIRE(game.level == 1U && game.lives == 3U && game.collected == 0U && game.state == GAME_RESTART_NOTICE);
}

static void test_manual_priority(void)
{
    Game_t game; init_playing(&game);
    game.lives = 2U;
    for (unsigned level = 2U; level <= 5U; ++level) {
        REQUIRE(game.collected == 0U && !game_core_gate_open(&game));
        uint32_t scene_id = game.scene_id;
        game_core_next_level(&game, now_ms);
        REQUIRE(game.level == level && game.lives == 2U && game.state == GAME_LEVEL_NOTICE);
        REQUIRE(game.collected_mask == 0U && game.scene_id != scene_id);
        REQUIRE(game.vx_q == 0 && game.vy_q == 0);

    }
    game_core_next_level(&game, now_ms); REQUIRE(game.level == 1U && game.lives == 2U);
    game.state = GAME_TRAP_NOTICE; game_core_next_level(&game, now_ms);
    REQUIRE(game.level == 2U && game.state == GAME_LEVEL_NOTICE);
    game.state = GAME_OVER; game.lives = 0U;
    game_core_next_level(&game, now_ms); REQUIRE(game.level == 3U && game.lives == 3U);
    game.state = GAME_WON; game.level = 5U;
    game_core_next_level(&game, now_ms); REQUIRE(game.level == 1U);
}

static void test_button(void)
{
    ButtonGesture_t button;
    button_gesture_init(&button, false, 0U);
    REQUIRE(button_gesture_update(&button, true, 5U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 10U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 15U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 45U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 100U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 105U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 110U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 140U) == BUTTON_EVENT_NEXT_LEVEL);
    REQUIRE(button_gesture_update(&button, false, 200U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 205U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 235U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 1234U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 1235U) == BUTTON_EVENT_RESTART);
    REQUIRE(button_gesture_update(&button, true, 10000U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 10005U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 10035U) == BUTTON_EVENT_NONE);
    /* Holding at boot produces no command until release and a new press. */
    button_gesture_init(&button, true, 0U);
    REQUIRE(button_gesture_update(&button, true, 5000U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 5005U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, false, 5035U) == BUTTON_EVENT_NONE);
    /* Unsigned elapsed-time calculations survive the millisecond wrap. */
    button_gesture_init(&button, false, UINT32_MAX - 100U);
    REQUIRE(button_gesture_update(&button, true, UINT32_MAX - 80U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, UINT32_MAX - 50U) == BUTTON_EVENT_NONE);
    REQUIRE(button_gesture_update(&button, true, 949U) == BUTTON_EVENT_RESTART);
    Game_t game; game_core_init(&game, UINT32_MAX - 20U);
    game_core_restart(&game, UINT32_MAX - 20U);
    game_core_step(&game, 0, 0, 978U); REQUIRE(game.state == GAME_RESTART_NOTICE);
    game_core_step(&game, 0, 0, 979U); REQUIRE(game.state == GAME_PLAYING);
}

static unsigned route(const Game_t *game, unsigned target, unsigned *path)
{
    int previous[144]; unsigned queue[144], head = 0U, tail = 0U;
    for (unsigned i = 0U; i < 144U; ++i) { previous[i] = -1; }
    unsigned start = (unsigned)(game->y_q / (10 * GAME_Q)) * 12U +
                     (unsigned)(game->x_q / (10 * GAME_Q));
    queue[tail++] = start; previous[start] = (int)start;
    while (head < tail && previous[target] == -1) {
        unsigned cell = queue[head++];
        const int delta[4] = {-12, 1, 12, -1};
        for (unsigned i = 0U; i < 4U; ++i) {
            int next = (int)cell + delta[i];
            if (next < 0 || next >= 144 || previous[next] != -1) { continue; }
            if ((i == 1U || i == 3U) && (unsigned)next / 12U != cell / 12U) { continue; }
            uint8_t tile = game_core_map(game)[(unsigned)next / 12U][(unsigned)next % 12U];
            if (tile == TILE_WALL || tile == TILE_TRAP ||
                (tile == TILE_GATE && !game_core_gate_open(game))) { continue; }
            previous[next] = (int)cell; queue[tail++] = (unsigned)next;
        }
    }
    REQUIRE(previous[target] != -1);
    unsigned reversed[144], length = 0U;
    for (unsigned p = target; p != start; p = (unsigned)previous[p]) { reversed[length++] = p; }
    for (unsigned i = 0U; i < length; ++i) { path[i] = reversed[length - 1U - i]; }
    return length;
}

static int16_t control(int32_t error_q, int32_t velocity_q)
{
    int32_t value = (error_q * 100 - velocity_q * 20) / GAME_Q;
    if (value > 650) { value = 650; }
    if (value < -650) { value = -650; }
    return (int16_t)value;
}

static void drive(Game_t *game, unsigned target)
{
    unsigned path[144]; unsigned length = route(game, target, path);
    for (unsigned i = 0U; i < length; ++i) {
        int32_t x = ((int32_t)(path[i] % 12U) * 10 + 5) * GAME_Q;
        int32_t y = ((int32_t)(path[i] / 12U) * 10 + 5) * GAME_Q;
        unsigned attempts;
        for (attempts = 0U; attempts < 600U; ++attempts) {
            int32_t ex = x - game->x_q, ey = y - game->y_q;
            if (abs(ex) < GAME_Q / 2 && abs(ey) < GAME_Q / 2 &&
                abs(game->vx_q) < GAME_Q && abs(game->vy_q) < GAME_Q) { break; }
            frame(game, control(ex, game->vx_q), control(ey, game->vy_q));
            REQUIRE(game->lives == 3U);
            if (game->state != GAME_PLAYING) { return; }
        }
        if (attempts >= 600U) {
            fprintf(stderr, "Route stuck: level=%u target=%u xy=%.2f,%.2f\n", game->level,
                    path[i], game->x_q / 256.0, game->y_q / 256.0);
        }
        REQUIRE(attempts < 600U);
    }
}

static void test_tilt_only_solution(void)
{
    Game_t game; init_playing(&game);
    for (unsigned level = 1U; level <= 5U; ++level) {
        REQUIRE(game.level == level);
        for (unsigned cell = 0U; cell < 144U; ++cell) {
            if (game_core_map(&game)[cell / 12U][cell % 12U] == TILE_STAR) { drive(&game, cell); }
        }
        REQUIRE(game_core_gate_open(&game));
        for (unsigned cell = 0U; cell < 144U; ++cell) {
            if (game_core_map(&game)[cell / 12U][cell % 12U] == TILE_GATE) { drive(&game, cell); }
        }
        REQUIRE(game.lives == 3U);
        REQUIRE(game.state == (level == 5U ? GAME_WON : GAME_LEVEL_NOTICE));
        if (level < 5U) { now_ms += GAME_LEVEL_NOTICE_MS; game_core_step(&game, 0, 0, now_ms); }
    }
    REQUIRE(game.state == GAME_WON && game.lives == 3U);
}

static void test_boot_and_restart(void)
{
    Game_t game; game_core_init(&game, now_ms);
    REQUIRE(game.state == GAME_SPLASH);
    now_ms += GAME_SPLASH_MS - 1U; game_core_step(&game, 1000, 1000, now_ms);
    REQUIRE(game.state == GAME_SPLASH && game.x_q == 15 * GAME_Q);
    ++now_ms; game_core_step(&game, 0, 0, now_ms);
    REQUIRE(game.state == GAME_LEVEL_NOTICE);
    now_ms += GAME_LEVEL_NOTICE_MS - 1U; game_core_step(&game, 1000, 1000, now_ms);
    REQUIRE(game.state == GAME_LEVEL_NOTICE && game.y_q == 15 * GAME_Q);
    ++now_ms; game_core_step(&game, 0, 0, now_ms);
    REQUIRE(game.state == GAME_PLAYING);
    game.lives = 1U; game_core_restart(&game, now_ms);
    REQUIRE(game.state == GAME_RESTART_NOTICE && game.level == 1U && game.lives == 3U);
    now_ms += GAME_RESTART_NOTICE_MS; game_core_step(&game, 0, 0, now_ms);
    REQUIRE(game.state == GAME_PLAYING);
}

int main(void)
{
    test_boot_and_restart(); test_installed_sensor_orientation(); test_tilt_strength();
    test_rules(); test_manual_priority(); test_button(); test_tilt_only_solution();
    printf("PASS: %u checks, %u frames; solved all 5 levels with 3 hearts.\n", checks, frames);
    printf("Tilt speed at 800 ms: %.2f / %.2f / %.2f / %.2f px/s.\n",
           tilt_speeds_q[0] / (double)GAME_Q, tilt_speeds_q[1] / (double)GAME_Q,
           tilt_speeds_q[2] / (double)GAME_Q, tilt_speeds_q[3] / (double)GAME_Q);
    return 0;
}
