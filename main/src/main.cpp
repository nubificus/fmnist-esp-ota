#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "main_functions.h"
#include "wifi.h"
#include "http_server.h"

extern "C" {
#include "esp32-akri.h"
#include "ota-service.h"
}

const char *TAG = "main";

void tf_main(int argc, char* argv[]) {
  setup();
  while (true) {
    loop();
  }
}

// esp_err_t info_get_handler(httpd_req_t *req)
// {
//     char json_response[100];  // Adjust size based on your expected response length
//     snprintf(json_response, sizeof(json_response),
//                 "{\"device\":\"%s\",\"application\":\"%s\",\"version\":\"%s\"}",
//                 "esp32s3", "fmnist", "0.0.1");

//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, json_response, strlen(json_response));
//     return ESP_OK;
// }
extern "C" void app_main() {
	esp_err_t ret; 
	
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ret = connect_wifi(); // connect_wifi("nbfc-iot", "nbfcIoTOTA");
	if (WIFI_SUCCESS != ret) {
		ESP_LOGI(TAG, "Failed to associate to AP, dying...");
		return;
	}

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

	xTaskCreate((TaskFunction_t)&tf_main, "tf_main", 4 * 1024, NULL, 8, NULL);
	vTaskDelete(NULL);
}
