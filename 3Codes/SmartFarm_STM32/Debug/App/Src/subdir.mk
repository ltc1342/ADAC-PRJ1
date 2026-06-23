################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/Src/communication_manager.c \
../App/Src/control_manager.c \
../App/Src/display_manager.c \
../App/Src/schedule_manager.c 

OBJS += \
./App/Src/communication_manager.o \
./App/Src/control_manager.o \
./App/Src/display_manager.o \
./App/Src/schedule_manager.o 

C_DEPS += \
./App/Src/communication_manager.d \
./App/Src/control_manager.d \
./App/Src/display_manager.d \
./App/Src/schedule_manager.d 


# Each subdirectory must supply rules for building sources it contributes
App/Src/%.o App/Src/%.su App/Src/%.cyclo: ../App/Src/%.c App/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/App/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Common/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Devices/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Services/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-Src

clean-App-2f-Src:
	-$(RM) ./App/Src/communication_manager.cyclo ./App/Src/communication_manager.d ./App/Src/communication_manager.o ./App/Src/communication_manager.su ./App/Src/control_manager.cyclo ./App/Src/control_manager.d ./App/Src/control_manager.o ./App/Src/control_manager.su ./App/Src/display_manager.cyclo ./App/Src/display_manager.d ./App/Src/display_manager.o ./App/Src/display_manager.su ./App/Src/schedule_manager.cyclo ./App/Src/schedule_manager.d ./App/Src/schedule_manager.o ./App/Src/schedule_manager.su

.PHONY: clean-App-2f-Src

