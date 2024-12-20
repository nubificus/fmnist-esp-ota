#ifndef __WIFI_H__
#define __WIFI_H__

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define MAX_FAILURES 10

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t connect_wifi();

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_H__ */
