################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSP/Src/bsp_gpio.c \
../BSP/Src/bsp_time.c 

OBJS += \
./BSP/Src/bsp_gpio.o \
./BSP/Src/bsp_time.o 

C_DEPS += \
./BSP/Src/bsp_gpio.d \
./BSP/Src/bsp_time.d 


# Each subdirectory must supply rules for building sources it contributes
BSP/Src/%.o BSP/Src/%.su BSP/Src/%.cyclo: ../BSP/Src/%.c BSP/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/App/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/BSP/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Common/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Devices/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Services/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-BSP-2f-Src

clean-BSP-2f-Src:
	-$(RM) ./BSP/Src/bsp_gpio.cyclo ./BSP/Src/bsp_gpio.d ./BSP/Src/bsp_gpio.o ./BSP/Src/bsp_gpio.su ./BSP/Src/bsp_time.cyclo ./BSP/Src/bsp_time.d ./BSP/Src/bsp_time.o ./BSP/Src/bsp_time.su

.PHONY: clean-BSP-2f-Src

