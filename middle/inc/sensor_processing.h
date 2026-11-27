#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t count, calibration_restarts;
    int32_t sum_x, sum_y;
    int16_t min_x, max_x, min_y, max_y;
    int16_t offset_x_mg, offset_y_mg;
    bool calibrated;
} SensorProcessing_t;

typedef struct { int16_t ax_mg, ay_mg; bool valid; } SensorSample_t;

/* Layer 2 (Middle): Chuyen doi don vi + hieu chuan */
void sensor_processing_init(SensorProcessing_t *state);
/* Keep a completed calibration across a transient I2C failure. */
void sensor_processing_reset_window(SensorProcessing_t *state);
void sensor_processing_update(SensorProcessing_t *state, int16_t raw_x,
                              int16_t raw_y, SensorSample_t *sample);

/* Layer 2 (Middle): BMI160 Accelerometer driver */
bool bmi160_configure(void);
bool bmi160_read_raw(int16_t *raw_x, int16_t *raw_y);

#endif
