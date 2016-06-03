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

#if 1
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, state) \
{ \
    totemInstruction_Print(stdout, (ins)); \
    totemInstructionType type = totemOperationType_GetInstructionType(TOTEM_INSTRUCTION_GET_OP((ins))); \
    switch(type) \
    { \
        case totemInstructionType_Abc: \
        { \
            totemRegister *a = TOTEM_GET_OPERANDA_REGISTER((state), (ins)); \
            totemRegister *b = TOTEM_GET_OPERANDB_REGISTER((state), (ins)); \
            totemRegister *c = TOTEM_GET_OPERANDC_REGISTER((state), (ins)); \
            fprintf(stdout, "a:");\
            totemRegister_Print(stdout, a); \
            fprintf(stdout, "b:");\
            totemRegister_Print(stdout, b); \
            fprintf(stdout, "c:");\
            totemRegister_Print(stdout, c); \
            break; \
        } \
        case totemInstructionType_Abx: \
        { \
            totemRegister *a = TOTEM_GET_OPERANDA_REGISTER((state), (ins)); \
            totemRegister_Print(stdout, a); \
            break; \
        } \
        case totemInstructionType_Axx: \
            break; \
    }\
    fprintf(stdout, "\n");\
}
#else
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, state) //totemInstruction_Print(stdout, (ins))
#endif

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

totemFunctionCall *totemExecState_SecureFunctionCall(totemExecState *state)
{
    totemFunctionCall *call = NULL;
    
    if (state->FunctionCallFreeList)
    {
        call = state->FunctionCallFreeList;
        state->FunctionCallFreeList = call->Prev;
    }
    else
    {
        call = totem_CacheMalloc(sizeof(totemFunctionCall));
        if (!call)
        {
            return NULL;
        }
    }
    
    memset(call, 0, sizeof(totemFunctionCall));
    return call;
}

void totemExecState_FreeFunctionCall(totemExecState *state, totemFunctionCall *call)
{
    call->Prev = NULL;
    
    if (state->FunctionCallFreeList)
    {
        call->Prev = state->FunctionCallFreeList;
    }
    
    state->FunctionCallFreeList = call;
}

totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut)
{
    totemRegister newStr;
    memset(&newStr, 0, sizeof(totemRegister));
    
    if(totemRuntime_InternString(state->Runtime, str, &newStr.Value, &newStr.DataType) != totemLinkStatus_Success)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    TOTEM_REGISTER_ASSIGN_GENERIC(state, strOut, &newStr);
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

void totemActor_Init(totemActor *actor)
{
    totemMemoryBuffer_Init(&actor->GlobalRegisters, sizeof(totemRegister));
    actor->Script = NULL;
}

void totemActor_Cleanup(totemActor *actor)
{
    totemMemoryBuffer_Cleanup(&actor->GlobalRegisters);
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

totemLinkStatus totemScript_LinkActor(totemScript *script, totemActor *actor)
{
    actor->Script = script;
    
    if (!totemMemoryBuffer_TakeFrom(&actor->GlobalRegisters, &script->GlobalRegisters))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    return totemLinkStatus_Success;
}

void totemRuntime_Init(totemRuntime *runtime)
{
    totemMemoryBuffer_Init(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Init(&runtime->NativeFunctionNames, sizeof(totemRegister));
    totemHashMap_Init(&runtime->NativeFunctionsLookup);
    totemHashMap_Init(&runtime->InternedStrings);
}

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions);
    totemMemoryBuffer_Reset(&runtime->NativeFunctionNames);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
    totemHashMap_Reset(&runtime->InternedStrings);
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
    
    totemHashMapEntry *result = totemHashMap_Find(&runtime->InternedStrings, str->Value, str->Length);
    if(result)
    {
        *typeOut = totemPrivateDataType_InternedString;
        valOut->InternedString = (totemInternedStringHeader*)result->Value;
        return totemLinkStatus_Success;
    }
    
    size_t toAllocate = str->Length + sizeof(totemInternedStringHeader) - 1;
    
    totemInternedStringHeader *newStr = totem_CacheMalloc(toAllocate);
    if(!newStr)
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    newStr->Length = str->Length;
    
    newStr->Hash = totem_Hash(str->Value, newStr->Length);
    memcpy(newStr->Data, str->Value, newStr->Length);
    newStr->Data[newStr->Length] = 0;
    
    if(!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, newStr->Data, newStr->Length, (uintptr_t)newStr, newStr->Hash))
    {
        totemInternedStringHeader_Destroy(newStr);
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    *typeOut = totemPrivateDataType_InternedString;
    valOut->InternedString = newStr;
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *script)
{
    totemScript_Reset(script);
    
    // check script function names against existing native functions
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->Functions); i++)
    {
        totemOperandXUnsigned dummyAddr;
        totemScriptFunctionPrototype *func = totemMemoryBuffer_Get(&build->Functions, i);
        
        if(totemRuntime_GetNativeFunctionAddress(runtime, &func->Name, &dummyAddr))
        {
            build->ErrorContext = func;
            return totemLinkStatus_Break(totemLinkStatus_FunctionAlreadyDeclared);
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
                totemString *str = (totemString*)prototype->Value.InternedString;
                
                if(totemRuntime_InternString(runtime, str, &reg->Value, &reg->DataType) != totemLinkStatus_Success)
                {
                    return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
                }
                break;
            }
                
                // fix function pointers
            case totemPublicDataType_Function:
            {
                reg->DataType = totemPrivateDataType_Function;
                
                if(prototype->Value.FunctionPointer.Type == totemFunctionType_Native)
                {
                    totemString *funcName = totemMemoryBuffer_Get(&build->NativeFunctionNames, prototype->Value.FunctionPointer.Address);
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
                    
                    reg->Value.FunctionPointer.Address = (totemOperandXUnsigned)funcAddr;
                    reg->Value.FunctionPointer.Type = totemFunctionType_Native;
                }
                else
                {
                    reg->Value.FunctionPointer.Address = prototype->Value.FunctionPointer.Address;
                    reg->Value.FunctionPointer.Type = totemFunctionType_Script;
                }
                break;
            }
                
            case totemPublicDataType_Int:
                reg->DataType = totemPrivateDataType_Int;
                reg->Value.Data = prototype->Value.Data;
                break;
                
            case totemPublicDataType_Float:
                reg->DataType = totemPrivateDataType_Float;
                reg->Value.Data = prototype->Value.Data;
                break;
                
            case totemPublicDataType_Type:
                reg->DataType = totemPrivateDataType_Type;
                reg->Value.Data = prototype->Value.Data;
                break;
                
            default:
                return totemLinkStatus_Break(totemLinkStatus_UnexpectedValueType);
        }
    }
    
    // instructions
    if(!totemMemoryBuffer_TakeFrom(&script->Instructions, &build->Instructions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    // functions
    size_t numFunctions = totemMemoryBuffer_GetNumObjects(&build->Functions);
    if(!totemMemoryBuffer_Secure(&script->Functions, numFunctions) || !totemMemoryBuffer_Secure(&script->FunctionNames, numFunctions))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    totemScriptFunction *funcs = totemMemoryBuffer_Get(&script->Functions, 0);
    totemRegister *strs = totemMemoryBuffer_Get(&script->FunctionNames, 0);
    totemScriptFunctionPrototype *funcProts = totemMemoryBuffer_Get(&build->Functions, 0);
    for(size_t i = 0; i < numFunctions; i++)
    {
        funcs[i].InstructionsStart = funcProts[i].InstructionsStart;
        funcs[i].RegistersNeeded = funcProts[i].RegistersNeeded;
        
        if (totemRuntime_InternString(runtime, &funcProts[i].Name, &strs[i].Value, &strs[i].DataType) != totemLinkStatus_Success)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
    }
    
    // function name lookup
    if(!totemHashMap_TakeFrom(&script->FunctionNameLookup, &build->FunctionLookup))
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    return totemLinkStatus_Success;
}

totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, totemOperandXUnsigned *addressOut)
{
    totemHashMapEntry *result = totemHashMap_Find(&runtime->NativeFunctionsLookup, name->Value, name->Length);
    totemNativeFunction *addr = NULL;;
    if(result != NULL)
    {
        addr = totemMemoryBuffer_Get(&runtime->NativeFunctions, result->Value);
        *addressOut = (totemOperandXUnsigned)result->Value;
    }
    else
    {
        if(totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions) >= TOTEM_MAX_NATIVEFUNCTIONS - 1)
        {
            return totemLinkStatus_Break(totemLinkStatus_TooManyNativeFunctions);
        }
        
        *addressOut = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&runtime->NativeFunctions);
        addr = totemMemoryBuffer_Secure(&runtime->NativeFunctions, 1);
        if(!addr)
        {
            return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
        }
        
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
    
    *addr = func;
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

totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters)
{
    state->Runtime = runtime;
    
    if (state->Registers[totemOperandType_LocalRegister])
    {
        totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
    }
    
    if (numRegisters > 0)
    {
        totemRegister *regs = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
        
        if (!regs)
        {
            return totemLinkStatus_OutOfMemory;
        }
        
        memset(regs, 0, sizeof(totemRegister) * numRegisters);
        state->Registers[totemOperandType_LocalRegister] = regs;
    }
    else
    {
        state->Registers[totemOperandType_LocalRegister] = NULL;
    }
    
    state->MaxLocalRegisters = numRegisters;
    state->UsedLocalRegisters = 0;
    return totemLinkStatus_Success;
}

void totemExecState_Init(totemExecState *state)
{
    memset(state, 0, sizeof(*state));
}

void totemExecState_Cleanup(totemExecState *state)
{
    totem_CacheFree(state->Registers[totemOperandType_LocalRegister], sizeof(totemRegister) * state->MaxLocalRegisters);
    
    state->Registers[totemOperandType_LocalRegister] = NULL;
    
    while (state->FunctionCallFreeList)
    {
        totemFunctionCall *call = state->FunctionCallFreeList;
        state->FunctionCallFreeList = call->Prev;
        totem_CacheFree(call, sizeof(totemFunctionCall));
    }
}

totemExecStatus totemExecState_PushFunctionArgs(totemExecState *state, totemFunctionCall *call)
{
    if (state->CurrentInstruction)
    {
        totemOperationType type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
        
        if (type == totemOperationType_FunctionArg)
        {
            totemOperandXUnsigned numArgs = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*state->CurrentInstruction);
            
            if (numArgs > call->NumRegisters)
            {
                return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
            }
            
            do
            {
                TOTEM_INSTRUCTION_PRINT_DEBUG(*state->CurrentInstruction, state);
                totemRegister *argument = TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));
                
                TOTEM_REGISTER_ASSIGN_GENERIC(state, &call->FrameStart[call->NumArguments], argument);
                call->NumArguments++;
                
                state->CurrentInstruction++;
                type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
            } while (type == totemOperationType_FunctionArg && call->NumArguments < call->NumRegisters);
        }
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateSubroutine(totemExecState *state, size_t numRegisters, totemActor *actor, totemRegister *returnReg, totemFunctionType funcType, totemOperandXUnsigned functionAddress, totemFunctionCall **callOut)
{
    totemFunctionCall *call = totemExecState_SecureFunctionCall(state);
    if (call == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    if ((state->MaxLocalRegisters - state->UsedLocalRegisters) < numRegisters)
    {
        TOTEM_SETBITS(call->Flags, totemFunctionCallFlag_FreeStack);
        
        call->FrameStart = totem_CacheMalloc(sizeof(totemRegister) * numRegisters);
        if (!call->FrameStart)
        {
            return totemExecStatus_Break(totemExecStatus_OutOfMemory);
        }
    }
    else
    {
        call->FrameStart = state->Registers[totemOperandType_LocalRegister] + (state->CallStack ? state->CallStack->NumRegisters : 0);
        state->UsedLocalRegisters += numRegisters;
    }
    
    // reset registers to be used
    memset(call->FrameStart, 0, numRegisters * sizeof(totemRegister));
    
    call->CurrentActor = actor;
    call->ReturnRegister = returnReg;
    call->Type = funcType;
    call->FunctionHandle = functionAddress;
    call->ResumeAt = NULL;
    call->Prev = NULL;
    call->NumArguments = 0;
    call->NumRegisters = numRegisters;
    
    *callOut = call;
    return totemExecStatus_Continue;
}

void totemExecState_PushRoutine(totemExecState *state, totemFunctionCall *call, totemInstruction *startAt)
{
    call->PreviousFrameStart = state->Registers[totemOperandType_LocalRegister];
    call->ResumeAt = state->CurrentInstruction;
    
    state->Registers[totemOperandType_LocalRegister] = call->FrameStart;
    state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)call->CurrentActor->GlobalRegisters.Data;
    state->CurrentInstruction = startAt;
    
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
        totemInstruction *currentPos = state->CurrentInstruction;
        totemFunctionCall *call = state->CallStack;
        totemFunctionCall *prev = call->Prev;
        
        state->CurrentInstruction = call->ResumeAt;
        state->Registers[totemOperandType_LocalRegister] = call->PreviousFrameStart;
        state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)call->CurrentActor->GlobalRegisters.Data;
        
        if (TOTEM_HASBITS(call->Flags, totemFunctionCallFlag_IsCoroutine))
        {
            call->ResumeAt = currentPos;
            
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
            }
            
            totemExecState_FreeFunctionCall(state, call);
        }
        
        state->CallStack = prev;
    }
}

totemExecStatus totemExecState_ExecuteInstructions(totemExecState *state)
{
    totemExecStatus status = totemExecStatus_Continue;
    
    do
    {
        status = totemExecState_ExecInstruction(state);
    } while (status == totemExecStatus_Continue);
    
    return status;
}

totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionAddress, totemRegister *returnRegister)
{
    totemScriptFunction *function = totemMemoryBuffer_Get(&actor->Script->Functions, functionAddress);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    totemInstruction *startAt = totemMemoryBuffer_Get(&actor->Script->Instructions, function->InstructionsStart);
    if (startAt == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    totemFunctionCall *call = NULL;
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateSubroutine(state, function->RegistersNeeded, actor, returnRegister, totemFunctionType_Script, functionAddress, &call));
    TOTEM_EXEC_CHECKRETURN(totemExecState_PushFunctionArgs(state, call));
    totemExecState_PushRoutine(state, call, startAt);
    
    totemExecStatus status = totemExecState_ExecuteInstructions(state);
    totemExecState_PopRoutine(state);
    
    if(status == totemExecStatus_Return)
    {
        if(state->CallStack)
        {
            return totemExecStatus_Continue;
        }
    }
    
    return status;
}

totemExecStatus totemExecState_ExecNative(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionHandle, totemRegister *returnRegister)
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
    
    totemFunctionCall *call = NULL;
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateSubroutine(state, numArgs, actor, returnRegister, totemFunctionType_Native, functionHandle, &call));
    TOTEM_EXEC_CHECKRETURN(totemExecState_PushFunctionArgs(state, call));
    totemExecState_PushRoutine(state, call, NULL);
    
    totemExecStatus status = (*function)(state);
    totemExecState_PopRoutine(state);
    return status;
}

totemExecStatus totemExecState_ExecInstruction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    
    TOTEM_INSTRUCTION_PRINT_DEBUG(instruction, state);
    
    totemOperationType op = TOTEM_INSTRUCTION_GET_OP(instruction);
    totemExecStatus status = totemExecStatus_Continue;
    
    switch(op)
    {
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
            
        case totemOperationType_NewObject:
            status = totemExecState_ExecNewObject(state);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
    }
    
    TOTEM_INSTRUCTION_PRINT_DEBUG(instruction, state);
    
    return status;
}

totemExecStatus totemExecState_ExecNewObject(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    
    totemGCObject *gc = NULL;
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateObject(state, &gc));
    TOTEM_REGISTER_ASSIGN_OBJECT(state, dst, gc);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecFunctionPointer(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    switch (src->DataType)
    {
        case totemPrivateDataType_Coroutine:
        {
            totemFunctionCall *call = src->Value.GCObject->Coroutine;
            call->ReturnRegister = dst;
            state->CurrentInstruction++;
            
            totemInstruction *startFrom = call->ResumeAt;
            
            if (startFrom == NULL)
            {
                TOTEM_EXEC_CHECKRETURN(totemExecState_PushFunctionArgs(state, call));
                
                totemScriptFunction *function = totemMemoryBuffer_Get(&call->CurrentActor->Script->Functions, call->FunctionHandle);
                if (function == NULL)
                {
                    return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
                }
                
                startFrom = totemMemoryBuffer_Get(&call->CurrentActor->Script->Instructions, function->InstructionsStart);
                if (startFrom == NULL)
                {
                    return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
                }
            }
            else
            {
                // skip function args when resuming
                while (TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction) == totemOperationType_FunctionArg)
                {
                    TOTEM_INSTRUCTION_PRINT_DEBUG(*state->CurrentInstruction, state);
                    state->CurrentInstruction++;
                }
            }
            
            totemExecState_PushRoutine(state, call, startFrom);
            totemExecStatus status = totemExecState_ExecuteInstructions(state);
            totemExecState_PopRoutine(state);
            
            if (status != totemExecStatus_Return)
            {
                return status;
            }
            
            return totemExecStatus_Continue;
        }
            
        case totemPrivateDataType_Function:
            switch (src->Value.FunctionPointer.Type)
        {
            case totemFunctionType_Native:
                state->CurrentInstruction++;
                return totemExecState_ExecNative(state, state->CallStack->CurrentActor, src->Value.FunctionPointer.Address, dst);
                
            case totemFunctionType_Script:
                state->CurrentInstruction++;
                return totemExecState_Exec(state, state->CallStack->CurrentActor, src->Value.FunctionPointer.Address, dst);
                
            default:
                return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_ExecAs(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *typeReg = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemPublicDataType srcType = totemPrivateDataType_ToPublic(src->DataType);
    totemPublicDataType toType = typeReg->Value.DataType;
    
    if(typeReg->DataType != totemPrivateDataType_Type)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    // type
    if (toType == totemPublicDataType_Type)
    {
        TOTEM_REGISTER_ASSIGN_TYPE(state, dst, srcType);
    }
    
    // convert value to 1-size array
    else if (toType == totemPublicDataType_Array)
    {
        switch (srcType)
        {
                // clone array
            case totemPublicDataType_Array:
            {
                totemGCObject *gc = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArrayFromExisting(state, src->Value.GCObject->Array->Registers, src->Value.GCObject->Array->NumRegisters, &gc));
                TOTEM_REGISTER_ASSIGN_ARRAY(state, dst, gc);
                break;
            }
                
                // explode string into array
            case totemPublicDataType_String:
            {
                const char *str = totemRegister_GetStringValue(src);
                totemGCObject *gc = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, totemRegister_GetStringLength(src), &gc));
                
                totemRegister *regs = gc->Array->Registers;
                for (size_t i = 0; i < gc->Array->NumRegisters; i++)
                {
                    totemString newStr;
                    newStr.Length = 1;
                    newStr.Value = &str[i];
                    
                    if (totemRuntime_InternString(state->Runtime, &newStr, &regs[i].Value, &regs[i].DataType) != totemLinkStatus_Success)
                    {
                        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
                    }
                }
                
                TOTEM_REGISTER_ASSIGN_ARRAY(state, dst, gc);
                break;
            }
                
            default:
            {
                totemGCObject *gc = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArrayFromExisting(state, src, 1, &gc));
                TOTEM_REGISTER_ASSIGN_ARRAY(state, dst, gc);
                break;
            }
        }
    }
    else
    {
        switch (TOTEM_TYPEPAIR(srcType, toType))
        {
                /*
                 * int
                 */
                
                // int as int
            case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Int):
                TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, src);
                break;
                
                // int as float
            case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Float):
                TOTEM_REGISTER_ASSIGN_FLOAT(state, dst, (totemFloat)src->Value.Int);
                break;
                
                // int as string
            case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_String):
                TOTEM_EXEC_CHECKRETURN(totemExecState_IntToString(state, src->Value.Int, dst));
                break;
                
                /*
                 * floats
                 */
                
                // float as float
            case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Float):
                TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, src);
                break;
                
                // float as int
            case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Int):
                TOTEM_REGISTER_ASSIGN_INT(state, dst, (totemInt)src->Value.Float);
                break;
                
                // float as string
            case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_String):
                TOTEM_EXEC_CHECKRETURN(totemExecState_FloatToString(state, src->Value.Float, dst));
                break;
                
                /*
                 * arrays
                 */
                
                // array as int (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Int):
                TOTEM_REGISTER_ASSIGN_INT(state, dst, (totemInt)src->Value.GCObject->Array->NumRegisters);
                break;
                
                // array as float (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Float):
                TOTEM_REGISTER_ASSIGN_FLOAT(state, dst, (totemFloat)src->Value.GCObject->Array->NumRegisters);
                break;
                
                // array as string (implode)
            case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_String):
                TOTEM_EXEC_CHECKRETURN(totemExecState_ArrayToString(state, src->Value.GCObject->Array, dst));
                break;
                
                /*
                 * types
                 */
                
                // type as type
            case TOTEM_TYPEPAIR(totemPublicDataType_Type, totemPublicDataType_Type):
                TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, src);
                break;
                
                // type as string (type name)
            case TOTEM_TYPEPAIR(totemPublicDataType_Type, totemPublicDataType_String):
                TOTEM_EXEC_CHECKRETURN(totemExecState_TypeToString(state, src->Value.DataType, dst));
                break;
                
                /*
                 * strings
                 */
                
                // string as int (attempt atoi)
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Int):
            {
                const char *str = totemRegister_GetStringValue(src);
                totemInt val = strtoll(str, NULL, 10);
                TOTEM_REGISTER_ASSIGN_INT(state, dst, val);
                break;
            }
                
                // string as float (attempt atof)
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Float):
            {
                const char *str = totemRegister_GetStringValue(src);
                totemFloat val = strtod(str, NULL);
                TOTEM_REGISTER_ASSIGN_FLOAT(state, dst, val);
                break;
            }
                
                // string as string
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_String):
                TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, src);
                break;
                
                // lookup function pointer by name
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Function):
                TOTEM_EXEC_CHECKRETURN(totemExecState_StringToFunctionPointer(state, src, dst));
                break;
                
                /*
                 * functions
                 */
                
                // function as string
            case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_String):
                TOTEM_EXEC_CHECKRETURN(totemExecState_FunctionPointerToString(state, &src->Value.FunctionPointer, dst));
                break;
                
                // function as function
            case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_Function):
                TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, src);
                break;
                
                // create coroutine
            case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_Coroutine):
            {
                if (src->Value.FunctionPointer.Type != totemFunctionType_Script)
                {
                    return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
                }
                
                totemGCObject *obj = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateCoroutine(state, src->Value.FunctionPointer.Address, &obj));
                TOTEM_REGISTER_ASSIGN_COROUTINE(state, dst, obj);
                break;
            }
                
                /*
                 * coroutine
                 */
                
                // coroutine as string
            case TOTEM_TYPEPAIR(totemPublicDataType_Coroutine, totemPublicDataType_String):
            {
                totemFunctionPointer ptr;
                ptr.Address = src->Value.GCObject->Coroutine->FunctionHandle;
                ptr.Type = totemFunctionType_Script;
                TOTEM_EXEC_CHECKRETURN(totemExecState_FunctionPointerToString(state, &ptr, dst));
                break;
            }
                
                // clone coroutine
            case TOTEM_TYPEPAIR(totemPublicDataType_Coroutine, totemPublicDataType_Coroutine):
            {
                totemGCObject *obj = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateCoroutine(state, src->Value.GCObject->Coroutine->FunctionHandle, &obj));
                TOTEM_REGISTER_ASSIGN_COROUTINE(state, dst, obj);
                break;
            }
                
                // extract function pointer from coroutine
            case TOTEM_TYPEPAIR(totemPublicDataType_Coroutine, totemPublicDataType_Function):
                TOTEM_REGISTER_ASSIGN_FUNC(state, dst, src->Value.GCObject->Coroutine->FunctionHandle, totemFunctionType_Script);
                break;
                
                /*
                 * objects
                 */
                
                // object as int (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Object, totemPublicDataType_Int):
                TOTEM_REGISTER_ASSIGN_INT(state, dst, src->Value.GCObject->Object->Lookup.NumKeys);
                break;
                
                // object as float (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Object, totemPublicDataType_Float):
                TOTEM_REGISTER_ASSIGN_FLOAT(state, dst, src->Value.GCObject->Object->Lookup.NumKeys);
                break;
                
            default:
                return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
        }
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
    
    totemPublicDataType type = typeSrc->Value.DataType;
    
    if(typeSrc->DataType != totemPrivateDataType_Type)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    TOTEM_REGISTER_ASSIGN_INT(state, dst, totemPrivateDataType_ToPublic(src->DataType) == type);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMove(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_GENERIC(state, destination, source);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoveToGlobal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *src = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    TOTEM_REGISTER_ASSIGN_GENERIC(state, globalReg, src);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoveToLocal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, globalReg);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecAdd(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemPublicDataType src1Type = totemPrivateDataType_ToPublic(source1->DataType);
    totemPublicDataType src2Type = totemPrivateDataType_ToPublic(source2->DataType);
    
    switch(TOTEM_TYPEPAIR(src1Type, src2Type))
    {
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int + source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, ((totemFloat)source1->Value.Int) + source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float + source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float + ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Array):
        {
            totemGCObject *gc = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, source1->Value.GCObject->Array->NumRegisters + source2->Value.GCObject->Array->NumRegisters, &gc));
            
            totemRegister *newRegs = gc->Array->Registers;
            totemRegister *source1Regs = source1->Value.GCObject->Array->Registers;
            
            for(size_t i = 0; i < source1->Value.GCObject->Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(state, &newRegs[i], &source1Regs[i]);
            }
            
            totemRegister *source2Regs = source2->Value.GCObject->Array->Registers;
            for(size_t i = source1->Value.GCObject->Array->NumRegisters; i < gc->Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(state, &newRegs[i], &source2Regs[i - source1->Value.GCObject->Array->NumRegisters]);
            }
            
            TOTEM_REGISTER_ASSIGN_ARRAY(state, destination, gc);
            break;
        }
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Function):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_String):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Float):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Type):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Int):
        {
            totemGCObject *gc = NULL;
            TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, source1->Value.GCObject->Array->NumRegisters + 1, &gc));
            
            totemRegister *newRegs = gc->Array->Registers;
            totemRegister *source1Regs = source1->Value.GCObject->Array->Registers;
            
            for(size_t i = 0; i < source1->Value.GCObject->Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(state, &newRegs[i], &source1Regs[i]);
            }
            
            TOTEM_REGISTER_ASSIGN_GENERIC(state, &newRegs[gc->Array->NumRegisters - 1], source2);
            TOTEM_REGISTER_ASSIGN_ARRAY(state, destination, gc);
            break;
        }
            
        case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_String):
            TOTEM_EXEC_CHECKRETURN(totemExecState_ConcatStrings(state, source1, source2, destination));
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int - source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float - source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float - ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, ((totemFloat)source1->Value.Int) - source2->Value.Float);
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int * source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float * source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float * ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, ((totemFloat)source1->Value.Int) * source2->Value.Float);
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
    
    if(source2->Value.Int == 0)
    {
        return totemExecStatus_Break(totemExecStatus_DivideByZero);
    }
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int / source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float / source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, source1->Value.Float / ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, ((totemFloat)source1->Value.Int) / source2->Value.Float);
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
    
    TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Data == source2->Value.Data && source1->DataType == source2->DataType);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecNotEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Data != source2->Value.Data || source1->DataType != source2->DataType);
    
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int < source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float < source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float < ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, ((totemFloat)source1->Value.Float) < source2->Value.Float);
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int <= source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float <= source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float <= ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, ((totemFloat)source1->Value.Float) <= source2->Value.Float);
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int > source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float > source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float > ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, ((totemFloat)source1->Value.Float) > source2->Value.Float);
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Int >= source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float >= source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, source1->Value.Float >= ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(state, destination, ((totemFloat)source1->Value.Float) >= source2->Value.Float);
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

totemExecStatus totemExecState_ExecReturn(totemExecState *state)
{
    totemFunctionCall *call = state->CallStack;
    
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandXUnsigned flags = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    if (TOTEM_HASBITS(flags, totemReturnFlag_Register))
    {
        totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
        TOTEM_REGISTER_ASSIGN_GENERIC(state, call->ReturnRegister, source);
    }
    
    return totemExecStatus_Return;
}

totemExecStatus totemExecState_ExecNewArray(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemPrivateDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    uint32_t numRegisters = (uint32_t)indexSrc->Value.Int;
    
    if(numRegisters == 0 || indexSrc->Value.Int == UINT32_MAX)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemGCObject *arr = NULL;
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, numRegisters, &arr));
    TOTEM_REGISTER_ASSIGN_ARRAY(state, dst, arr);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecArrayGet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemPublicDataType type = totemPrivateDataType_ToPublic(src->DataType);
    switch (type)
    {
        case totemPublicDataType_String:
        {
            if (indexSrc->DataType != totemPrivateDataType_Int)
            {
                return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
            }
            
            const char *str = totemRegister_GetStringValue(src);
            totemStringLength len = totemRegister_GetStringLength(src);
            totemInt index = indexSrc->Value.Int;
            
            if (index >= len || index < 0)
            {
                return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
            }
            
            totemString toIntern;
            toIntern.Length = 1;
            toIntern.Value = str + index;
            
            TOTEM_EXEC_CHECKRETURN(totemExecState_InternString(state, &toIntern, dst));
            break;
        }
            
        case totemPublicDataType_Array:
        {
            if (indexSrc->DataType != totemPrivateDataType_Int)
            {
                return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
            }
            
            totemInt index = indexSrc->Value.Int;
            
            if (index < 0 || index >= UINT32_MAX || index >= src->Value.GCObject->Array->NumRegisters)
            {
                return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
            }
            
            totemRegister *regs = src->Value.GCObject->Array->Registers;
            TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, &regs[index]);
            break;
        }
            
        case totemPublicDataType_Object:
        {
            totemObject *obj = src->Value.GCObject->Object;
            
            switch (totemPrivateDataType_ToPublic(indexSrc->DataType))
            {
                case totemPublicDataType_String:
                {
                    const char *str = totemRegister_GetStringValue(indexSrc);
                    totemStringLength len = totemRegister_GetStringLength(indexSrc);
                    
                    totemHashMapEntry *result = totemHashMap_Find(&obj->Lookup, str, len);
                    if (!result)
                    {
                        return totemExecStatus_Break(totemExecStatus_InvalidKey);
                    }
                    
                    totemRegister *reg = totemMemoryBuffer_Get(&obj->Registers, result->Value);
                    if (!reg)
                    {
                        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
                    }
                    
                    TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, reg);
                    break;
                }
                    
                default:
                    return totemExecStatus_Break(totemExecStatus_InvalidKey);
            }
            
            break;
        }
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecArraySet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch (dst->DataType)
    {
        case totemPrivateDataType_Array:
        {
            if (indexSrc->DataType != totemPrivateDataType_Int)
            {
                return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
            }
            
            totemInt index = indexSrc->Value.Int;
            
            if (index < 0 || index >= UINT32_MAX || index >= dst->Value.GCObject->Array->NumRegisters)
            {
                return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
            }
            
            totemRegister *regs = dst->Value.GCObject->Array->Registers;
            TOTEM_REGISTER_ASSIGN_GENERIC(state, &regs[index], src);
            break;
        }
            
        case totemPrivateDataType_Object:
        {
            if (totemPrivateDataType_ToPublic(indexSrc->DataType) != totemPublicDataType_String)
            {
                return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
            }
            
            const char *str = totemRegister_GetStringValue(indexSrc);
            totemStringLength len = totemRegister_GetStringLength(indexSrc);
            totemHash hash = totemRegister_GetStringHash(indexSrc);
            
            totemObject *obj = dst->Value.GCObject->Object;
            totemRegister *newReg = NULL;
            totemHashValue lookupVal = 0;
            
            if (obj->Lookup.FreeList)
            {
                lookupVal = obj->Lookup.FreeList->Value;
                newReg = totemMemoryBuffer_Get(&obj->Registers, lookupVal);
            }
            else
            {
                lookupVal = totemMemoryBuffer_GetNumObjects(&obj->Registers);
                newReg = totemMemoryBuffer_Secure(&obj->Registers, 1);
                if (!newReg)
                {
                    return totemExecStatus_Break(totemExecStatus_OutOfMemory);
                }
                
                if (!totemHashMap_InsertPrecomputed(&obj->Lookup, str, len, lookupVal, hash))
                {
                    return totemExecStatus_Break(totemExecStatus_OutOfMemory);
                }
            }
            
            TOTEM_REGISTER_ASSIGN_GENERIC(state, newReg, src);
            break;
        }
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
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
            TOTEM_STRINGIFY_CASE(totemExecStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InternalBufferOverrun);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InvalidKey);
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