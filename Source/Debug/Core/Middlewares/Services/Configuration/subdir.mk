################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Middlewares/Services/Configuration/configuration.c \
../Core/Middlewares/Services/Configuration/configurationDef.c 

OBJS += \
./Core/Middlewares/Services/Configuration/configuration.o \
./Core/Middlewares/Services/Configuration/configurationDef.o 

C_DEPS += \
./Core/Middlewares/Services/Configuration/configuration.d \
./Core/Middlewares/Services/Configuration/configurationDef.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Middlewares/Services/Configuration/%.o Core/Middlewares/Services/Configuration/%.su Core/Middlewares/Services/Configuration/%.cyclo: ../Core/Middlewares/Services/Configuration/%.c Core/Middlewares/Services/Configuration/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../Core/HAL/M24C32/ -I../Core/Configuration -I../Core/Drivers/Platform/System -I../Core/Drivers/Platform/GPIO -I../Core/Drivers/Platform/I2C -I../Core/Drivers/Platform/Timer -I../Core/Drivers/Platform/AnalogIN -I../Core/Drivers/Platform/UART -I../Core/Drivers/Platform/USB_CDC -I../Core/HAL/BQ25180 -I../Core/Middlewares/Services/System -I../Core/Middlewares/Services/Charger -I../Core/Middlewares/Services/Logging -I../Core/Middlewares/Services/Control -I../Core/Middlewares/Services/Configuration -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Middlewares-2f-Services-2f-Configuration

clean-Core-2f-Middlewares-2f-Services-2f-Configuration:
	-$(RM) ./Core/Middlewares/Services/Configuration/configuration.cyclo ./Core/Middlewares/Services/Configuration/configuration.d ./Core/Middlewares/Services/Configuration/configuration.o ./Core/Middlewares/Services/Configuration/configuration.su ./Core/Middlewares/Services/Configuration/configurationDef.cyclo ./Core/Middlewares/Services/Configuration/configurationDef.d ./Core/Middlewares/Services/Configuration/configurationDef.o ./Core/Middlewares/Services/Configuration/configurationDef.su

.PHONY: clean-Core-2f-Middlewares-2f-Services-2f-Configuration

