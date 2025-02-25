set(SOURCES ./src/main.cpp
			./src/DataProvider.cpp
			./src/main_functions.cpp
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

if (DEFINED ENV{LOAD_MODEL_FROM_PARTITION})
	list(APPEND REQUIRES_LIST esp_partition)
	message("Adding esp_partition dependency")
else ()
	list(APPEND SOURCES ./src/micro_model.cpp)
	message("Adding micro_model.cpp to the sources")
endif()

if (NOT DEFINED ENV{INTERNAL_MEMORY_USAGE})
	list(APPEND REQUIRES_LIST esp_psram)
	message("Adding esp_psram dependency")
endif()

idf_component_register(SRCS ${SOURCES}
						INCLUDE_DIRS . inc
						REQUIRES ${REQUIRES_LIST}
						PRIV_REQUIRES spi_flash esp_netif esp_wifi nvs_flash)
