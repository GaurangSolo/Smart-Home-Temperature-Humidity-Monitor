#pragma once
#include "bme280.h"
#include "npm1100.h"

void wifi_connect(void);
void mqtt_publish_data(const bme280_data_t *data, float battery, const npm1100_status_t *status);
