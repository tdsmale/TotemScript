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
#include <string.h>

#define TOTEM_GET_OPERANDA_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction)]
#define TOTEM_GET_OPERANDB_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction)]
#define TOTEM_GET_OPERANDC_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction)]
#define TOTEM_ENFORCE_TYPE(reg, type) if(reg->DataType != type) return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);

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
    if(arr->RefCount <= 1)
    {
        arr->RefCount = 0;
        totemRuntimeArray_Destroy(arr);
    }
    else
    {
        arr->RefCount--;
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

totemExecStatus totemRegister_AssignWeak(totemRegister *dst, totemRegister *src)
{
    if(dst->DataType == totemDataType_Array)
    {
        totemRuntimeArray_DefRefCount(dst->Value.Array);
    }
    
    memcpy(dst, src, sizeof(totemRegister));
    
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

void totemScript_Reset(totemScript *script)
{
    totemMemoryBuffer_Reset(&script->Functions, sizeof(totemFunction));
    totemMemoryBuffer_Reset(&script->GlobalData, sizeof(char));
    totemMemoryBuffer_Reset(&script->GlobalRegisters, sizeof(totemRegister));
    totemMemoryBuffer_Reset(&script->Instructions, sizeof(totemInstruction));
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
    totemRuntime_Reset(runtime);
}

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Reset(&runtime->Scripts, sizeof(totemScript));
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
}

const char *totemInternedStringHeader_GetString(totemInternedStringHeader *hdr)
{
    return (const char*)(hdr + sizeof(totemInternedStringHeader));
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
    
    newStr->Hash = totem_Hash(str->Value, str->Length);
    newStr->Length = str->Length;
    memcpy(newStr + sizeof(totemInternedStringHeader), str->Value, str->Length);
    
    if(!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, str->Value, str->Length, (uintptr_t)newStr, newStr->Hash))
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
    }
    
    totemScript_Reset(script);
    
    // 1. intern strings
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.GlobalRegisterStrings); i++)
    {
        totemRegisterIndex index = *((totemRegisterIndex*)totemMemoryBuffer_Get(&build->GlobalRegisters.GlobalRegisterStrings, i));
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
    
    // TODO: No real need to copy this data, replace all of this with a single pointer swap...
    memcpy(&script->Instructions, &build->Instructions, sizeof(totemMemoryBuffer));
    memcpy(&script->FunctionNameLookup, &build->FunctionLookup, sizeof(totemHashMap));
    memcpy(&script->GlobalRegisters, &build->GlobalRegisters.Registers, sizeof(totemMemoryBuffer));
    memcpy(&script->Functions, &build->Functions, sizeof(totemMemoryBuffer));
    
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
    state->Runtime = runtime;
    
    if(state->Registers[totemOperandType_LocalRegister])
    {
        totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
    }
    
    if(numRegisters > TOTEM_MAX_REGISTERS)
    {
        numRegisters = TOTEM_MAX_REGISTERS;
    }
    
    state->Registers[totemOperandType_LocalRegister] = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
    if(state->Registers[totemOperandType_LocalRegister] == NULL)
    {
        return totemBool_False;
    }
    
    state->MaxLocalRegisters = numRegisters;
    state->UsedLocalRegisters = 0;
    return totemBool_True;
}

void totemExecState_Cleanup(totemExecState *state)
{
    totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
}

totemExecStatus totemExecState_PushFunctionCall(totemExecState *state, totemFunctionType type, size_t functionAddress, totemActor *actor, totemRegister *returnReg)
{
    totemFunctionCall *call = totemRuntime_SecureFunctionCall(state->Runtime);
    if(call == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    call->ReturnRegister = returnReg;
    call->RegisterFrameStart = &state->Registers[totemRegisterScopeType_Local][state->UsedLocalRegisters];
    call->Type = type;
    call->Actor = actor;
    call->FunctionHandle = functionAddress;
    call->ResumeAt = NULL;
    call->Prev = NULL;
    call->NumArguments = 0;
    
    if(state->CurrentInstruction)
    {
        totemOperationType type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
        
        if(type == totemOperationType_FunctionArg)
        {
            if(state->UsedLocalRegisters + TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*state->CurrentInstruction) > state->MaxLocalRegisters)
            {
                return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
            }
        }
        
        for(/* nada */; type == totemOperationType_FunctionArg; type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction))
        {
            call->RegisterFrameStart[call->NumArguments] = *TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));
            state->CurrentInstruction++;
            call->NumArguments++;
        }
    }
    
    if(state->CallStack)
    {
        state->CallStack->ResumeAt = state->CurrentInstruction;
        call->Prev = state->CallStack;
    }
    
    state->CallStack = call;
    state->Registers[totemRegisterScopeType_Global] = (totemRegister*)actor->GlobalRegisters.Data;
    state->Registers[totemRegisterScopeType_Local] = call->RegisterFrameStart;
    
    return totemExecStatus_Continue;
}

void totemExecState_PopFunctionCall(totemExecState *state)
{
    if(state->CallStack)
    {
        totemFunctionCall *call = state->CallStack;
        totemFunctionCall *prev = call->Prev;
        totemRuntime_FreeFunctionCall(state->Runtime, call);
        state->CallStack = prev;
        
        if(state->CallStack)
        {
            state->Registers[totemRegisterScopeType_Global] = (totemRegister*)state->CallStack->Actor->GlobalRegisters.Data;
            state->Registers[totemRegisterScopeType_Local] = state->CallStack->RegisterFrameStart;
            state->CurrentInstruction = state->CallStack->ResumeAt;
        }
    }
}

totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, size_t functionAddress, totemRegister *returnRegister)
{
    totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, actor->ScriptHandle);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }
    
    totemFunction *function = totemMemoryBuffer_Get(&script->Functions, functionAddress);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }

    if((state->MaxLocalRegisters - state->UsedLocalRegisters) < function->RegistersNeeded)
    {
        return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
    }
    
    // reset values to be used
    memset(&state->Registers[totemRegisterScopeType_Local][state->UsedLocalRegisters], 0, function->RegistersNeeded * sizeof(totemRegister));
    state->UsedLocalRegisters += function->RegistersNeeded;
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Script, functionAddress, actor, returnRegister);
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
    
    state->UsedLocalRegisters -= function->RegistersNeeded;
    
    // clean up any remaining arrays
    for(size_t i = 0; i < function->RegistersNeeded; i++)
    {
        totemRegister *reg = &state->Registers[totemRegisterScopeType_Local][state->UsedLocalRegisters + i];
        
        if(reg->DataType == totemDataType_Array)
        {
            if(reg->Value.Array->RefCount > 0)
            {
                totemRuntimeArray_DefRefCount(reg->Value.Array);
            }
        }
    }
    
    if(status == totemExecStatus_Return)
    {
        totemExecState_PopFunctionCall(state);
        if(state->CallStack)
        {
            return totemExecStatus_Continue;
        }
    }
    
    return status;
}

totemExecStatus totemExecState_ExecInstruction(totemExecState *state)
{
    totemInstruction_Print(stdout, *state->CurrentInstruction);
    totemOperationType op = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
    
    switch(op)
    {
        case totemOperationType_None:
            state->CurrentInstruction++;
            return totemExecStatus_Continue;
            
        case totemOperationType_Move:
            return totemExecState_ExecMove(state);

        case totemOperationType_Add:
            return totemExecState_ExecAdd(state);
            
        case totemOperationType_Subtract:
            return totemExecState_ExecSubtract(state);
            
        case totemOperationType_Multiply:
            return totemExecState_ExecMultiply(state);
            
        case totemOperationType_Divide:
            return totemExecState_ExecDivide(state);
            
        case totemOperationType_Power:
            return totemExecState_ExecPower(state);
            
        case totemOperationType_Equals:
            return totemExecState_ExecEquals(state);

        case totemOperationType_NotEquals:
            return totemExecState_ExecNotEquals(state);
            
        case totemOperationType_LessThan:
            return totemExecState_ExecLessThan(state);
            
        case totemOperationType_LessThanEquals:
            return totemExecState_ExecLessThanEquals(state);
            
        case totemOperationType_MoreThan:
            return totemExecState_ExecMoreThan(state);
            
        case totemOperationType_MoreThanEquals:
            return totemExecState_ExecMoreThanEquals(state);
            
        case totemOperationType_ConditionalGoto:
            return totemExecState_ExecConditionalGoto(state);
            
        case totemOperationType_Goto:
            return totemExecState_ExecGoto(state);
            
        case totemOperationType_NativeFunction:
            return totemExecState_ExecNativeFunction(state);
            
        case totemOperationType_ScriptFunction:
            return totemExecState_ExecScriptFunction(state);

        case totemOperationType_Return:
            return totemExecState_ExecReturn(state);
            
        case totemOperationType_NewArray:
            return totemExecState_ExecNewArray(state);
            
        case totemOperationType_ArrayGet:
            return totemExecState_ExecArrayGet(state);
            
        case totemOperationType_ArraySet:
            return totemExecState_ExecArraySet(state);

        default:
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
    }
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
            destination->Value.Float = pow(source1->Value.Int, source2->Value.Int);
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Float):
            destination->Value.Float = pow(source1->Value.Float, source2->Value.Float);
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Float, totemDataType_Int):
            destination->Value.Float = pow(source1->Value.Float, source2->Value.Int);
            destination->DataType = totemDataType_Float;
            break;
            
        case TOTEM_TYPEPAIR(totemDataType_Int, totemDataType_Float):
            destination->Value.Float = pow(source1->Value.Int, source2->Value.Float);
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
    
    TOTEM_ENFORCE_TYPE(source1, source2->DataType);
    
    destination->Value.Int = source1->Value.Data == source2->Value.Data;
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
    
    TOTEM_ENFORCE_TYPE(source1, source2->DataType);
    
    destination->Value.Int = source1->Value.Data != source2->Value.Data;
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
    return totemExecState_Exec(state, state->CallStack->Actor, functionIndex, destination);
}

totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *returnRegister = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned functionHandle = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    totemNativeFunction *function = totemMemoryBuffer_Get(&state->Runtime->NativeFunctions, functionHandle);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
    }
    
    state->CurrentInstruction++;
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Native, functionHandle, state->CallStack->Actor, returnRegister);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    status = (*function)(state);
    totemExecState_PopFunctionCall(state);
    return status;
}

totemExecStatus totemExecState_ExecReturn(totemExecState *state)
{
    totemFunctionCall *call = state->CallStack;
    
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandXUnsigned option = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);

    if(option == totemReturnOption_Register)
    {
        totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
        call->ReturnRegister->Value = source->Value;
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
    
    totemExecStatus status = totemRegister_AssignWeak(dst, &src);
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
    
    totemExecStatus status = totemRegister_AssignWeak(dst, &src->Value.Array->Registers[index]);
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
    
    uint32_t index = (uint32_t)indexSrc->Value.Int;
    
    if(index >= dst->Value.Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecStatus status = totemRegister_AssignWeak(&dst->Value.Array->Registers[index], src);
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