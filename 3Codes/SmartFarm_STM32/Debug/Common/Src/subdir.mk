################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Common/Src/app_config.c \
../Common/Src/debug_log.c \
../Common/Src/timing.c 

OBJS += \
./Common/Src/app_config.o \
./Common/Src/debug_log.o \
./Common/Src/timing.o 

C_DEPS += \
./Common/Src/app_config.d \
./Common/Src/debug_log.d \
./Common/Src/timing.d 


# Each subdirectory must supply rules for building sources it contributes
Common/Src/%.o Common/Src/%.su Common/Src/%.cyclo: ../Common/Src/%.c Common/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/App/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Common/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Devices/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Services/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Common-2f-Src

clean-Common-2f-Src:
	-$(RM) ./Common/Src/app_config.cyclo ./Common/Src/app_config.d ./Common/Src/app_config.o ./Common/Src/app_config.su ./Common/Src/debug_log.cyclo ./Common/Src/debug_log.d ./Common/Src/debug_log.o ./Common/Src/debug_log.su ./Common/Src/timing.cyclo ./Common/Src/timing.d ./Common/Src/timing.o ./Common/Src/timing.su

.PHONY: clean-Common-2f-Src

