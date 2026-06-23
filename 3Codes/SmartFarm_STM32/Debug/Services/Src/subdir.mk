################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Services/Src/relay_manager.c \
../Services/Src/rtc_manager.c \
../Services/Src/sensor_manager.c 

OBJS += \
./Services/Src/relay_manager.o \
./Services/Src/rtc_manager.o \
./Services/Src/sensor_manager.o 

C_DEPS += \
./Services/Src/relay_manager.d \
./Services/Src/rtc_manager.d \
./Services/Src/sensor_manager.d 


# Each subdirectory must supply rules for building sources it contributes
Services/Src/%.o Services/Src/%.su Services/Src/%.cyclo: ../Services/Src/%.c Services/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/App/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Common/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Devices/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Services/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Services-2f-Src

clean-Services-2f-Src:
	-$(RM) ./Services/Src/relay_manager.cyclo ./Services/Src/relay_manager.d ./Services/Src/relay_manager.o ./Services/Src/relay_manager.su ./Services/Src/rtc_manager.cyclo ./Services/Src/rtc_manager.d ./Services/Src/rtc_manager.o ./Services/Src/rtc_manager.su ./Services/Src/sensor_manager.cyclo ./Services/Src/sensor_manager.d ./Services/Src/sensor_manager.o ./Services/Src/sensor_manager.su

.PHONY: clean-Services-2f-Src

