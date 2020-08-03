# I know this looks a bit mad but to persaude the ESP-IDF
# tools to pick this up as a test component it needs
# this directory level as the main component with the
# stuff in the test sub-directory as the tests.
set(COMPONENT_SRCS "")
set(COMPONENT_ADD_INCLUDEDIRS "test")
set(COMPONENT_REQUIRES "esp32" "esptool_py" "unity")
register_component()