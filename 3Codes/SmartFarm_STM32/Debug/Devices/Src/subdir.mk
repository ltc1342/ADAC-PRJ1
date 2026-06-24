################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Devices/Src/bh1750.c \
../Devices/Src/dht.c \
../Devices/Src/ds18b20.c \
../Devices/Src/font.c \
../Devices/Src/relay.c \
../Devices/Src/sh1106.c \
../Devices/Src/soil_moisture.c 

OBJS += \
./Devices/Src/bh1750.o \
./Devices/Src/dht.o \
./Devices/Src/ds18b20.o \
./Devices/Src/font.o \
./Devices/Src/relay.o \
./Devices/Src/sh1106.o \
./Devices/Src/soil_moisture.o 

C_DEPS += \
./Devices/Src/bh1750.d \
./Devices/Src/dht.d \
./Devices/Src/ds18b20.d \
./Devices/Src/font.d \
./Devices/Src/relay.d \
./Devices/Src/sh1106.d \
./Devices/Src/soil_moisture.d 


# Each subdirectory must supply rules for building sources it contributes
Devices/Src/%.o Devices/Src/%.su Devices/Src/%.cyclo: ../Devices/Src/%.c Devices/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/App/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Common/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Devices/Inc" -I"/run/media/ltc1342/DATA/Student/ChipDesign_FPT/HK1/ADAC-PRJ1/3Codes/SmartFarm_STM32/Services/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Devices-2f-Src

clean-Devices-2f-Src:
	-$(RM) ./Devices/Src/bh1750.cyclo ./Devices/Src/bh1750.d ./Devices/Src/bh1750.o ./Devices/Src/bh1750.su ./Devices/Src/dht.cyclo ./Devices/Src/dht.d ./Devices/Src/dht.o ./Devices/Src/dht.su ./Devices/Src/ds18b20.cyclo ./Devices/Src/ds18b20.d ./Devices/Src/ds18b20.o ./Devices/Src/ds18b20.su ./Devices/Src/font.cyclo ./Devices/Src/font.d ./Devices/Src/font.o ./Devices/Src/font.su ./Devices/Src/relay.cyclo ./Devices/Src/relay.d ./Devices/Src/relay.o ./Devices/Src/relay.su ./Devices/Src/sh1106.cyclo ./Devices/Src/sh1106.d ./Devices/Src/sh1106.o ./Devices/Src/sh1106.su ./Devices/Src/soil_moisture.cyclo ./Devices/Src/soil_moisture.d ./Devices/Src/soil_moisture.o ./Devices/Src/soil_moisture.su

.PHONY: clean-Devices-2f-Src

