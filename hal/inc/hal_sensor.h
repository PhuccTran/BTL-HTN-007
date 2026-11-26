#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H
#include <stdbool.h>
#include <stdint.h>
/* Sensor setup and raw readings only; mg conversion belongs to middle. */
bool hal_sensor_configure(void);
bool hal_sensor_read(int16_t *raw_x, int16_t *raw_y);
#endif
