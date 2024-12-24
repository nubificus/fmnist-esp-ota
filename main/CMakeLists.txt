set(SOURCES ./src/main.cpp
			./src/DataProvider.cpp
			./src/main_functions.cpp
			./src/micro_model.cpp
			./src/PredictionInterpreter.cpp
			./src/PredictionHandler.cpp
			./src/wifi.c
			./src/tcp_server.c)

set(REQUIRES_LIST freertos esp_common tfmicro esp-nn esp_timer)

if(NOT DEFINED ENV{STOCK})
	list(APPEND REQUIRES_LIST esp32-akri ota-service esp_http_server)
	list(APPEND SOURCES ./src/http_server.c)
	message("Adding OTA update dependencies")
endif()			

idf_component_register(SRCS ${SOURCES}
						INCLUDE_DIRS . inc
						REQUIRES ${REQUIRES_LIST}
						PRIV_REQUIRES spi_flash esp_netif esp_wifi nvs_flash)
