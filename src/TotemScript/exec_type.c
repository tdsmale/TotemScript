//
//  exec_type.c
//  TotemScript
//
//  Created by Timothy Smale on 03/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

totemBool totemPublicDataType_IsGarbageCollected(totemPublicDataType type)
{
	return type >= totemPublicDataType_Array && type < totemPublicDataType_Max;
}

totemPublicDataType totemPrivateDataType_ToPublic(totemPrivateDataType type)
{
    switch (type)
    {
        case totemPrivateDataType_InternedString:
        case totemPrivateDataType_MiniString:
            return totemPublicDataType_String;
            
        case totemPrivateDataType_Float:
            return totemPublicDataType_Float;
            
        case totemPrivateDataType_Type:
            return totemPublicDataType_Type;
            
        case totemPrivateDataType_Int:
            return totemPublicDataType_Int;
            
        case totemPrivateDataType_NativeFunction:
        case totemPrivateDataType_InstanceFunction:
            return totemPublicDataType_Function;
            
        case totemPrivateDataType_Array:
            return totemPublicDataType_Array;
            
        case totemPrivateDataType_Coroutine:
            return totemPublicDataType_Coroutine;
            
        case totemPrivateDataType_Object:
            return totemPublicDataType_Object;
            
        case totemPrivateDataType_Userdata:
            return totemPublicDataType_Userdata;
            
        case totemPrivateDataType_Boolean:
            return totemPublicDataType_Boolean;
            
        case totemPrivateDataType_Null:
            return totemPublicDataType_Null;
    }
    
    return totemPublicDataType_Max;
}

totemExecStatus totemExecState_InternStringChar(totemExecState *state, totemRegister *src, totemInt index, totemRegister *dst)
{
    totemStringLength len = totemRegister_GetStringLength(src);
    
    if (index < 0 || index >= (totemInt)len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRuntimeStringValue val;
    totemRegister_GetStringValue(src, &val);
    
    totemString toIntern;
    toIntern.Length = 1;
    toIntern.Value = val.Value + index;
    
    return totemExecState_InternString(state, &toIntern, dst);
}

totemExecStatus totemExecState_ArraySet(totemExecState *state, totemRegister *arr, totemInt len, totemInt index, totemRegister *src)
{
    if (index < 0 || index >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecState_Assign(state, &arr[index], src);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayGet(totemExecState *state, totemRegister *arr, totemInt len, totemInt index, totemRegister *dst)
{
    if (index < 0 || index >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecState_Assign(state, dst, &arr[index]);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ObjectGet(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *dst)
{
    totemRegister actualKey;
    
    if (!totemRegister_IsString(key))
    {
        TOTEM_EXEC_CHECKRETURN(totemExecState_ToString(state, key, &actualKey));
    }
    else
    {
        actualKey = *key;
    }
    
    totemHashValue hash = totemRegister_GetStringHash(&actualKey);
    
    totemHashMapEntry *result = totemHashMap_FindPrecomputed(obj->Ptr.Object, &actualKey, sizeof(actualKey), hash);
    if (!result)
    {
        totemExecState_AssignNull(state, dst);
    }
    else
    {
        totemExecState_Assign(state, dst, obj->Data.Registers + result->Value);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ObjectSet(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *src)
{
    totemRegister actualKey;
    
    if (!totemRegister_IsString(key))
    {
        TOTEM_EXEC_CHECKRETURN(totemExecState_ToString(state, key, &actualKey));
    }
    else
    {
        actualKey = *key;
    }
    
    totemHashValue hash = totemRegister_GetStringHash(&actualKey);

	if (totemRegister_IsNull(src))
	{
		// just remove the entry
		totemHashMap_RemovePrecomputed(obj->Ptr.Object, &actualKey, sizeof(actualKey), hash);
		return totemExecStatus_Continue;
	}

    totemHashMapValue registerIndex = 0;
    totemHashMapEntry *entry = totemHashMap_FindPrecomputed(obj->Ptr.Object, &actualKey, sizeof(actualKey), hash);
    
    if (entry)
    {
        // already in hash map
        registerIndex = entry->Value;
    }
    else
    {
        totemBool expanded = totemBool_False;
        
        // we need to insert a new lookup value
        if (obj->Ptr.Object->FreeList)
        {
            // we can reuse the register index sitting on top of the freelist
            registerIndex = obj->Ptr.Object->FreeList->Value;
        }
        else
        {
            expanded = totemBool_True;
            
            // otherwise we need a new one
            registerIndex = obj->NumRegisters;
            
            if (!totemExecState_ExpandGCObject(state, obj))
            {
                return totemExecStatus_Break(totemExecStatus_OutOfMemory);
            }
        }
        
        if (!totemHashMap_InsertPrecomputedWithoutSearch(obj->Ptr.Object, &actualKey, sizeof(actualKey), registerIndex, hash))
        {
            return totemExecStatus_Break(totemExecStatus_OutOfMemory);
        }
        
        if (expanded)
        {
            state->GCNumBytes += sizeof(totemRegister) + sizeof(totemHashMapEntry);
        }
    }
    
    totemRegister *newReg = obj->Data.Registers + registerIndex;
    totemExecState_Assign(state, newReg, src);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut)
{
    totemStringLength len1 = totemRegister_GetStringLength(str1);
    totemStringLength len2 = totemRegister_GetStringLength(str2);
    
    totemRuntimeStringValue str1Val, str2Val;
    totemRegister_GetStringValue(str1, &str1Val);
    totemRegister_GetStringValue(str2, &str2Val);
    
    if (len1 + len2 <= TOTEM_MINISTRING_MAXLENGTH)
    {
        totemRuntimeStringValue val;
        val.Type = totemPrivateDataType_MiniString;
        
		TOTEM_ZERO(&val.Src.MiniString);
		memcpy(val.Src.MiniString, str1Val.Value, len1);
		memcpy(val.Src.MiniString + len1, str2Val.Value, len2);
        
        totemExecState_AssignNewString(state, strOut, &val);
        
        return totemExecStatus_Continue;
    }
    
    char *buffer = totemExecState_Alloc(state, len1 + len2);
    if (!buffer)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memcpy(buffer, str1Val.Value, len1);
    memcpy(buffer + len1, str2Val.Value, len2);
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = len1 + len2;
    
    totemExecStatus status = totemExecState_InternString(state, &toIntern, strOut);
    totem_CacheFree(buffer, len1 + len2);
    return status;
}

totemExecStatus totemExecState_ToString(totemExecState *state, totemRegister *src, totemRegister *dst)
{
    switch (totemRegister_GetType(src))
    {
		case totemPrivateDataType_Null:
		{
			totemString str;
			totemString_FromLiteral(&str, "null");
			return totemExecState_InternString(state, &str, dst);
		}

		case totemPrivateDataType_Boolean:
		{
			totemString str;
			if (totemRegister_IsNotZero(src))
			{
				totemString_FromLiteral(&str, "true");
			}
			else
			{
				totemString_FromLiteral(&str, "false");
			}

			return totemExecState_InternString(state, &str, dst);
		}

        case totemPrivateDataType_Int:
            return totemExecState_IntToString(state, totemRegister_GetInt(src), dst);
            
        case totemPrivateDataType_Type:
            return totemExecState_TypeToString(state, totemRegister_GetTypeValue(src), dst);
            
        case totemPrivateDataType_Array:
        {
            totemGCObject *gc = totemRegister_GetGCObject(src);
            return totemExecState_ArrayToString(state, gc->Data.Registers, gc->NumRegisters, dst);
        }
            
        case totemPrivateDataType_Float:
            return totemExecState_FloatToString(state, totemRegister_GetFloat(src), dst);
            
        case totemPrivateDataType_InternedString:
        case totemPrivateDataType_MiniString:
			totemExecState_Assign(state, dst, src);
            return totemExecStatus_Continue;
            
        case totemPrivateDataType_Coroutine:
        {
            totemGCObject *gc = totemRegister_GetGCObject(src);
			return totemExecState_InstanceFunctionToString(state, gc->Ptr.Coroutine->Function.Instance, dst);
        }
            
        case totemPrivateDataType_NativeFunction:
            return totemExecState_NativeFunctionToString(state, totemRegister_GetNativeFunction(src), dst);
            
        case totemPrivateDataType_InstanceFunction:
            return totemExecState_InstanceFunctionToString(state, totemRegister_GetInstanceFunction(src), dst);
            
        default:
            return totemExecState_EmptyString(state, dst);
    }
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
    int result = totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "%"TOTEM_INT_PRINTF, val);
    
    if (result < 0 || result >= TOTEM_ARRAY_SIZE(buffer))
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
    int result = totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "%.6g", val);
    if (result < 0 || result >= TOTEM_ARRAY_SIZE(buffer))
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
    totemRuntimeStringValue val;
    if (!totemRuntime_GetNativeFunctionName(state->Runtime, func->Address, &val))
    {
        return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
    }
    
    totemExecState_AssignNewString(state, strOut, &val);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_InstanceFunctionToString(totemExecState *state, totemInstanceFunction *func, totemRegister *strOut)
{
    totemRuntimeStringValue val;
    if (!totemScript_GetFunctionName(func->Instance->Ptr.Instance->Script, func->Function->Address, &val))
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
            
        case totemPublicDataType_Null:
            totemString_FromLiteral(&str, "null");
            break;
            
        case totemPublicDataType_Boolean:
            totemString_FromLiteral(&str, "boolean");
            break;
            
        default:
            return totemExecState_EmptyString(state, strOut);
    }
    
    return totemExecState_InternString(state, &str, strOut);
}

totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemRegister *arr, size_t numRegisters, totemRegister *strOut)
{
    if (numRegisters == 0)
    {
        return totemExecState_EmptyString(state, strOut);
    }
    
    totemRegister *strings = totemExecState_Alloc(state, sizeof(totemRegister) * numRegisters);
    if (!strings)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
	TOTEM_ZERO_ARRAY(strings, numRegisters);
    totemStringLength totalLen = 0;
    totemExecStatus status = totemExecStatus_Continue;
    
    for (size_t i = 0; i < numRegisters; i++)
    {
        totemRegister *src = &arr[i];
        totemRegister *dst = &strings[i];
        
        status = totemExecState_ToString(state, src, dst);
        
        if (status != totemExecStatus_Continue)
        {
            totem_CacheFree(strings, sizeof(totemRegister) * numRegisters);
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
        
        totalLen += totemRegister_GetStringLength(dst);
    }
    
    if (!totalLen)
    {
        totem_CacheFree(strings, sizeof(totemRegister) * numRegisters);
        return totemExecState_EmptyString(state, strOut);
    }
    
    char *buffer = totemExecState_Alloc(state, totalLen + 1);
    if (!buffer)
    {
        totem_CacheFree(strings, sizeof(totemRegister) * numRegisters);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    char *current = buffer;
    for (size_t i = 0; i < numRegisters; i++)
    {
        size_t len = totemRegister_GetStringLength(&strings[i]);
        
        totemRuntimeStringValue strVal;
        totemRegister_GetStringValue(&strings[i], &strVal);
        
        memcpy(current, strVal.Value, len);
        current += len;
    }
    
    totem_CacheFree(strings, sizeof(totemRegister) * numRegisters);
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = totalLen;
    
    status = totemExecState_InternString(state, &toIntern, strOut);
    totem_CacheFree(buffer, totalLen + 1);
    
    return status;
}

totemExecStatus totemExecState_ConcatArrays(totemExecState *state, totemRegister *src1, totemRegister *src2, totemRegister *dst)
{
    totemGCObject *gc = NULL;
    totemGCObject *src1Gc = totemRegister_GetGCObject(src1);
    totemGCObject *src2Gc = totemRegister_GetGCObject(src2);

	if (src1Gc->NumRegisters + src2Gc->NumRegisters >= TOTEM_INT_MAX)
	{
		return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
	}
    
	TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, (totemInt)(src1Gc->NumRegisters + src2Gc->NumRegisters), &gc));
    
    totemRegister *newRegs = gc->Data.Registers;
    totemRegister *source1Regs = src1Gc->Data.Registers;
    
    for (totemInt i = 0; i < src1Gc->NumRegisters; i++)
    {
        totemExecState_Assign(state, &newRegs[i], &source1Regs[i]);
    }
    
    totemRegister *source2Regs = src2Gc->Data.Registers;
	for (totemInt i = src1Gc->NumRegisters; i < gc->NumRegisters; i++)
    {
        totemExecState_Assign(state, &newRegs[i], &source2Regs[i - src1Gc->NumRegisters]);
    }
    
    totemExecState_AssignNewArray(state, dst, gc);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_StringToFunction(totemExecState *state, totemRegister *src, totemRegister *dst)
{
    totemRuntimeStringValue strVal;
    totemRegister_GetStringValue(src, &strVal);
    
    totemString lookup;
    lookup.Value = strVal.Value;
    lookup.Length = totemRegister_GetStringLength(src);
    
    totemOperandXUnsigned addr = 0;
    if (totemRuntime_GetNativeFunctionAddress(state->Runtime, &lookup, &addr))
    {
        totemNativeFunction *nativeFunction = totemNativeFunctionBuffer_At(&state->Runtime->NativeFunctions, addr);
        if (!nativeFunction)
        {
            return totemExecStatus_Break(totemExecStatus_NativeFunctionNotFound);
        }
        
        totemExecState_AssignNewNativeFunction(state, dst, nativeFunction);
    }
    else
    {
        totemHashMapEntry *entry = totemHashMap_Find(&state->CallStack->Instance->Ptr.Instance->Script->FunctionNameLookup, lookup.Value, lookup.Length);
        if (entry)
        {
			totemInstanceFunction *func = totemInstanceFunctionBuffer_At(&state->CallStack->Instance->Ptr.Instance->LocalFunctions, (size_t)entry->Value);
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
