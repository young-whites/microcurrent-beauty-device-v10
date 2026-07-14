################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../rt-thread/libcpu/arm/cortex-m3/cpuport.c 

S_UPPER_SRCS += \
../rt-thread/libcpu/arm/cortex-m3/context_gcc.S 

OBJS += \
./rt-thread/libcpu/arm/cortex-m3/context_gcc.o \
./rt-thread/libcpu/arm/cortex-m3/cpuport.o 

S_UPPER_DEPS += \
./rt-thread/libcpu/arm/cortex-m3/context_gcc.d 

C_DEPS += \
./rt-thread/libcpu/arm/cortex-m3/cpuport.d 


# Each subdirectory must supply rules for building sources it contributes
rt-thread/libcpu/arm/cortex-m3/%.o: ../rt-thread/libcpu/arm/cortex-m3/%.S
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -x assembler-with-cpp -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -Xassembler -mimplicit-it=thumb -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
rt-thread/libcpu/arm/cortex-m3/%.o: ../rt-thread/libcpu/arm/cortex-m3/%.c
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -ffunction-sections -fdata-sections -Wall  -g -gdwarf-2 -DSOC_FAMILY_STM32 -DSOC_SERIES_STM32F1 -DUSE_HAL_DRIVER -DSTM32F103xE -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\drivers\include\config" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\Device\ST\STM32F1xx\Include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\Include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\CMSIS\RTOS\Template" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\STM32F1xx_HAL_Driver\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\libraries\STM32F1xx_HAL_Driver\Inc\Legacy" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macBSP\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macSYS\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\applications\macTask\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\cubemx\Inc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\cubemx" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\drivers\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\finsh" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\compilers\common\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\compilers\newlib" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\cplusplus" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\epoll" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\eventfd" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\io\poll" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\libc\posix\ipc" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\components\utilities\ulog" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\include" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\libcpu\arm\common" -I"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rt-thread\libcpu\arm\cortex-m3" -include"C:\Users\18452\Documents\GitHub-young-whites\microcurrent-beauty-device-v10\workItem-djm-v10\rtconfig_preinc.h" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

