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
#define TOTEM_REGISTER_DECIFGC(dst) if (TOTEM_REGISTER_ISGC(dst)) totemExecState_DecRefCount(state, (dst)->Value.GCObject);

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

void totemExecState_AssignQuick(totemExecState *state, totemRegister *dst, totemRegister *src)
{
    TOTEM_REGISTER_DECIFGC(dst);
    memcpy((dst), (src), sizeof(totemRegister));
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

void totemExecState_AssignNewNativeFunction(totemExecState *state, totemRegister *dst, totemNativeFunction *func)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_NativeFunction;
    dst->Value.NativeFunction = func;
}

void totemExecState_AssignNewInstanceFunction(totemExecState *state, totemRegister *dst, totemInstanceFunction *func)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_InstanceFunction;
    dst->Value.InstanceFunction = func;
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

void totemExecState_AssignNewChannel(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    TOTEM_REGISTER_DECIFGC(dst);
    dst->DataType = totemPrivateDataType_Channel;
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

totemExecStatus totemExecState_NativeFunctionToString(totemExecState *state, totemNativeFunction *func, totemRegister *strOut)
{
    totemRegister val;
    memset(&val, 0, sizeof(val));
    
    if (!totemRuntime_GetNativeFunctionName(state->Runtime, func->Address, &val.Value, &val.DataType))
    {
        return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
    }
    
    totemExecState_AssignNewString(state, strOut, &val);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_InstanceFunctionToString(totemExecState *state, totemInstanceFunction *func, totemRegister *strOut)
{
    totemRegister val;
    memset(&val, 0, sizeof(val));
    
    if (!totemScript_GetFunctionName(func->Instance->Script, func->Function->Address, &val.Value, &val.DataType))
    {
        return totemExecStatus_Break(totemExecStatus_InstanceFunctionNotFound);
    }
    
    totemExecState_AssignNewString(state, strOut, &val);
    return totemExecStatus_Continue;
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
            
        case totemPublicDataType_Channel:
            totemString_FromLiteral(&str, "channel");
            break;
            
        default:
            return totemExecState_EmptyString(state, strOut);
    }
    
    return totemExecState_InternString(state, &str, strOut);
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
        totemRegister *src = &arr->Registers[i];
        totemRegister *dst = &strings[i];
        
        switch (src->DataType)
        {
            case totemPrivateDataType_Channel:
                status = totemExecState_IntToString(state, (totemInt)(src->Value.GCObject->Channel->Count), dst);
                break;
                
            case totemPrivateDataType_Int:
                status = totemExecState_IntToString(state, src->Value.Int, dst);
                break;
                
            case totemPrivateDataType_Type:
                status = totemExecState_TypeToString(state, src->Value.DataType, dst);
                break;
                
            case totemPrivateDataType_Array:
                status = totemExecState_ArrayToString(state, src->Value.GCObject->Array, dst);
                break;
                
            case totemPrivateDataType_Float:
                status = totemExecState_FloatToString(state, src->Value.Float, dst);
                break;
                
            case totemPrivateDataType_InternedString:
            case totemPrivateDataType_MiniString:
                memcpy(dst, src, sizeof(totemRegister));
                break;
                
            case totemPrivateDataType_Coroutine:
                status = totemExecState_InstanceFunctionToString(state, src->Value.GCObject->Coroutine->InstanceFunction, dst);
                break;
                
            case totemPrivateDataType_NativeFunction:
                status = totemExecState_NativeFunctionToString(state, src->Value.NativeFunction, dst);
                break;
                
            case totemPrivateDataType_InstanceFunction:
                status = totemExecState_InstanceFunctionToString(state, src->Value.InstanceFunction, dst);
                break;
                
            case totemPrivateDataType_Userdata:
                status = totemExecState_EmptyString(state, dst);
        }
        
        if (status != totemExecStatus_Continue)
        {
            totem_CacheFree(strings, sizeof(totemRegister) * arr->NumRegisters);
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
        
        totalLen += totemRegister_GetStringLength(dst);
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

totemExecStatus totemExecState_StringToFunction(totemExecState *state, totemRegister *src, totemRegister *dst)
{
    totemString lookup;
    lookup.Value = totemRegister_GetStringValue(src);
    lookup.Length = totemRegister_GetStringLength(src);
    
    totemOperandXUnsigned addr = 0;
    if (totemRuntime_GetNativeFunctionAddress(state->Runtime, &lookup, &addr))
    {
        totemNativeFunction *nativeFunction = totemMemoryBuffer_Get(&state->Runtime->NativeFunctions, addr);
        if (!nativeFunction)
        {
            return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
        }
        
        totemExecState_AssignNewNativeFunction(state, dst, nativeFunction);
    }
    else
    {
        totemHashMapEntry *entry = totemHashMap_Find(&state->CallStack->CurrentInstance->Script->FunctionNameLookup, lookup.Value, lookup.Length);
        if (entry)
        {
            totemInstanceFunction *func = totemMemoryBuffer_Get(&state->CallStack->CurrentInstance->LocalFunctions, (size_t)entry->Value);
            if (!func)
            {
                return totemExecStatus_Break(totemExecStatus_InstanceFunctionNotFound);
            }
            
            totemExecState_AssignNewInstanceFunction(state, dst, func);
        }
        else
        {
            return totemExecStatus_Break(totemExecStatus_InstanceFunctionNotFound);
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

totemExecStatus totemExecState_CreateChannel(totemExecState *state, totemGCObject **gcOut)
{
    totemChannel *obj = totem_CacheMalloc(sizeof(totemChannel));
    if (!obj)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(obj, 0, sizeof(*obj));
    totemLock_Init(&obj->Lock);
    
    totemGCObject *gc = totemExecState_CreateGCObject(state, totemGCObjectType_Channel);
    if (!gc)
    {
        totem_CacheFree(obj, sizeof(*obj));
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    gc->Channel = obj;
    *gcOut = gc;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemInstanceFunction *function, totemGCObject **gcOut)
{
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
    gc->Coroutine->Function = function;
    gc->Coroutine->Type = totemFunctionType_Script;
    gc->Coroutine->CurrentInstance = state->CallStack->CurrentInstance;
    gc->Coroutine->Flags = totemFunctionCallFlag_IsCoroutine | totemFunctionCallFlag_FreeStack;
    gc->Coroutine->NumRegisters = function->Function->RegistersNeeded;
    gc->Coroutine->FrameStart = totem_CacheMalloc(sizeof(totemRegister) * function->Function->RegistersNeeded);
    
    if (!gc->Coroutine->FrameStart)
    {
        totemExecState_FreeFunctionCall(state, gc->Coroutine);
        totemExecState_DestroyGCObject(state, gc);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memset(gc->Coroutine->FrameStart, 0, sizeof(totemRegister) * function->Function->RegistersNeeded);
    
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

totemExecStatus totemExecState_PushToChannel(totemExecState *state, totemChannel *dst, totemRegister *src)
{
    totemExecStatus status = totemExecStatus_Continue;
    
    totemChannelNode *newNode = totem_CacheMalloc(sizeof(totemChannelNode));
    if (!newNode)
    {
        status = totemExecStatus_OutOfMemory;
    }
    else
    {
        memset(newNode, 0, sizeof(*newNode));
        status = totemExecState_Assign(state, &newNode->Value, src);
        if (status != totemExecStatus_Continue)
        {
            totem_CacheFree(newNode, sizeof(*newNode));
        }
        else
        {
            totemLock_Acquire(&dst->Lock);
            
            if (dst->Tail)
            {
                dst->Tail->Next = newNode;
            }
            
            dst->Tail = newNode;
            dst->Count++;
            
            if (!dst->Head)
            {
                dst->Head = newNode;
            }
            
            totemLock_Release(&dst->Lock);
        }
    }
    
    return status;
}

totemExecStatus totemExecState_PopFromChannel(totemExecState *state, totemChannel *src, totemRegister *dst)
{
    totemExecStatus status = totemExecStatus_Continue;
    totemChannelNode *node = NULL;
    
    totemLock_Acquire(&src->Lock);
    
    if (src->Head)
    {
        totemChannelNode *node = src->Head;
        
        src->Head = node->Next;
        src->Count--;
        
        if (!src->Head)
        {
            src->Tail = NULL;
        }
    }
    
    totemLock_Release(&src->Lock);
    
    if (node)
    {
        status = totemExecState_Assign(state, dst, &node->Value);
        TOTEM_REGISTER_DECIFGC(&node->Value);
        totem_CacheFree(node, sizeof(*node));
    }
    
    return status;
}

void totemExecState_DestroyChannel(totemExecState *state, totemChannel *obj)
{
    while (obj->Head)
    {
        totemChannelNode *node = obj->Head;
        obj->Head = node->Next;
        
        TOTEM_REGISTER_DECIFGC(&node->Value);
        totem_CacheFree(node, sizeof(totemChannelNode));
    }
    
    totemLock_Cleanup(&obj->Lock);
    totem_CacheFree(obj, sizeof(totemChannel));
}