#pragma once

#include <stdint.h>

void bme280_init();
void bme280_measure(int32_t *temp, uint32_t *press, uint32_t *hum);
