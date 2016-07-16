//
//  exec.c
//  TotemScript
//
//  Created by Timothy Smale on 17/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <stdarg.h>
#include <math.h>
#include <TotemScript/exec.h>
#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <string.h>

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

void totemInstance_Init(totemInstance *instance)
{
    totemMemoryBuffer_Init(&instance->GlobalRegisters, sizeof(totemRegister));
    totemMemoryBuffer_Init(&instance->LocalFunctions, sizeof(totemInstanceFunction));
    instance->Script = NULL;
}

void totemInstance_Reset(totemInstance *instance)
{
    totemMemoryBuffer_Reset(&instance->GlobalRegisters);
    totemMemoryBuffer_Reset(&instance->LocalFunctions);
}

void totemInstance_Cleanup(totemInstance *instance)
{
    totemMemoryBuffer_Cleanup(&instance->GlobalRegisters);
    totemMemoryBuffer_Cleanup(&instance->LocalFunctions);
}

void totemInternedStringHeader_Destroy(totemInternedStringHeader *str)
{
    totem_CacheFree(str, sizeof(totemInternedStringHeader) + str->Length - 1);
}

void totemScript_Init(totemScript *script)
{
    totemMemoryBuffer_Init(&script->Functions, sizeof(totemScriptFunction));
    totemMemoryBuffer_Init(&script->FunctionNames, sizeof(totemRegister));
    totemMemoryBuffer_Init(&script->GlobalRegisters, sizeof(totemRegister));
    totemMemoryBuffer_Init(&script->Instructions, sizeof(totemInstruction));
    totemHashMap_Init(&script->FunctionNameLookup);
}

void totemScript_Reset(totemScript *script)
{
    totemMemoryBuffer_Reset(&script->Functions);
    totemMemoryBuffer_Reset(&script->FunctionNames);
    totemMemoryBuffer_Reset(&script->GlobalRegisters);
    totemMemoryBuffer_Reset(&script->Instructions);
    totemHashMap_Reset(&script->FunctionNameLookup);
}

void totemScript_Cleanup(totemScript *script)
{
    totemMemoryBuffer_Cleanup(&script->Functions);
    totemMemoryBuffer_Cleanup(&script->FunctionNames);
    totemMemoryBuffer_Cleanup(&script->GlobalRegisters);
    totemMemoryBuffer_Cleanup(&script->Instructions);
    totemHashMap_Cleanup(&script->FunctionNameLookup);
}

totemBool totemScript_GetFunctionName(totemScript *script, totemOperandXUnsigned addr, totemRegisterValue *valOut, totemPrivateDataType *dataTypeOut)
{
    totemRegister *name = totemMemoryBuffer_Get(&script->FunctionNames, addr);
    if (!name)
    {
        return totemBool_False;
    }
    
    *dataTypeOut = name->DataType;
    memcpy(valOut, &name->Value, sizeof(totemRegisterValue));
    return totemBool_True;
}

totemLinkStatus totemScript_LinkInstance(totemScript *script, totemInstance *instance)
{
    totemInstance_Reset(instance);
    
    instance->Script = script;
    
    // copy initial global register state
    if (!totemMemoryBuffer_TakeFrom(&instance->GlobalRegisters, &script->GlobalRegisters))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    // setup instance functions
    size_t numFunctions = totemMemoryBuffer_GetNumObjects(&script->Functions);
    if (!totemMemoryBuffer_Secure(&instance->LocalFunctions, numFunctions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    for (size_t i = 0; i < numFunctions; i++)
    {
        totemInstanceFunction *instanceFunc = totemMemoryBuffer_Get(&instance->LocalFunctions, i);
        totemScriptFunction *scriptFunc = totemMemoryBuffer_Get(&script->Functions, i);
        
        instanceFunc->Function = scriptFunc;
        instanceFunc->Instance = instance;
    }
    
    // fix instance function pointers
    size_t numGlobalRegisters = totemMemoryBuffer_GetNumObjects(&instance->GlobalRegisters);
    for (size_t i = 0; i < numGlobalRegisters; i++)
    {
        totemRegister *reg = totemMemoryBuffer_Get(&instance->GlobalRegisters, i);
        if (reg->DataType == totemPrivateDataType_InstanceFunction)
        {
            reg->Value.InstanceFunction = totemMemoryBuffer_Get(&instance->LocalFunctions, (size_t)reg->Value.Data);
        }
    }
    
    return totemLinkStatus_Success;
}

void totemRuntime_Init(totemRuntime *runtime)
{
    totemMemoryBuffer_Init(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Init(&runtime->NativeFunctionNames, sizeof(totemRegister));
    totemHashMap_Init(&runtime->NativeFunctionsLookup);
    totemHashMap_Init(&runtime->InternedStrings);
    totemLock_Init(&runtime->InternedStringsLock);
}

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions);
    totemMemoryBuffer_Reset(&runtime->NativeFunctionNames);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
    totemHashMap_Reset(&runtime->InternedStrings);
    totemLock_Release(&runtime->InternedStringsLock);
}

void totemRuntime_Cleanup(totemRuntime *runtime)
{
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctions);
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctionNames);
    totemHashMap_Cleanup(&runtime->NativeFunctionsLookup);
    
    // clean up interned strings
    for(size_t i = 0; i < runtime->InternedStrings.NumBuckets; i++)
    {
        for(totemHashMapEntry *entry = runtime->InternedStrings.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemInternedStringHeader *hdr = (totemInternedStringHeader*)entry->Value;
            if(hdr)
            {
                totemInternedStringHeader_Destroy(hdr);
            }
        }
    }
    
    totemHashMap_Cleanup(&runtime->InternedStrings);
    totemLock_Cleanup(&runtime->InternedStringsLock);
}

totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRegisterValue *valOut, totemPrivateDataType *typeOut)
{
    if(str->Length <= TOTEM_MINISTRING_MAXLENGTH)
    {
        *typeOut = totemPrivateDataType_MiniString;
        memset(valOut->MiniString.Value, 0, sizeof(valOut->MiniString.Value));
        memcpy(valOut->MiniString.Value, str->Value, str->Length);
        
        return totemLinkStatus_Success;
    }
    
    totemLinkStatus status = totemLinkStatus_Success;
    totemLock_Acquire(&runtime->InternedStringsLock);
    
    totemHashMapEntry *result = totemHashMap_Find(&runtime->InternedStrings, str->Value, str->Length);
    if(result)
    {
        *typeOut = totemPrivateDataType_InternedString;
        valOut->InternedString = (totemInternedStringHeader*)result->Value;
    }
    else
    {
        size_t toAllocate = str->Length + sizeof(totemInternedStringHeader) - 1;
        
        totemInternedStringHeader *newStr = totem_CacheMalloc(toAllocate);
        if (!newStr)
        {
            status = totemLinkStatus_OutOfMemory;
        }
        else
        {
            newStr->Length = str->Length;
            
            newStr->Hash = totem_Hash(str->Value, newStr->Length);
            memcpy(newStr->Data, str->Value, newStr->Length);
            newStr->Data[newStr->Length] = 0;
            
            if (!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, newStr->Data, newStr->Length, (uintptr_t)newStr, newStr->Hash))
            {
                totemInternedStringHeader_Destroy(newStr);
                status = totemLinkStatus_OutOfMemory;
            }
            else
            {
                *typeOut = totemPrivateDataType_InternedString;
                valOut->InternedString = newStr;
            }
        }
    }
    
    totemLock_Release(&runtime->InternedStringsLock);
    
    if (status != totemLinkStatus_Success)
    {
        return totemLinkStatus_Break(status);
    }
    
    return status;
}

totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *script)
{
    totemScript_Reset(script);
    
    // instructions
    if (!totemMemoryBuffer_TakeFrom(&script->Instructions, &build->Instructions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    // functions
    size_t numFunctions = totemMemoryBuffer_GetNumObjects(&build->Functions);
    if (!totemMemoryBuffer_Secure(&script->Functions, numFunctions) || !totemMemoryBuffer_Secure(&script->FunctionNames, numFunctions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    totemInstruction *instructions = totemMemoryBuffer_Bottom(&script->Instructions);
    totemScriptFunction *funcs = totemMemoryBuffer_Get(&script->Functions, 0);
    totemRegister *strs = totemMemoryBuffer_Get(&script->FunctionNames, 0);
    totemScriptFunctionPrototype *funcProts = totemMemoryBuffer_Get(&build->Functions, 0);
    
    for (size_t i = 0; i < numFunctions; i++)
    {
        funcs[i].Address = (totemOperandXUnsigned)i;
        funcs[i].InstructionsStart = instructions + (funcProts[i].InstructionsStart);
        funcs[i].RegistersNeeded = funcProts[i].RegistersNeeded;
        
        if (totemRuntime_InternString(runtime, &funcProts[i].Name, &strs[i].Value, &strs[i].DataType) != totemLinkStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    // function name lookup
    if (!totemHashMap_TakeFrom(&script->FunctionNameLookup, &build->FunctionLookup))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    // check script function names against existing native functions
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->Functions); i++)
    {
        totemOperandXUnsigned dummyAddr;
        totemScriptFunctionPrototype *func = totemMemoryBuffer_Get(&build->Functions, i);
        
        if (func->Name.Value != NULL)
        {
            if (totemRuntime_GetNativeFunctionAddress(runtime, &func->Name, &dummyAddr) == totemBool_True)
            {
                build->ErrorContext = func;
                return totemLinkStatus_Break(totemLinkStatus_FunctionAlreadyDeclared);
            }
        }
    }
    
    // global register values
    size_t numGlobalRegs = totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.Registers);
    
    if(!totemMemoryBuffer_Secure(&script->GlobalRegisters, numGlobalRegs))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    for(size_t i = 0; i < numGlobalRegs; i++)
    {
        totemRegisterPrototype *prototype = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, i);
        totemRegister *reg = totemMemoryBuffer_Get(&script->GlobalRegisters, i);
        
        switch(prototype->DataType)
        {
                // intern string values
            case totemPublicDataType_String:
            {
                if(totemRuntime_InternString(runtime, prototype->String, &reg->Value, &reg->DataType) != totemLinkStatus_Success)
                {
                    return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
                }
                break;
            }
                
                // fix function pointers
            case totemPublicDataType_Function:
            {
                if(prototype->FunctionPointer.Type == totemFunctionType_Native)
                {
                    reg->DataType = totemPrivateDataType_NativeFunction;
                    
                    totemString *funcName = totemMemoryBuffer_Get(&build->NativeFunctionNames, prototype->FunctionPointer.Address);
                    if(!funcName)
                    {
                        return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
                    }
                    
                    totemOperandXUnsigned funcAddr = 0;
                    if(!totemRuntime_GetNativeFunctionAddress(runtime, funcName, &funcAddr))
                    {
                        build->ErrorContext = funcName;
                        return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                    }
                    
                    totemNativeFunction *func = totemMemoryBuffer_Get(&runtime->NativeFunctions, funcAddr);
                    if (!func)
                    {
                        build->ErrorContext = funcName;
                        return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                    }
                    
                    reg->Value.NativeFunction = func;
                }
                else
                {
                    reg->DataType = totemPrivateDataType_InstanceFunction;
                    reg->Value.Data = prototype->FunctionPointer.Address;
                }
                break;
            }
                
            case totemPublicDataType_Int:
                reg->DataType = totemPrivateDataType_Int;
                reg->Value.Int = prototype->Int;
                break;
                
            case totemPublicDataType_Float:
                reg->DataType = totemPrivateDataType_Float;
                reg->Value.Float = prototype->Float;
                break;
                
            case totemPublicDataType_Type:
                reg->DataType = totemPrivateDataType_Type;
                reg->Value.DataType = prototype->TypeValue;
                break;
                
            case totemPublicDataType_Boolean:
                reg->DataType = totemPrivateDataType_Boolean;
                reg->Value.Data = prototype->Boolean != 0;
                break;
                
            case totemPublicDataType_Null:
                reg->DataType = totemPrivateDataType_Null;
                reg->Value.Data = 0;
                break;
                
            default:
                return totemLinkStatus_Break(totemLinkStatus_UnexpectedValueType);
        }
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunctionCb cb, totemString *name, totemOperandXUnsigned *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    totemNativeFunction *func = NULL;
    if(result != NULL)
    {
        func = totemMemoryBuffer_Get(&runtime->NativeFunctions, result->Value);
        *addressOut = (totemOperandXUnsigned)result->Value;
    }
    else
    {
        if(totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions) >= TOTEM_MAX_NATIVEFUNCTIONS - 1)
        {
            return totemLinkStatus_Break(totemLinkStatus_TooManyNativeFunctions);
        }
        
        *addressOut = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions);
        func = totemMemoryBuffer_Secure(&runtime->NativeFunctions, 1);
        if(!func)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        func->Address = *addressOut;
        
        totemRegister value;
        memset(&value, 0, sizeof(totemRegister));
        
        if(totemRuntime_InternString(runtime, name, &value.Value, &value.DataType) != totemLinkStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemMemoryBuffer_Insert(&runtime->NativeFunctionNames, &value, sizeof(totemRegister)))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemHashMap_Insert(&runtime->NativeFunctionsLookup, name->Value, name->Length, *addressOut))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    func->Callback = cb;
    return totemLinkStatus_Success;
}

totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, totemOperandXUnsigned *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    if(result != NULL)
    {
        *addressOut = (totemOperandXUnsigned)result->Value;
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemBool totemRuntime_GetNativeFunctionName(totemRuntime *runtime, totemOperandXUnsigned addr, totemRegisterValue *valOut, totemPrivateDataType *typeOut)
{
    totemRegister *reg = totemMemoryBuffer_Get(&runtime->NativeFunctionNames, addr);
    if (reg)
    {
        memcpy(valOut, &reg->Value, sizeof(totemRegisterValue));
        *typeOut = reg->DataType;
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters)
{
    state->Runtime = runtime;
    
    if (state->LocalRegisters)
    {
        totem_CacheFree(state->LocalRegisters, sizeof(totemRegister) * state->MaxLocalRegisters);
    }
    
    if (numRegisters > 0)
    {
        totemRegister *regs = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
        
        if (!regs)
        {
            return totemLinkStatus_OutOfMemory;
        }
        
        memset(regs, 0, sizeof(totemRegister) * numRegisters);
        state->LocalRegisters = regs;
        state->NextFreeRegister = regs;
    }
    else
    {
        state->LocalRegisters = NULL;
        state->NextFreeRegister = NULL;
    }
    
    state->MaxLocalRegisters = numRegisters;
    state->UsedLocalRegisters = 0;
    return totemLinkStatus_Success;
}

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

void *totemExecState_Alloc(totemExecState *state, size_t size)
{
    void *ptr = totem_CacheMalloc(size);
    if (!ptr)
    {
        totemExecState_CollectGarbage(state, totemBool_True);
        ptr = totem_CacheMalloc(size);
    }
    
    return ptr;
}

totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state)
{
    totemFunctionCall *call = totemExecState_Alloc(state, sizeof(totemFunctionCall));
    if (!call)
    {
        return NULL;
    }
    
    memset(call, 0, sizeof(totemFunctionCall));
    return call;
}

void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call)
{
    totem_CacheFree(call, sizeof(*call));
}

totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut)
{
    totemRegister newStr;
    memset(&newStr, 0, sizeof(totemRegister));
    
    if (totemRuntime_InternString(state->Runtime, str, &newStr.Value, &newStr.DataType) != totemLinkStatus_Success)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemExecState_Assign(state, strOut, &newStr);
    return totemExecStatus_Continue;
}

void totemExecState_CleanupRegisterList(totemExecState *state, totemRegister *regs, uint32_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        totemRegister *reg = &regs[i];
        
        if (TOTEM_REGISTER_ISGC(reg))
        {
            totemExecState_DecRefCount(state, reg->Value.GCObject);
        }
    }
}

void totemExecState_Init(totemExecState *state)
{
    memset(state, 0, sizeof(*state));
}

void totemExecState_Cleanup(totemExecState *state)
{
    // clean up remaining registers
    totemExecState_CleanupRegisterList(state, state->LocalRegisters, (uint32_t)state->UsedLocalRegisters);
    
    // collect until it's all gone
    while(state->GCStart || state->GCStart2)
    {
        totemExecState_CollectGarbage(state, totemBool_True);
    }
    
    totem_CacheFree(state->LocalRegisters, sizeof(totemRegister) * state->MaxLocalRegisters);
    state->LocalRegisters = NULL;
}

totemExecStatus totemExecState_CreateSubroutine(totemExecState *state, size_t numRegisters, totemInstance *instance, totemRegister *returnReg, totemFunctionType funcType, void *function, totemFunctionCall **callOut)
{
    totemFunctionCall *call = totemExecState_SecureFunctionCall(state);
    if (call == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    if ((state->MaxLocalRegisters - state->UsedLocalRegisters) < numRegisters)
    {
        TOTEM_SETBITS(call->Flags, totemFunctionCallFlag_FreeStack);
        
        call->FrameStart = totemExecState_Alloc(state, sizeof(totemRegister) * numRegisters);
        if (!call->FrameStart)
        {
            totemExecState_FreeFunctionCall(state, call);
            return totemExecStatus_Break(totemExecStatus_OutOfMemory);
        }
    }
    else
    {
        call->FrameStart = state->NextFreeRegister;
        state->NextFreeRegister += numRegisters;
        state->UsedLocalRegisters += numRegisters;
    }
    
    // reset registers to be used
    memset(call->FrameStart, 0, numRegisters * sizeof(totemRegister));
    
    call->CurrentInstance = instance;
    call->ReturnRegister = returnReg;
    call->Type = funcType;
    call->Function = function;
    call->ResumeAt = NULL;
    call->Prev = NULL;
    call->NumArguments = 0;
    call->NumRegisters = numRegisters;
    
    *callOut = call;
    return totemExecStatus_Continue;
}

void totemExecState_PushRoutine(totemExecState *state, totemFunctionCall *call, totemInstruction *startAt)
{
    call->PreviousFrameStart = state->LocalRegisters;
    call->ResumeAt = startAt;
    
    state->LocalRegisters = call->FrameStart;
    
    if (call->CurrentInstance)
    {
        state->GlobalRegisters = (totemRegister*)call->CurrentInstance->GlobalRegisters.Data;
    }
    
    if (state->CallStack)
    {
        call->Prev = state->CallStack;
    }
    
    state->CallStack = call;
}

void totemExecState_PopRoutine(totemExecState *state)
{
    if(state->CallStack)
    {
        totemExecState_CollectGarbage(state, totemBool_False);
        
        totemFunctionCall *call = state->CallStack;
        totemFunctionCall *prev = call->Prev;
        
        state->LocalRegisters = call->PreviousFrameStart;
        
        if (call->Type == totemFunctionType_Script)
        {
            state->GlobalRegisters = (totemRegister*)call->CurrentInstance->GlobalRegisters.Data;
        }
        
        if (TOTEM_HASBITS(call->Flags, totemFunctionCallFlag_IsCoroutine))
        {
            totemInstruction ins = *call->ResumeAt;
            totemOperationType opType = TOTEM_INSTRUCTION_GET_OP(ins);
            
            // no more instructions - ensure it starts from the beginning next time
            if (opType == totemOperationType_Return)
            {
                totemOperandXUnsigned flags = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(ins);
                
                if (TOTEM_HASBITS(flags, totemReturnFlag_Last))
                {
                    call->ResumeAt = NULL;
                }
                else
                {
                    // start at next instruction
                    call->ResumeAt++;
                }
            }
        }
        else
        {
            totemExecState_CleanupRegisterList(state, call->FrameStart, call->NumRegisters);
            
            if (TOTEM_HASBITS(call->Flags, totemFunctionCallFlag_FreeStack))
            {
                totem_CacheFree(call->FrameStart, sizeof(totemRegister) * call->NumRegisters);
            }
            else
            {
                state->UsedLocalRegisters -= call->NumRegisters;
                state->NextFreeRegister -= call->NumRegisters;
            }
            
            totemExecState_FreeFunctionCall(state, call);
        }
        
        state->CallStack = prev;
    }
}

totemExecStatus totemExecState_UnsafeExec(totemExecState *state, totemInstanceFunction *function, totemRegister *returnRegister)
{
    totemFunctionCall *call = NULL;
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateSubroutine(state, function->Function->RegistersNeeded, function->Instance, returnRegister, totemFunctionType_Script, function, &call));
    totemExecState_PushRoutine(state, call, function->Function->InstructionsStart);
    
    totemExecState_ExecuteInstructions(state);
    totemExecState_PopRoutine(state);
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ProtectedExec(totemExecState *state, totemInstanceFunction *function, totemRegister *returnRegister)
{
    totemFunctionCall *retain = state->CallStack;
    
    totemJmpNode node;
    node.Status = totemExecStatus_Continue;
    node.Prev = state->JmpNode;
    state->JmpNode = &node;
    
    TOTEM_JMP_TRY(node.Buffer)
    {
        node.Status = totemExecState_UnsafeExec(state, function, returnRegister);
    }
    TOTEM_JMP_CATCH(node.Buffer)
    {
        // rollback calls
        while (state->CallStack != retain)
        {
            totemExecState_PopRoutine(state);
        }
    }
    
    state->JmpNode = node.Prev;
    return node.Status;
}

const char *totemExecStatus_Describe(totemExecStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemExecStatus_Continue);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_NativeFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RegisterOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Return);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InstanceFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_ScriptNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnexpectedDataType);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnrecognisedOperation);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Stop);
            TOTEM_STRINGIFY_CASE(totemExecStatus_IndexOutOfBounds);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RefCountOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InternalBufferOverrun);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InvalidKey);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InvalidDispatch);
    }
    
    return "UNKNOWN";
}

const char *totemLinkStatus_Describe(totemLinkStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemLinkStatus_UnexpectedValueType);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_TooManyNativeFunctions);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_FunctionAlreadyDeclared);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_FunctionNotDeclared);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_InvalidNativeFunctionAddress);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_InvalidNativeFunctionName);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_Success);
    }
    
    return "UNKNOWN";
}