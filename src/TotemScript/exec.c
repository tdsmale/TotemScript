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
    
    totemRegister *globalRegisters = totem_Malloc(script->GlobalRegisters.Length);
    if(globalRegisters == NULL)
    {
        return totemExecStatus_OutOfMemory;
    }
    
    memcpy(globalData, script->GlobalData.Data, script->GlobalData.Length);
    memcpy(&actor->GlobalData, &script->GlobalData, sizeof(totemMemoryBuffer));
    actor->GlobalData.Data = globalData;
    
    memcpy(globalRegisters, script->GlobalRegisters.Data, script->GlobalRegisters.Length);
    memcpy(&actor->GlobalRegisters, &script->GlobalRegisters, sizeof(totemMemoryBuffer));
    actor->GlobalRegisters.Data = (char*)globalRegisters;
    
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
        call = totem_CacheMalloc(sizeof(totemFunctionCall));
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
    state->UsedLocalRegisters = 0;
    return totemBool_True;
}

totemExecStatus totemExecState_PushFunctionCall(totemExecState *state, totemFunctionType type, size_t functionAddress, totemActor *actor, totemRegister *returnReg)
{
    totemFunctionCall *call = totemRuntime_SecureFunctionCall(state->Runtime);
    if(call == NULL)
    {
        return totemExecStatus_OutOfMemory;
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
        while(TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction) == totemOperationType_FunctionArg)
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
            state->CurrentInstruction = state->CallStack->ResumeAt;
        }
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

    // TODO: Dynamically allocate register frames per function call, instead of just having one big stack per exec state...
    if((state->MaxLocalRegisters - state->UsedLocalRegisters) < function->RegistersNeeded)
    {
        return totemExecStatus_RegisterOverflow;
    }
    
    state->UsedLocalRegisters += function->RegistersNeeded;
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Script, functionAddress, actor, returnRegister);
    if(status != totemExecStatus_Continue)
    {
        return status;
    }
    
    state->CurrentInstruction = totemMemoryBuffer_Get(&script->Instructions, function->InstructionsStart);
    if(state->CurrentInstruction == NULL)
    {
        return totemExecStatus_ScriptFunctionNotFound;
    }
    
    do
    {
        status = totemExecState_ExecInstruction(state);
    }
    while(status == totemExecStatus_Continue);

    state->CallStack->ResumeAt = state->CurrentInstruction;
    
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

        default:
            return totemExecStatus_UnrecognisedOperation;
    }
}

totemExecStatus totemExecState_ExecMove(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    memcpy(destination, source, sizeof(totemRegister));
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecAdd(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecSubtract(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMultiply(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecDivide(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecPower(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
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
    
    destination->Value.Int = source1->Value.Data == source2->Value.Data;
    destination->DataType = totemDataType_Int;
    
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
    
    destination->Value.Int = source1->Value.Data != source2->Value.Data;
    destination->DataType = totemDataType_Int;
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecLessThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThan(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
    }
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoreThanEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination =TOTEM_GET_OPERANDA_REGISTER(state, instruction);
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
            return totemExecStatus_UnexpectedDataType;
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
        return totemExecStatus_NativeFunctionNotFound;
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
    }
    
    return "UNKNOWN";
}