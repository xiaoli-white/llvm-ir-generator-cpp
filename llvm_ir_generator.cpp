#include <jni.h>
#include "./com_xiaoli_llvmir_generator_LLVMIRGenerator.h"
#include "./com_xiaoli_llvmir_generator_LLVMIRGenerator_LLVMModuleGenerator.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <map>
#include <stack>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Argument.h>

JNIEXPORT jlong JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_createLLVMContext(JNIEnv* env, jclass clazz)
{
    return reinterpret_cast<jlong>(new llvm::LLVMContext());
}

JNIEXPORT jlong JNICALL Java_com_xiaoli_llvmir_1generator_LLVMIRGenerator_createLLVMModule(
    JNIEnv* env, jclass clazz, jlong llvmContext)
{
    return reinterpret_cast<jlong>(new llvm::Module("module", *reinterpret_cast<llvm::LLVMContext*>(llvmContext)));
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
                field, env->GetFieldID(env->GetObjectClass(field), "type", "Lldk/l/lg/ir/type/IRType;")), context));
    }
    llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, types, false);
    llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage,
                                                      env->GetStringUTFChars(name, nullptr), module);
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "currentFunction", "J"), reinterpret_cast<jlong>(function));

    auto* stack = new std::stack<llvm::Value*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "stack", "J"), reinterpret_cast<jlong>(stack));

    auto* basicBlockMap = new std::map<std::string, llvm::BasicBlock*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "basicBlockMap", "J"), reinterpret_cast<jlong>(basicBlockMap));

    auto* virtualRegister2Value = new std::map<std::string, llvm::Value*>();
    env->SetLongField(thisPtr, env->GetFieldID(clazz, "virtualRegister2Value", "J"),
                      reinterpret_cast<jlong>(virtualRegister2Value));

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
            instructions, env->GetMethodID(env->GetObjectClass(instructions), "iterator", "()Ljava/util/Iterator;"));
        jmethodID hasNextMethod2 = env->GetMethodID(env->GetObjectClass(iterator2), "hasNext", "()Z");
        jmethodID nextMethod2 = env->GetMethodID(env->GetObjectClass(iterator2), "next", "()Ljava/lang/Object;");
        while (env->CallBooleanMethod(iterator2, hasNextMethod2))
        {
            auto* instruction = env->CallObjectMethod(iterator2, nextMethod2);
            env->CallObjectMethod(thisPtr, visitMethod, instruction, additional);
            // 临时解决方案
            while (!stack->empty())stack->pop();
        }
    }

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
            // Temp
            builder->CreateRetVoid();
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
    env->CallObjectMethod(thisPtr, visitMethod, operand2Object, additional);
    if (stack->empty()) return nullptr;
    auto* operand2 = stack->top();
    stack->pop();
    auto* name = reinterpret_cast<jstring>(env->GetObjectField(
        irConditionalJump, env->GetFieldID(env->GetObjectClass(irConditionalJump), "target", "Ljava/lang/String;")));
    auto* target = env->GetStringUTFChars(name, nullptr);
    if (isAtomic)
    {
    }
    else
    {
        llvm::Value* ret;
        jclass irIntegerTypeClazz = env->FindClass("ldk/l/lg/ir/type/IRIntegerType");
        if (env->IsInstanceOf(typeObject, irIntegerTypeClazz))
        {
            jboolean isUnsigned = env->GetBooleanField(
                typeObject, env->GetFieldID(env->GetObjectClass(typeObject), "isUnsigned", "Z"));
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
        auto* currentBasicBlock = env->GetObjectField(
            thisPtr, env->GetFieldID(clazz, "currentBasicBlock", "Lldk/l/lg/ir/base/IRControlFlowGraph$BasicBlock;"));
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
            list, env->GetMethodID(env->GetObjectClass(list), "indexOf", "(Ljava/lang/Object;)I"), currentBasicBlock);
        auto* nextBasicBlock = env->CallObjectMethod(
            list, env->GetMethodID(env->GetObjectClass(list), "get", "(I)Ljava/lang/Object;"), index + 1);
        auto* nextBasicBlockNameObject = reinterpret_cast<jstring>(env->GetObjectField(
            nextBasicBlock, env->GetFieldID(env->GetObjectClass(nextBasicBlock), "name", "Ljava/lang/String;")));
        auto* nextBasicBlockName = env->GetStringUTFChars(nextBasicBlockNameObject, nullptr);
        builder->CreateCondBr(ret, basicBlockMap->at(target), basicBlockMap->at(nextBasicBlockName));
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
        irCalculate, env->GetFieldID(irCalculateClazz, "operator", "Lldk/l/lg/ir/instruction/IRCalculate$Operator;"));

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
    if (isAtomic)
    {
    }
    else
    {
        llvm::Value* result;
        if (strcmp(text, "add") == 0)
            result = builder->CreateAdd(operand1, operand2);
        else if (strcmp(text, "sub") == 0)
            result = builder->CreateSub(operand1, operand2);
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

    if (targetRegister == nullptr)
    {
        // atomic
    }
    else
    {
        auto* type = getType(env, typeObject, context);
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

    if (targetRegister == nullptr)
    {
        // atomic
    }
    else
    {
        auto* type = getType(env, typeObject, context);
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
        auto* name = reinterpret_cast<jstring>(env->GetObjectArrayElement(names, i));
        map.insert(std::make_pair(env->GetStringUTFChars(name, nullptr), i));
    }
    std::string code = env->GetStringUTFChars(codeString, nullptr);
    // TODO process code and constraints

    auto* functionType = llvm::FunctionType::get(llvm::Type::getVoidTy(*context), argTypes, false);
    auto* inlineAsm = llvm::InlineAsm::get(functionType, code, constraints, false);
    builder->CreateCall(inlineAsm, args);

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
                    GetBooleanField(type, env->GetFieldID(irIntegerTypeClazz, "isUnsigned", "Z"));
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
            if (env->IsInstanceOf(value, jStringClazz))
            {
                // TODO implement string
            }
            else
            {
                jlong longValue = value == nullptr
                                      ? 0
                                      : env->CallLongMethod(value, env->GetMethodID(jNumberClazz, "longValue", "()J"));
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
    auto* context = reinterpret_cast<llvm::LLVMContext*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "llvmContext", "J")));
    auto* stack = reinterpret_cast<std::stack<llvm::Value*>*>(env->GetLongField(
        thisPtr, env->GetFieldID(clazz, "stack", "J")));

    auto* irMacroClazz = env->GetObjectClass(irMacro);
    auto* nameObject = reinterpret_cast<jstring>(env->GetObjectField(
        irMacro, env->GetFieldID(irMacroClazz, "name", "Ljava/lang/String;")));
    auto* argsArray = reinterpret_cast<jobjectArray>(env->GetObjectField(
        irMacro, env->GetFieldID(irMacroClazz, "args", "[Ljava/lang/String;")));
    auto* name = env->GetStringUTFChars(nameObject, nullptr);
    if (strcmp(name, "field_address") == 0)
    {
        auto* fieldNameObject = reinterpret_cast<jstring>(env->GetObjectArrayElement(argsArray, 0));
        auto* fieldName = env->GetStringUTFChars(fieldNameObject, nullptr);
        auto* currentFunction = reinterpret_cast<llvm::Function*>(env->GetLongField(
            thisPtr, env->GetFieldID(clazz, "currentFunction", "J")));
        auto* arg = currentFunction->arg_begin();
        while (arg != currentFunction->arg_end())
        {
            if (strcmp(arg->getName().data(), fieldName) == 0)stack->push(arg);
            arg = std::next(arg);
        }
    }
    return nullptr;
}
