#ifndef JSON_HANDLE_H
#define JSON_HANDLE_H

#include "tx_api.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdint.h>
extern TX_THREAD module_thread;

UINT add_sensor_data(int16_t data, UINT tag);

UINT json_handle_initialize(void);
#endif /* MODULE_H */