#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "main_functions.h"
#include "wifi.h"

#ifndef STOCK
#include "http_server.h"

extern "C" {
#include "esp32-akri.h"
#include "ota-service.h"
}
#endif

const char *TAG = "main";

extern "C" void app_main() {
	esp_err_t ret; 
	
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ret = connect_wifi();
	if (WIFI_SUCCESS != ret) {
		ESP_LOGI(TAG, "Failed to associate to AP, dying...");
		return;
	}

#ifndef STOCK
	ret = akri_server_start();
	if (ret) {
		ESP_LOGE(TAG, "Cannot start akri server");
		abort();
	}
	ESP_LOGI(TAG, "HTTP Server started");
	
	ret = akri_set_update_handler(ota_request_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set OTA request handler");
		abort();
	}
	ESP_LOGI(TAG, "OTA Handler set");

	ret = akri_set_info_handler(info_get_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set info handler");
		abort();
	}
	ESP_LOGI(TAG, "Info handler set");

	ret = akri_set_temp_handler(temp_get_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set temp handler");
		abort();
	}
#endif

	// Start of the actual application
	tcp_server_t server;
	
	setup(&server);
	loop(&server);

	close(server.server_fd);
}
