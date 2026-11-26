#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

/* Pure game constants: no registers or FreeRTOS dependencies. */
#define GAME_STEP_MS             20U
#define GAME_BALL_RADIUS_PX      3
#define GAME_Q                   256
#define GAME_MAX_SPEED_PX_S      60
#define GAME_DEADZONE_MG         40
/* After deadzone: retain gentle control, progressively boost stronger tilts.
 * Gain above the knee is 1.5x, continuous at the threshold. */
#define GAME_TILT_BOOST_START_MG 220
#define GAME_TILT_INPUT_LIMIT_MG 1000
#define GAME_TRAP_NOTICE_MS      700U
#define GAME_SPLASH_MS           2000U
#define GAME_LEVEL_NOTICE_MS     1000U
#define GAME_RESTART_NOTICE_MS   1000U
#define GAME_BUTTON_DEBOUNCE_MS  30U
#define GAME_BUTTON_LONG_MS      1000U

/* Axis correction after sensor calibration. X grows right, Y grows down.
 * User verified vertical direction; horizontal was reversed at X_SIGN=-1.
 * Current mapping: screen X = sensor Y, screen Y = -sensor X. */
#ifndef GAME_SWAP_AXES
#define GAME_SWAP_AXES           1
#endif
#ifndef GAME_X_SIGN
#define GAME_X_SIGN              1
#endif
#ifndef GAME_Y_SIGN
#define GAME_Y_SIGN              -1
#endif

/* RTOS task resources. The shared app_cfg.h / FreeRTOSConfig.h are unchanged. */
#define APP_GAME_STACK_WORDS      320U
#define APP_GAME_PRIORITY         (tskIDLE_PRIORITY + 2U)
#define APP_SENSOR_STACK_WORDS    256U
#define APP_SENSOR_PRIORITY       (tskIDLE_PRIORITY + 3U)
#define APP_SENSOR_PERIOD_MS      20U
#define APP_SENSOR_STALE_MS       200U
#define APP_SENSOR_RETRY_MS       1000U
#define APP_BUTTON_STACK_WORDS    128U
#define APP_BUTTON_PRIORITY       (tskIDLE_PRIORITY + 4U)
#define APP_BUTTON_PERIOD_MS      5U
#define APP_TELEMETRY_STACK_WORDS 160U
#define APP_TELEMETRY_PRIORITY    (tskIDLE_PRIORITY + 1U)
#define APP_TELEMETRY_PERIOD_MS   1000U
#define APP_UART_BAUDRATE         115200U
#define APP_SENSOR_CAL_SAMPLES    32U
#define APP_SENSOR_CAL_SPREAD_MG  60


#endif
