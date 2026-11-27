#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int8_t sign_x, sign_y;
    bool swap_xy;
} SensorProcessing_t;

typedef struct { int16_t ax_mg, ay_mg; bool valid; } SensorSample_t;

/* Layer 2 (Middle): Chuyen doi don vi + cau hinh huong */
void sensor_processing_init(SensorProcessing_t *state);
void sensor_processing_set_orientation(SensorProcessing_t *state, bool swap_xy,
                                       int8_t sign_x, int8_t sign_y);
void sensor_processing_update(SensorProcessing_t *state, int16_t raw_x,
                              int16_t raw_y, SensorSample_t *sample);

/* Layer 2 (Middle): BMI160 Accelerometer driver */
bool bmi160_configure(void);
bool bmi160_read_raw(int16_t *raw_x, int16_t *raw_y);

#endif
