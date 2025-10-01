cmake_minimum_required(VERSION 3.10)
project(LLVM_IR_GENERATOR)

include_directories("$ENV{JAVA_HOME}\\include")
include_directories("$ENV{JAVA_HOME}\\include\\win32")

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories("C:\\Program Files\\Java\\jdk-23\\include")
include_directories("C:\\Program Files\\Java\\jdk-23\\include\\win32")

include(AddLLVM)

set(llvm_components core irreader support analysis passes codegen target mc object linker option)
llvm_map_components_to_libnames(llvm_libs ${llvm_components})

include_directories("C:\\msys64\\clang64\\include")

add_library(llvm_ir_generator SHARED llvm_ir_generator.cpp)

target_link_libraries(llvm_ir_generator PRIVATE
        ${llvm_libs}
        ClangDriver       # 驱动模块 (Driver, ToolChain)
        ClangBasic        # 基础库 (DiagnosticIDs, DiagnosticsEngine)
        ClangFrontend     # 前端支持 (TextDiagnosticPrinter)
        ClangTooling      # 工具链支持
        ClangParse        # 解析器
        ClangSerialization # AST 序列化
        ClangSema         # 语义分析
        ClangEdit         # 代码编辑
        ClangAST          # 抽象语法树
        ClangLex          # 词法分析
        ClangAnalysis     # 静态分析
)


if (WIN32)
    target_compile_definitions(llvm_ir_generator PRIVATE _WINDLL _MBCS)
    set_target_properties(llvm_ir_generator PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    target_link_libraries(llvm_ir_generator PRIVATE
            LLVMWindowsDriver
            version
    )
endif ()