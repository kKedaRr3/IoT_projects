idf_component_register(SRCS "hello_world_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event driver nvs_flash esp_http_client)
