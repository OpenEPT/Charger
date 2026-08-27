################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include USB_DEVICE/Target/subdir.mk
-include USB_DEVICE/App/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/subdir.mk
-include Middlewares/ST/STM32_USB_Device_Library/Core/Src/subdir.mk
-include Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/subdir.mk
-include Drivers/STM32L4xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include Core/Middlewares/Services/System/subdir.mk
-include Core/Middlewares/Services/Logging/subdir.mk
-include Core/Middlewares/Services/Control/CMParse/subdir.mk
-include Core/Middlewares/Services/Control/subdir.mk
-include Core/Middlewares/Services/Configuration/subdir.mk
-include Core/Middlewares/Services/Charger/subdir.mk
-include Core/HAL/BQ25180/subdir.mk
-include Core/HAL/AT24CS01/subdir.mk
-include Core/Drivers/Platform/USB_CDC/subdir.mk
-include Core/Drivers/Platform/UART/subdir.mk
-include Core/Drivers/Platform/Timer/subdir.mk
-include Core/Drivers/Platform/System/subdir.mk
-include Core/Drivers/Platform/Interrupts/subdir.mk
-include Core/Drivers/Platform/I2C/subdir.mk
-include Core/Drivers/Platform/GPIO/subdir.mk
-include Core/Drivers/Platform/AnalogIN/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := ADFirmware_Charger
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
ADFirmware_Charger.elf \

MAP_FILES += \
ADFirmware_Charger.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
ADFirmware_Charger.list \


# All Target
all: main-build

# Main-build Target
main-build: ADFirmware_Charger.elf secondary-outputs

# Tool invocations
ADFirmware_Charger.elf ADFirmware_Charger.map: $(OBJS) $(USER_OBJS) /home/elektronika/Desktop/Projects/OpenEPT/Forks/OpenEPT_Charger/Source/STM32L476RGTX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "ADFirmware_Charger.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"/home/elektronika/Desktop/Projects/OpenEPT/Forks/OpenEPT_Charger/Source/STM32L476RGTX_FLASH.ld" --specs=nosys.specs -Wl,-Map="ADFirmware_Charger.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -u _printf_float -u _scanf_float -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

ADFirmware_Charger.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "ADFirmware_Charger.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) ADFirmware_Charger.elf ADFirmware_Charger.list ADFirmware_Charger.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
