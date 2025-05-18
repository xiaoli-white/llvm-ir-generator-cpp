cmake_minimum_required(VERSION 3.10)
project(LLVM_IR_GENERATOR)

include_directories("$ENV{JAVA_HOME}\\include")
include_directories("$ENV{JAVA_HOME}\\include\\win32")

find_package(LLVM REQUIRED CONFIG)

include_directories("C:\\Program Files\\Java\\jdk-23\\include")
include_directories("C:\\Program Files\\Java\\jdk-23\\include\\win32")

set(llvm_components core irreader support analysis passes codegen target mc object linker option)
llvm_map_components_to_libnames(llvm_libs ${llvm_components})

include_directories("C:\\msys64\\clang64\\include")

add_library(llvm_ir_generator SHARED llvm_ir_generator.cpp)

target_link_libraries(llvm_ir_generator PRIVATE ${llvm_libs})
