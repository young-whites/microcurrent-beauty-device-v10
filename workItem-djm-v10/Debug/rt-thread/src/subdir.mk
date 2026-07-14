################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../rt-thread/src/clock.c \
../rt-thread/src/components.c \
../rt-thread/src/idle.c \
../rt-thread/src/ipc.c \
../rt-thread/src/irq.c \
../rt-thread/src/kservice.c \
../rt-thread/src/mem.c \
../rt-thread/src/mempool.c \
../rt-thread/src/object.c \
../rt-thread/src/scheduler_comm.c \
../rt-thread/src/scheduler_up.c \
../rt-thread/src/thread.c \
../rt-thread/src/timer.c 

OBJS += \
./rt-thread/src/clock.o \
./rt-thread/src/components.o \
./rt-thread/src/idle.o \
./rt-thread/src/ipc.o \
./rt-thread/src/irq.o \
./rt-thread/src/kservice.o \
./rt-thread/src/mem.o \
./rt-thread/src/mempool.o \
./rt-thread/src/object.o \
./rt-thread/src/scheduler_comm.o \
./rt-thread/src/scheduler_up.o \
./rt-thread/src/thread.o \
./rt-thread/src/timer.o 

C_DEPS += \
./rt-thread/src/clock.d \
./rt-thread/src/components.d \
./rt-thread/src/idle.d \
./rt-thread/src/ipc.d \
./rt-thread/src/irq.d \
./rt-thread/src/kservice.d \
./rt-thread/src/mem.d \
./rt-thread/src/mempool.d \
./rt-thread/src/object.d \
./rt-thread/src/scheduler_comm.d \
./rt-thread/src/scheduler_up.d \
./rt-thread/src/thread.d \
./rt-thread/src/timer.d 


# Each subdirectory must supply rules for building sources it contributes
rt-thread/src/%.o: ../rt-thread/src/%.c
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F1 -DUSE_HAL_DRIVER -DSTM32F103xE -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers\include\config" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\Device\ST\STM32F1xx\Include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\Include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\RTOS\Template" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\STM32F1xx_HAL_Driver\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\STM32F1xx_HAL_Driver\Inc\Legacy" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macBSP\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macSYS\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macTask\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\cubemx\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\cubemx" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\drivers\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\finsh" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\compilers\common\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\compilers\newlib" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\cplusplus" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\epoll" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\eventfd" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\poll" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\ipc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\utilities\ulog" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\libcpu\arm\common" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\libcpu\arm\cortex-m3" -include"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

