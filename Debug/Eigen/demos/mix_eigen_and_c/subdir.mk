################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Eigen/demos/mix_eigen_and_c/binary_library.cpp 

C_SRCS += \
../Eigen/demos/mix_eigen_and_c/example.c 

OBJS += \
./Eigen/demos/mix_eigen_and_c/binary_library.o \
./Eigen/demos/mix_eigen_and_c/example.o 

C_DEPS += \
./Eigen/demos/mix_eigen_and_c/example.d 

CPP_DEPS += \
./Eigen/demos/mix_eigen_and_c/binary_library.d 


# Each subdirectory must supply rules for building sources it contributes
Eigen/demos/mix_eigen_and_c/%.o: ../Eigen/demos/mix_eigen_and_c/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/include/ -I/home/jason.b/Desktop/Github/NICE/Eigen -I/usr/include/gtest -I/home/jason.b/Desktop/Github/NICE/cpp/src/ -I/home/jason.b/Desktop/Github/NICE/ -O0 -g3 -Wall -c -fmessage-length=0 -std=c++14 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Eigen/demos/mix_eigen_and_c/%.o: ../Eigen/demos/mix_eigen_and_c/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


