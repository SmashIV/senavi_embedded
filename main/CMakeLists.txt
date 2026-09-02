idf_component_register(SRCS "senavi_project.c" "config.c" "log.c" "memory.c"
                            "time.c" "utils/str_utils.c"
                            "../test/dht_22_proto.c"
                            "../test/ublox_neo_6m_proto.c"
                            "../test/mq_135_proto.c"
                    INCLUDE_DIRS "."
                    REQUIRES log heap nvs_flash driver esp_timer esp_adc)
