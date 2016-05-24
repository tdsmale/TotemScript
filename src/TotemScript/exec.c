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

#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }

#define TOTEM_GET_OPERANDA_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))]
#define TOTEM_GET_OPERANDB_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))]
#define TOTEM_GET_OPERANDC_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))]

#define TOTEM_REGISTER_ASSIGN(dst, perform) \
if((dst)->DataType == totemPrivateDataType_Array) \
{ \
totemRuntimeArrayHeader_DefRefCount((dst)->Value.Array); \
} \
perform; \
if((dst)->DataType == totemPrivateDataType_Array) \
{ \
(dst)->Value.Array->RefCount++; \
}

#define TOTEM_REGISTER_ASSIGN_GENERIC(dst, src) TOTEM_REGISTER_ASSIGN((dst), memcpy((dst), (src), sizeof(totemRegister)))
#define TOTEM_REGISTER_ASSIGN_FLOAT(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_Float; (dst)->Value.Float = (val))
#define TOTEM_REGISTER_ASSIGN_INT(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_Int; (dst)->Value.Int = (val))
#define TOTEM_REGISTER_ASSIGN_TYPE(dst,val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_Type; (dst)->Value.DataType = (val))
#define TOTEM_REGISTER_ASSIGN_NULL(dst) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_Int; (dst)->Value.Int = 0)
#define TOTEM_REGISTER_ASSIGN_ARRAY(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_Array; (dst)->Value.Array = (val))
#define TOTEM_REGISTER_ASSIGN_STRING(dst, val) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_String; (dst)->Value.InternedString = (val))
#define TOTEM_REGISTER_ASSIGN_FUNC(dst, val, type) TOTEM_REGISTER_ASSIGN((dst), (dst)->DataType = totemPrivateDataType_Function; (dst)->Value.FunctionPointer.Address = (val); (dst)->Value.FunctionPointer.Type = (type))

#if 1
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, state) { \
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
#define TOTEM_INSTRUCTION_PRINT_DEBUG(ins, state) totemInstruction_Print(stdout, (ins))
#endif

totemLinkStatus totemLinkStatus_Break(totemLinkStatus status)
{
    return status;
}

totemExecStatus totemExecStatus_Break(totemExecStatus status)
{
    return status;
}

totemExecStatus totemExecState_InternString(totemExecState *state, totemString *str, totemRegister *strOut)
{
    if(totemRuntime_InternString(state->Runtime, str, strOut) != totemLinkStatus_Success)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut)
{
    totemStringLength len1 = totemRegister_GetStringLength(str1);
    totemStringLength len2 = totemRegister_GetStringLength(str2);
    const char *str1Val = totemRegister_GetStringValue(str1);
    const char *str2Val = totemRegister_GetStringValue(str2);
    
    if(len1 + len2 <= TOTEM_MINISTRING_MAXLENGTH)
    {
        TOTEM_REGISTER_ASSIGN(strOut,
                              {
                                  strOut->DataType = totemPrivateDataType_MiniString;
                                  memset(strOut->Value.MiniString.Value, 0, sizeof(strOut->Value.MiniString.Value));
                                  memcpy(strOut->Value.MiniString.Value, str1Val, len1);
                                  memcpy(strOut->Value.MiniString.Value + len1, str2Val, len2);
                              });
        
        return totemExecStatus_Continue;
    }
    
    char *buffer = totem_CacheMalloc(len1 + len2);
    if(!buffer)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memcpy(buffer, str1Val, len1);
    memcpy(buffer + len1, str2Val, len2);
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = len1 + len2;
    
    totemExecStatus status = totemExecState_InternString(state, &toIntern, strOut);
    totem_CacheFree(buffer, len1 + len2);
    return status;
}

totemExecStatus totemExecState_EmptyString(totemExecState *state, totemRegister *strOut)
{
    totemString empty;
    empty.Value = "";
    empty.Length = 0;
    
    return totemExecState_InternString(state, &empty, strOut);
}

totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemRegister *strOut)
{
    char buffer[256];
    int result = snprintf(buffer, TOTEM_ARRAYSIZE(buffer), "%llu", val);
    if(result < 0 || result >= TOTEM_ARRAYSIZE(buffer))
    {
        return totemExecStatus_Break(totemExecStatus_InternalBufferOverrun);
    }
    
    totemString string;
    string.Length = result;
    string.Value = buffer;
    
    return totemExecState_InternString(state, &string, strOut);
}

totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemRegister *strOut)
{
    char buffer[256];
    int result = snprintf(buffer, TOTEM_ARRAYSIZE(buffer), "%.6g", val);
    if(result < 0 || result >= TOTEM_ARRAYSIZE(buffer))
    {
        return totemExecStatus_Break(totemExecStatus_InternalBufferOverrun);
    }
    
    totemString string;
    string.Length = result;
    string.Value = buffer;
    
    return totemExecState_InternString(state, &string, strOut);
}

totemExecStatus totemExecState_TypeToString(totemExecState *state, totemPublicDataType type, totemRegister *strOut)
{
    totemString str;
    
    switch(type)
    {
        case totemPublicDataType_Int:
            totemString_FromLiteral(&str, "int");
            break;
            
        case totemPublicDataType_Type:
            totemString_FromLiteral(&str, "type");
            break;
            
        case totemPublicDataType_Array:
            totemString_FromLiteral(&str, "array");
            break;
            
        case totemPublicDataType_Float:
            totemString_FromLiteral(&str, "float");
            break;
            
        case totemPublicDataType_String:
            totemString_FromLiteral(&str, "string");
            break;
            
        case totemPublicDataType_Function:
            totemString_FromLiteral(&str, "function");
            break;
            
        default:
            return totemExecState_EmptyString(state, strOut);
    }
    
    return totemExecState_InternString(state, &str, strOut);
}

totemExecStatus totemExecState_FunctionPointerToString(totemExecState *state, totemFunctionPointer *ptr, totemRegister *strOut)
{
    totemRegister *result = NULL;
    
    switch(ptr->Type)
    {
        case totemFunctionType_Native:
        {
            result = totemMemoryBuffer_Get(&state->Runtime->NativeFunctionNames, ptr->Address);
            if(!result)
            {
                return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
            }
            break;
        }
            
        case totemFunctionType_Script:
        {
            totemScript *scr = totemMemoryBuffer_Get(&state->Runtime->Scripts, state->CallStack->CurrentActor->ScriptHandle);
            if(!scr)
            {
                return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
            }
            
            result = totemMemoryBuffer_Get(&scr->FunctionNames, ptr->Address);
            if(!result)
            {
                return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
            }
            
            break;
        }
    }
    
	TOTEM_REGISTER_ASSIGN_GENERIC(strOut, result);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemRuntimeArrayHeader *arr, totemRegister *strOut)
{
    if(arr->NumRegisters == 0)
    {
        return totemExecState_EmptyString(state, strOut);
    }
    
    totemRegister *strings = totem_CacheMalloc(sizeof(totemRegister) * arr->NumRegisters);
    if(!strings)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(strings, 0, sizeof(totemRegister) * arr->NumRegisters);
    uint32_t totalLen = 0;
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
    totemExecStatus status = totemExecStatus_Continue;
    
    for(size_t i = 0; i < arr->NumRegisters; i++)
    {
        switch(regs[i].DataType)
        {
            case totemPrivateDataType_Int:
                status = totemExecState_IntToString(state, regs[i].Value.Int, &strings[i]);
                break;
                
            case totemPrivateDataType_Type:
                status = totemExecState_TypeToString(state, regs[i].Value.DataType, &strings[i]);
                break;
                
            case totemPrivateDataType_Array:
                status = totemExecState_ArrayToString(state, regs[i].Value.Array, &strings[i]);
                break;
                
            case totemPrivateDataType_Float:
                status = totemExecState_FloatToString(state, regs[i].Value.Float, &strings[i]);
                break;
                
            case totemPrivateDataType_InternedString:
            case totemPrivateDataType_MiniString:
                memcpy(&strings[i], &regs[i], sizeof(totemRegister));
                break;
                
            case totemPrivateDataType_Function:
                status = totemExecState_FunctionPointerToString(state, &regs[i].Value.FunctionPointer, &strings[i]);
                break;
        }
        
        if(status != totemExecStatus_Continue)
        {
            totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
        
        totalLen += totemRegister_GetStringLength(&strings[i]);
    }
    
    if(!totalLen)
    {
        totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
        return totemExecState_EmptyString(state, strOut);
    }
    
    char *buffer = totem_CacheMalloc(totalLen + 1);
    if(!buffer)
    {
        totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    char *current = buffer;
    for(size_t i = 0; i < arr->NumRegisters; i++)
    {
        size_t len = totemRegister_GetStringLength(&strings[i]);
        const char *val = totemRegister_GetStringValue(&strings[i]);
        
        memcpy(current, val, len);
        current += len;
    }

	totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = totalLen;
    
    status = totemExecState_InternString(state, &toIntern, strOut);
    totem_CacheFree(buffer, totalLen + 1);
    
    return status;
}

totemRegister *totemRuntimeArrayHeader_GetRegisters(totemRuntimeArrayHeader *hdr)
{
    char *ptr = (char*)hdr;
    return (totemRegister*)(ptr + (sizeof(totemRuntimeArrayHeader)));
}

totemRuntimeArrayHeader *totemRuntimeArrayHeader_Create(uint32_t numRegisters)
{
    size_t toAllocate = sizeof(totemRuntimeArrayHeader) + (numRegisters * sizeof(totemRegister));
    totemRuntimeArrayHeader *hdr = totem_CacheMalloc(toAllocate);
    if(!hdr)
    {
        return NULL;
    }
    
    hdr->RefCount = 0;
    hdr->NumRegisters = numRegisters;
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(hdr);
    memset(regs, 0, sizeof(totemRegister) * numRegisters);
    return hdr;
}

void totemRuntimeArrayHeader_Destroy(totemRuntimeArrayHeader *arr)
{
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
    
    for(uint32_t i = 0; i < arr->NumRegisters; i++)
    {
        if(regs[i].DataType == totemPrivateDataType_Array)
        {
            totemRuntimeArrayHeader_DefRefCount(regs[i].Value.Array);
        }
    }
    
    totem_CacheFree(arr, sizeof(totemRuntimeArrayHeader) + (sizeof(totemRegister) * arr->NumRegisters));
}

void totemRuntimeArrayHeader_DefRefCount(totemRuntimeArrayHeader *arr)
{
    switch(arr->RefCount)
    {
        case 0:
            break;
            
        case 1:
            arr->RefCount = 0;
            totemRuntimeArrayHeader_Destroy(arr);
            break;
            
        default:
            arr->RefCount--;
            break;
    }
}

totemExecStatus totemActor_Init(totemActor *actor, totemRuntime *runtime, size_t scriptAddress)
{
    memset(actor, 0, sizeof(totemActor));
    totemMemoryBuffer_Init(&actor->GlobalRegisters, sizeof(totemRegister));
    
    actor->ScriptHandle = scriptAddress;
    
    totemScript *script = totemMemoryBuffer_Get(&runtime->Scripts, scriptAddress);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }
    
    if (!totemMemoryBuffer_TakeFrom(&actor->GlobalRegisters, &script->GlobalRegisters))
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    return totemExecStatus_Continue;
}

void totemActor_Cleanup(totemActor *actor)
{
    totemMemoryBuffer_Cleanup(&actor->GlobalRegisters);
}

const char *totemInternedStringHeader_GetString(totemInternedStringHeader *hdr)
{
    char *ptr = (char*)hdr;
    return (const char*)(ptr + sizeof(totemInternedStringHeader));
}

void totemInternedStringHeader_Destroy(totemInternedStringHeader *str)
{
    totem_CacheFree(str, sizeof(totemInternedStringHeader) + 1 + str->Length);
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

void totemRuntime_Init(totemRuntime *runtime)
{
    totemMemoryBuffer_Init(&runtime->NativeFunctions, sizeof(totemNativeFunction));
    totemMemoryBuffer_Init(&runtime->Scripts, sizeof(totemScript));
    totemMemoryBuffer_Init(&runtime->NativeFunctionNames, sizeof(totemRegister));
    totemHashMap_Init(&runtime->ScriptLookup);
    totemHashMap_Init(&runtime->NativeFunctionsLookup);
    totemHashMap_Init(&runtime->InternedStrings);
    runtime->FunctionCallFreeList = NULL;
}

void totemRuntime_Reset(totemRuntime *runtime)
{
    totemMemoryBuffer_Reset(&runtime->NativeFunctions);
    totemMemoryBuffer_Reset(&runtime->Scripts);
    totemMemoryBuffer_Reset(&runtime->NativeFunctionNames);
    totemHashMap_Reset(&runtime->ScriptLookup);
    totemHashMap_Reset(&runtime->NativeFunctionsLookup);
    totemHashMap_Reset(&runtime->InternedStrings);
}

void totemRuntime_Cleanup(totemRuntime *runtime)
{
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctions);
    totemMemoryBuffer_Cleanup(&runtime->Scripts);
    totemMemoryBuffer_Cleanup(&runtime->NativeFunctionNames);
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
                totemInternedStringHeader_Destroy(hdr);
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

totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRegister *strOut)
{
    if(str->Length <= TOTEM_MINISTRING_MAXLENGTH)
    {
        TOTEM_REGISTER_ASSIGN(strOut,
        {
            strOut->DataType = totemPrivateDataType_MiniString;
            memset(strOut->Value.MiniString.Value, 0, sizeof(strOut->Value.MiniString.Value));
            memcpy(strOut->Value.MiniString.Value, str->Value, str->Length);
        });
        
        return totemLinkStatus_Success;
    }
    
    totemHashMapEntry *result = totemHashMap_Find(&runtime->InternedStrings, str->Value, str->Length);
    if(result)
    {
        strOut->DataType = totemPrivateDataType_InternedString;
        strOut->Value.InternedString = (totemInternedStringHeader*)result->Value;
        return totemLinkStatus_Success;
    }
    
    size_t toAllocate = str->Length + sizeof(totemInternedStringHeader) + 1;
    
    totemInternedStringHeader *newStr = totem_CacheMalloc(toAllocate);
    if(!newStr)
    {
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    newStr->Length = str->Length;
    char *newStrVal = (char*)totemInternedStringHeader_GetString(newStr);
    
    newStr->Hash = totem_Hash(str->Value, newStr->Length);
    memcpy(newStrVal, str->Value, newStr->Length);
    newStrVal[newStr->Length] = 0;
    
    if(!totemHashMap_InsertPrecomputed(&runtime->InternedStrings, newStrVal, newStr->Length, (uintptr_t)newStr, newStr->Hash))
    {
        totemInternedStringHeader_Destroy(newStr);
        return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
    }
    
    strOut->DataType = totemPrivateDataType_InternedString;
    strOut->Value.InternedString = newStr;
    return totemLinkStatus_Success;
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
        
        totemScript_Init(script);
    }
    else
    {
        *indexOut = existing->Value;
        script = totemMemoryBuffer_Get(&runtime->Scripts, *indexOut);
        totemScript_Reset(script);
    }
    
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
    
    // link native function calls
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
        
        totemOperandXUnsigned funcAddr = 0;
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
            case totemPublicDataType_String:
            {
                totemString *str = (totemString*)prototype->Value.InternedString;
                
                if(totemRuntime_InternString(runtime, str, reg) != totemLinkStatus_Success)
                {
                    return totemLinkStatus_Break(totemLinkStatus_OutOfMemory);
                }
                break;
            }
                
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
        
        if(totemRuntime_InternString(runtime, &funcProts[i].Name, &strs[i]) != totemLinkStatus_Success)
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
        
        if(totemRuntime_InternString(runtime, name, &value) != totemLinkStatus_Success)
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

totemExecStatus totemExecState_PushFunctionCall(totemExecState *state, totemFunctionType funcType, size_t functionAddress, totemActor *actor, totemRegister *returnReg, uint8_t numRegisters)
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
    
    call->CurrentActor = actor;
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
                TOTEM_INSTRUCTION_PRINT_DEBUG(*state->CurrentInstruction, state);
                totemRegister *argument = TOTEM_GET_OPERANDA_REGISTER(state, (*state->CurrentInstruction));
                
                TOTEM_REGISTER_ASSIGN_GENERIC(&call->FrameStart[call->NumArguments], argument);
                
                state->CurrentInstruction++;
                call->NumArguments++;
                type = TOTEM_INSTRUCTION_GET_OP(*state->CurrentInstruction);
            }
            while(type == totemOperationType_FunctionArg);
        }
    }
    
    state->UsedLocalRegisters += numRegisters;
    state->Registers[totemOperandType_LocalRegister] = call->FrameStart;
    state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)actor->GlobalRegisters.Data;
    
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
            
            if(reg->DataType == totemPrivateDataType_Array)
            {
                totemRuntimeArrayHeader_DefRefCount(reg->Value.Array);
            }
        }
        
        state->CurrentInstruction = state->CallStack->ResumeAt;
        state->UsedLocalRegisters -= state->CallStack->NumRegisters;
        state->Registers[totemOperandType_LocalRegister] = state->CallStack->PreviousFrameStart;
        state->Registers[totemOperandType_GlobalRegister] = (totemRegister*)state->CallStack->CurrentActor->GlobalRegisters.Data;
        
        totemFunctionCall *call = state->CallStack;
        totemFunctionCall *prev = call->Prev;
        totemRuntime_FreeFunctionCall(state->Runtime, call);
        state->CallStack = prev;
    }
}

totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionAddress, totemRegister *returnRegister)
{
    totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, actor->ScriptHandle);
    if(script == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
    }
    
    totemScriptFunction *function = totemMemoryBuffer_Get(&script->Functions, functionAddress);
    if(function == NULL)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Script, functionAddress, actor, returnRegister, function->RegistersNeeded);
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
    
    totemExecStatus status = totemExecState_PushFunctionCall(state, totemFunctionType_Native, functionHandle, actor, returnRegister, numArgs);
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
    
    TOTEM_INSTRUCTION_PRINT_DEBUG(instruction, state);
    
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
    
    if(src->DataType != totemPrivateDataType_Function)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemExecStatus status = totemExecStatus_Continue;
    
    switch(src->Value.FunctionPointer.Type)
    {
        case totemFunctionType_Native:
            state->CurrentInstruction++;
            status = totemExecState_ExecNative(state, state->CallStack->CurrentActor, src->Value.FunctionPointer.Address, dst);
            break;
            
        case totemFunctionType_Script:
            state->CurrentInstruction++;
            status = totemExecState_Exec(state, state->CallStack->CurrentActor, src->Value.FunctionPointer.Address, dst);
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
    totemRegister *typeReg = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    totemPublicDataType srcType = totemPrivateDataType_ToPublic(src->DataType);
    totemPublicDataType type = typeReg->Value.DataType;
    
    if(typeReg->DataType != totemPrivateDataType_Type)
    {
        type = totemPrivateDataType_ToPublic(typeReg->DataType);
    }
    
    switch(TOTEM_TYPEPAIR(srcType, type))
    {
            /*
             * int
             */
            
            // int as int
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Int):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
            // int as float
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, (totemFloat)src->Value.Int);
            break;
            
            // int as new array
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_INT(&regs[0], src->Value.Int);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
            // int as string
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_String):
            TOTEM_EXEC_CHECKRETURN(totemExecState_IntToString(state, src->Value.Int, dst));
            break;
            
            // int as type
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemPublicDataType_Int);
            break;
            
            /*
             * floats
             */
            
            // float as float
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
            // float as int
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(dst, (totemInt)src->Value.Float);
            break;
            
            // float as new array
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_FLOAT(&regs[0], src->Value.Float);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
            // float as string
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_String):
            TOTEM_EXEC_CHECKRETURN(totemExecState_FloatToString(state, src->Value.Float, dst));
            break;
            
            // float as type
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemPublicDataType_Float);
            break;
            
            /*
             * arrays
             */
            
            // array as new array
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(src->Value.Array->NumRegisters);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            totemRegister *srcRegs = totemRuntimeArrayHeader_GetRegisters(src->Value.Array);
            
            for(uint32_t i = 0; i < src->Value.Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(&regs[i], &srcRegs[i]);
            }
            
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
            // array as int (length)
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(dst, (totemInt)src->Value.Array->NumRegisters);
            break;
            
            // array as float (length)
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, (totemFloat)src->Value.Array->NumRegisters);
            break;
            
            // array as string (implode)
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_String):
            TOTEM_EXEC_CHECKRETURN(totemExecState_ArrayToString(state, src->Value.Array, dst));
            break;
            
            // array as type
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemPublicDataType_Array);
            break;
            
            /*
             * types
             */
            // type as array
        case TOTEM_TYPEPAIR(totemPublicDataType_Type, totemPublicDataType_Array):
        {
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(1);
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            TOTEM_REGISTER_ASSIGN_TYPE(&regs[0], src->Value.DataType);
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
            // type as type
        case TOTEM_TYPEPAIR(totemPublicDataType_Type, totemPublicDataType_Type):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
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
            TOTEM_REGISTER_ASSIGN_INT(dst, val);
            break;
        }
            
            // string as float (attempt atof)
        case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Float):
        {
            const char *str = totemRegister_GetStringValue(src);
            totemFloat val = strtod(str, NULL);
            TOTEM_REGISTER_ASSIGN_FLOAT(dst, val);
            break;
        }
            
            // explode string into array of strings, one for each char
        case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Array):
        {
            const char *str = totemRegister_GetStringValue(src);
            totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(totemRegister_GetStringLength(src));
            if(!arr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(arr);
            for(size_t i = 0; i < arr->NumRegisters; i++)
            {
                totemString newStr;
                newStr.Length = 1;
                newStr.Value = &str[i];
                
                if(totemRuntime_InternString(state->Runtime, &newStr, &regs[i]) != totemLinkStatus_Success)
                {
                    return totemExecStatus_Break(totemExecStatus_OutOfMemory);
                }
            }
            
            TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
            break;
        }
            
            // string as type
        case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemPublicDataType_String);
            break;
            
            // string as string
        case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_String):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
            break;
            
            // lookup function pointer by name
        case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Function):
        {
            totemString lookup;
            lookup.Value = totemRegister_GetStringValue(src);
            lookup.Length = totemRegister_GetStringLength(src);
            
            totemOperandXUnsigned addr = 0;
            if(totemRuntime_GetNativeFunctionAddress(state->Runtime, &lookup, &addr))
            {
                TOTEM_REGISTER_ASSIGN_FUNC(dst, addr, totemFunctionType_Native);
            }
            else
            {
                totemScript *script = totemMemoryBuffer_Get(&state->Runtime->Scripts, state->CallStack->CurrentActor->ScriptHandle);
                if(!script)
                {
                    return totemExecStatus_Break(totemExecStatus_ScriptNotFound);
                }
                
                totemHashMapEntry *entry = totemHashMap_Find(&script->FunctionNameLookup, lookup.Value, lookup.Length);
                if(entry)
                {
                    addr = (totemOperandXUnsigned)entry->Value;
                    TOTEM_REGISTER_ASSIGN_FUNC(dst, addr, totemFunctionType_Script);
                }
                else
                {
                    TOTEM_REGISTER_ASSIGN_NULL(dst);
                }
            }
            
            break;
        }
            
            /*
             * functions
             */
            // function as type
        case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_Type):
            TOTEM_REGISTER_ASSIGN_TYPE(dst, totemPublicDataType_Function);
            break;
            
            // function as string
        case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_String):
            TOTEM_EXEC_CHECKRETURN(totemExecState_FunctionPointerToString(state, &src->Value.FunctionPointer, dst));
            break;
            
            // function as function
        case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_Function):
            TOTEM_REGISTER_ASSIGN_GENERIC(dst, src);
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
    
    totemPrivateDataType type = typeSrc->Value.DataType;
    
    if(typeSrc->DataType != totemPrivateDataType_Type)
    {
        type = totemPrivateDataType_ToPublic(typeSrc->DataType);
    }
    
    TOTEM_REGISTER_ASSIGN_INT(dst, src->DataType == type);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMove(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_GENERIC(destination, source);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoveToGlobal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *src = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    TOTEM_REGISTER_ASSIGN_GENERIC(globalReg, src);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecMoveToLocal(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned globalVarIndex = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    totemRegister *globalReg = &state->Registers[totemOperandType_GlobalRegister][globalVarIndex];
    
    TOTEM_REGISTER_ASSIGN_GENERIC(dst, globalReg);
    
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int + source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) + source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float + source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float + ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Array):
        {
            totemRuntimeArrayHeader *newArr = totemRuntimeArrayHeader_Create(source1->Value.Array->NumRegisters + source2->Value.Array->NumRegisters);
            if(!newArr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *newRegs = totemRuntimeArrayHeader_GetRegisters(newArr);
            totemRegister *source1Regs = totemRuntimeArrayHeader_GetRegisters(source1->Value.Array);
            
            for(size_t i = 0; i < source1->Value.Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(&newRegs[i], &source1Regs[i]);
            }
            
            totemRegister *source2Regs = totemRuntimeArrayHeader_GetRegisters(source2->Value.Array);
            for(size_t i = source1->Value.Array->NumRegisters; i < newArr->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(&newRegs[i], &source2Regs[i - source1->Value.Array->NumRegisters]);
            }
            
            TOTEM_REGISTER_ASSIGN_ARRAY(destination, newArr);
            break;
        }
            
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Function):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_String):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Float):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Type):
        case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Int):
        {
            totemRuntimeArrayHeader *newArr = totemRuntimeArrayHeader_Create(source1->Value.Array->NumRegisters + 1);
            if(!newArr)
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
            
            totemRegister *newRegs = totemRuntimeArrayHeader_GetRegisters(newArr);
            totemRegister *source1Regs = totemRuntimeArrayHeader_GetRegisters(source1->Value.Array);
            
            for(size_t i = 0; i < source1->Value.Array->NumRegisters; i++)
            {
                TOTEM_REGISTER_ASSIGN_GENERIC(&newRegs[i], &source1Regs[i]);
            }
            
            TOTEM_REGISTER_ASSIGN_GENERIC(&newRegs[newArr->NumRegisters - 1], source2);
            TOTEM_REGISTER_ASSIGN_ARRAY(destination, newArr);
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int - source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float - source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float - ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) - source2->Value.Float);
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int * source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float * source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float * ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) * source2->Value.Float);
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int / source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float / source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, source1->Value.Float / ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, ((totemFloat)source1->Value.Int) / source2->Value.Float);
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
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow((totemFloat)source1->Value.Int, (totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow(source1->Value.Float, source2->Value.Float));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow(source1->Value.Float, (totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(destination, pow((totemFloat)source1->Value.Int, source2->Value.Float));
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
    
    TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Data == source2->Value.Data && source1->DataType == source2->DataType);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecNotEquals(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Data != source2->Value.Data || source1->DataType != source2->DataType);
    
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int < source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float < source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float < ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) < source2->Value.Float);
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int <= source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float <= source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float <= ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) <= source2->Value.Float);
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int > source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float > source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float > ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) > source2->Value.Float);
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
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Int >= source2->Value.Int);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float >= source2->Value.Float);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_INT(destination, source1->Value.Float >= ((totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_INT(destination, ((totemFloat)source1->Value.Float) >= source2->Value.Float);
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
    return totemExecState_Exec(state, state->CallStack->CurrentActor, functionIndex, destination);
}

totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *returnRegister = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemOperandXUnsigned functionHandle = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    state->CurrentInstruction++;
    return totemExecState_ExecNative(state, state->CallStack->CurrentActor, functionHandle, returnRegister);
}

totemExecStatus totemExecState_ExecReturn(totemExecState *state)
{
    totemFunctionCall *call = state->CallStack;
    
    totemInstruction instruction = *state->CurrentInstruction;
    totemOperandXUnsigned option = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(instruction);
    
    if(option == totemReturnOption_Register)
    {
        totemRegister *source = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
        TOTEM_REGISTER_ASSIGN_GENERIC(call->ReturnRegister, source);
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
    
    totemRuntimeArrayHeader *arr = totemRuntimeArrayHeader_Create(numRegisters);
    if(!arr)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    TOTEM_REGISTER_ASSIGN_ARRAY(dst, arr);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ExecArrayGet(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *dst = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *src = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *indexSrc = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    if(indexSrc->DataType != totemPrivateDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemInt index = indexSrc->Value.Int;
    
    if(index < 0 || index >= UINT32_MAX)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    if(totemPrivateDataType_ToPublic(src->DataType) == totemPublicDataType_String)
    {
        const char *str = totemRegister_GetStringValue(src);
        totemStringLength len = totemRegister_GetStringLength(src);
        
        if(index >= len)
        {
            return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
        }
        
        totemString toIntern;
        toIntern.Length = 1;
        toIntern.Value = str + index;
        
        TOTEM_EXEC_CHECKRETURN(totemExecState_InternString(state, &toIntern, dst));
    }
    else if(src->DataType != totemPrivateDataType_Array)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    else
    {
        if(index < 0 || index >= UINT32_MAX || index >= src->Value.Array->NumRegisters)
        {
            return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
        }
        
        totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(src->Value.Array);
        TOTEM_REGISTER_ASSIGN_GENERIC(dst, &regs[index]);
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
    
    if(indexSrc->DataType != totemPrivateDataType_Int)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    if(dst->DataType != totemPrivateDataType_Array)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    totemInt index = indexSrc->Value.Int;
    
    if(index < 0 || index >= UINT32_MAX || index >= dst->Value.Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRegister *regs = totemRuntimeArrayHeader_GetRegisters(dst->Value.Array);
    TOTEM_REGISTER_ASSIGN_GENERIC(&regs[index], src);
    
    state->CurrentInstruction++;
    return totemExecStatus_Continue;
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
            TOTEM_STRINGIFY_CASE(totemExecStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemExecStatus_InternalBufferOverrun);
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