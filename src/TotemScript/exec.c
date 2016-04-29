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

#define TOTEM_GET_OPERANDA_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))]
#define TOTEM_GET_OPERANDB_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))]
#define TOTEM_GET_OPERANDC_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))]

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

void totemRuntimeArray_Destroy(totemRuntimeArray *arr)
{
    for(uint32_t i = 0; i < arr->NumRegisters; i++)
    {
        if(arr->Registers[i].DataType == totemDataType_Array)
        {
            totemRuntimeArray_DefRefCount(arr->Registers[i].Value.Array);
        }
    }
    
    totem_CacheFree(arr->Registers, sizeof(totemRegister) * arr->NumRegisters);
    totem_CacheFree(arr, sizeof(totemRuntimeArray));
}

void totemRuntimeArray_DefRefCount(totemRuntimeArray *arr)
{
    switch(arr->RefCount)
    {
        case 0:
            break;
            
        case 1:
            arr->RefCount = 0;
            totemRuntimeArray_Destroy(arr);
            break;
            
        default:
            arr->RefCount--;
            break;
    }
}

totemExecStatus totemRuntimeArray_IncRefCount(totemRuntimeArray *arr)
{
    if(arr->RefCount == UINT32_MAX - 1)
    {
        return totemExecStatus_Break(totemExecStatus_RefCountOverflow);
    }
    
    arr->RefCount++;
    
    return totemExecStatus_Continue;
}

totemExecStatus totemRegister_Assign(totemRegister *dst, totemRegister *src)
{
    if(dst->DataType == totemDataType_Array)
    {
        totemRuntimeArray_DefRefCount(dst->Value.Array);
    }
    
    memcpy(dst, src, sizeof(totemRegister));
    
    if(dst->DataType == totemDataType_Array)
    {
        return totemRuntimeArray_IncRefCount(dst->Value.Array);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemActor_Init(totemActor *actor, totemRuntime *runtime, size_t scriptAddress)
{
    memset(actor, 0, sizeof(totemActor));
    
    actor->ScriptHandle = scriptAddress;
    
    totemScript *script = totemMemoryBuffer_Get(&runtime->Scripts, scriptAddress);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }
    
    totemRegister *globalRegisters = totem_CacheMalloc(script->GlobalRegisters.Length);
    if(globalRegisters == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memcpy(globalRegisters, script->GlobalRegisters.Data, script->GlobalRegisters.Length);
    memcpy(&actor->GlobalRegisters, &script->GlobalRegisters, sizeof(totemMemoryBuffer));
    actor->GlobalRegisters.Data = (char*)globalRegisters;
    
    return totemExecStatus_Continue;
}

void totemActor_Cleanup(totemActor *actor)
{
    totemMemoryBuffer_Cleanup(&actor->GlobalRegisters);
}

void totemScript_Init(totemScript *script)
{
    totemMemoryBuffer_Init(&script->Functions, sizeof(totemFunction));
    totemMemoryBuffer_Init(&script->GlobalData, sizeof(char));
    totemMemoryBuffer_Init(&script->GlobalRegisters, sizeof(totemRegister));
    totemMemoryBuffer_Init(&script->Instructions, sizeof(totemInstruction));
    totemHashMap_Init(&script->FunctionNameLookup);
}

void totemScript_Reset(totemScript *script)
{
    totemMemoryBuffer_Reset(&script->Functions);
    totemMemoryBuffer_Reset(&script->GlobalData);
    totemMemoryBuffer_Reset(&script->GlobalRegisters);
    totemMemoryBuffer_Reset(&script->Instructions);
    totemHashMap_Reset(&script->FunctionNameLookup);
}

void totemScript_Cleanup(totemScript *script)
{
    totemMemoryBuffer_Cleanup(&script->Functions);
    totemMemoryBuffer_Cleanup(&script->GlobalData);
    totemMemoryBuffer_Cleanup(&script->GlobalRegisters);
    totemMemoryBuffer_Cleanup(&script->Instructions);
    totemHashMap_Cleanup(&script->FunctionNameLookup);
}

void totemRuntime_Init(totemRuntime *runtime)
{
    totemMemoryBuffer_Init(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Init(&runtime->Scripts, sizeof(totemScript));
    totemHashMap_Init(&runtime->ScriptLookup);
    totemHashMap_Init(&runtime->NativeFunctionsLookup);
    totemHashMap_Init(&runtime->InternedStrings);
    runtime->FunctionCallFreeList = NULL;
}

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions);
    totemMemoryBuffer_Reset(&runtime->Scripts);
    totemHashMap_Reset(&runtime->ScriptLookup);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
    totemHashMap_Reset(&runtime->InternedStrings);
}

void totemRuntime_Cleanup(totemRuntime *runtime)
{
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctions);
    totemMemoryBuffer_Cleanup(&runtime->Scripts);
    totemHashMap_Cleanup(&runtime->ScriptLookup);
    totemHashMap_Cleanup(&runtime->NativeFunctionsLookup);
    
    // clean up interned strings
    for(size_t i = 0; i < runtime->InternedStrings.NumBuckets; i++)
    {
        for(totemHashMapEntry *entry = runtime->InternedStrings.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemInternedStringHeader *hdr = (totemInternedStringHeader*)entry->Value;
            if(hdr)
            {
                totem_CacheFree(hdr, sizeof(totemInternedStringHeader) + hdr->Length);
            }
        }
    }
    
    totemHashMap_Cleanup(&runtime->InternedStrings);
    
    // clean up function calls
    while(runtime->FunctionCallFreeList)
    {
        totemFunctionCall *call = runtime->FunctionCallFreeList;
        runtime->FunctionCallFreeList = call->Prev;
        totem_CacheFree(call, sizeof(totemFunctionCall));
    }
}

const char *totemInternedStringHeader_GetString(totemInternedStringHeader *hdr)
{
    char *ptr = (char*)hdr;
    return (const char*)(ptr + sizeof(totemInternedStringHeader));
}

totemInternedStringHeader *totemRuntime_InternString(totemRuntime *runtime, totemString *str)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->InternedStrings, str->Value, str->Length);
    if(result)
    {
        return (totemInternedStringHeader*)result->Value;
    }
    
    size_t toAllocate = str->Length + sizeof(totemInternedStringHeader);
    
    totemInternedStringHeader *newStr = totem_CacheMalloc(toAllocate);
    if(!newStr)
    {
        return NULL;
    }
    
    char *newStrVal = (char*)totemInternedStringHeader_GetString(newStr);
    
    newStr->Hash = totem_Hash(str->Value, str->Length);
    newStr->Length = str->Length;
    memcpy(newStrVal, str->Value, str->Length);
    
    if(!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, newStrVal, newStr->Length, (uintptr_t)newStr, newStr->Hash))
    {
        totem_CacheFree(newStr, toAllocate);
        return NULL;
    }
    
    return newStr;
}

totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemString *name, size_t *indexOut)
{
    totemScript *script = NULL;
    totemHashMapEntry *existing = totemHashMap_Find(&runtime->ScriptLookup, name->Value, name->Length);
    if(existing == NULL)
    {
        *indexOut = totemMemoryBuffer_GetNumObjects(&runtime->Scripts);
        if(!totemHashMap_Insert(&runtime->ScriptLookup, name->Value, name->Length, *indexOut))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        script = totemMemoryBuffer_Secure(&runtime->Scripts, 1);
        if(!script)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    else
    {
        *indexOut = existing->Value;
        script = totemMemoryBuffer_Get(&runtime->Scripts, *indexOut);
        totemScript_Reset(script);
    }
    
    totemScript_Init(script);
    
    // 1. intern strings
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.GlobalRegisterStrings); i++)
    {
        totemOperandXUnsigned index = *((totemOperandXUnsigned*)totemMemoryBuffer_Get(&build->GlobalRegisters.GlobalRegisterStrings, i));
        totemRegister *reg = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, index);
        
        totemString *str = (totemString*)reg->Value.InternedString;
        totemInternedStringHeader *internedStr = totemRuntime_InternString(runtime, str);
        if(!internedStr)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        reg->Value.InternedString = internedStr;
    }
    
    // 2. check script function names
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->Functions); i++)
    {
        size_t dummyAddr;
        totemFunction *func = totemMemoryBuffer_Get(&build->Functions, i);
        
        if(totemRuntime_GetNativeFunctionAddress(runtime, &func->Name, &dummyAddr))
        {
            build->ErrorContext = func;
            return totemLinkStatus_Break(totemLinkStatus_FunctionAlreadyDeclared);
        }
    }
    
    // 3. link native functions
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->NativeFunctionCallInstructions); i++)
    {
        size_t *instructionAddr = totemMemoryBuffer_Get(&build->NativeFunctionCallInstructions, i);
        totemInstruction *instruction = totemMemoryBuffer_Get(&build->Instructions, *instructionAddr);
        
        totemOperandXUnsigned functionAddr = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*instruction);
        
        totemString *funcName = totemMemoryBuffer_Get(&build->NativeFunctionNames, functionAddr);
        if(!funcName)
        {
            return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
        }
        
        size_t funcAddr = 0;
        if(!totemRuntime_GetNativeFunctionAddress(runtime, funcName, &funcAddr))
        {
            build->ErrorContext = funcName;
            return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
        }
        
        totemOperandXUnsigned realFuncAddr = (totemOperandXUnsigned)funcAddr;
        if(totemInstruction_SetBxUnsigned(instruction, realFuncAddr) != totemEvalStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionAddress);
        }
    }
    
    // 4. link native function pointers
    for(size_t i = 0; i < build->GlobalRegisters.FunctionPointers.NumBuckets; i++)
    {
        for(totemHashMapEntry *entry = build->GlobalRegisters.FunctionPointers.Buckets[i]; entry != NULL; entry = entry->Next)
        {
            totemOperandXUnsigned registerIndex = (totemOperandXUnsigned)entry->Value;
            totemRegisterPrototype *reg = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, registerIndex);
            
            if(reg == NULL || reg->DataType != totemDataType_Function)
            {
                return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionAddress);
            }
            
            if(reg->Value.FunctionPointer.Type == totemFunctionType_Native)
            {
                totemString *funcName = totemMemoryBuffer_Get(&build->NativeFunctionNames, reg->Value.FunctionPointer.Address);
                if(!funcName)
                {
                    return totemLinkStatus_Break(totemLinkStatus_InvalidNativeFunctionName);
                }
                
                size_t funcAddr = 0;
                if(!totemRuntime_GetNativeFunctionAddress(runtime, funcName, &funcAddr))
                {
                    build->ErrorContext = funcName;
                    return totemLinkStatus_Break(totemLinkStatus_FunctionNotDeclared);
                }
                
                reg->Value.FunctionPointer.Address = (totemOperandXUnsigned)funcAddr;
            }
        }
    }
    
    // TODO: No real need to copy this data, replace all of this with a single pointer swap...
    
    // 4. global register values
    size_t numGlobalRegs = totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.Registers);
    
    if(!totemMemoryBuffer_Secure(&script->GlobalRegisters, numGlobalRegs))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    for(size_t i = 0; i < numGlobalRegs; i++)
    {
        totemRegisterPrototype *prototype = totemMemoryBuffer_Get(&build->GlobalRegisters.Registers, i);
        totemRegister *reg = totemMemoryBuffer_Get(&script->GlobalRegisters, i);
        
        reg->DataType = prototype->DataType;
        reg->Value.Data = prototype->Value.Data;
    }
    
    // 5. instructions
    totemMemoryBuffer_TakeFrom(&script->Instructions, &build->Instructions);
    
    // 6. functions
    totemMemoryBuffer_TakeFrom(&script->Functions, &build->Functions);
    
    // 7. function name lookup
    totemHashMap_TakeFrom(&script->FunctionNameLookup, &build->FunctionLookup);
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, size_t *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    totemNativeFunction *addr = NULL;;
    if(result != NULL)
    {
        addr = totemMemoryBuffer_Get(&runtime->NativeFunctions, result->Value);
        *addressOut = result->Value;
    }
    else
    {
        *addressOut = totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions);
        addr = totemMemoryBuffer_Secure(&runtime->NativeFunctions, 1);
        if(!addr)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
        if(!totemHashMap_Insert(&runtime->NativeFunctionsLookup, name->Value, name->Length, *addressOut))
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    *addr = func;
    return totemLinkStatus_Success;
}

totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, size_t *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    if(result != NULL)
    {
        *addressOut = result->Value;
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemFunctionCall *totemRuntime_SecureFunctionCall(totemRuntime *runtime)
{
    totemFunctionCall *call = NULL;
    
    if(runtime->FunctionCallFreeList)
    {
        call = runtime->FunctionCallFreeList;
        runtime->FunctionCallFreeList = call->Prev;
    }
    else
    {
        call = totem_CacheMalloc(sizeof(totemFunctionCall));
        if(!call)
        {
            return NULL;
        }
    }
    
    memset(call, 0, sizeof(totemFunctionCall));
    return call;
}

void totemRuntime_FreeFunctionCall(totemRuntime *runtime, totemFunctionCall *call)
{
    call->Prev = NULL;
    
    if(runtime->FunctionCallFreeList)
    {
        call->Prev = runtime->FunctionCallFreeList;
    }
    
    runtime->FunctionCallFreeList = call;
}

totemBool totemExecState_Init(totemExecState *state, totemRuntime *runtime, size_t numRegisters)
{
    memset(state, 0, sizeof(totemExecState));
    
    state->Runtime = runtime;
    
    if(state->Registers[totemOperandType_LocalRegister])
    {
        totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
    }
    
    state->Registers[totemOperandType_LocalRegister] = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
    if(state->Registers[totemOperandType_LocalRegister] == NULL)
    {
        return totemBool_False;
    }
    
    memset(state->Registers[totemOperandType_LocalRegister], 0, sizeof(totemRegister) * state->MaxLocalRegisters);
    state->MaxLocalRegisters = numRegisters;
    state->UsedLocalRegisters = 0;
    return totemBool_True;
}

void totemExecState_Cleanup(totemExecState *state)
{
    totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
}

totemExecStatus totemExecState_PushFunctionCall(totemExecState *state, totemFunctionType funcType, size_t functionAddress, totemRegister *returnReg, uint8_t numRegisters)
{
    totemFunctionCall *call = totemRuntime_SecureFunctionCall(state->Runtime);
    if(call == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    if((state->MaxLocalRegisters - state->UsedLocalRegisters) < numRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
    }
    
    call->ReturnRegister = returnReg;
    call->PreviousFrameStart = state->Registers[totemOperandType_LocalRegister];
    call->Type = funcType;
    call->FunctionHandle = functionAddress;
    call->ResumeAt = NULL;
    call->Prev = NULL;
    call->NumArguments = 0;
    call->NumRegisters = numRegisters;
    call->FrameStart = state->Registers[totemOperandType_LocalRegister] + (state->CallStack ? state->CallStack->NumRegisters : 0);
    
    // reset registers to be used
    memset(call->FrameStart, 0, numRegisters * sizeof(totemRegister));
    
    if(state->CurrentInstruction)
    {
        totemOperationType type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
        
        if(type == totemOperationType_FunctionArg)
        {
            totemOperandXUnsigned numArgs = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*state->CurrentInstruction);
            
            if(numArgs > call->NumRegisters)
            {
                return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
            }
            
            do
            {
                //totemInstruction_Print(stdout, *state->CurrentInstruction);
                totemRegister *argument = TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));
                
                totemRegister_Assign(&call->FrameStart[call->NumArguments], argument);
                
                state->CurrentInstruction++;
                call->NumArguments++;
                type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
            }
            while(type == totemOperationType_FunctionArg);
        }
    }
    
    state->UsedLocalRegisters += numRegisters;
    state->Registers[totemOperandType_LocalRegister] = call->FrameStart;
    
    call->ResumeAt = state->CurrentInstruction;
    
    if(state->CallStack)
    {
        call->Prev = state->CallStack;
    }
    
    state->CallStack = call;
    
    return totemExecStatus_Continue;
}

void totemExecState_PopFunctionCall(totemExecState *state)
{
    if(state->CallStack)
    {
        // clean up any remaining arrays
        for(size_t i = 0; i < state->CallStack->NumRegisters; i++)
        {
            totemRegister *reg = &state->Registers[totemOperandType_LocalRegister][i];
            
            if(reg->DataType == totemDataType_Array)
            {
                totemRuntimeArray_DefRefCount(reg->Value.Array);
            }
        }
        
        state->CurrentInstruction = state->CallStack->ResumeAt;
        state->UsedLocalRegisters -= state->CallStack->NumRegisters;
        state->Registers[totemOperandType_LocalRegister] = state->CallStack->PreviousFrameStart;
        
        totemFunctionCall *call = state->CallStack;
        totemFunctionCall *prev = call->Prev;
        totemRuntime_FreeFunctionCall(state->Runtime, call);
        state->CallStack = prev;
    }
}

totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionAddress, totemRegister *returnRegister)
{
    totemActor *prevActor = state->CurrentActor;
    
    if(actor)
    {
        state->CurrentActor = actor;
    }
    
    totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, state->CurrentActor->ScriptHandle);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }
    
    state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)state->CurrentActor->GlobalRegisters.Data;
    
    totemFunction *function = totemMemoryBuffer_Get(&script->Functions, functionAddress);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Script, functionAddress, returnRegister, function->RegistersNeeded);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    state->CurrentInstruction = totemMemoryBuffer_Get(&script->Instructions, function->InstructionsStart);
    if(state->CurrentInstruction == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    do
    {
        status = totemExecState_ExecInstruction(state);
    }
    while(status == totemExecStatus_Continue);
    
    totemExecState_PopFunctionCall(state);
    
    if(actor)
    {
        state->CurrentActor = prevActor;
    }
    
    if(status == totemExecStatus_Return)
    {
        if(state->CallStack)
        {
            return totemExecStatus_Continue;
        }
    }
    
    return status;
}

totemExecStatus totemExecState_ExecNative(totemExecState *state, totemRegister *returnRegister, totemOperandXUnsigned functionHandle)
{
    totemNativeFunction *function = totemMemoryBuffer_Get(&state->Runtime->NativeFunctions, functionHandle);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
    }
    
    // num args
    uint8_t numArgs = 0;
    if(TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction) == totemOperationType_FunctionArg)
    {
        numArgs = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*state->CurrentInstruction);
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Native, functionHandle, returnRegister, numArgs);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    status = (*function)(state);
    totemExecState_PopFunctionCall(state);
    return status;
}

totemExecStatus totemExecState_ExecInstruction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    
    totemOperationType op = TOTEM_INSTRUCTION_GET_OP(instruction);
    totemExecStatus status = totemExecStatus_Continue;

    switch(op)
    {
        case totemOperationType_None:
            state->CurrentInstruction++;
            status = totemExecStatus_Continue;
            break;
            
        case totemOperationType_Move:
            status = totemExecState_ExecMove(state);
            break;
            
        case totemOperationType_Add:
            status = totemExecState_ExecAdd(state);
            break;
            
        case totemOperationType_Subtract:
            status = totemExecState_ExecSubtract(state);
            break;
            
        case totemOperationType_Multiply:
            status = totemExecState_ExecMultiply(state);
            break;
            
        case totemOperationType_Divide:
            status = totemExecState_ExecDivide(state);
            break;
            
        case totemOperationType_Power:
            status = totemExecState_ExecPower(state);
            break;
            
        case totemOperationType_Equals:
            status = totemExecState_ExecEquals(state);
            break;
            
        case totemOperationType_NotEquals:
            status = totemExecState_ExecNotEquals(state);
            break;
            
        case totemOperationType_LessThan:
            status = totemExecState_ExecLessThan(state);
            break;
            
        case totemOperationType_LessThanEquals:
            status = totemExecState_ExecLessThanEquals(state);
            break;
            
        case totemOperationType_MoreThan:
            status = totemExecState_ExecMoreThan(state);
            break;
            
        case totemOperationType_MoreThanEquals:
            status = totemExecState_ExecMoreThanEquals(state);
            break;
            
        case totemOperationType_ConditionalGoto:
            status = totemExecState_ExecConditionalGoto(state);
            break;
            
        case totemOperationType_Goto:
            status = totemExecState_ExecGoto(state);
            break;
            
        case totemOperationType_NativeFunction:
            status = totemExecState_ExecNativeFunction(state);
            break;
            
        case totemOperationType_ScriptFunction:
            status = totemExecState_ExecScriptFunction(state);
            break;
            
        case totemOperationType_Return:
            status = totemExecState_ExecReturn(state);
            break;
            
        case totemOperationType_NewArray:
            status = totemExecState_ExecNewArray(state);
            break;
            
        case totemOperationType_ArrayGet:
            status = totemExecState_ExecArrayGet(state);
            break;
            
        case totemOperationType_ArraySet:
            status = totemExecState_ExecArraySet(state);
            break;
            
        case totemOperationType_MoveToGlobal:
            status = totemExecState_ExecMoveToGlobal(state);
            break;
            
        case totemOperationType_MoveToLocal:
            status = totemExecState_ExecMoveToLocal(state);
            break;
            
        case totemOperationType_Is:
            status = totemExecState_ExecIs(state);
            break;
            
        case totemOperationType_As:
            status = totemExecState_ExecAs(state);
            break;
            
        case totemOperationType_FunctionPointer:
            status = totemExecState_ExecFunctionPointer(state);
            break;
            
        case totemOperationType_Assert:
            status = totemExecState_ExecAssert(state);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
    }
    
    // debug
    /*
    totemInstruction_Print(stdout, instruction);
    totemInstructionType type = totemOperationType_GetInstructionType(op);
    switch(type)
    {
        case totemInstructionType_Abc:
        {
            totemRegister *a = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
            totemRegister *b = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
            totemRegister *c = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
            totemRegister_Print(stdout, a);
            totemRegister_Print(stdout, b);
            totemRegister_Print(stdout, c);
            break;
        }
            
        case totemInstructionType_Abx:
        {
            totemRegister *a = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
            totemRegister_Print(stdout, a);
            break;
        }
            
        case totemInstructionType_Axx:
            break;
    }*/
    
    return status;
}

totemExecStatus totemExecState_ExecAssert(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    
    if(dst->Value.Data != 0)
    {
        state->CurrentInstruction++;
        return totemExecStatus_Continue;
    }
    else
    {
        return totemExecStatus_Break(totemExecStatus_FailedAssertion);
    }
}

totemExecStatus totemExecState_ExecFunctionPointer(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    if(src->DataType != totemDataType_Function)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemExecStatus status = totemExecStatus_Continue;
    
    switch(src->Value.FunctionPointer.Type)
    {
        case totemFunctionType_Native:
            state->CurrentInstruction++;
            status = totemExecState_ExecNative(state, dst, src->Value.FunctionPointer.Address);
            break;
            
        case totemFunctionType_Script:
            state->CurrentInstruction++;
            status = totemExecState_Exec(state, NULL, src->Value.FunctionPointer.Address, dst);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
    }
    
    return status;
}

totemExecStatus totemExecState_ExecAs(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *typeSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemDataType type = typeSrc->Value.DataType;
    
    if(typeSrc->DataType != totemDataType_Type)
    {
        type = typeSrc->DataType;
    }
    
    switch(TOTEM_TYPEPAIR(src->DataType, type))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            dst->Value.Data = src->Value.Data;
            dst->DataType = src->DataType;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            dst->Value.Float = (totemFloat)src->Value.Int;
            dst->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            dst->Value.Int = (totemInt)src->Value.Float;
            dst->DataType = totemDataType_Int;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecIs(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *typeSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemDataType type = typeSrc->Value.DataType;
    
    if(typeSrc->DataType != totemDataType_Type)
    {
        type = typeSrc->DataType;
    }
    
    dst->Value.Int = src->DataType == type;
    dst->DataType = totemDataType_Int;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMove(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    totemExecStatus status = totemRegister_Assign(destination, source);
    if(status == totemExecStatus_Continue)
    {
        state->CurrentInstruction++;
    }
    
    return status;
}

totemExecStatus totemExecState_ExecMoveToGlobal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *src = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    totemExecStatus status = totemRegister_Assign(globalReg, src);
    if(status == totemExecStatus_Continue)
    {
        state->CurrentInstruction++;
    }
    
    return status;
}

totemExecStatus totemExecState_ExecMoveToLocal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    totemExecStatus status = totemRegister_Assign(dst, globalReg);
    if(status == totemExecStatus_Continue)
    {
        state->CurrentInstruction++;
    }
    
    return status;
}

totemExecStatus totemExecState_ExecAdd(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int + source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Float = source1->Value.Float + source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Float = source1->Value.Float + source2->Value.Int;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Float = source1->Value.Int + source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecSubtract(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int - source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Float = source1->Value.Float - source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Float = source1->Value.Float - source2->Value.Int;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Float = source1->Value.Int - source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMultiply(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int * source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Float = source1->Value.Float * source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Float = source1->Value.Float * source2->Value.Int;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Float = source1->Value.Int * source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecDivide(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int / source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Float = source1->Value.Float / source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Float = source1->Value.Float / source2->Value.Int;
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Float = source1->Value.Int / source2->Value.Float;
            destination->DataType = totemDataType_Float;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecPower(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Float = pow((totemFloat)source1->Value.Int, (totemFloat)source2->Value.Int);
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Float = pow(source1->Value.Float, source2->Value.Float);
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Float = pow(source1->Value.Float, (totemFloat)source2->Value.Int);
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Float = pow((totemFloat)source1->Value.Int, source2->Value.Float);
            destination->DataType = totemDataType_Float;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    destination->Value.Int = source1->Value.Data == source2->Value.Data && source1->DataType == source2->DataType;
    destination->DataType = totemDataType_Int;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecNotEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    destination->Value.Int = source1->Value.Data != source2->Value.Data || source1->DataType != source2->DataType;
    destination->DataType = totemDataType_Int;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int < source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Int = source1->Value.Float < source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Int = source1->Value.Float < source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Int = source1->Value.Int < source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int <= source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Int = source1->Value.Float <= source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Int = source1->Value.Float <= source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Int = source1->Value.Int <= source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int > source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Int = source1->Value.Float > source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Int = source1->Value.Float > source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Int = source1->Value.Int > source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }

    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Int):
            destination->Value.Int = source1->Value.Int >= source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Int = source1->Value.Float >= source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Int = source1->Value.Float >= source2->Value.Int;
            destination->DataType = totemDataType_Int;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Int = source1->Value.Int >= source2->Value.Float;
            destination->DataType = totemDataType_Int;
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecGoto(totemExecState *state)
{
    totemOperandXSigned offset = TOTEM_INSTRUCTION_GET_AX_SIGNED(*state->CurrentInstruction);
    
    state->CurrentInstruction += offset;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecConditionalGoto(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    
    totemOperandXSigned offset = TOTEM_INSTRUCTION_GET_BX_SIGNED(instruction);
    
    if(source->Value.Data == 0)
    {
        state->CurrentInstruction += offset;
    }
    else
    {
        state->CurrentInstruction++;
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecScriptFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned functionIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    state->CurrentInstruction++;
    return totemExecState_Exec(state, NULL, functionIndex, destination);
}

totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *returnRegister = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned functionHandle = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    state->CurrentInstruction++;
    return totemExecState_ExecNative(state, returnRegister, functionHandle);
}

totemExecStatus totemExecState_ExecReturn(totemExecState *state)
{
    totemFunctionCall *call = state->CallStack;
    
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandXUnsigned option = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    if(option == totemReturnOption_Register)
    {
        totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
        totemRegister_Assign(call->ReturnRegister, source);
    }
    
    return totemExecStatus_Return;
}

totemExecStatus totemExecState_ExecNewArray(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(indexSrc->Value.Int < 0 || indexSrc->Value.Int >= UINT32_MAX)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    uint32_t numRegisters = (uint32_t)indexSrc->Value.Int;
    
    totemRegister src;
    src.DataType = totemDataType_Array;
    src.Value.Array = totem_CacheMalloc(sizeof(totemRuntimeArray));
    if(!src.Value.Array)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    src.Value.Array->RefCount = 0;
    src.Value.Array->NumRegisters = numRegisters;
    src.Value.Array->Registers = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
    if(!src.Value.Array->Registers)
    {
        totem_CacheFree(src.Value.Array, sizeof(totemRuntimeArray));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(src.Value.Array->Registers, 0, sizeof(totemRegister) * numRegisters);
    
    totemExecStatus status = totemRegister_Assign(dst, &src);
    if(status == totemExecStatus_Continue)
    {
        state->CurrentInstruction++;
    }
    
    return status;
}

totemExecStatus totemExecState_ExecArrayGet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(src->DataType != totemDataType_Array)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemInt index = indexSrc->Value.Int;
    
    if(index >= src->Value.Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecStatus status = totemRegister_Assign(dst, &src->Value.Array->Registers[index]);
    if(status == totemExecStatus_Continue)
    {
        state->CurrentInstruction++;
    }
    
    return status;
}

totemExecStatus totemExecState_ExecArraySet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(dst->DataType != totemDataType_Array)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(indexSrc->Value.Int < 0 || indexSrc->Value.Int >= UINT32_MAX)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemInt index = indexSrc->Value.Int;
    
    if(index >= dst->Value.Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecStatus status = totemRegister_Assign(&dst->Value.Array->Registers[index], src);
    if(status == totemExecStatus_Continue)
    {
        state->CurrentInstruction++;
    }
    
    return status;
}

const char *totemExecStatus_Describe(totemExecStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemExecStatus_FailedAssertion);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Continue);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_NativeFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RegisterOverflow);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Return);
            TOTEM_STRINGIFY_CASE(totemExecStatus_ScriptFunctionNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_ScriptNotFound);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnexpectedDataType);
            TOTEM_STRINGIFY_CASE(totemExecStatus_UnrecognisedOperation);
            TOTEM_STRINGIFY_CASE(totemExecStatus_Stop);
            TOTEM_STRINGIFY_CASE(totemExecStatus_IndexOutOfBounds);
            TOTEM_STRINGIFY_CASE(totemExecStatus_RefCountOverflow);
    }
    
    return "UNKNOWN";
}

const char *totemLinkStatus_Describe(totemLinkStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemLinkStatus_FunctionAlreadyDeclared);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_FunctionNotDeclared);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_InvalidNativeFunctionAddress);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_InvalidNativeFunctionName);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLinkStatus_Success);
    }
    
    return "UNKNOWN";
}