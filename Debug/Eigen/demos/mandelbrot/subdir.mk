################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Eigen/demos/mandelbrot/mandelbrot.cpp 

OBJS += \
./Eigen/demos/mandelbrot/mandelbrot.o 

CPP_DEPS += \
./Eigen/demos/mandelbrot/mandelbrot.d 


# Each subdirectory must supply rules for building sources it contributes
Eigen/demos/mandelbrot/%.o: ../Eigen/demos/mandelbrot/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/include/ -I/home/jason.b/Desktop/Github/NICE/Eigen -I/usr/include/gtest -I/home/jason.b/Desktop/Github/NICE/cpp/src/ -I/home/jason.b/Desktop/Github/NICE/ -O0 -g3 -Wall -c -fmessage-length=0 -std=c++14 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


