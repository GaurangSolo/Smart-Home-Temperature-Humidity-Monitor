#pragma once
#include <stdint.h>

typedef struct {
    float temperature;
    float humidity;
    float pressure;
} bme280_data_t;

void bme280_init(void);
void bme280_read_data(bme280_data_t *data);
