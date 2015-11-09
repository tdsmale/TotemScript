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

#define TOTEM_GET_OPERANDA_REGISTER(state, instruction) &state->Registers[instruction.Abc.OperandAType][instruction.Abc.OperandAIndex]
#define TOTEM_GET_OPERANDB_REGISTER(state, instruction) &state->Registers[instruction.Abc.OperandBType][instruction.Abc.OperandBIndex]
#define TOTEM_GET_OPERANDC_REGISTER(state, instruction) &state->Registers[instruction.Abc.OperandCType][instruction.Abc.OperandCIndex]
#define TOTEM_ENFORCE_TYPE(reg, type) if(reg->DataType != type) return totemExecStatus_UnexpectedDataType;

totemExecStatus totemActor_Init(totemActor *actor, totemRuntime *runtime, size_t scriptAddress)
{
    actor->ScriptHandle = scriptAddress;
    
    totemScript *script = totemMemoryBuffer_Get(&runtime->Scripts, scriptAddress);
    if(script == NULL)
    {
        return totemExecStatus_ScriptNotFound;
    }
    
    void *globalData = totem_Malloc(script->GlobalData.Length);
    if(globalData == NULL)
    {
        return totemExecStatus_OutOfMemory;
    }
    
    memcpy(globalData, script->GlobalData.Data, script->GlobalData.Length);
    memcpy(&actor->GlobalData, &script->GlobalData, sizeof(totemMemoryBuffer));
    actor->GlobalData.Data = globalData;
    
    return totemExecStatus_Continue;
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

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Reset(&runtime->Scripts, sizeof(totemScript));
    totemHashMap_Reset(&runtime->ScriptLookup);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
}

totemBool totemRuntime_RegisterScript(totemRuntime *runtime, totemBuildPrototype *build, totemString *name, size_t *indexOut)
{
    totemHashMapEntry *existing = totemHashMap_Find(&runtime->ScriptLookup, name->Value, name->Length);
    if(existing == NULL)
    {
        *indexOut = totemMemoryBuffer_GetNumObjects(&runtime->Scripts);
        if(!totemHashMap_Insert(&runtime->ScriptLookup, name->Value, name->Length, *indexOut))
        {
            return totemBool_False;
        }
        
        if(!totemMemoryBuffer_Secure(&runtime->Scripts, 1))
        {
            return totemBool_False;
        }
    }
    else
    {
        *indexOut = existing->Value;
    }
    
    totemScript *script = totemMemoryBuffer_Get(&runtime->Scripts, *indexOut);
    totemScript_Reset(script);
    
    // TODO: No real need to copy this data, replace all of this with a single pointer swap...
    memcpy(&script->Instructions, &build->Instructions, sizeof(totemMemoryBuffer));
    memcpy(&script->FunctionNameLookup, &build->FunctionLookup, sizeof(totemHashMap));
    memcpy(&script->GlobalData, &build->GlobalRegisters.StringData, sizeof(totemMemoryBuffer));
    memcpy(&script->GlobalRegisters, &build->GlobalRegisters.Registers, sizeof(totemMemoryBuffer));
    memcpy(&script->Functions, &build->Functions, sizeof(totemMemoryBuffer));
    
    return totemBool_True;
}

totemBool totemRuntime_RegisterNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, size_t *addressOut)
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
        if(!totemMemoryBuffer_Secure(&runtime->NativeFunctions, 1))
        {
            return totemBool_False;
        }
        
        addr = totemMemoryBuffer_Get(&runtime->NativeFunctions, *addressOut);
        if(!totemHashMap_Insert(&runtime->NativeFunctionsLookup, name->Value, name->Length, *addressOut))
        {
            return totemBool_False;
        }
    }
    
    *addr = func;
    return totemBool_True;
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
        call = totemMemoryBlock_Alloc(&runtime->LastMemBlock, sizeof(totemFunctionCall));
    }
    
    memset(call, 0, sizeof(totemFunctionCall));
    return call;
}

void totemRuntime_FreeFunctionCall(totemRuntime *runtime, totemFunctionCall *call)
{
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
        totem_Free(state->Registers[totemOperandType_LocalRegister]);
    }
    
    if(numRegisters > TOTEM_MAX_REGISTERS)
    {
        numRegisters = TOTEM_MAX_REGISTERS;
    }
    
    state->Registers[totemOperandType_LocalRegister] = totem_Malloc(sizeof(totemRegister) * numRegisters);
    if(state->Registers[totemOperandType_LocalRegister] == NULL)
    {
        return totemBool_False;
    }
    
    state->MaxLocalRegisters = numRegisters;
    return totemBool_True;
}

totemExecStatus totemExecState_PushFunctionCall(totemExecState *state, totemFunctionType type, size_t functionAddress, totemActor *actor)
{
    totemFunctionCall *call = totemRuntime_SecureFunctionCall(state->Runtime);
    if(call == NULL)
    {
        return totemExecStatus_OutOfMemory;
    }
    
    // get return register from current instruction
    call->ReturnRegister = TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));

    // determine start of next register frame
    call->RegisterFrameStart = &state->Registers[totemRegisterScopeType_Local][state->UsedLocalRegisters];
    
    // push arguments to next register frame & count them
    call->NumArguments = 0;
    state->CurrentInstruction++;
    while(state->CurrentInstruction->Abc.Operation == totemOperation_FunctionArg)
    {
        call->RegisterFrameStart[call->NumArguments] = *TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));
        state->CurrentInstruction++;
    }
    
    call->Type = type;
    call->Actor = actor;
    call->FunctionHandle = functionAddress;
    call->LastInstruction = state->CurrentInstruction;
    
    if(state->CallStack)
    {
        state->CallStack->LastInstruction = state->CurrentInstruction;
        call->Prev = state->CallStack;
    }
    
    state->CallStack = call;
    
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
    }
}

totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, size_t functionAddress, totemRegister *returnRegister)
{
    totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, actor->ScriptHandle);
    if(script == NULL)
    {
        return totemExecStatus_ScriptNotFound;
    }
    
    totemFunction *function = totemMemoryBuffer_Get(&script->Functions, functionAddress);
    if(function == NULL)
    {
        return totemExecStatus_ScriptFunctionNotFound;
    }
    
    if((state->MaxLocalRegisters - state->UsedLocalRegisters) < function->RegistersNeeded)
    {
        return totemExecStatus_RegisterOverflow;
    }
    
    state->UsedLocalRegisters += function->RegistersNeeded;
    
    state->CurrentInstruction = totemMemoryBuffer_Get(&script->Instructions, function->InstructionsStart);
    if(state->CurrentInstruction == NULL)
    {
        return totemExecStatus_ScriptFunctionNotFound;
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Script, functionAddress, actor);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    do
    {
        status = totemExecState_ExecInstruction(state);
    }
    while(status == totemExecStatus_Continue);

    state->CallStack->LastInstruction = state->CurrentInstruction;
    
    if(status == totemExecStatus_Return && state->CallStack)
    {
        state->CallStack = state->CallStack->Prev;
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
    switch(state->CurrentInstruction->Abc.Operation)
    {
        case totemOperation_None:
            state->CurrentInstruction++;
            return totemExecStatus_Continue;
            
        case totemOperation_Move:
            return totemExecState_ExecMove(state);

        case totemOperation_Add:
            return totemExecState_ExecAdd(state);
            
        case totemOperation_Subtract:
            return totemExecState_ExecSubtract(state);
            
        case totemOperation_Multiply:
            return totemExecState_ExecMultiply(state);
            
        case totemOperation_Divide:
            return totemExecState_ExecDivide(state);
            
        case totemOperation_Power:
            return totemExecState_ExecPower(state);
            
        case totemOperation_Equals:
            return totemExecState_ExecEquals(state);

        case totemOperation_NotEquals:
            return totemExecState_ExecNotEquals(state);
            
        case totemOperation_LessThan:
            return totemExecState_ExecLessThan(state);
            
        case totemOperation_LessThanEquals:
            return totemExecState_ExecLessThanEquals(state);
            
        case totemOperation_MoreThan:
            return totemExecState_ExecMoreThan(state);
            
        case totemOperation_MoreThanEquals:
            return totemExecState_ExecMoreThanEquals(state);
            
        case totemOperation_ConditionalGoto:
            return totemExecState_ExecConditionalGoto(state);
            
        case totemOperation_Goto:
            return totemExecState_ExecGoto(state);
            
        case totemOperation_NativeFunction:
            return totemExecState_ExecNativeFunction(state);
            
        case totemOperation_ScriptFunction:
            return totemExecState_ExecScriptFunction(state);

		case totemOperation_Return:
            return totemExecState_ExecReturn(state);

        default:
            return totemExecStatus_UnrecognisedOperation;
    }
}

totemExecStatus totemExecState_ExecMove(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    *destination = *source;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecAdd(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number + source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecSubtract(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number - source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMultiply(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number * source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecDivide(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number / source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecPower(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = pow(source1->Value.Number, source2->Value.Number);
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, source2->DataType);
    
    destination->Value.Number = source1->Value.Data == source2->Value.Data;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecNotEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, source2->DataType);
    
    destination->Value.Number = source1->Value.Data != source2->Value.Data;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number < source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number <= source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number > source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_ENFORCE_TYPE(source1, totemDataType_Number);
    TOTEM_ENFORCE_TYPE(source2, totemDataType_Number);
    
    destination->Value.Number = source1->Value.Number >= source2->Value.Number;
    destination->DataType = totemDataType_Number;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecGoto(totemExecState *state)
{
    totemOperandX offset = state->CurrentInstruction->Axx.OperandAxx;
    state->CurrentInstruction += offset;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecConditionalGoto(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandX offset = instruction.Abx.OperandBx;
    
    if(source->Value.Data == 0)
    {
        state->CurrentInstruction += offset;
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecScriptFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandX functionIndex = instruction.Abx.OperandBx;
    
    return totemExecState_Exec(state, state->CallStack->Actor, functionIndex, destination);
}

totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandX functionHandle = instruction.Abx.OperandBx;
    
    totemNativeFunction function = totemMemoryBuffer_Get(&state->Runtime->NativeFunctions, functionHandle);
    if(function == NULL)
    {
        return totemExecStatus_NativeFunctionNotFound;
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Native, functionHandle, state->CallStack->Actor);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    status = function(state);
    totemExecState_PopFunctionCall(state);
    return status;
}

totemExecStatus totemExecState_ExecReturn(totemExecState *state)
{
    totemFunctionCall *call = state->CallStack;
    
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandX option = instruction.Abx.OperandBx;

    if(option == totemReturnOption_Register)
    {
        totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
        call->ReturnRegister->Value = source->Value;
    }
    
    return totemExecStatus_Return;
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
        default: return "UNKNOWN";
    }
}