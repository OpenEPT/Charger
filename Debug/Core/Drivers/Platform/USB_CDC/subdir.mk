################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Drivers/Platform/USB_CDC/drv_usb_cdc.c 

OBJS += \
./Core/Drivers/Platform/USB_CDC/drv_usb_cdc.o 

C_DEPS += \
./Core/Drivers/Platform/USB_CDC/drv_usb_cdc.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Drivers/Platform/USB_CDC/%.o Core/Drivers/Platform/USB_CDC/%.su Core/Drivers/Platform/USB_CDC/%.cyclo: ../Core/Drivers/Platform/USB_CDC/%.c Core/Drivers/Platform/USB_CDC/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../Core/Configuration -I../Core/Drivers/Platform/System -I../Core/Drivers/Platform/GPIO -I../Core/Drivers/Platform/I2C -I../Core/Drivers/Platform/AnalogIN -I../Core/Drivers/Platform/UART -I../Core/Drivers/Platform/USB_CDC -I../Core/HAL/BQ25180 -I../Core/Middlewares/Services/System -I../Core/Middlewares/Services/Charger -I../Core/Middlewares/Services/Logging -I../Core/Middlewares/Services/Control -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Drivers-2f-Platform-2f-USB_CDC

clean-Core-2f-Drivers-2f-Platform-2f-USB_CDC:
	-$(RM) ./Core/Drivers/Platform/USB_CDC/drv_usb_cdc.cyclo ./Core/Drivers/Platform/USB_CDC/drv_usb_cdc.d ./Core/Drivers/Platform/USB_CDC/drv_usb_cdc.o ./Core/Drivers/Platform/USB_CDC/drv_usb_cdc.su

.PHONY: clean-Core-2f-Drivers-2f-Platform-2f-USB_CDC

