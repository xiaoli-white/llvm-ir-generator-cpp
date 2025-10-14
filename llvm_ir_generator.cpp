#include <fstream>
#include <jni.h>
#include "./com_xiaoli_llvmir_generator_LLVMIRGenerator.h"
#include "./com_xiaoli_llvmir_generator_LLVMIRGenerator_LLVMModuleGenerator.h"


#include <iostream>
#include <map>
#include <stack>
#include <queue>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/WithColor.h>

#include "clang/Driver/Driver.h"
#include "clang/Driver/Compilation.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include "llvm/IR/GlobalVariable.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/Support/raw_ostream.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <llvm/Support/VirtualFileSystem.h>

static void ThrowJava(JNIEnv* env, const char* clazz, const std::string& msg)
{
    jclass c = env->FindClass(clazz);
    if (!c) c = env->FindClass("java/lang/RuntimeException");
    env->ThrowNew(c, msg.c_str());
}

JNIEXPORT jlong JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_createLLVMContext(JNIEnv* env, jclass clazz)
{
    return reinterpret_cast<jlong>(new llvm::LLVMContext());
}

JNIEXPORT jlong JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_createLLVMModule(
    JNIEnv* env, jclass clazz, jlong llvmContext)
{
    auto* module = new llvm::Module("module", *reinterpret_cast<llvm::LLVMContext*>(llvmContext));
    auto* mallocFunctionType = llvm::FunctionType::get(
        llvm::Type::getInt8Ty(*reinterpret_cast<llvm::LLVMContext*>(llvmContext))->getPointerTo(),
        {llvm::Type::getInt64Ty(*reinterpret_cast<llvm::LLVMContext*>(llvmContext))}, false);
    module->getOrInsertFunction("malloc", mallocFunctionType);
    auto* freeFunctionType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*reinterpret_cast<llvm::LLVMContext*>(llvmContext)),
        {llvm::Type::getInt8Ty(*reinterpret_cast<llvm::LLVMContext*>(llvmContext))->getPointerTo()}, false);
    module->getOrInsertFunction("free", freeFunctionType);
    auto* reallocFunctionType = llvm::FunctionType::get(
        llvm::Type::getInt8Ty(*reinterpret_cast<llvm::LLVMContext*>(llvmContext))->getPointerTo(),
        {
            llvm::Type::getInt8Ty(*reinterpret_cast<llvm::LLVMContext*>(llvmContext))->getPointerTo(),
            llvm::Type::getInt64Ty(*reinterpret_cast<llvm::LLVMContext*>(llvmContext))
        }, false);
    module->getOrInsertFunction("realloc", reallocFunctionType);
    return reinterpret_cast<jlong>(module);
}

JNIEXPORT jlong JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_createLLVMBuilder(
    JNIEnv* env, jclass clazz, jlong llvmContext)
{
    return reinterpret_cast<jlong>(new llvm::IRBuilder(*reinterpret_cast<llvm::LLVMContext*>(llvmContext)));
}

JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_destroyLLVMContext(
    JNIEnv* env, jclass clazz, jlong llvmContext)
{
    delete reinterpret_cast<llvm::LLVMContext*>(llvmContext);
}

JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_destroyLLVMModule(
    JNIEnv* env, jclass clazz, jlong llvmModule)
{
    delete reinterpret_cast<llvm::Module*>(llvmModule);
}

JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_destroyLLVMBuilder(
    JNIEnv* env, jclass clazz, jlong llvmBuilder)
{
    delete reinterpret_cast<llvm::IRBuilder<>*>(llvmBuilder);
}

JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_dumpLLVMModule(
    JNIEnv* env, jclass clazz, jlong llvmModule)
{
    reinterpret_cast<llvm::Module*>(llvmModule)->print(llvm::outs(), nullptr);
}

JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_compile(
    JNIEnv* env, jclass clazz, jlong llvmModule, jobject options)
{
    auto* module = reinterpret_cast<llvm::Module*>(llvmModule);
    if (!module)
    {
        ThrowJava(env, "java/lang/NullPointerException", "llvmModule is null");
        return;
    }

    jclass optionsClazz = env->GetObjectClass(options);
    if (!optionsClazz)
    {
        ThrowJava(env, "java/lang/NullPointerException", "options is null");
        return;
    }

    jclass stringClazz = env->FindClass("java/lang/String");
    if (!stringClazz)
    {
        if (env->ExceptionCheck()) return;
        ThrowJava(env, "java/lang/ClassNotFoundException", "java/lang/String");
        return;
    }

    jmethodID getMethodID = env->GetMethodID(optionsClazz, "get",
                                             "(Ljava/lang/String;Ljava/lang/Class;)Ljava/lang/Object;");
    if (!getMethodID)
    {
        ThrowJava(env, "java/lang/NoSuchMethodError", "options.get(String, Class) not found");
        return;
    }

    jstring kPlatform = env->NewStringUTF("platform");
    jstring kOutput = env->NewStringUTF("output");
    if (!kPlatform || !kOutput)
    {
        ThrowJava(env, "java/lang/OutOfMemoryError", "NewStringUTF failed");
        return;
    }

    jobject platformObj = env->CallObjectMethod(options, getMethodID, kPlatform, stringClazz);
    if (env->ExceptionCheck()) return;
    jobject outputObj = env->CallObjectMethod(options, getMethodID, kOutput, stringClazz);
    if (env->ExceptionCheck()) return;

    if (!platformObj || !outputObj)
    {
        ThrowJava(env, "java/lang/IllegalArgumentException", "platform/output not provided");
        return;
    }

    auto platformJ = reinterpret_cast<jstring>(platformObj);
    auto outputJ = reinterpret_cast<jstring>(outputObj);

    const char* platform = env->GetStringUTFChars(platformJ, nullptr);
    const char* outputFilename = env->GetStringUTFChars(outputJ, nullptr);
    if (!platform || !outputFilename)
    {
        ThrowJava(env, "java/lang/OutOfMemoryError", "GetStringUTFChars failed");
        return;
    }

    std::string tmpFile = std::string(outputFilename) + ".ll";

    module->setTargetTriple(platform);
    if (outputFilename != nullptr)
    {
        std::ofstream stream(outputFilename);
        if (!stream)
        {
            ThrowJava(env, "java/lang/RuntimeException", std::string("Failed to create file: ") + outputFilename);
            return;
        }
    }

    std::error_code EC;
    llvm::raw_fd_ostream Out(tmpFile, EC);
    if (EC)
    {
        ThrowJava(env, "java/lang/RuntimeException", std::string("Failed to open file: ") + EC.message());
        return;
    }
    module->print(Out, nullptr);
    Out.flush();

    const auto DiagOpts = llvm::makeIntrusiveRefCnt<clang::DiagnosticOptions>();
    auto* DiagClient = new clang::TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);
    const auto Diags = llvm::makeIntrusiveRefCnt<clang::DiagnosticsEngine>(
        llvm::makeIntrusiveRefCnt<clang::DiagnosticIDs>(), &*DiagOpts, DiagClient);

    std::string ClangPath = "clang";
    llvm::ErrorOr<std::string> ClangPathOrErr = llvm::sys::findProgramByName("clang");
    if (ClangPathOrErr)
    {
        ClangPath = ClangPathOrErr.get();
    }

    clang::driver::Driver Driver(ClangPath, platform, *Diags);

    std::vector<std::string> args = {ClangPath, "-x", "ir", tmpFile};

    auto addArgIf = [&args](const bool condition, const std::string& flag, const std::string& value = "")
    {
        if (condition)
        {
            args.emplace_back(flag);
            if (!value.empty())
            {
                args.emplace_back(value);
            }
        }
    };

    addArgIf(outputFilename != nullptr, "-o", outputFilename);

    std::vector<const char*> argsText;
    for (const auto& arg : args)
    {
        argsText.push_back(arg.c_str());
    }
    argsText.push_back(nullptr);

    const auto C = Driver.BuildCompilation(argsText);
    llvm::SmallVector<std::pair<int, const clang::driver::Command*>, 4> Failing;
    C->ExecuteJobs(C->getJobs(), Failing);

    if (llvm::sys::fs::exists(tmpFile))
    {
        if (std::error_code ec = llvm::sys::fs::remove(tmpFile))
        {
            llvm::errs() << "Failed to remove file: " << ec.message() << "\n";
        }
    }
}

llvm::Type* getType(JNIEnv* env, jobject irType, llvm::LLVMContext* context)
{
    jclass irVoidTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRVoidType");
    if (env->IsInstanceOf(irType, irVoidTypeClazz))
    {
        return llvm::Type::getVoidTy(*context);
    }
    jclass irIntegerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRIntegerType");
    if (env->IsInstanceOf(irType, irIntegerTypeClazz))
    {
        jobject size = env->GetObjectField(
            irType, env->GetFieldID(irIntegerTypeClazz, "size", "Lldk/l/lg/ir/type/IRIntegerType$Size;"));
        jlong realSize = env->GetLongField(size, env->GetFieldID(env->GetObjectClass(size), "size", "J"));
        return llvm::IntegerType::get(*context, realSize);
    }
    jclass irPointerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRPointerType");
    if (env->IsInstanceOf(irType, irPointerTypeClazz))
    {
        jobject base = env->GetObjectField(
            irType, env->GetFieldID(irPointerTypeClazz, "base", "Lldk/l/lg/ir/type/IRType;"));
        return llvm::PointerType::get(getType(env, base, context), 0);
    }
    auto* irFloatTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRFloatType");
    if (env->IsInstanceOf(irType, irFloatTypeClazz))
    {
        return llvm::Type::getFloatTy(*context);
    }
    auto* irDoubleTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRDoubleType");
    if (env->IsInstanceOf(irType, irDoubleTypeClazz))
    {
        return llvm::Type::getDoubleTy(*context);
    }
    return nullptr;
}

JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_createFunction
(JNIEnv* env, jobject thisPtr, jobject irFunction)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->
        GetLongField(thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    jclass irFunctionClazz = env->GetObjectClass(irFunction);
    const auto name = reinterpret_cast<jstring>(env->
        GetObjectField(irFunction, env->GetFieldID(irFunctionClazz, "name", "Ljava/lang/String;")));
    llvm::Type* returnType = getType(
        env, env->GetObjectField(
            irFunction, env->GetFieldID(irFunctionClazz, "returnType", "Lldk/l/lg/ir/type/IRType;")), context);
    jlong argumentCount = env->GetLongField(irFunction, env->GetFieldID(irFunctionClazz, "argumentCount", "J"));
    const auto fields = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irFunction, env->GetFieldID(irFunctionClazz, "fields", "[Lldk/l/lg/ir/structure/IRField;")));
    std::vector<llvm::Type*> types;
    for (int i = 0; i < argumentCount; i++)
    {
        auto* field = env->GetObjectArrayElement(fields, i);
        types.push_back(getType(
            env, env->GetObjectField(
                field, env->GetFieldID(env->GetObjectClass(field), "type", "Lldk/l/lg/ir/type/IRType;")),
            context));
    }
    llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, types, false);
    llvm::Function::Create(functionType, llvm::Function::ExternalLinkage,
                           env->GetStringUTFChars(name, nullptr), module);
}


JNIEXPORT void JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_initializeQueue(
    JNIEnv* env, jobject thisPtr)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* queue = new std::queue<std::pair<std::string, std::string>>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "queue", "J"), reinterpret_cast<jlong>(queue));
}

JNIEXPORT void JNICALL
Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_initializeITableInitializer(
    JNIEnv* env, jobject thisPtr)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->
        GetLongField(thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* queue = reinterpret_cast<std::queue<std::pair<std::string, std::string>>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "queue", "J")));

    auto* function = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*context), {}, false),
                                            llvm::Function::ExternalLinkage, "<itableInitializer>", module);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", function);
    builder->SetInsertPoint(entry);
    while (!queue->empty())
    {
        auto pair = queue->front();
        queue->pop();
        auto* entryVar = module->getGlobalVariable(pair.first);
        auto* elementPtr = builder->CreateInBoundsGEP(
            entryVar->getType(),
            entryVar,
            {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0)
            });
        auto* inst = builder->CreateStore(module->getGlobalVariable(pair.second), elementPtr);
        inst->setAlignment(llvm::Align(1));
    }
    builder->CreateRetVoid();
    auto* ctorArrayType = llvm::ArrayType::get(llvm::PointerType::get(llvm::IntegerType::getInt8Ty(*context), 0), 2);
    std::vector<llvm::Constant*> ctors;
    ctors.push_back(
        llvm::ConstantExpr::getBitCast(function, llvm::PointerType::get(llvm::IntegerType::getInt8Ty(*context), 0)));
    ctors.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::IntegerType::getInt8Ty(*context), 0)));
    auto* ctorArrayInit = llvm::ConstantArray::get(ctorArrayType, ctors);
    auto* ctorsGV = new llvm::GlobalVariable(*module,
                                             ctorArrayType,
                                             false,
                                             llvm::GlobalValue::AppendingLinkage,
                                             ctorArrayInit,
                                             "llvm.global_ctors");
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitGlobalData(
    JNIEnv* env, jobject thisPtr, jobject irGlobalData, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* irModule = env->GetObjectField(
        thisPtr, env->GetFieldID(clazz, "module", "Lldk/l/lg/ir/IRModule;"));
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->
        GetLongField(thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));

    auto* globalDataClazz = env->GetObjectClass(irGlobalData);
    auto* nameObject = reinterpret_cast<jstring>(env->GetObjectField(
        irGlobalData, env->GetFieldID(globalDataClazz, "name", "Ljava/lang/String;")));
    auto* sizeObject = env->GetObjectField(
        irGlobalData, env->GetFieldID(globalDataClazz, "size", "Lldk/l/lg/ir/operand/IROperand;"));

    auto* name = env->GetStringUTFChars(nameObject, nullptr);

    auto* irConstantClazz = env->FindClass("ldk/l/lg/ir/operand/IRConstant");
    auto* irMacroClazz = env->FindClass("ldk/l/lg/ir/operand/IRMacro");
    if (sizeObject != nullptr)
    {
        jlong size;
        if (env->IsInstanceOf(sizeObject, irMacroClazz))
        {
            auto* macroNameObject = reinterpret_cast<jstring>(env->GetObjectField(
                sizeObject, env->GetFieldID(irMacroClazz, "name", "Ljava/lang/String;")));
            auto* macroName = env->GetStringUTFChars(macroNameObject, nullptr);
            if (strcmp(macroName, "structure_length") == 0)
            {
                auto* argsArray = reinterpret_cast<jobjectArray>(env->GetObjectField(
                    sizeObject, env->GetFieldID(irMacroClazz, "args", "[Ljava/lang/String;")));
                auto* structures = env->GetObjectField(irModule, env->GetFieldID(
                                                           env->GetObjectClass(irModule), "structures",
                                                           "Ljava/util/Map;"));
                auto* structureNameObject = reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0));
                auto* structureName = env->GetStringUTFChars(structureNameObject, nullptr);
                auto* structure = env->CallObjectMethod(
                    structures, env->GetMethodID(env->GetObjectClass(structures), "get",
                                                 "(Ljava/lang/Object;)Ljava/lang/Object;"),
                    env->NewStringUTF(structureName));
                size = env->CallLongMethod(structure, env->GetMethodID(
                                               env->GetObjectClass(structure), "getLength",
                                               "()J"));
                env->ReleaseStringUTFChars(structureNameObject, structureName);
            }
            else
            {
                env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                              (std::string("Not expected macro ") + macroName).c_str());
                return nullptr;
            }
            env->ReleaseStringUTFChars(macroNameObject, macroName);
        }
        else if (env->IsInstanceOf(sizeObject, irConstantClazz))
        {
            jint index = env->GetIntField(sizeObject, env->GetFieldID(irConstantClazz, "index", "I"));
            auto* irModuleClazz = env->GetObjectClass(irModule);
            auto* constantPool = env->GetObjectField(
                irModule, env->GetFieldID(irModuleClazz, "constantPool", "Lldk/l/lg/ir/IRConstantPool;"));
            auto* entry = env->CallObjectMethod(constantPool,
                                                env->GetMethodID(env->GetObjectClass(constantPool), "get",
                                                                 "(I)Lldk/l/lg/ir/IRConstantPool$Entry;"), index);
            if (entry == nullptr)
            {
                env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                              "Not expected type of size");
                return nullptr;
            }
            auto* entryClazz = env->GetObjectClass(entry);
            auto* value = env->GetObjectField(entry, env->GetFieldID(entryClazz, "value", "Ljava/lang/Object;"));
            auto* numberClazz = env->FindClass("Ljava/lang/Number;");
            if (env->IsInstanceOf(value, numberClazz))
            {
                size = env->CallLongMethod(value, env->GetMethodID(numberClazz, "longValue", "()J"));
            }
            else
            {
                env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                              "Not expected type of size");
                return nullptr;
            }
        }
        else
        {
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                          "Not expected type of size");
            return nullptr;
        }
        auto* type = llvm::ArrayType::get(llvm::Type::getInt8Ty(*context), size);
        auto* globalVariable = new llvm::GlobalVariable(
            *module, type, false,
            llvm::GlobalValue::ExternalLinkage, llvm::ConstantAggregateZero::get(type), name);
    }
    else
    {
        auto* values = reinterpret_cast<jobjectArray>(env->GetObjectField(
            irGlobalData, env->GetFieldID(globalDataClazz, "values", "[Lldk/l/lg/ir/operand/IROperand;")));
        auto* irVirtualTableClazz = env->FindClass("ldk/l/lg/ir/operand/IRVirtualTable");
        auto* irInterfaceTableClazz = env->FindClass("ldk/l/lg/ir/operand/IRInterfaceTable");
        for (int i = 0; i < env->GetArrayLength(values); ++i)
        {
            auto* value = env->GetObjectArrayElement(values, i);
            llvm::GlobalVariable* globalVariable;
            if (env->IsInstanceOf(value, irConstantClazz))
            {
                jint index = env->GetIntField(value, env->GetFieldID(irConstantClazz, "index", "I"));
                auto* irModuleClazz = env->GetObjectClass(irModule);
                auto* constantPool = env->GetObjectField(
                    irModule, env->GetFieldID(irModuleClazz, "constantPool", "Lldk/l/lg/ir/IRConstantPool;"));
                auto* entry = env->CallObjectMethod(constantPool,
                                                    env->GetMethodID(env->GetObjectClass(constantPool), "get",
                                                                     "(I)Lldk/l/lg/ir/IRConstantPool$Entry;"), index);
                if (entry == nullptr)
                {
                    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                                  "Not expected type of size");
                    return nullptr;
                }
                auto* entryClazz = env->GetObjectClass(entry);
                auto* value = env->GetObjectField(entry, env->GetFieldID(entryClazz, "value", "Ljava/lang/Object;"));
                auto* byteClazz = env->FindClass("java/lang/Byte");
                auto* shortClazz = env->FindClass("java/lang/Short");
                auto* intClazz = env->FindClass("java/lang/Integer");
                auto* longClazz = env->FindClass("java/lang/Long");
                if (env->IsInstanceOf(value, byteClazz))
                {
                    auto byteValue = env->CallByteMethod(value, env->GetMethodID(byteClazz, "byteValue", "()B"));
                    globalVariable = new llvm::GlobalVariable(
                        *module, llvm::Type::getInt8Ty(*context), false,
                        llvm::GlobalValue::ExternalLinkage,
                        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context), byteValue),
                        name);
                }
                else if (env->IsInstanceOf(value, shortClazz))
                {
                    auto shortValue = env->CallShortMethod(value, env->GetMethodID(shortClazz, "shortValue", "()S"));
                    globalVariable = new llvm::GlobalVariable(
                        *module, llvm::Type::getInt16Ty(*context), false,
                        llvm::GlobalValue::ExternalLinkage,
                        llvm::ConstantInt::get(llvm::Type::getInt16Ty(*context), shortValue),
                        name);
                }
                else if (env->IsInstanceOf(value, intClazz))
                {
                    auto intValue = env->CallIntMethod(value, env->GetMethodID(intClazz, "intValue", "()I"));
                    globalVariable = new llvm::GlobalVariable(
                        *module, llvm::Type::getInt32Ty(*context), false,
                        llvm::GlobalValue::ExternalLinkage,
                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), intValue),
                        name);
                }
                else if (env->IsInstanceOf(value, longClazz))
                {
                    auto longValue = env->CallLongMethod(value, env->GetMethodID(longClazz, "longValue", "()J"));
                    globalVariable = new llvm::GlobalVariable(
                        *module, llvm::Type::getInt64Ty(*context), false,
                        llvm::GlobalValue::ExternalLinkage,
                        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), longValue),
                        name);
                }
                else
                {
                    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                                  "Not expected type of size");
                    return nullptr;
                }
            }
            else if (env->IsInstanceOf(value, irMacroClazz))
            {
                auto* macroNameObject = reinterpret_cast<jstring>(env->GetObjectField(
                    value, env->GetFieldID(irMacroClazz, "name", "Ljava/lang/String;")));
                auto* macroName = env->GetStringUTFChars(macroNameObject, nullptr);
                if (strcmp(macroName, "structure_length") == 0)
                {
                    auto* argsArray = reinterpret_cast<jobjectArray>(env->GetObjectField(
                        value, env->GetFieldID(irMacroClazz, "args", "[Ljava/lang/String;")));
                    auto* structures = env->GetObjectField(irModule, env->GetFieldID(
                                                               env->GetObjectClass(irModule), "structures",
                                                               "Ljava/util/Map;"));
                    auto* structureNameObject = reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0));
                    auto* structureName = env->GetStringUTFChars(structureNameObject, nullptr);
                    auto* structure = env->CallObjectMethod(
                        structures, env->GetMethodID(env->GetObjectClass(structures), "get",
                                                     "(Ljava/lang/Object;)Ljava/lang/Object;"),
                        env->NewStringUTF(structureName));
                    long size = env->CallLongMethod(structure, env->GetMethodID(
                                                        env->GetObjectClass(structure), "getLength",
                                                        "()J"));
                    env->ReleaseStringUTFChars(structureNameObject, structureName);
                    globalVariable = new llvm::GlobalVariable(
                        *module, llvm::Type::getInt64Ty(*context), false,
                        llvm::GlobalValue::ExternalLinkage,
                        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), size),
                        name);
                }
                else
                {
                    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                                  (std::string("Not expected macro ") + macroName).c_str());
                    return nullptr;
                }
                env->ReleaseStringUTFChars(macroNameObject, macroName);
            }
            else if (env->IsInstanceOf(value, irVirtualTableClazz))
            {
                auto* functions = reinterpret_cast<jobjectArray>(env->GetObjectField(
                    value, env->GetFieldID(irVirtualTableClazz, "functions", "[Ljava/lang/String;")));
                std::vector<llvm::Constant*> globals;
                for (int j = 0; j < env->GetArrayLength(functions); ++j)
                {
                    auto* function = reinterpret_cast<jstring>(env->GetObjectArrayElement(functions, j));
                    auto* functionName = env->GetStringUTFChars(function, nullptr);
                    if (*functionName == '\0')
                    {
                        auto* type = llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0);
                        globals.push_back(llvm::ConstantPointerNull::get(type));
                        continue;
                    }
                    globals.push_back(module->getFunction(functionName));
                    env->ReleaseStringUTFChars(function, functionName);
                }
                auto* ty = llvm::ArrayType::get(llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0),
                                                globals.size());
                globalVariable = new llvm::GlobalVariable(*module, ty, false, llvm::GlobalValue::ExternalLinkage,
                                                          llvm::ConstantArray::get(ty, globals), name);
            }
            else if (env->IsInstanceOf(value, irInterfaceTableClazz))
            {
                auto* queue = reinterpret_cast<std::queue<std::pair<std::string, std::string>>*>(env->GetLongField(
                    thisPtr, env->GetFieldID(clazz, "queue", "J")));
                auto* entries = reinterpret_cast<jobjectArray>(env->GetObjectField(
                    value, env->GetFieldID(irInterfaceTableClazz, "entries",
                                           "[Lldk/l/lg/ir/operand/IRInterfaceTable$Entry;")));
                std::vector<llvm::Constant*> globals;
                for (int j = 0; j < env->GetArrayLength(entries); ++j)
                {
                    auto* entry = env->GetObjectArrayElement(entries, j);
                    auto* entryClazz = env->GetObjectClass(entry);
                    auto* functions = reinterpret_cast<jobjectArray>(env->CallObjectMethod(
                        entry, env->GetMethodID(entryClazz, "functions", "()[Ljava/lang/String;")));
                    auto* entryNameObject = reinterpret_cast<jstring>(env->CallObjectMethod(
                        entry, env->GetMethodID(entryClazz, "name", "()Ljava/lang/String;")));
                    auto* entryName = env->GetStringUTFChars(entryNameObject, nullptr);

                    std::string globalVarName = std::string(name) + "_entry_" + std::to_string(j);

                    std::vector<llvm::Constant*> ptrs;
                    auto* voidPtrType = llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0);
                    ptrs.push_back(llvm::ConstantPointerNull::get(voidPtrType));
                    queue->emplace(globalVarName, std::string("<class_instance ") + entryName + ">");
                    env->ReleaseStringUTFChars(entryNameObject, entryName);
                    for (int k = 0; k < env->GetArrayLength(functions); ++k)
                    {
                        auto* function = reinterpret_cast<jstring>(env->GetObjectArrayElement(functions, k));
                        auto* functionName = env->GetStringUTFChars(function, nullptr);
                        if (*functionName == '\0')
                        {
                            ptrs.push_back(llvm::ConstantPointerNull::get(voidPtrType));
                            continue;
                        }
                        ptrs.push_back(module->getFunction(functionName));
                        env->ReleaseStringUTFChars(function, functionName);
                    }
                    auto* type = llvm::ArrayType::get(llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0),
                                                      ptrs.size());
                    globals.push_back(new llvm::GlobalVariable(*module, type, false,
                                                               llvm::GlobalValue::ExternalLinkage,
                                                               llvm::ConstantArray::get(type, ptrs),
                                                               std::string(name) + "_entry_" + std::to_string(j)));
                }
                auto* ty = llvm::ArrayType::get(llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0),
                                                globals.size());
                globalVariable = new llvm::GlobalVariable(*module, ty, false, llvm::GlobalValue::ExternalLinkage,
                                                          llvm::ConstantArray::get(ty, globals), name);
            }
        }
    }

    env->ReleaseStringUTFChars(nameObject, name);

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitFunction(
    JNIEnv* env, jobject thisPtr, jobject irFunction, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->
        GetLongField(thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));

    jclass irFunctionClazz = env->GetObjectClass(irFunction);
    const auto name = reinterpret_cast<jstring>(env->
        GetObjectField(irFunction, env->GetFieldID(irFunctionClazz, "name", "Ljava/lang/String;")));
    jlong argumentCount = env->GetLongField(irFunction, env->GetFieldID(irFunctionClazz, "argumentCount", "J"));
    const auto fields = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irFunction, env->GetFieldID(irFunctionClazz, "fields", "[Lldk/l/lg/ir/structure/IRField;")));

    auto* functionName = env->GetStringUTFChars(name, nullptr);
    auto* function = module->getFunction(functionName);
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "currentFunction", "J"), reinterpret_cast<jlong>(function));

    auto* stack = new std::stack<llvm::Value*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "stack", "J"), reinterpret_cast<jlong>(stack));

    auto* basicBlockMap = new std::map<std::string, llvm::BasicBlock*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "basicBlockMap", "J"), reinterpret_cast<jlong>(basicBlockMap));

    auto* virtualRegister2Value = new std::map<std::string, llvm::Value*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J"),
                      reinterpret_cast<jlong>(virtualRegister2Value));

    auto* field2LocalVar = new std::map<std::string, llvm::AllocaInst*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "field2LocalVar", "J"), reinterpret_cast<jlong>(field2LocalVar));

    auto* initBlock = llvm::BasicBlock::Create(*context, "init", function);
    builder->SetInsertPoint(initBlock);
    int i;
    for (i = 0; i < argumentCount; i++)
    {
        auto* field = env->GetObjectArrayElement(fields, i);
        auto* irFieldClazz = env->GetObjectClass(field);
        auto* fieldNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            field, env->GetFieldID(irFieldClazz, "name", "Ljava/lang/String;")));
        auto* fieldName = env->GetStringUTFChars(fieldNameObject, nullptr);
        auto* inst = builder->CreateAlloca(getType(
                                               env, env->GetObjectField(
                                                   field, env->GetFieldID(
                                                       irFieldClazz, "type", "Lldk/l/lg/ir/type/IRType;")),
                                               context), nullptr, fieldName);
        inst->setAlignment(llvm::Align(1));
        auto* arg = function->getArg(i);
        builder->CreateStore(arg, inst);

        field2LocalVar->insert(std::make_pair(std::string(env->GetStringUTFChars(
                                                  reinterpret_cast<jstring>(env->GetObjectField(
                                                      field, env->GetFieldID(
                                                          env->GetObjectClass(field), "name", "Ljava/lang/String;"))),
                                                  nullptr)), inst));
        env->ReleaseStringUTFChars(fieldNameObject, fieldName);
    }
    for (; i < env->GetArrayLength(fields); i++)
    {
        auto* field = env->GetObjectArrayElement(fields, i);
        auto* irFieldClazz = env->GetObjectClass(field);
        auto* fieldNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            field, env->GetFieldID(irFieldClazz, "name", "Ljava/lang/String;")));
        auto* fieldName = env->GetStringUTFChars(fieldNameObject, nullptr);
        auto* inst = builder->CreateAlloca(getType(
                                               env, env->GetObjectField(
                                                   field, env->GetFieldID(
                                                       irFieldClazz, "type", "Lldk/l/lg/ir/type/IRType;")),
                                               context), nullptr, fieldName);
        inst->setAlignment(llvm::Align(1));

        field2LocalVar->insert(std::make_pair(std::string(env->GetStringUTFChars(
                                                  reinterpret_cast<jstring>(env->GetObjectField(
                                                      field, env->GetFieldID(
                                                          env->GetObjectClass(field), "name", "Ljava/lang/String;"))),
                                                  nullptr)), inst));
        env->ReleaseStringUTFChars(fieldNameObject, fieldName);
    }

    int basicBlockCount = 0;

    auto* cfg = env->GetObjectField(
        irFunction, env->GetFieldID(irFunctionClazz, "controlFlowGraph", "Lldk/l/lg/ir/base/IRControlFlowGraph;"));
    env->SetObjectField(
        thisPtr, env->GetFieldID(clazz, "currentCFG", "Lldk/l/lg/ir/base/IRControlFlowGraph;"), cfg);

    auto* basicBlocks = env->GetObjectField(
        cfg, env->GetFieldID(env->GetObjectClass(cfg), "basicBlocks", "Ljava/util/Map;"));
    auto* values = env->CallObjectMethod(basicBlocks,
                                         env->GetMethodID(env->GetObjectClass(basicBlocks), "values",
                                                          "()Ljava/util/Collection;"));
    auto* iterator = env->CallObjectMethod(
        values, env->GetMethodID(env->GetObjectClass(values), "iterator", "()Ljava/util/Iterator;"));
    jmethodID hasNextMethod = env->GetMethodID(env->GetObjectClass(iterator), "hasNext", "()Z");
    jmethodID nextMethod = env->GetMethodID(env->GetObjectClass(iterator), "next", "()Ljava/lang/Object;");
    while (env->CallBooleanMethod(iterator, hasNextMethod))
    {
        auto* llvmIRBasicBlock = llvm::BasicBlock::Create(*context, "basicBlock_" + std::to_string(basicBlockCount++),
                                                          function);
        auto* basicBlock = env->CallObjectMethod(iterator, nextMethod);
        auto* basicBlockName = reinterpret_cast<jstring>(env->GetObjectField(
            basicBlock, env->GetFieldID(env->GetObjectClass(basicBlock), "name", "Ljava/lang/String;")));
        auto* cString = env->GetStringUTFChars(basicBlockName, nullptr);
        basicBlockMap->insert(std::make_pair(cString, llvmIRBasicBlock));
    }
    iterator = env->CallObjectMethod(
        values, env->GetMethodID(env->GetObjectClass(values), "iterator", "()Ljava/util/Iterator;"));
    hasNextMethod = env->GetMethodID(env->GetObjectClass(iterator), "hasNext", "()Z");
    nextMethod = env->GetMethodID(env->GetObjectClass(iterator), "next", "()Ljava/lang/Object;");
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    jfieldID currentBasicBlockFieldID = env->GetFieldID(clazz, "currentBasicBlock",
                                                        "Lldk/l/lg/ir/base/IRControlFlowGraph$BasicBlock;");
    while (env->CallBooleanMethod(iterator, hasNextMethod))
    {
        auto* basicBlock = env->CallObjectMethod(iterator, nextMethod);
        env->SetObjectField(thisPtr, currentBasicBlockFieldID, basicBlock);
        auto* basicBlockName = reinterpret_cast<jstring>(env->GetObjectField(
            basicBlock, env->GetFieldID(env->GetObjectClass(basicBlock), "name", "Ljava/lang/String;")));
        auto* cString = env->GetStringUTFChars(basicBlockName, nullptr);
        builder->SetInsertPoint(basicBlockMap->at(cString));
        auto* instructions = env->GetObjectField(
            basicBlock, env->GetFieldID(env->GetObjectClass(basicBlock), "instructions", "Ljava/util/List;"));
        auto* iterator2 = env->CallObjectMethod(
            instructions,
            env->GetMethodID(env->GetObjectClass(instructions), "iterator", "()Ljava/util/Iterator;"));
        auto* isEmptyMethod = env->GetMethodID(env->GetObjectClass(instructions), "isEmpty", "()Z");
        jmethodID hasNextMethod2 = env->GetMethodID(env->GetObjectClass(iterator2), "hasNext", "()Z");
        jmethodID nextMethod2 = env->GetMethodID(env->GetObjectClass(iterator2), "next", "()Ljava/lang/Object;");
        if (env->CallBooleanMethod(instructions, isEmptyMethod))
        {
            auto* doNothing = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::donothing, {});
            builder->CreateCall(doNothing, {});
        }
        else
        {
            while (env->CallBooleanMethod(iterator2, hasNextMethod2))
            {
                auto* instruction = env->CallObjectMethod(iterator2, nextMethod2);
                env->CallObjectMethod(thisPtr, visitMethod, instruction, additional);
                while (!stack->empty()) stack->pop();
            }
        }
        env->ReleaseStringUTFChars(basicBlockName, cString);
    }

    for (auto it = function->begin(); it != function->end(); ++it)
    {
        auto tmp = it;
        ++tmp;
        auto end = it->end();
        --end;
        if (tmp != function->end() && (it->empty() || (!llvm::isa<llvm::BranchInst>(end) && !llvm::isa<
            llvm::ReturnInst>(end))))
        {
            builder->SetInsertPoint(&*it);
            builder->CreateBr(&*tmp);
        }
    }

    env->ReleaseStringUTFChars(name, functionName);

    delete virtualRegister2Value;
    delete basicBlockMap;
    delete stack;

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitReturn(
    JNIEnv* env, jobject thisPtr, jobject irReturn, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));

    auto* value = env->GetObjectField(
        irReturn, env->GetFieldID(env->GetObjectClass(irReturn), "value", "Lldk/l/lg/ir/operand/IROperand;"));
    if (value != nullptr)
    {
        jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                                 "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
        env->CallObjectMethod(thisPtr, visitMethod, value, additional);
        auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
            thisPtr, env->GetFieldID(clazz, "stack", "J")));
        if (stack->empty())
        {
            // TODO dump error
            return nullptr;
        }
        builder->CreateRet(stack->top());
        stack->pop();
    }
    else
    {
        builder->CreateRetVoid();
    }

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitGoto(
    JNIEnv* env, jobject thisPtr, jobject irGoto, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* basicBlockMap = reinterpret_cast<std::map<std::string, llvm::BasicBlock*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "basicBlockMap", "J")));
    auto* name = reinterpret_cast<jstring>(env->GetObjectField(
        irGoto, env->GetFieldID(env->GetObjectClass(irGoto), "target", "Ljava/lang/String;")));
    auto* cString = env->GetStringUTFChars(name, nullptr);
    builder->CreateBr(basicBlockMap->at(cString));
    env->ReleaseStringUTFChars(name, cString);
    return nullptr;
}

JNIEXPORT jobject JNICALL
Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitConditionalJump
(JNIEnv* env, jobject thisPtr, jobject irConditionalJump, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* basicBlockMap = reinterpret_cast<std::map<std::string, llvm::BasicBlock*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "basicBlockMap", "J")));

    auto* irConditionalJumpClazz = env->GetObjectClass(irConditionalJump);
    jboolean isAtomic = env->GetBooleanField(irConditionalJump,
                                             env->GetFieldID(irConditionalJumpClazz, "isAtomic", "Z"));
    auto* condition = env->GetObjectField(
        irConditionalJump, env->GetFieldID(irConditionalJumpClazz, "condition", "Lldk/l/lg/ir/base/IRCondition;"));
    auto* typeObject = env->GetObjectField(
        irConditionalJump, env->GetFieldID(irConditionalJumpClazz, "type", "Lldk/l/lg/ir/type/IRType;"));
    auto* operand1Object = env->GetObjectField(
        irConditionalJump, env->GetFieldID(irConditionalJumpClazz, "operand1", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* operand2Object = env->GetObjectField(
        irConditionalJump, env->GetFieldID(irConditionalJumpClazz, "operand2", "Lldk/l/lg/ir/operand/IROperand;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    auto* textObject = reinterpret_cast<jstring>(env->GetObjectField(
        condition, env->GetFieldID(env->GetObjectClass(condition), "text", "Ljava/lang/String;")));
    auto* text = env->GetStringUTFChars(textObject, nullptr);
    env->CallObjectMethod(thisPtr, visitMethod, operand1Object, additional);
    if (stack->empty()) return nullptr;
    auto* operand1 = stack->top();
    stack->pop();
    llvm::Value* operand2;
    if (operand2Object != nullptr)
    {
        env->CallObjectMethod(thisPtr, visitMethod, operand2Object, additional);
        if (stack->empty()) return nullptr;
        operand2 = stack->top();
        stack->pop();
    }
    else
    {
        operand2 = nullptr;
    }
    auto* name = reinterpret_cast<jstring>(env->GetObjectField(
        irConditionalJump,
        env->GetFieldID(env->GetObjectClass(irConditionalJump), "target", "Ljava/lang/String;")));
    auto* target = env->GetStringUTFChars(name, nullptr);
    if (isAtomic)
    {
    }
    else
    {
        llvm::Value* ret;
        if ((strcmp(text, "if_true") == 0) || (strcmp(text, "if_false") == 0))
        {
            ret = operand1;
        }
        else
        {
            jclass irIntegerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRIntegerType");
            auto* irPointerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRPointerType");
            if (env->IsInstanceOf(typeObject, irIntegerTypeClazz))
            {
                jboolean isUnsigned = env->GetBooleanField(
                    typeObject, env->GetFieldID(env->GetObjectClass(typeObject), "unsigned", "Z"));
                if (strcmp(text, "e") == 0)
                {
                    ret = builder->CreateICmpEQ(operand1, operand2);
                }
                else if (strcmp(text, "ne") == 0)
                {
                    ret = builder->CreateICmpNE(operand1, operand2);
                }
                else if (strcmp(text, "g") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpUGT(operand1, operand2);
                    else
                        ret = builder->CreateICmpSGT(operand1, operand2);
                }
                else if (strcmp(text, "ge") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpUGE(operand1, operand2);
                    else
                        ret = builder->CreateICmpSGE(operand1, operand2);
                }
                else if (strcmp(text, "l") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpULT(operand1, operand2);
                    else
                        ret = builder->CreateICmpSLT(operand1, operand2);
                }
                else if (strcmp(text, "le") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpULE(operand1, operand2);
                    else
                        ret = builder->CreateICmpSLE(operand1, operand2);
                }
                else
                {
                    ret = nullptr;
                }
            }
            else if (env->IsInstanceOf(typeObject, irPointerTypeClazz))
            {
                if (strcmp(text, "e") == 0)
                {
                    ret = builder->CreateICmpEQ(operand1, operand2);
                }
                else if (strcmp(text, "ne") == 0)
                {
                    ret = builder->CreateICmpNE(operand1, operand2);
                }
                else if (strcmp(text, "g") == 0)
                {
                    ret = builder->CreateICmpUGT(operand1, operand2);
                }
                else if (strcmp(text, "ge") == 0)
                {
                    ret = builder->CreateICmpUGE(operand1, operand2);
                }
                else if (strcmp(text, "l") == 0)
                {
                    ret = builder->CreateICmpULT(operand1, operand2);
                }
                else if (strcmp(text, "le") == 0)
                {
                    ret = builder->CreateICmpULE(operand1, operand2);
                }
                else
                {
                    ret = nullptr;
                }
            }
            else
            {
                if (strcmp(text, "e") == 0)
                {
                    ret = builder->CreateFCmpOEQ(operand1, operand2);
                }
                else if (strcmp(text, "ne") == 0)
                {
                    ret = builder->CreateFCmpUNE(operand1, operand2);
                }
                else if (strcmp(text, "g") == 0)
                {
                    ret = builder->CreateFCmpOGT(operand1, operand2);
                }
                else if (strcmp(text, "ge") == 0)
                {
                    ret = builder->CreateFCmpOGE(operand1, operand2);
                }
                else if (strcmp(text, "l") == 0)
                {
                    ret = builder->CreateFCmpOLT(operand1, operand2);
                }
                else if (strcmp(text, "le") == 0)
                {
                    ret = builder->CreateFCmpOLE(operand1, operand2);
                }
                else
                {
                    ret = nullptr;
                }
            }
        }
        auto* currentBasicBlock = env->GetObjectField(
            thisPtr,
            env->GetFieldID(clazz, "currentBasicBlock", "Lldk/l/lg/ir/base/IRControlFlowGraph$BasicBlock;"));
        auto* currentCFG = env->GetObjectField(
            thisPtr, env->GetFieldID(clazz, "currentCFG", "Lldk/l/lg/ir/base/IRControlFlowGraph;"));
        auto* basicBlocks = env->GetObjectField(
            currentCFG, env->GetFieldID(env->GetObjectClass(currentCFG), "basicBlocks", "Ljava/util/Map;"));
        auto* values = env->CallObjectMethod(basicBlocks,
                                             env->GetMethodID(env->GetObjectClass(basicBlocks), "values",
                                                              "()Ljava/util/Collection;"));
        auto* stream = env->CallObjectMethod(values,
                                             env->GetMethodID(env->GetObjectClass(values), "stream",
                                                              "()Ljava/util/stream/Stream;"));
        auto* list = env->CallObjectMethod(stream,
                                           env->GetMethodID(env->GetObjectClass(stream), "toList",
                                                            "()Ljava/util/List;"));
        jint index = env->CallIntMethod(
            list, env->GetMethodID(env->GetObjectClass(list), "indexOf", "(Ljava/lang/Object;)I"),
            currentBasicBlock);
        auto* nextBasicBlock = env->CallObjectMethod(
            list, env->GetMethodID(env->GetObjectClass(list), "get", "(I)Ljava/lang/Object;"), index + 1);
        auto* nextBasicBlockNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            nextBasicBlock, env->GetFieldID(env->GetObjectClass(nextBasicBlock), "name", "Ljava/lang/String;")));
        auto* nextBasicBlockName = env->GetStringUTFChars(nextBasicBlockNameObject, nullptr);
        if (strcmp(text, "if_false") == 0)
        {
            builder->CreateCondBr(ret, basicBlockMap->at(nextBasicBlockName), basicBlockMap->at(target));
        }
        else
        {
            builder->CreateCondBr(ret, basicBlockMap->at(target), basicBlockMap->at(nextBasicBlockName));
        }
    }
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitCalculate(
    JNIEnv* env, jobject thisPtr, jobject irCalculate, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irCalculateClazz = env->GetObjectClass(irCalculate);
    jboolean isAtomic = env->GetBooleanField(irCalculate, env->GetFieldID(irCalculateClazz, "isAtomic", "Z"));
    auto* operatorObject = env->GetObjectField(
        irCalculate,
        env->GetFieldID(irCalculateClazz, "operator", "Lldk/l/lg/ir/instruction/IRCalculate$Operator;"));
    auto* typeObject = env->GetObjectField(irCalculate,
                                           env->GetFieldID(irCalculateClazz, "type", "Lldk/l/lg/ir/type/IRType;"));
    auto* operand1Object = env->GetObjectField(irCalculate,
                                               env->GetFieldID(irCalculateClazz, "operand1",
                                                               "Lldk/l/lg/ir/operand/IROperand;"));
    auto* operand2Object = env->GetObjectField(irCalculate, env->GetFieldID(irCalculateClazz, "operand2",
                                                                            "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegister = env->GetObjectField(irCalculate,
                                               env->GetFieldID(irCalculateClazz, "target",
                                                               "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    auto* textObject = reinterpret_cast<jstring>(env->GetObjectField(
        operatorObject, env->GetFieldID(env->GetObjectClass(operatorObject), "text", "Ljava/lang/String;")));
    auto* text = env->GetStringUTFChars(textObject, nullptr);
    env->CallObjectMethod(thisPtr, visitMethod, operand1Object, additional);
    if (stack->empty()) return nullptr;
    auto* operand1 = stack->top();
    stack->pop();
    env->CallObjectMethod(thisPtr, visitMethod, operand2Object, additional);
    if (stack->empty()) return nullptr;
    auto* operand2 = stack->top();
    stack->pop();

    auto* irPointerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRPointerType");
    if (isAtomic)
    {
    }
    else
    {
        llvm::Value* result;
        if (strcmp(text, "add") == 0)
        {
            if (env->IsInstanceOf(typeObject, irPointerTypeClazz))
            {
                result = builder->CreateGEP(getType(env, typeObject, context), operand1, operand2);
            }
            else
            {
                result = builder->CreateAdd(operand1, operand2);
            }
        }
        else if (strcmp(text, "sub") == 0)
        {
            if (env->IsInstanceOf(typeObject, irPointerTypeClazz))
            {
                auto* offset = builder->CreateSub(llvm::ConstantInt::get(operand2->getType(), 0), operand2);
                result = builder->CreateGEP(getType(env, typeObject, context), operand1, offset);
            }
            else
            {
                result = builder->CreateSub(operand1, operand2);
            }
        }
        else if (strcmp(text, "mul") == 0)
            result = builder->CreateMul(operand1, operand2);
        else if (strcmp(text, "div") == 0)
            result = builder->CreateUDiv(operand1, operand2);
        else if (strcmp(text, "mod") == 0)
            result = builder->CreateURem(operand1, operand2);
        else if (strcmp(text, "and") == 0)
            result = builder->CreateAnd(operand1, operand2);
        else if (strcmp(text, "or") == 0)
            result = builder->CreateOr(operand1, operand2);
        else if (strcmp(text, "xor") == 0)
            result = builder->CreateXor(operand1, operand2);
        else if (strcmp(text, "shl") == 0)
            result = builder->CreateShl(operand1, operand2);
        else if (strcmp(text, "shr") == 0)
            result = builder->CreateAShr(operand1, operand2);
        else if (strcmp(text, "ushr") == 0)
            result = builder->CreateLShr(operand1, operand2);
        auto targetRegisterNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
        virtualRegister2Value->insert(
            std::make_pair(std::string(env->GetStringUTFChars(targetRegisterNameObject, nullptr)), result));
    }
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitNot
(JNIEnv* env, jobject thisPtr, jobject irNot, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irNotClazz = env->GetObjectClass(irNot);
    jboolean isAtomic = env->GetBooleanField(irNot, env->GetFieldID(irNotClazz, "isAtomic", "Z"));
    jobject operandObject = env->GetObjectField(irNot, env->GetFieldID(irNotClazz, "operand",
                                                                       "Lldk/l/lg/ir/operand/IROperand;"));
    jobject targetRegister = env->GetObjectField(irNot, env->GetFieldID(irNotClazz, "target",
                                                                        "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, operandObject, additional);
    if (stack->empty()) return nullptr;
    auto* operand = stack->top();
    stack->pop();
    if (isAtomic)
    {
    }
    else
    {
        auto* result = builder->CreateNot(operand);
        auto targetRegisterNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
        virtualRegister2Value->insert(
            std::make_pair(std::string(env->GetStringUTFChars(targetRegisterNameObject, nullptr)), result));
    }
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitNegate
(JNIEnv* env, jobject thisPtr, jobject irNegate, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irNotClazz = env->GetObjectClass(irNegate);
    jboolean isAtomic = env->GetBooleanField(irNegate, env->GetFieldID(irNotClazz, "isAtomic", "Z"));
    jobject operandObject = env->GetObjectField(irNegate, env->GetFieldID(irNotClazz, "operand",
                                                                          "Lldk/l/lg/ir/operand/IROperand;"));
    jobject targetRegister = env->GetObjectField(irNegate, env->GetFieldID(irNotClazz, "target",
                                                                           "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, operandObject, additional);
    if (stack->empty()) return nullptr;
    auto* operand = stack->top();
    stack->pop();
    if (isAtomic)
    {
    }
    else
    {
        auto* result = builder->CreateNeg(operand);
        auto target = reinterpret_cast<jstring>(env->GetObjectField(
            targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
        virtualRegister2Value->insert(
            std::make_pair(std::string(env->GetStringUTFChars(target, nullptr)), result));
    }
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitIncrease(
    JNIEnv* env, jobject thisPtr, jobject irIncrease, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irIncreaseClazz = env->GetObjectClass(irIncrease);
    auto* typeObject = env->GetObjectField(irIncrease, env->GetFieldID(irIncreaseClazz, "type",
                                                                       "Lldk/l/lg/ir/type/IRType;"));
    auto* operandObject = env->GetObjectField(irIncrease, env->GetFieldID(irIncreaseClazz, "operand",
                                                                          "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegister = env->GetObjectField(irIncrease, env->GetFieldID(irIncreaseClazz, "target",
                                                                           "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, operandObject, additional);
    if (stack->empty()) return nullptr;
    auto* operand = stack->top();
    stack->pop();

    auto* type = getType(env, typeObject, context);
    if (targetRegister == nullptr)
    {
        // atomic
        // temporary solution
        auto* value = builder->CreateLoad(type, operand);
        auto* result = builder->CreateAdd(value, llvm::ConstantInt::get(type, 1));
        builder->CreateStore(result, operand);
    }
    else
    {
        auto* result = builder->CreateAdd(operand, llvm::ConstantInt::get(type, 1));
        auto* target = reinterpret_cast<jstring>(env->GetObjectField(
            targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
        virtualRegister2Value->insert(
            std::make_pair(std::string(env->GetStringUTFChars(target, nullptr)), result));
    }

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitDecrease(
    JNIEnv* env, jobject thisPtr, jobject irDecrease, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irDecreaseClazz = env->GetObjectClass(irDecrease);
    auto* typeObject = env->GetObjectField(irDecrease, env->GetFieldID(irDecreaseClazz, "type",
                                                                       "Lldk/l/lg/ir/type/IRType;"));
    auto* operandObject = env->GetObjectField(irDecrease, env->GetFieldID(irDecreaseClazz, "operand",
                                                                          "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegister = env->GetObjectField(irDecrease, env->GetFieldID(irDecreaseClazz, "target",
                                                                           "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, operandObject, additional);
    if (stack->empty()) return nullptr;
    auto* operand = stack->top();
    stack->pop();

    auto* type = getType(env, typeObject, context);
    if (targetRegister == nullptr)
    {
        // atomic
        // temporary solution
        auto* value = builder->CreateLoad(type, operand);
        auto* result = builder->CreateSub(value, llvm::ConstantInt::get(type, 1));
        builder->CreateStore(result, operand);
    }
    else
    {
        auto* result = builder->CreateSub(operand, llvm::ConstantInt::get(type, 1));
        auto* target = reinterpret_cast<jstring>(env->GetObjectField(
            targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
        virtualRegister2Value->insert(
            std::make_pair(std::string(env->GetStringUTFChars(target, nullptr)), result));
    }

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitSet(
    JNIEnv* env, jobject thisPtr, jobject irSet, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->
        GetLongField(thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* irSetClazz = env->GetObjectClass(irSet);
    auto* addressObject = env->GetObjectField(irSet, env->GetFieldID(irSetClazz, "address",
                                                                     "Lldk/l/lg/ir/operand/IROperand;"));
    auto* valueObject = env->GetObjectField(irSet, env->GetFieldID(irSetClazz, "value",
                                                                   "Lldk/l/lg/ir/operand/IROperand;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, addressObject, additional);
    if (stack->empty()) return nullptr;
    auto* address = stack->top();
    stack->pop();
    env->CallObjectMethod(thisPtr, visitMethod, valueObject, additional);
    if (stack->empty()) return nullptr;
    auto* value = stack->top();
    stack->pop();
    auto* inst = builder->CreateStore(value, address);
    inst->setAlignment(llvm::Align(1));

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitGet(
    JNIEnv* env, jobject thisPtr, jobject irGet, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irGetClazz = env->GetObjectClass(irGet);
    auto* typeObject = env->GetObjectField(irGet, env->GetFieldID(irGetClazz, "type",
                                                                  "Lldk/l/lg/ir/type/IRType;"));
    auto* addressObject = env->GetObjectField(irGet, env->GetFieldID(irGetClazz, "address",
                                                                     "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegister = env->GetObjectField(irGet, env->GetFieldID(irGetClazz, "target",
                                                                      "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, addressObject, additional);
    if (stack->empty()) return nullptr;
    auto* address = stack->top();
    stack->pop();
    auto* type = getType(env, typeObject, context);
    auto* result = builder->CreateLoad(type, address);
    result->setAlignment(llvm::Align(1));
    auto* targetRegisterNameObject = reinterpret_cast<jstring>(env->GetObjectField(
        targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
    virtualRegister2Value->insert(
        std::make_pair(std::string(env->GetStringUTFChars(targetRegisterNameObject, nullptr)), result));
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitInvoke(
    JNIEnv* env, jobject thisPtr, jobject irInvoke, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irInvokeClazz = env->GetObjectClass(irInvoke);
    auto* returnTypeObject = env->GetObjectField(irInvoke, env->GetFieldID(irInvokeClazz, "returnType",
                                                                           "Lldk/l/lg/ir/type/IRType;"));
    auto* addressObject = env->GetObjectField(irInvoke, env->GetFieldID(irInvokeClazz, "address",
                                                                        "Lldk/l/lg/ir/operand/IROperand;"));
    auto* argumentTypesObject = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irInvoke, env->GetFieldID(irInvokeClazz, "argumentTypes",
                                  "[Lldk/l/lg/ir/type/IRType;")));
    auto* argumentsObject = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irInvoke, env->GetFieldID(irInvokeClazz, "arguments",
                                  "[Lldk/l/lg/ir/operand/IROperand;")));
    auto* targetRegisterObject = env->GetObjectField(irInvoke, env->GetFieldID(irInvokeClazz, "target",
                                                                               "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");

    env->CallObjectMethod(thisPtr, visitMethod, addressObject, additional);
    if (stack->empty()) return nullptr;
    auto* address = stack->top();
    stack->pop();

    auto* returnType = getType(env, returnTypeObject, context);
    std::vector<llvm::Type*> argumentTypes;
    for (int i = 0; i < env->GetArrayLength(argumentTypesObject); i++)
    {
        auto* argumentTypeObject = env->GetObjectArrayElement(argumentTypesObject, i);
        auto* argumentType = getType(env, argumentTypeObject, context);
        argumentTypes.push_back(argumentType);
    }
    llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, argumentTypes, false);

    std::vector<llvm::Value*> arguments;
    for (int i = 0; i < env->GetArrayLength(argumentsObject); i++)
    {
        auto* argumentObject = env->GetObjectArrayElement(argumentsObject, i);
        env->CallObjectMethod(thisPtr, visitMethod, argumentObject, additional);
        if (stack->empty()) return nullptr;
        auto* argument = stack->top();
        stack->pop();
        arguments.push_back(argument);
    }

    auto* result = builder->CreateCall(functionType, address, arguments);

    if (targetRegisterObject != nullptr)
    {
        auto* targetRegisterNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            targetRegisterObject,
            env->GetFieldID(env->GetObjectClass(targetRegisterObject), "name", "Ljava/lang/String;")));
        virtualRegister2Value->insert(
            std::make_pair(std::string(env->GetStringUTFChars(targetRegisterNameObject, nullptr)), result));
    }

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitStackAllocate(
    JNIEnv* env, jobject thisPtr, jobject irStackAllocate, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irStackAllocateClazz = env->GetObjectClass(irStackAllocate);
    auto* sizeObject = env->GetObjectField(irStackAllocate, env->GetFieldID(irStackAllocateClazz, "size",
                                                                            "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegister = env->GetObjectField(irStackAllocate, env->GetFieldID(irStackAllocateClazz, "target",
                                                   "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, sizeObject, additional);
    if (stack->empty()) return nullptr;
    auto* size = stack->top();
    stack->pop();

    auto* result = builder->CreateAlloca(llvm::Type::getInt8Ty(*context), size);
    auto* target = reinterpret_cast<jstring>(env->GetObjectField(
        targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
    virtualRegister2Value->insert(
        std::make_pair(std::string(env->GetStringUTFChars(target, nullptr)), result));
    return nullptr;
}

JNIEXPORT jobject JNICALL
Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitSetVirtualRegister(
    JNIEnv* env, jobject thisPtr, jobject irSetVirtualRegister, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    jclass irSetVirtualRegisterClazz = env->GetObjectClass(irSetVirtualRegister);
    auto* sourceObject = env->GetObjectField(irSetVirtualRegister, env->GetFieldID(irSetVirtualRegisterClazz, "source",
                                                 "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegister = env->GetObjectField(irSetVirtualRegister, env->GetFieldID(
                                                   irSetVirtualRegisterClazz, "target",
                                                   "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, sourceObject, additional);
    if (stack->empty()) return nullptr;
    auto* source = stack->top();
    stack->pop();

    auto* target = reinterpret_cast<jstring>(env->GetObjectField(
        targetRegister, env->GetFieldID(env->GetObjectClass(targetRegister), "name", "Ljava/lang/String;")));
    virtualRegister2Value->insert(
        std::make_pair(std::string(env->GetStringUTFChars(target, nullptr)), source));
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitAsm(
    JNIEnv* env, jobject thisPtr, jobject irAsm, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irAsmClazz = env->GetObjectClass(irAsm);
    auto* codeString = reinterpret_cast<jstring>(env->GetObjectField(
        irAsm, env->GetFieldID(irAsmClazz, "code", "Ljava/lang/String;")));
    auto* types = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irAsm, env->GetFieldID(irAsmClazz, "types", "[Lldk/l/lg/ir/type/IRType;")));
    auto* resources = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irAsm, env->GetFieldID(irAsmClazz, "resources", "[Lldk/l/lg/ir/operand/IROperand;")));
    auto* names = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irAsm, env->GetFieldID(irAsmClazz, "names", "[Ljava/lang/String;")));
    std::vector<llvm::Type*> argTypes;
    std::vector<llvm::Value*> args;
    std::string constraints;
    std::map<std::string, int> map;
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    for (jsize i = 0; i < env->GetArrayLength(types); ++i)
    {
        auto* type = env->GetObjectArrayElement(types, i);
        argTypes.push_back(getType(env, type, context));
        auto* resourceObject = env->GetObjectArrayElement(resources, i);
        env->CallObjectMethod(thisPtr, visitMethod, resourceObject, additional);
        if (stack->empty()) return nullptr;
        auto* resource = stack->top();
        stack->pop();
        args.push_back(resource);
        auto* name = env->GetStringUTFChars(reinterpret_cast<jstring>(env->GetObjectArrayElement(names, i)), nullptr);
        map.insert(std::make_pair(name, i));
    }
    for (jsize i = 0; i < env->GetArrayLength(types); ++i)
    {
        jobject resourceObject = env->GetObjectArrayElement(resources, i);

        const char* virtualRegisterName = "ldk/l/lg/ir/operand/IRVirtualRegister";
        if (env->IsInstanceOf(resourceObject, env->FindClass(virtualRegisterName)))
        {
            constraints += (i > 0 ? "," : "") + std::string("r"); // Register constraint
        }
        else if (env->IsInstanceOf(resourceObject, env->FindClass("ldk/l/lg/ir/operand/IRConstant")))
        {
            constraints += (i > 0 ? "," : "") + std::string("i");
        }
        else if (env->IsInstanceOf(resourceObject, env->FindClass("ldk/l/lg/ir/structure/IRField")))
        {
            constraints += (i > 0 ? "," : "") + std::string("m");
        }
        else
        {
            constraints += (i > 0 ? "," : "") + std::string("g");
        }
    }
    std::string code = env->GetStringUTFChars(codeString, nullptr);
    std::string newCode;
    int i = 0;
    while (i < env->GetStringUTFLength(codeString))
    {
        char c = code[i++];
        if (c == '@')
        {
            std::string name;
            while (true)
            {
                c = code[i];
                if (c == ',' || std::isspace(c)) break;
                name += c;
                i++;
            }
            newCode += "%";
            newCode += std::to_string(map[name]);
        }
        else
        {
            newCode += c;
        }
    }

    auto* functionType = llvm::FunctionType::get(llvm::Type::getVoidTy(*context), argTypes, false);
    auto* inlineAsm = llvm::InlineAsm::get(functionType, newCode, constraints, false);
    builder->CreateCall(inlineAsm, args);

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitTypeCast(
    JNIEnv* env, jobject thisPtr, jobject irTypeCast, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irTypeCastClazz = env->GetObjectClass(irTypeCast);
    auto* kind = env->GetObjectField(
        irTypeCast, env->GetFieldID(irTypeCastClazz, "kind", "Lldk/l/lg/ir/instruction/IRTypeCast$Kind;"));
    auto* sourceObject = env->GetObjectField(
        irTypeCast, env->GetFieldID(irTypeCastClazz, "source", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetType = env->GetObjectField(
        irTypeCast, env->GetFieldID(irTypeCastClazz, "targetType", "Lldk/l/lg/ir/type/IRType;"));
    auto* target = env->GetObjectField(
        irTypeCast, env->GetFieldID(irTypeCastClazz, "target", "Lldk/l/lg/ir/operand/IRVirtualRegister;"));

    auto* name = env->GetStringUTFChars(reinterpret_cast<jstring>(env->GetObjectField(
                                            kind, env->GetFieldID(env->GetObjectClass(kind), "name",
                                                                  "Ljava/lang/String;"))), nullptr);
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, sourceObject, additional);
    if (stack->empty()) return nullptr;
    auto* source = stack->top();
    stack->pop();

    llvm::Value* result;
    if (strcmp(name, "zext") == 0)
    {
        result = builder->CreateZExt(source, getType(env, targetType, context));
    }
    else if (strcmp(name, "trunc") == 0)
    {
        result = builder->CreateTrunc(source, getType(env, targetType, context));
    }
    else if (strcmp(name, "sext") == 0)
    {
        result = builder->CreateSExt(source, getType(env, targetType, context));
    }
    else if (strcmp(name, "itof") == 0)
    {
        result = builder->CreateSIToFP(source, getType(env, targetType, context));
    }
    else if (strcmp(name, "ftoi") == 0)
    {
        result = builder->CreateFPToSI(source, getType(env, targetType, context));
    }
    else if (strcmp(name, "fext") == 0)
    {
        result = builder->CreateFPExt(source, getType(env, targetType, context));
    }
    else if (strcmp(name, "ftrunc") == 0)
    {
        result = builder->CreateFPTrunc(source, getType(env, targetType, context));
    }
    auto* targetRegisterName = reinterpret_cast<jstring>(env->GetObjectField(
        target, env->GetFieldID(env->GetObjectClass(target), "name", "Ljava/lang/String;")));
    virtualRegister2Value->insert(std::make_pair(env->GetStringUTFChars(targetRegisterName, nullptr), result));

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitNoOperate
(JNIEnv* env, jobject thisPtr, jobject irNoOperate, jobject additional)
{
    jclass clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->
        GetLongField(thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* doNothing = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::donothing, {});
    builder->CreateCall(doNothing, {});
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitMalloc(
    JNIEnv* env, jobject thisPtr, jobject irMalloc, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irMallocClazz = env->GetObjectClass(irMalloc);
    auto* sizeObject = env->GetObjectField(
        irMalloc, env->GetFieldID(irMallocClazz, "size", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegisterObject = env->GetObjectField(
        irMalloc, env->GetFieldID(irMallocClazz, "target", "Lldk/l/lg/ir/operand/IRVirtualRegister;"));

    auto* visitMethod = env->GetMethodID(clazz, "visit",
                                         "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, sizeObject, additional);
    if (stack->empty()) return nullptr;
    auto* size = stack->top();
    stack->pop();
    auto* targetRegisterName = env->GetStringUTFChars(reinterpret_cast<jstring>(env->GetObjectField(
                                                          targetRegisterObject, env->GetFieldID(
                                                              env->GetObjectClass(targetRegisterObject), "name",
                                                              "Ljava/lang/String;"))), nullptr);
    virtualRegister2Value->insert(std::make_pair(targetRegisterName,
                                                 builder->CreateCall(module->getFunction("malloc"), {size})));

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitFree(
    JNIEnv* env, jobject thisPtr, jobject irFree, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irFreeClazz = env->GetObjectClass(irFree);
    auto* ptrOperand = env->GetObjectField(
        irFree, env->GetFieldID(irFreeClazz, "ptr", "Lldk/l/lg/ir/operand/IROperand;"));

    auto* visitMethod = env->GetMethodID(clazz, "visit",
                                         "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, ptrOperand, additional);
    if (stack->empty()) return nullptr;
    auto* ptr = stack->top();
    stack->pop();
    builder->CreateCall(module->getFunction("free"), {ptr});

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitRealloc(
    JNIEnv* env, jobject thisPtr, jobject irRealloc, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irReallocClazz = env->GetObjectClass(irRealloc);
    auto* ptrOperand = env->GetObjectField(
        irRealloc, env->GetFieldID(irReallocClazz, "ptr", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* sizeOperand = env->GetObjectField(
        irRealloc, env->GetFieldID(irReallocClazz, "size", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegisterObject = env->GetObjectField(
        irRealloc, env->GetFieldID(irReallocClazz, "target", "Lldk/l/lg/ir/operand/IRVirtualRegister;"));

    auto* visitMethod = env->GetMethodID(clazz, "visit",
                                         "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(thisPtr, visitMethod, ptrOperand, additional);
    if (stack->empty()) return nullptr;
    auto* ptr = stack->top();
    stack->pop();
    env->CallObjectMethod(thisPtr, visitMethod, sizeOperand, additional);
    if (stack->empty()) return nullptr;
    auto* size = stack->top();
    stack->pop();

    auto* targetRegisterName = env->GetStringUTFChars(reinterpret_cast<jstring>(env->GetObjectField(
                                                          targetRegisterObject, env->GetFieldID(
                                                              env->GetObjectClass(targetRegisterObject), "name",
                                                              "Ljava/lang/String;"))), nullptr);
    virtualRegister2Value->insert(std::make_pair(targetRegisterName,
                                                 builder->CreateCall(module->getFunction("realloc"), {ptr, size})));

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitCompare
(JNIEnv* env, jobject thisPtr, jobject irCompare, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));

    auto* irCompareClazz = env->GetObjectClass(irCompare);
    jboolean isAtomic = env->GetBooleanField(irCompare,
                                             env->GetFieldID(irCompareClazz, "isAtomic", "Z"));
    auto* condition = env->GetObjectField(
        irCompare, env->GetFieldID(irCompareClazz, "condition", "Lldk/l/lg/ir/base/IRCondition;"));
    auto* typeObject = env->GetObjectField(
        irCompare, env->GetFieldID(irCompareClazz, "type", "Lldk/l/lg/ir/type/IRType;"));
    auto* operand1Object = env->GetObjectField(
        irCompare, env->GetFieldID(irCompareClazz, "operand1", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* operand2Object = env->GetObjectField(
        irCompare, env->GetFieldID(irCompareClazz, "operand2", "Lldk/l/lg/ir/operand/IROperand;"));
    auto* targetRegisterObject = env->GetObjectField(
        irCompare, env->GetFieldID(irCompareClazz, "target", "Lldk/l/lg/ir/operand/IRVirtualRegister;"));
    jmethodID visitMethod = env->GetMethodID(clazz, "visit",
                                             "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    auto* textObject = reinterpret_cast<jstring>(env->GetObjectField(
        condition, env->GetFieldID(env->GetObjectClass(condition), "text", "Ljava/lang/String;")));
    auto* text = env->GetStringUTFChars(textObject, nullptr);
    env->CallObjectMethod(thisPtr, visitMethod, operand1Object, additional);
    if (stack->empty()) return nullptr;
    auto* operand1 = stack->top();
    stack->pop();
    env->CallObjectMethod(thisPtr, visitMethod, operand2Object, additional);
    if (stack->empty()) return nullptr;
    auto* operand2 = stack->top();
    stack->pop();
    if (isAtomic)
    {
    }
    else
    {
        llvm::Value* ret;
        if (strcmp(text, "if_true") == 0 || strcmp(text, "if_false") == 0)
        {
            ret = operand1;
        }
        else
        {
            jclass irIntegerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRIntegerType");
            auto* irPointerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRPointerType");
            if (env->IsInstanceOf(typeObject, irIntegerTypeClazz))
            {
                jboolean isUnsigned = env->GetBooleanField(
                    typeObject, env->GetFieldID(env->GetObjectClass(typeObject), "unsigned", "Z"));
                if (strcmp(text, "e") == 0)
                {
                    ret = builder->CreateICmpEQ(operand1, operand2);
                }
                else if (strcmp(text, "ne") == 0)
                {
                    ret = builder->CreateICmpNE(operand1, operand2);
                }
                else if (strcmp(text, "g") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpUGT(operand1, operand2);
                    else
                        ret = builder->CreateICmpSGT(operand1, operand2);
                }
                else if (strcmp(text, "ge") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpUGE(operand1, operand2);
                    else
                        ret = builder->CreateICmpSGE(operand1, operand2);
                }
                else if (strcmp(text, "l") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpULT(operand1, operand2);
                    else
                        ret = builder->CreateICmpSLT(operand1, operand2);
                }
                else if (strcmp(text, "le") == 0)
                {
                    if (isUnsigned)
                        ret = builder->CreateICmpULE(operand1, operand2);
                    else
                        ret = builder->CreateICmpSLE(operand1, operand2);
                }
                else
                {
                    ret = nullptr;
                }
            }
            else if (env->IsInstanceOf(typeObject, irPointerTypeClazz))
            {
                if (strcmp(text, "e") == 0)
                {
                    ret = builder->CreateICmpEQ(operand1, operand2);
                }
                else if (strcmp(text, "ne") == 0)
                {
                    ret = builder->CreateICmpNE(operand1, operand2);
                }
                else if (strcmp(text, "g") == 0)
                {
                    ret = builder->CreateICmpUGT(operand1, operand2);
                }
                else if (strcmp(text, "ge") == 0)
                {
                    ret = builder->CreateICmpUGE(operand1, operand2);
                }
                else if (strcmp(text, "l") == 0)
                {
                    ret = builder->CreateICmpULT(operand1, operand2);
                }
                else if (strcmp(text, "le") == 0)
                {
                    ret = builder->CreateICmpULE(operand1, operand2);
                }
                else
                {
                    return nullptr;
                }
            }
            else
            {
                if (strcmp(text, "e") == 0)
                {
                    ret = builder->CreateFCmpOEQ(operand1, operand2);
                }
                else if (strcmp(text, "ne") == 0)
                {
                    ret = builder->CreateFCmpUNE(operand1, operand2);
                }
                else if (strcmp(text, "g") == 0)
                {
                    ret = builder->CreateFCmpOGT(operand1, operand2);
                }
                else if (strcmp(text, "ge") == 0)
                {
                    ret = builder->CreateFCmpOGE(operand1, operand2);
                }
                else if (strcmp(text, "l") == 0)
                {
                    ret = builder->CreateFCmpOLT(operand1, operand2);
                }
                else if (strcmp(text, "le") == 0)
                {
                    ret = builder->CreateFCmpOLE(operand1, operand2);
                }
                else
                {
                    return nullptr;
                }
            }
        }
        auto* targetRegisterName = env->GetStringUTFChars(reinterpret_cast<jstring>(env->GetObjectField(
                                                              targetRegisterObject, env->GetFieldID(
                                                                  env->GetObjectClass(targetRegisterObject), "name",
                                                                  "Ljava/lang/String;"))), nullptr);
        virtualRegister2Value->insert(std::make_pair(targetRegisterName, ret));
    }

    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitPhi(
    JNIEnv* env, jobject thisPtr, jobject irPhi, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* module = reinterpret_cast<llvm::Module*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));
    auto* basicBlockMap = reinterpret_cast<std::map<std::string, llvm::BasicBlock*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "basicBlockMap", "J")));

    auto* irPhiClazz = env->GetObjectClass(irPhi);
    auto* typeObject = env->GetObjectField(
        irPhi, env->GetFieldID(irPhiClazz, "type", "Lldk/l/lg/ir/operand/IRType;"));
    auto* labels = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irPhi, env->GetFieldID(irPhiClazz, "labels", "[Ljava/lang/String;")));
    auto* values = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irPhi, env->GetFieldID(irPhiClazz, "values", "[Lldk/l/lg/ir/operand/IROperand;")));

    auto* type = getType(env, typeObject, context);
    auto* phi = builder->CreatePHI(type, env->GetArrayLength(labels));

    auto* visitMethod = env->GetMethodID(clazz, "visit",
                                         "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
    for (int i = 0; i < env->GetArrayLength(labels); i++)
    {
        auto* labelObject = reinterpret_cast<jstring>(env->GetObjectArrayElement(labels, i));
        auto* label = env->GetStringUTFChars(labelObject, nullptr);
        auto* basicBlock = basicBlockMap->at(label);

        auto* valueObject = reinterpret_cast<jobject>(env->GetObjectArrayElement(values, i));
        env->CallObjectMethod(thisPtr, visitMethod, valueObject, additional);
        if (stack->empty()) return nullptr;
        auto* value = stack->top();
        stack->pop();
        phi->addIncoming(value, basicBlock);
    }
    stack->push(phi);
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitConstant(
    JNIEnv* env, jobject thisPtr, jobject irConstant, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    jint index = env->GetIntField(irConstant, env->GetFieldID(env->GetObjectClass(irConstant), "index", "I"));

    auto* irModule = env->GetObjectField(thisPtr, env->GetFieldID(clazz, "module", "Lldk/l/lg/ir/IRModule;"));
    auto* irModuleClazz = env->GetObjectClass(irModule);
    auto* constantPool = env->GetObjectField(
        irModule, env->GetFieldID(irModuleClazz, "constantPool", "Lldk/l/lg/ir/IRConstantPool;"));
    auto* entry = env->CallObjectMethod(constantPool,
                                        env->GetMethodID(env->GetObjectClass(constantPool), "get",
                                                         "(I)Lldk/l/lg/ir/IRConstantPool$Entry;"), index);
    if (entry == nullptr)
    {
    }
    else
    {
        auto* entryClazz = env->GetObjectClass(entry);
        auto* type = env->GetObjectField(entry, env->GetFieldID(entryClazz, "type", "Lldk/l/lg/ir/type/IRType;"));
        auto* value = env->GetObjectField(entry, env->GetFieldID(entryClazz, "value", "Ljava/lang/Object;"));
        auto* irIntegerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRIntegerType");
        auto* irFloatTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRFloatType");
        auto* irDoubleTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRDoubleType");
        auto* irPointerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRPointerType");
        auto* jCharacterClazz = env->FindClass("java/lang/Character");
        auto* jNumberClazz = env->FindClass("java/lang/Number");
        if (env->IsInstanceOf(type, irIntegerTypeClazz))
        {
            jobject size = env->GetObjectField(
                type, env->GetFieldID(irIntegerTypeClazz, "size", "Lldk/l/lg/ir/type/IRIntegerType$Size;"));
            jlong realSize = env->GetLongField(size, env->GetFieldID(env->GetObjectClass(size), "size", "J"));
            if (realSize == 1)
            {
                auto* jBooleanClazz = env->FindClass("java/lang/Boolean");
                jboolean booleanValue = env->CallBooleanMethod(
                    value, env->GetMethodID(jBooleanClazz, "booleanValue", "()Z"));
                stack->push(llvm::ConstantInt::get(*context, llvm::APInt(1, 1, booleanValue)));
            }
            else if (env->IsInstanceOf(value, jCharacterClazz))
            {
                auto* string = env->CallObjectMethod(
                    value, env->GetMethodID(jCharacterClazz, "toString", "()Ljava/lang/String;"));
                auto* byteArray = reinterpret_cast<jbyteArray>(env->CallObjectMethod(
                    string, env->GetMethodID(env->GetObjectClass(string), "getBytes", "()[B")));
                auto* bytes = env->GetByteArrayElements(byteArray, nullptr);
                int length = env->GetArrayLength(byteArray);
                int intValue = 0;
                for (int i = 0; i < length; i++)intValue = (intValue << 8) | bytes[i];
                stack->push(llvm::ConstantInt::get(*context, llvm::APInt(32, intValue, false)));
            }
            else
            {
                jlong longValue = env->CallLongMethod(value, env->GetMethodID(jNumberClazz, "longValue", "()J"));
                jboolean isUnsigned = env->
                    GetBooleanField(type, env->GetFieldID(irIntegerTypeClazz, "unsigned", "Z"));
                stack->push(llvm::ConstantInt::get(*context, llvm::APInt(realSize, longValue, !isUnsigned)));
            }
        }
        else if (env->IsInstanceOf(type, irFloatTypeClazz))
        {
            auto floatValue = env->CallFloatMethod(value, env->GetMethodID(jNumberClazz, "floatValue", "()F"));
            stack->push(llvm::ConstantFP::get(*context, llvm::APFloat(floatValue)));
        }
        else if (env->IsInstanceOf(type, irDoubleTypeClazz))
        {
            auto doubleValue = env->CallDoubleMethod(value, env->GetMethodID(jNumberClazz, "doubleValue", "()D"));
            stack->push(llvm::ConstantFP::get(*context, llvm::APFloat(doubleValue)));
        }
        else if (env->IsInstanceOf(type, irPointerTypeClazz))
        {
            auto* jStringClazz = env->FindClass("java/lang/String");
            if (value == nullptr)
            {
                stack->push(
                    llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::Type::getVoidTy(*context), 0)));
            }
            else if (env->IsInstanceOf(value, jStringClazz))
            {
                // TODO implement string
            }
            else if (env->IsInstanceOf(value, jNumberClazz))
            {
                jlong longValue = env->CallLongMethod(value, env->GetMethodID(jNumberClazz, "longValue", "()J"));
                stack->push(llvm::ConstantInt::get(*context, llvm::APInt(64, longValue, false)));
            }
        }
    }

    return nullptr;
}

JNIEXPORT jobject JNICALL
Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitVirtualRegister(
    JNIEnv* env, jobject thisPtr, jobject irVirtualRegister, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));
    auto* virtualRegister2Value = reinterpret_cast<std::map<std::string, llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J")));
    auto registerNameObject = reinterpret_cast<jstring>(env->GetObjectField(
        irVirtualRegister, env->GetFieldID(env->GetObjectClass(irVirtualRegister), "name", "Ljava/lang/String;")));
    if (virtualRegister2Value->count(env->GetStringUTFChars(registerNameObject, nullptr)) == 0)return nullptr;
    stack->push(virtualRegister2Value->at(env->GetStringUTFChars(registerNameObject, nullptr)));
    return nullptr;
}

JNIEXPORT jobject JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_00024LLVMModuleGenerator_visitMacro(
    JNIEnv* env, jobject thisPtr, jobject irMacro, jobject additional)
{
    auto* clazz = env->GetObjectClass(thisPtr);
    auto* irModule = env->GetObjectField(
        thisPtr, env->GetFieldID(clazz, "module", "Lldk/l/lg/ir/IRModule;"));
    auto* module = reinterpret_cast<llvm::Module*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmModule", "J")));
    auto* builder = reinterpret_cast<llvm::IRBuilder<>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmBuilder", "J")));
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));

    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));

    auto* irModuleClazz = env->GetObjectClass(irModule);

    auto* irMacroClazz = env->GetObjectClass(irMacro);
    auto* nameObject = reinterpret_cast<jstring>(env->GetObjectField(
        irMacro, env->GetFieldID(irMacroClazz, "name", "Ljava/lang/String;")));
    auto* argsArray = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irMacro, env->GetFieldID(irMacroClazz, "args", "[Ljava/lang/String;")));

    auto* name = env->GetStringUTFChars(nameObject, nullptr);
    if (strcmp(name, "field_address") == 0)
    {
        if (env->GetArrayLength(argsArray) == 1)
        {
            auto* fieldName = env->GetStringUTFChars(
                reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0)),
                nullptr);
            auto* field2LocalVar = reinterpret_cast<std::map<std::string, llvm::AllocaInst*>*>(env->GetLongField(
                thisPtr, env->GetFieldID(clazz, "field2LocalVar", "J")));
            stack->push(field2LocalVar->at(fieldName));
        }
        else
        {
            auto* structures = env->GetObjectField(irModule, env->GetFieldID(irModuleClazz, "structures",
                                                                             "Ljava/util/Map;"));
            auto* structureName = env->GetStringUTFChars(
                reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0)),
                nullptr);
            auto* structure = env->CallObjectMethod(
                structures, env->GetMethodID(env->GetObjectClass(structures), "get",
                                             "(Ljava/lang/Object;)Ljava/lang/Object;"),
                env->NewStringUTF(structureName));
            auto* fields = reinterpret_cast<jobjectArray>(env->GetObjectField(
                structure,
                env->GetFieldID(env->GetObjectClass(structure), "fields", "[Lldk/l/lg/ir/structure/IRField;")));
            auto* fieldName = env->GetStringUTFChars(
                reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 1)),
                nullptr);
            jlong length = 0;
            jlong offset = -1;
            auto* irTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRType");
            auto* getLengthMethod = env->
                GetStaticMethodID(irTypeClazz, "getLength", "(Lldk/l/lg/ir/type/IRType;)J");
            for (int i = 0; i < env->GetArrayLength(fields); i++)
            {
                auto* field = env->GetObjectArrayElement(fields, i);
                auto* _name = env->GetStringUTFChars(
                    reinterpret_cast<jstring>(env->GetObjectField(
                        field, env->GetFieldID(env->GetObjectClass(field), "name", "Ljava/lang/String;"))),
                    nullptr);
                if (strcmp(_name, fieldName) == 0)
                {
                    offset = length;
                }
                auto* type = env->GetObjectField(
                    field, env->GetFieldID(env->GetObjectClass(field), "type", "Lldk/l/lg/ir/type/IRType;"));
                length += env->CallStaticLongMethod(irTypeClazz, getLengthMethod, type);
            }
            auto* additionalOperandsArray = reinterpret_cast<jobjectArray>(env->GetObjectField(
                irMacro, env->GetFieldID(irMacroClazz, "additionalOperands", "[Lldk/l/lg/ir/operand/IROperand;")));
            auto* visitMethod = env->GetMethodID(clazz, "visit",
                                                 "(Lldk/l/lg/ir/base/IRNode;Ljava/lang/Object;)Ljava/lang/Object;");
            env->CallObjectMethod(thisPtr, visitMethod, env->GetObjectArrayElement(additionalOperandsArray, 0),
                                  additional);
            auto* object = stack->top();
            stack->pop();
            auto* addr = builder->CreateGEP(object->getType(), object, builder->getInt64(offset));
            stack->push(addr);
        }
    }
    else if (strcmp(name, "function_address") == 0)
    {
        auto* functionName = env->GetStringUTFChars(
            reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0)),
            nullptr);
        stack->push(module->getFunction(functionName));
    }
    else if (strcmp(name, "structure_length") == 0)
    {
        auto* structures = env->GetObjectField(irModule, env->GetFieldID(irModuleClazz, "structures",
                                                                         "Ljava/util/Map;"));
        auto* structureName = env->GetStringUTFChars(
            reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0)),
            nullptr);
        auto* structure = env->CallObjectMethod(
            structures, env->GetMethodID(env->GetObjectClass(structures), "get",
                                         "(Ljava/lang/Object;)Ljava/lang/Object;"),
            env->NewStringUTF(structureName));
        jlong length = env->CallLongMethod(structure, env->GetMethodID(env->GetObjectClass(structure), "getLength",
                                                                       "()J"));
        stack->push(llvm::ConstantInt::get(*context, llvm::APInt(64, length, false)));
    }
    else if (strcmp(name, "structure_field_offset") == 0)
    {
        auto* structures = env->GetObjectField(irModule, env->GetFieldID(irModuleClazz, "structures",
                                                                         "Ljava/util/Map;"));
        auto* structureName = env->GetStringUTFChars(
            reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0)),
            nullptr);
        auto* structure = env->CallObjectMethod(
            structures, env->GetMethodID(env->GetObjectClass(structures), "get",
                                         "(Ljava/lang/Object;)Ljava/lang/Object;"),
            env->NewStringUTF(structureName));
        auto* fields = reinterpret_cast<jobjectArray>(env->GetObjectField(
            structure,
            env->GetFieldID(env->GetObjectClass(structure), "fields", "[Lldk/l/lg/ir/structure/IRField;")));
        auto* fieldName = env->GetStringUTFChars(
            reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 1)),
            nullptr);
        jlong length = 0;
        jlong offset = -1;
        auto* irTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRType");
        auto* getLengthMethod = env->GetStaticMethodID(irTypeClazz, "getLength", "(Lldk/l/lg/ir/type/IRType;)J");
        for (int i = 0; i < env->GetArrayLength(fields); i++)
        {
            auto* field = env->GetObjectArrayElement(fields, i);
            auto* _name = env->GetStringUTFChars(
                reinterpret_cast<jstring>(env->GetObjectField(
                    field, env->GetFieldID(env->GetObjectClass(field), "name", "Ljava/lang/String;"))),
                nullptr);
            if (strcmp(_name, fieldName) == 0)
            {
                offset = length;
            }
            auto* type = env->GetObjectField(
                field, env->GetFieldID(env->GetObjectClass(field), "type", "Lldk/l/lg/ir/type/IRType;"));
            length += env->CallStaticLongMethod(irTypeClazz, getLengthMethod, type);
        }
        stack->push(llvm::ConstantInt::get(*context, llvm::APInt(64, offset, false)));
    }
    else if (strcmp(name, "global_data_address") == 0)
    {
        auto* globalDataNameObject = reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0));
        auto* globalDataName = env->GetStringUTFChars(globalDataNameObject, nullptr);
        stack->push(module->getGlobalVariable(globalDataName));
        env->ReleaseStringUTFChars(globalDataNameObject, globalDataName);
    }
    else if (strcmp(name, "vtable_entry_offset") == 0)
    {
        auto* arg1 = env->GetObjectArrayElement(argsArray, 0);
        auto* arg2 = env->GetObjectArrayElement(argsArray, 1);
        auto* name2VTableKeys = env->GetObjectField(
            irModule, env->GetFieldID(irModuleClazz, "name2VTableKeys", "Ljava/util/Map;"));
        auto* mapClazz = env->FindClass("java/util/Map");
        auto* getMethod = env->GetMethodID(mapClazz, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
        auto* list = env->CallObjectMethod(name2VTableKeys, getMethod, arg1);
        auto offset = env->CallIntMethod(
            list, env->GetMethodID(env->GetObjectClass(list), "indexOf", "(Ljava/lang/Object;)I"), arg2) * 8L;
        stack->push(llvm::ConstantInt::get(*context, llvm::APInt(64, offset, false)));
    }
    else if (strcmp(name, "itable_entry_offset") == 0)
    {
        auto* arg1 = env->GetObjectArrayElement(argsArray, 0);
        auto* arg2 = env->GetObjectArrayElement(argsArray, 1);
        auto* name2ITableKeys = env->GetObjectField(
            irModule, env->GetFieldID(irModuleClazz, "name2ITableKeys", "Ljava/util/Map;"));
        auto* mapClazz = env->FindClass("java/util/Map");
        auto* getMethod = env->GetMethodID(mapClazz, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
        auto* list = env->CallObjectMethod(name2ITableKeys, getMethod, arg1);
        auto offset = env->CallIntMethod(
            list, env->GetMethodID(env->GetObjectClass(list), "indexOf", "(Ljava/lang/Object;)I"), arg2) * 8L + 8;
        stack->push(llvm::ConstantInt::get(*context, llvm::APInt(64, offset, false)));
    }
    return nullptr;
}
