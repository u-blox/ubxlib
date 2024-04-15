# The STM32Cube parts of STM32U5, leaving out any RTOS-related parts

define USAGE_MESSAGE
You must set STM32CUBE_HAL_PATH to the path of the STM32U5xx_HAL_Driver direcory under either (https://github.com/STMicroelectronics/STM32CubeU5.git) or (https://github.com/STMicroelectronics/x-cube-freertos.git) to use this file
endef

ifndef STM32CUBE_HAL_PATH
$(error $(USAGE_MESSAGE))
endif

$(info STM32CUBE_HAL_PATH:        $(STM32CUBE_HAL_PATH))

# STM32U5 HAL
STM32CUBE_FW_SRC += \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_cortex.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_dma.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_dma_ex.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_exti.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_flash.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_flash_ex.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_gpio.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_pwr.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_pwr_ex.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_rcc.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_rcc_ex.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_tim.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_hal_tim_ex.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_rcc.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_gpio.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_dma.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_spi.c \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_usart.c

STM32CUBE_FW_INC += \
	$(STM32CUBE_HAL_PATH)/Inc/Legacy \
	$(STM32CUBE_HAL_PATH)/Inc