idf_component_register(
    SRCS "src/non_volatile_storage.c"
    INCLUDE_DIRS "include"

    REQUIRES nvs_flash
)