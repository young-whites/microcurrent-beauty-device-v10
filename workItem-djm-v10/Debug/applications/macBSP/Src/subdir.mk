################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../applications/macBSP/Src/bsp_beep.c \
../applications/macBSP/Src/bsp_hard.c \
../applications/macBSP/Src/bsp_key.c \
../applications/macBSP/Src/bsp_led.c \
../applications/macBSP/Src/nnc6521_drv.c \
../applications/macBSP/Src/nnc6521_spi.c \
../applications/macBSP/Src/nnc6521_waveform.c \
../applications/macBSP/Src/nnc6521_waveform_config.c \
../applications/macBSP/Src/ntc_sensor.c \
../applications/macBSP/Src/protocol.c \
../applications/macBSP/Src/protocol_act.c \
../applications/macBSP/Src/temp_pid.c 

OBJS += \
./applications/macBSP/Src/bsp_beep.o \
./applications/macBSP/Src/bsp_hard.o \
./applications/macBSP/Src/bsp_key.o \
./applications/macBSP/Src/bsp_led.o \
./applications/macBSP/Src/nnc6521_drv.o \
./applications/macBSP/Src/nnc6521_spi.o \
./applications/macBSP/Src/nnc6521_waveform.o \
./applications/macBSP/Src/nnc6521_waveform_config.o \
./applications/macBSP/Src/ntc_sensor.o \
./applications/macBSP/Src/protocol.o \
./applications/macBSP/Src/protocol_act.o \
./applications/macBSP/Src/temp_pid.o 

C_DEPS += \
./applications/macBSP/Src/bsp_beep.d \
./applications/macBSP/Src/bsp_hard.d \
./applications/macBSP/Src/bsp_key.d \
./applications/macBSP/Src/bsp_led.d \
./applications/macBSP/Src/nnc6521_drv.d \
./applications/macBSP/Src/nnc6521_spi.d \
./applications/macBSP/Src/nnc6521_waveform.d \
./applications/macBSP/Src/nnc6521_waveform_config.d \
./applications/macBSP/Src/ntc_sensor.d \
./applications/macBSP/Src/protocol.d \
./applications/macBSP/Src/protocol_act.d \
./applications/macBSP/Src/temp_pid.d 


# Each subdirectory must supply rules for building sources it contributes
applications/macBSP/Src/%.o: ../applications/macBSP/Src/%.c
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F1 -DUSE_HAL_DRIVER -DSTM32F103xE -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers\include\config" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\Device\ST\STM32F1xx\Include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\Include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\RTOS\Template" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\STM32F1xx_HAL_Driver\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\STM32F1xx_HAL_Driver\Inc\Legacy" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macBSP\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macSYS\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macTask\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\cubemx\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\cubemx" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\drivers\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\finsh" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\compilers\common\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\compilers\newlib" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\cplusplus" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\epoll" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\eventfd" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\poll" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\ipc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\utilities\ulog" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\libcpu\arm\common" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\libcpu\arm\cortex-m3" -include"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

