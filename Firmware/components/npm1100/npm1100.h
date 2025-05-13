#pragma once
#include <stdint.h>

typedef struct {
    bool is_charging;
    float battery_voltage;
    uint8_t charge_percentage;
} npm1100_status_t;

void npm1100_init(void);
float npm1100_read_battery(void);
npm1100_status_t npm1100_get_status(void);
