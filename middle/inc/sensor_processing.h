#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { int16_t ax_mg, ay_mg; bool valid; } SensorSample_t;

/* Layer 2 (Middle): Chuyen doi don vi + huong co dinh theo game_config.h */
void sensor_processing_update(int16_t raw_x, int16_t raw_y, SensorSample_t *sample);

/* Layer 2 (Middle): BMI160 Accelerometer driver */
bool bmi160_configure(void);
bool bmi160_read_raw(int16_t *raw_x, int16_t *raw_y);

#endif
