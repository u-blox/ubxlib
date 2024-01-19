# I know this looks a bit mad but to persuade the ESP-IDF
# tools to pick this up as a test component it needs
# this directory level as the main component with the
# stuff in the test sub-directory as the tests.
set(COMPONENT_SRCS "")
set(COMPONENT_ADD_INCLUDEDIRS "test")
set(COMPONENT_REQUIRES "driver" "esptool_py" "unity" "esp_netif")
register_component()