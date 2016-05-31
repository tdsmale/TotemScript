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

#define TOTEM_EXEC_ARRAYSIZE(numRegisters) (sizeof(totemArray) + (sizeof(totemRegister) * (numRegisters - 1)))
#define TOTEM_REGISTER_ISGC(x) ((x)->DataType == totemPrivateDataType_Array || (x)->DataType == totemPrivateDataType_Coroutine)

#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }

#define TOTEM_GET_OPERANDA_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(instruction))]
#define TOTEM_GET_OPERANDB_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(instruction))]
#define TOTEM_GET_OPERANDC_REGISTER(state, instruction) &state->Registers[TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(instruction)][((totemLocalRegisterIndex)TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(instruction))]

#define TOTEM_REGISTER_ASSIGN(state, dst, perform) \
if(TOTEM_REGISTER_ISGC(dst)) \
{ \
totemExecState_DecRefCount(state, (dst)->Value.GCObject); \
} \
perform; \
if(TOTEM_REGISTER_ISGC(dst)) \
{ \
(dst)->Value.GCObject->RefCount++; \
}

#define TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, src) TOTEM_REGISTER_ASSIGN(state, (dst), memcpy((dst), (src), sizeof(totemRegister)))
#define TOTEM_REGISTER_ASSIGN_FLOAT(state, dst, val) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Float; (dst)->Value.Float = (val))
#define TOTEM_REGISTER_ASSIGN_INT(state, dst, val) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Int; (dst)->Value.Int = (val))
#define TOTEM_REGISTER_ASSIGN_TYPE(state, dst,val) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Type; (dst)->Value.DataType = (val))
#define TOTEM_REGISTER_ASSIGN_NULL(state, dst) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Int; (dst)->Value.Int = 0)
#define TOTEM_REGISTER_ASSIGN_ARRAY(state, dst, val) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Array; (dst)->Value.GCObject = (val))
#define TOTEM_REGISTER_ASSIGN_STRING(state, dst, val) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_String; (dst)->Value.InternedString = (val))
#define TOTEM_REGISTER_ASSIGN_FUNC(state, dst, val, type) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Function; (dst)->Value.FunctionPointer.Address = (val); (dst)->Value.FunctionPointer.Type = (type))
#define TOTEM_REGISTER_ASSIGN_COROUTINE(state, dst, val) TOTEM_REGISTER_ASSIGN(state, (dst), (dst)->DataType = totemPrivateDataType_Coroutine; (dst)->Value.GCObject = (val))

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

totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut)
{
    totemStringLength len1 = totemRegister_GetStringLength(str1);
    totemStringLength len2 = totemRegister_GetStringLength(str2);
    const char *str1Val = totemRegister_GetStringValue(str1);
    const char *str2Val = totemRegister_GetStringValue(str2);
    
    if(len1 + len2 <= TOTEM_MINISTRING_MAXLENGTH)
    {
        TOTEM_REGISTER_ASSIGN(state, strOut,
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
    int result = totem_snprintf(buffer, TOTEM_ARRAYSIZE(buffer), "%llu", val);
    
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
    int result = totem_snprintf(buffer, TOTEM_ARRAYSIZE(buffer), "%.6g", val);
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
            
        case totemPublicDataType_Coroutine:
            totemString_FromLiteral(&str, "coroutine");
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
            result = totemMemoryBuffer_Get(&state->CallStack->CurrentActor->Script->FunctionNames, ptr->Address);
            if(!result)
            {
                return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
            }
            
            break;
        }
    }
    
    TOTEM_REGISTER_ASSIGN_GENERIC(state, strOut, result);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemArray *arr, totemRegister *strOut)
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
    totemExecStatus status = totemExecStatus_Continue;
    
    for(size_t i = 0; i < arr->NumRegisters; i++)
    {
        switch(arr->Registers[i].DataType)
        {
            case totemPrivateDataType_Int:
                status = totemExecState_IntToString(state, arr->Registers[i].Value.Int, &strings[i]);
                break;
                
            case totemPrivateDataType_Type:
                status = totemExecState_TypeToString(state, arr->Registers[i].Value.DataType, &strings[i]);
                break;
                
            case totemPrivateDataType_Array:
                status = totemExecState_ArrayToString(state, arr->Registers[i].Value.GCObject->Array, &strings[i]);
                break;
                
            case totemPrivateDataType_Float:
                status = totemExecState_FloatToString(state, arr->Registers[i].Value.Float, &strings[i]);
                break;
                
            case totemPrivateDataType_InternedString:
            case totemPrivateDataType_MiniString:
                memcpy(&strings[i], &arr->Registers[i], sizeof(totemRegister));
                break;
                
            case totemPrivateDataType_Coroutine:
            {
                totemFunctionPointer ptr;
                ptr.Address = arr->Registers[i].Value.GCObject->Coroutine->FunctionHandle;
                ptr.Type = totemFunctionType_Script;
                status = totemExecState_FunctionPointerToString(state, &ptr, &strings[i]);
                break;
            }
                
                
            case totemPrivateDataType_Function:
                status = totemExecState_FunctionPointerToString(state, &arr->Registers[i].Value.FunctionPointer, &strings[i]);
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

totemExecStatus totemExecState_StringToFunctionPointer(totemExecState *state, totemRegister *src, totemRegister *dst)
{
    totemString lookup;
    lookup.Value = totemRegister_GetStringValue(src);
    lookup.Length = totemRegister_GetStringLength(src);
    
    totemOperandXUnsigned addr = 0;
    if (totemRuntime_GetNativeFunctionAddress(state->Runtime, &lookup, &addr))
    {
        TOTEM_REGISTER_ASSIGN_FUNC(state, dst, addr, totemFunctionType_Native);
    }
    else
    {
        totemHashMapEntry *entry = totemHashMap_Find(&state->CallStack->CurrentActor->Script->FunctionNameLookup, lookup.Value, lookup.Length);
        if (entry)
        {
            addr = (totemOperandXUnsigned)entry->Value;
            TOTEM_REGISTER_ASSIGN_FUNC(state, dst, addr, totemFunctionType_Script);
        }
        else
        {
            TOTEM_REGISTER_ASSIGN_NULL(state, dst);
        }
    }
    
    return totemExecStatus_Continue;
}

totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type)
{
    totemGCObject *hdr = totem_CacheMalloc(sizeof(totemGCObject));
    if(!hdr)
    {
        return NULL;
    }
    
    hdr->RefCount = 0;
    hdr->Type = type;
    hdr->Array = NULL;
    return hdr;
}

totemExecStatus totemExecState_CreateArray(totemExecState *state, uint32_t numRegisters, totemGCObject **gcOut)
{
    size_t toAllocate = TOTEM_EXEC_ARRAYSIZE(numRegisters);
    totemArray *arr = totem_CacheMalloc(toAllocate);
    if (!arr)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    arr->NumRegisters = numRegisters;
    memset(arr->Registers, 0, sizeof(totemRegister) * numRegisters);
    
    totemGCObject *obj = totemExecState_CreateGCObject(state, totemGCObjectType_Array);
    if (!obj)
    {
        totem_CacheFree(arr, TOTEM_EXEC_ARRAYSIZE(numRegisters));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    obj->Array = arr;
    *gcOut = obj;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, uint32_t numRegisters, totemGCObject **gcOut)
{
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, numRegisters, gcOut));
    
    for (uint32_t i = 0; i < numRegisters; i++)
    {
        TOTEM_REGISTER_ASSIGN_GENERIC(state, &(*gcOut)->Array->Registers[i], &registers[i]);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemOperandXUnsigned address, totemGCObject **gcOut)
{
    totemScriptFunction *func = totemMemoryBuffer_Get(&state->CallStack->CurrentActor->Script->Functions, address);
    if (!func)
    {
        return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
    }
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Coroutine);
    if (!gc)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    gc->Coroutine = totemExecState_SecureFunctionCall(state);
    if (!gc->Coroutine)
    {
        totemExecState_DestroyGCObject(state, gc);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(gc->Coroutine, 0, sizeof(totemFunctionCall));
    gc->Coroutine->FunctionHandle = address;
    gc->Coroutine->Type = totemFunctionType_Script;
    gc->Coroutine->CurrentActor = state->CallStack->CurrentActor;
    gc->Coroutine->Flags = totemFunctionCallFlag_IsCoroutine | totemFunctionCallFlag_FreeStack;
    gc->Coroutine->NumRegisters = func->RegistersNeeded;
    gc->Coroutine->FrameStart = totem_CacheMalloc(sizeof(totemRegister) * func->RegistersNeeded);
    if (!gc->Coroutine->FrameStart)
    {
        totemExecState_FreeFunctionCall(state, gc->Coroutine);
        totemExecState_DestroyGCObject(state, gc);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    memset(gc->Coroutine->FrameStart, 0, sizeof(totemRegister) * func->RegistersNeeded);
    
    *gcOut = gc;
    return totemExecStatus_Continue;
}

void totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *obj)
{
    totem_CacheFree(obj, sizeof(totemGCObject));
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

void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc)
{
    switch (gc->RefCount)
    {
        case 0:
            break;
            
        case 1:
            gc->RefCount = 0;
            
            switch (gc->Type)
        {
            case totemGCObjectType_Array:
                totemExecState_DestroyArray(state, gc->Array);
                break;
                
            case totemGCObjectType_Coroutine:
                totemExecState_DestroyCoroutine(state, gc->Coroutine);
                break;
        }
            
            totemExecState_DestroyGCObject(state, gc);
            break;
            
        default:
            gc->RefCount--;
            break;
    }
}

void totemExecState_DestroyArray(totemExecState *state, totemArray *arr)
{
    totemExecState_CleanupRegisterList(state, arr->Registers, arr->NumRegisters);
    totem_CacheFree(arr, TOTEM_EXEC_ARRAYSIZE(arr->NumRegisters));
}

void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co)
{
    totemExecState_CleanupRegisterList(state, co->FrameStart, co->NumRegisters);
    totem_CacheFree(co->FrameStart, sizeof(totemRegister) * co->NumRegisters);
    totemExecState_FreeFunctionCall(state, co);
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
                
                if(totemRuntime_InternString(runtime, str, &reg->Value, &reg->DataType) != totemLinkStatus_Success)
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

totemExecStatus totemExecState_ExecPower(totemExecState *state)
{
    totemInstruction instruction = *state->CurrentInstruction;
    totemRegister *destination = TOTEM_GET_OPERANDA_REGISTER(state, instruction);
    totemRegister *source1 = TOTEM_GET_OPERANDB_REGISTER(state, instruction);
    totemRegister *source2 = TOTEM_GET_OPERANDC_REGISTER(state, instruction);
    
    switch(TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, pow((totemFloat)source1->Value.Int, (totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, pow(source1->Value.Float, source2->Value.Float));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, pow(source1->Value.Float, (totemFloat)source2->Value.Int));
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            TOTEM_REGISTER_ASSIGN_FLOAT(state, destination, pow((totemFloat)source1->Value.Int, source2->Value.Float));
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
        if(index < 0 || index >= UINT32_MAX || index >= src->Value.GCObject->Array->NumRegisters)
        {
            return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
        }
        
        totemRegister *regs = src->Value.GCObject->Array->Registers;
        TOTEM_REGISTER_ASSIGN_GENERIC(state, dst, &regs[index]);
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
    
    if(index < 0 || index >= UINT32_MAX || index >= dst->Value.GCObject->Array->NumRegisters)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRegister *regs = dst->Value.GCObject->Array->Registers;
    TOTEM_REGISTER_ASSIGN_GENERIC(state, &regs[index], src);
    
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