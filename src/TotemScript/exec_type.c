//
//  exec_type.c
//  TotemScript
//
//  Created by Timothy Smale on 03/08/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

#define TOTEM_EXEC_ARRAYSIZE(numRegisters) (sizeof(totemArray) + (sizeof(totemRegister) * (numRegisters - 1)))
#define TOTEM_REGISTER_DECIFGC(dst) if (TOTEM_REGISTER_ISGC(dst)) totemExecState_DecRefCount(state, dst->Value.GCObject);

totemExecStatus totemExecState_Assign(totemExecState *state, totemRegister *dst, totemRegister *src)
{
    TOTEM_REGISTER_DECIFGC(dst);
    memcpy((dst), (src), sizeof(totemRegister));
    
    if (TOTEM_REGISTER_ISGC(dst))
    {
        TOTEM_EXEC_CHECKRETURN(totemExecState_IncRefCount(state, dst->Value.GCObject));
    }
    
    return totemExecStatus_Continue;
}

void totemExecState_AssignNewInt(totemExecState *state, totemRegister *dst, totemInt newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Int;
    dst->Value.Int = newVal;
}

void totemExecState_AssignNewFloat(totemExecState *state, totemRegister *dst, totemFloat newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Float;
    dst->Value.Float = newVal;
}

void totemExecState_AssignNewType(totemExecState *state, totemRegister *dst, totemPublicDataType newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Type;
    dst->Value.DataType = newVal;
}

void totemExecState_AssignNewFunctionPointer(totemExecState *state, totemRegister *dst, totemOperandXUnsigned addr, totemFunctionType type)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Function;
    dst->Value.FunctionPointer.Address = addr;
    dst->Value.FunctionPointer.Type = type;
}

void totemExecState_AssignNewArray(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Array;
    dst->Value.GCObject = newVal;
    
    totemExecState_IncRefCount(state, dst->Value.GCObject);
}

void totemExecState_AssignNewCoroutine(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Coroutine;
    dst->Value.GCObject = newVal;
    
    totemExecState_IncRefCount(state, dst->Value.GCObject);
}

void totemExecState_AssignNewObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Object;
    dst->Value.GCObject = newVal;
    
    totemExecState_IncRefCount(state, dst->Value.GCObject);
}

void totemExecState_AssignNewUserdata(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Userdata;
    dst->Value.GCObject = newVal;
    
    totemExecState_IncRefCount(state, dst->Value.GCObject);
}

void totemExecState_AssignNewString(totemExecState *state, totemRegister *dst, totemRegister *src)
{
    TOTEM_REGISTER_DECIFGC(dst);
    memcpy((dst), (src), sizeof(totemRegister));
}

totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut)
{
    totemStringLength len1 = totemRegister_GetStringLength(str1);
    totemStringLength len2 = totemRegister_GetStringLength(str2);
    const char *str1Val = totemRegister_GetStringValue(str1);
    const char *str2Val = totemRegister_GetStringValue(str2);
    
    if (len1 + len2 <= TOTEM_MINISTRING_MAXLENGTH)
    {
        TOTEM_REGISTER_DECIFGC(strOut);
        strOut->DataType = totemPrivateDataType_MiniString;
        memset(strOut->Value.MiniString.Value, 0, sizeof(strOut->Value.MiniString.Value));
        memcpy(strOut->Value.MiniString.Value, str1Val, len1);
        memcpy(strOut->Value.MiniString.Value + len1, str2Val, len2);
        
        return totemExecStatus_Continue;
    }
    
    char *buffer = totem_CacheMalloc(len1 + len2);
    if (!buffer)
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
    
    if (result < 0 || result >= TOTEM_ARRAYSIZE(buffer))
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
    if (result < 0 || result >= TOTEM_ARRAYSIZE(buffer))
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
    
    switch (type)
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
            
        case totemPublicDataType_Object:
            totemString_FromLiteral(&str, "object");
            break;
            
        default:
            return totemExecState_EmptyString(state, strOut);
    }
    
    return totemExecState_InternString(state, &str, strOut);
}

totemExecStatus totemExecState_FunctionPointerToString(totemExecState *state, totemFunctionPointer *ptr, totemRegister *strOut)
{
    totemRegister *result = NULL;
    
    switch (ptr->Type)
    {
        case totemFunctionType_Native:
        {
            result = totemMemoryBuffer_Get(&state->Runtime->NativeFunctionNames, ptr->Address);
            if (!result)
            {
                return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
            }
            break;
        }
            
        case totemFunctionType_Script:
        {
            result = totemMemoryBuffer_Get(&state->CallStack->CurrentActor->Script->FunctionNames, ptr->Address);
            if (!result)
            {
                return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
            }
            
            break;
        }
    }
    
    TOTEM_EXEC_CHECKRETURN(totemExecState_Assign(state, strOut, result));
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemArray *arr, totemRegister *strOut)
{
    if (arr->NumRegisters == 0)
    {
        return totemExecState_EmptyString(state, strOut);
    }
    
    totemRegister *strings = totem_CacheMalloc(sizeof(totemRegister) * arr->NumRegisters);
    if (!strings)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(strings, 0, sizeof(totemRegister) * arr->NumRegisters);
    uint32_t totalLen = 0;
    totemExecStatus status = totemExecStatus_Continue;
    
    for (size_t i = 0; i < arr->NumRegisters; i++)
    {
        switch (arr->Registers[i].DataType)
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
        
        if (status != totemExecStatus_Continue)
        {
            totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
        
        totalLen += totemRegister_GetStringLength(&strings[i]);
    }
    
    if (!totalLen)
    {
        totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
        return totemExecState_EmptyString(state, strOut);
    }
    
    char *buffer = totem_CacheMalloc(totalLen + 1);
    if (!buffer)
    {
        totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    char *current = buffer;
    for (size_t i = 0; i < arr->NumRegisters; i++)
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
        totemExecState_AssignNewFunctionPointer(state, dst, addr, totemFunctionType_Native);
    }
    else
    {
        totemHashMapEntry *entry = totemHashMap_Find(&state->CallStack->CurrentActor->Script->FunctionNameLookup, lookup.Value, lookup.Length);
        if (entry)
        {
            addr = (totemOperandXUnsigned)entry->Value;
            totemExecState_AssignNewFunctionPointer(state, dst, addr, totemFunctionType_Script);
        }
        else
        {
            return totemExecStatus_Break(totemExecStatus_ScriptFunctionNotFound);
        }
    }
    
    return totemExecStatus_Continue;
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
        TOTEM_EXEC_CHECKRETURN(totemExecState_Assign(state, &(*gcOut)->Array->Registers[i], &registers[i]));
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateObject(totemExecState *state, totemGCObject **gcOut)
{
    totemObject *obj = totem_CacheMalloc(sizeof(totemObject));
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    totemHashMap_Init(&obj->Lookup);
    totemMemoryBuffer_Init(&obj->Registers, sizeof(totemRegister));
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Object);
    if (!gc)
    {
        totem_CacheFree(obj, sizeof(*obj));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    gc->Object = obj;
    *gcOut = gc;
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

totemExecStatus totemExecState_CreateUserdata(totemExecState *state, uint64_t data, totemUserdataDestructor destructor, totemGCObject **gcOut)
{
    totemUserdata *obj = totem_CacheMalloc(sizeof(totemUserdata));
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    obj->Data = data;
    obj->Destructor = destructor;
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Userdata);
    if (!gc)
    {
        totem_CacheFree(obj, sizeof(*obj));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    gc->Userdata = obj;
    *gcOut = gc;
    return totemExecStatus_Continue;
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

void totemExecState_DestroyObject(totemExecState *state, totemObject *obj)
{
    totemExecState_CleanupRegisterList(state, totemMemoryBuffer_Bottom(&obj->Registers), (uint32_t)totemMemoryBuffer_GetNumObjects(&obj->Registers));
    
    totemHashMap_Cleanup(&obj->Lookup);
    totemMemoryBuffer_Cleanup(&obj->Registers);
    totem_CacheFree(obj, sizeof(totemObject));
}

void totemExecState_DestroyUserdata(totemExecState *state, totemUserdata *data)
{
    if (data->Destructor)
    {
        data->Destructor(state, data);
    }
    
    totem_CacheFree(data, sizeof(totemUserdata));
}