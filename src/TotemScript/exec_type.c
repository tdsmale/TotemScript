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
            
        case totemPrivateDataType_Instance:
            return totemPublicDataType_Instance;
    }
    
    return totemPublicDataType_Max;
}

totemStringLength totemMiniString_GetLength(const char *c)
{
    for(size_t i = 0; i < TOTEM_MINISTRING_MAXLENGTH; i++)
    {
        if(!c[i])
        {
            return i;
        }
    }
    
    return TOTEM_MINISTRING_MAXLENGTH;
}

totemHashValue totemMiniString_GetHash(const char* s)
{
    totemHashValue hash = totem_HashBegin();
    for(size_t i = 0; i < TOTEM_MINISTRING_MAXLENGTH; i++)
    {
        char c = s[i];
        if(!c)
        {
            break;
        }
        
        hash = totem_HashChar(c, hash);
    }
    
    return hash;
}

totemBool totemMiniString_Compare(const char *a, const char *b)
{
    for(size_t i = 0; i < TOTEM_MINISTRING_MAXLENGTH; i++)
    {
        char ac = a[i];
        char bc = b[i];
        if((ac && !ac) || (bc && !bc))
        {
            return totemBool_False;
        }
        
        if(ac != bc)
        {
            return totemBool_False;
        }
    }
    
    return totemBool_True;
}

void totemRuntimeStringValue_FromInternedString(totemInternedStringHeader* str, totemRuntimeStringValue* val)
{
    val->Type = totemPrivateDataType_InternedString;
    val->Src.InternedString = str;
    val->Length = str->Length;
    val->Hash = str->Hash;
}

void totemRuntimeStringValue_SetMiniString(const char *str, uint8_t len, totemRuntimeStringValue *val)
{
    TOTEM_ASSERT(len <= TOTEM_MINISTRING_MAXLENGTH);
    val->Type = totemPrivateDataType_MiniString;
    memcpy(val->Src.MiniString, str, len);
    memset(val->Src.MiniString + len, 0, TOTEM_MINISTRING_MAXLENGTH - len);
    val->Length = len;
    val->Hash = totemMiniString_GetHash(str);
}

void totemRuntimeStringValue_FromMiniStringRegister(totemRegister *from, totemRuntimeStringValue* val)
{
    const char *miniString = totemRegister_GetMiniStringPtr(from);
    totemRuntimeStringValue_SetMiniString(miniString, (uint8_t)totemMiniString_GetLength(miniString), val);
}

void totemRuntimeStringValue_Print(totemRuntimeStringValue *val, totemLog *dst)
{
    const char *str = NULL;
    size_t len = 0;
    totemHashValue hash = 0;
    
    switch(val->Type)
    {
        case totemPrivateDataType_MiniString:
            str = val->Src.MiniString;
            len = totemMiniString_GetLength(val->Src.MiniString);
            hash = totemMiniString_GetHash(val->Src.MiniString);
            break;
            
        case totemPrivateDataType_InternedString:
            str = val->Src.InternedString->Data;
            len = val->Src.InternedString->Length;
            hash = val->Src.InternedString->Hash;
            break;
            
        default:
            TOTEM_ASSERT_V(totemBool_False, "unexpected data type %i in runtime string value", val->Type);
            return;
    }
    
    if(str)
    {
        totemLog_Write(dst, "\"%.*s\" (hash: %"TOTEM_HASH_PRINTF")", len, str, hash);
    }
}

const char *totemRuntimeStringValue_GetVal(totemRuntimeStringValue *val)
{
    switch(val->Type)
    {
        case totemPrivateDataType_MiniString:
            return val->Src.MiniString;
            
        case totemPrivateDataType_InternedString:
            return val->Src.InternedString->Data;
            break;
            
        default:
            TOTEM_ASSERT_V(totemBool_False, "unexpected data type %i in runtime string value", val->Type);
            return NULL;
    }
}

totemBool totemRuntimeStringValue_Compare(totemRuntimeStringValue *val, totemRuntimeStringValue *with)
{
    if(val->Type != with->Type)
    {
        return totemBool_False;
    }
    
    switch(val->Type)
    {
        case totemPrivateDataType_MiniString:
            return totemMiniString_Compare(val->Src.MiniString, with->Src.MiniString);
            break;
            
        case totemPrivateDataType_InternedString:
            return val->Src.InternedString == with->Src.InternedString;
            
        default:
            TOTEM_ASSERT_V(totemBool_False, "unexpected data type %i in runtime string value", val->Type);
            return totemBool_False;
    }
}

totemBool totemRuntimeStringValue_CompareWithString(totemRuntimeStringValue *val, totemString *with)
{
    if(val->Length != with->Length)
    {
        return totemBool_False;
    }
    
    const char *valPtr = totemRuntimeStringValue_GetVal(val);
    return strncmp(valPtr, with->Value, val->Length) == 0;
}

totemExecStatus totemExecState_InternStringChar(totemExecState *state, totemRegister *src, uint32_t index, totemRuntimeStringValue *dst)
{
    totemStringLength len = totemRegister_GetStringLength(src);
    
    if (index >= (uint32_t)len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRuntimeStringValue val;
    totemRegister_GetString(src, &val);
    
    totemString toIntern;
    toIntern.Length = 1;
    toIntern.Value = totemRuntimeStringValue_GetVal(&val) + index;
    
    return totemExecState_InternString(state, &toIntern, dst);
}

totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRuntimeStringValue *strOut)
{
    totemRuntimeStringValue str1Val, str2Val;
    totemRegister_GetString(str1, &str1Val);
    totemRegister_GetString(str2, &str2Val);
    const char *str1Ptr = totemRuntimeStringValue_GetVal(&str1Val);
    const char *str2Ptr = totemRuntimeStringValue_GetVal(&str2Val);
    totemStringLength len1 = str1Val.Length;
    totemStringLength len2 = str2Val.Length;
    
    if (len1 + len2 <= TOTEM_MINISTRING_MAXLENGTH)
    {
        char buffer[TOTEM_MINISTRING_MAXLENGTH];
        TOTEM_ZERO(buffer);
        
        memcpy(buffer, str1Ptr, len1);
        memcpy(buffer + len1, str2Ptr, len2);
        
        totemRuntimeStringValue_SetMiniString(buffer, (uint8_t)(len1 + len2), strOut);
        return totemExecStatus_Continue;
    }
    
    char *buffer = totemExecState_Alloc(state, len1 + len2);
    if (!buffer)
    {
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    memcpy(buffer, str1Ptr, len1);
    memcpy(buffer + len1, str2Ptr, len2);
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = len1 + len2;
    
    totemExecStatus status = totemExecState_InternString(state, &toIntern, strOut);
    TOTEM_FREE(buffer, len1 + len2);
    return status;
}

totemExecStatus totemExecState_ToString(totemExecState *state, totemRegister *src, totemRuntimeStringValue *dst)
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
			totemRegister_GetString(src, dst);
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

totemExecStatus totemExecState_EmptyString(totemExecState *state, totemRuntimeStringValue *strOut)
{
    totemString empty;
    empty.Value = "";
    empty.Length = 0;
    
    return totemExecState_InternString(state, &empty, strOut);
}

totemExecStatus totemExecState_IntToString(totemExecState *state, totemInt val, totemRuntimeStringValue *strOut)
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

totemExecStatus totemExecState_FloatToString(totemExecState *state, totemFloat val, totemRuntimeStringValue *strOut)
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

totemExecStatus totemExecState_NativeFunctionToString(totemExecState *state, totemNativeFunction *func, totemRuntimeStringValue *strOut)
{
    totemRuntimeStringValue* val = totemRuntimeStringValueDynBuffer_At(&state->Runtime->NativeFunctionNames, func->Address);
    if (!val)
    {
        return totemExecStatus_Break(totemExecStatus_FunctionNotFound);
    }
    
    *strOut = *val;
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_InstanceFunctionToString(totemExecState *state, totemInstanceFunction *func, totemRuntimeStringValue *strOut)
{
    if (!totemScript_GetFunctionName(func->Instance->Ptr.Instance->Script, func->Function->Address, strOut))
    {
        return totemExecStatus_Break(totemExecStatus_FunctionNotFound);
    }
 
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_TypeToString(totemExecState *state, totemPublicDataType type, totemRuntimeStringValue *strOut)
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
            totemString_FromLiteral(&str, "map");
            break;
            
        case totemPublicDataType_Null:
            totemString_FromLiteral(&str, "null");
            break;
            
        case totemPublicDataType_Boolean:
            totemString_FromLiteral(&str, "Bool");
            break;
            
        default:
            return totemExecState_EmptyString(state, strOut);
    }
    
    return totemExecState_InternString(state, &str, strOut);
}

totemExecStatus totemExecState_ArrayToString(totemExecState *state, totemRegister *arr, size_t numRegisters, totemRuntimeStringValue *strOut)
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
    
	TOTEM_ZERO_N(strings, numRegisters);
    totemStringLength totalLen = 0;
    totemExecStatus status = totemExecStatus_Continue;
    
    for (size_t i = 0; i < numRegisters; i++)
    {
        totemRegister *src = &arr[i];
        totemRegister *dst = &strings[i];
        
        totemRuntimeStringValue val;
        status = totemExecState_ToString(state, src, &val);
        
        if (status != totemExecStatus_Continue)
        {
            TOTEM_FREE(strings, sizeof(totemRegister) * numRegisters);
            return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
        }
        
        totalLen += val.Length;
        totemRegister_SetString(dst, &val);
    }

	//totalLen += strlen(", ") * (numRegisters - 1);
    
    if (!totalLen)
    {
        TOTEM_FREE(strings, sizeof(totemRegister) * numRegisters);
        return totemExecState_EmptyString(state, strOut);
    }
    
    char *buffer = totemExecState_Alloc(state, totalLen + 1);
    if (!buffer)
    {
        TOTEM_FREE(strings, sizeof(totemRegister) * numRegisters);
        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
    }
    
    char *current = buffer;
    for (size_t i = 0; i < numRegisters; i++)
    {
        totemRuntimeStringValue strVal;
        totemRegister_GetString(&strings[i], &strVal);
        const char *strPtr = totemRuntimeStringValue_GetVal(&strVal);
        
        memcpy(current, strPtr, strVal.Length);
        current += strVal.Length;

        /*
		if (i != numRegisters - 1)
		{
			memcpy(current, ", ", strlen(", "));
			current += strlen(", ");
		}*/
    }
    
    TOTEM_FREE(strings, sizeof(totemRegister) * numRegisters);
    
    totemString toIntern;
    toIntern.Value = buffer;
    toIntern.Length = totalLen;
    
    status = totemExecState_InternString(state, &toIntern, strOut);
    TOTEM_FREE(buffer, totalLen + 1);
    
    return status;
}

totemExecStatus totemExecState_ObjectToString(totemExecState *state, totemGCObject *gc, totemRuntimeStringValue *strOut)
{
	if (gc->NumRegisters == 0)
	{
		return totemExecState_EmptyString(state, strOut);
	}

	totemRegister *strings = totemExecState_Alloc(state, sizeof(totemRegister) * gc->NumRegisters);
	if (!strings)
	{
		return totemExecStatus_Break(totemExecStatus_OutOfMemory);
	}

	TOTEM_ZERO_N(strings, gc->NumRegisters);
	totemStringLength totalLen = 0;
	totemExecStatus status = totemExecStatus_Continue;

	for (size_t i = 0; i < gc->NumRegisters; i++)
	{
		totemRegister *src = &gc->Data.Registers[i];
		totemRegister *dst = &strings[i];

        totemRuntimeStringValue val;
		status = totemExecState_ToString(state, src, &val);

		if (status != totemExecStatus_Continue)
		{
            TOTEM_FREE(strings, sizeof(totemRegister) * gc->NumRegisters);
			return totemExecStatus_Break(totemExecStatus_UnrecognisedOperation);
		}

        totalLen += val.Length;
        totemRegister_SetString(dst, &val);
	}

	totalLen += strlen(", ") * (gc->NumRegisters - 1);
	totalLen += strlen(": ") * (gc->NumRegisters - 1);

	for (size_t i = 0; i < gc->Ptr.Object->NumBuckets; i++)
	{
		for (totemRuntimeStringMapEntry *entry = (totemRuntimeStringMapEntry*)gc->Ptr.Object->Buckets[i]; entry; entry = (totemRuntimeStringMapEntry*)entry->Entry.Next)
		{
            totalLen += entry->Data.Key.Length;
		}
	}

	if (!totalLen)
	{
        TOTEM_FREE(strings, sizeof(totemRegister) * gc->NumRegisters);
		return totemExecState_EmptyString(state, strOut);
	}

	char *buffer = totemExecState_Alloc(state, totalLen + 1);
	if (!buffer)
	{
        TOTEM_FREE(strings, sizeof(totemRegister) * gc->NumRegisters);
		return totemExecStatus_Break(totemExecStatus_OutOfMemory);
	}

	char *current = buffer;
	for (size_t i = 0; i < gc->Ptr.Object->NumBuckets; i++)
	{
		for (totemRuntimeStringMapEntry *entry = (totemRuntimeStringMapEntry*)gc->Ptr.Object->Buckets[i]; entry; entry = (totemRuntimeStringMapEntry*)entry->Entry.Next)
		{
			totemRuntimeStringValue strVal;
			totemRegister_GetString(&strings[entry->Data.Value], &strVal);
            const char *strValPtr = totemRuntimeStringValue_GetVal(&strVal);
            const char *entryValPtr = totemRuntimeStringValue_GetVal(&entry->Data.Key);
            
			memcpy(current, entryValPtr, entry->Data.Key.Length);
            current += entry->Data.Key.Length;

			memcpy(current, ": ", strlen(": "));
			current += strlen(": ");

			memcpy(current, strValPtr, strVal.Length);
			current += strVal.Length;

			if (i != gc->NumRegisters - 1)
			{
				memcpy(current, ", ", strlen(", "));
				current += strlen(", ");
			}
		}
	}

    TOTEM_FREE(strings, sizeof(totemRegister) * gc->NumRegisters);

	totemString toIntern;
	toIntern.Value = buffer;
	toIntern.Length = totalLen;

	status = totemExecState_InternString(state, &toIntern, strOut);
    TOTEM_FREE(buffer, totalLen + 1);

	return status;
}

totemExecStatus totemExecState_ConcatArrays(totemExecState *state, totemRegister *src1, totemRegister *src2, totemRegister *dst)
{
    totemGCObject *gc = NULL;
    totemGCObject *src1Gc = totemRegister_GetGCObject(src1);
    totemGCObject *src2Gc = totemRegister_GetGCObject(src2);

	if (TOTEM_INT_MAX - src1Gc->NumRegisters < src2Gc->NumRegisters)
	{
		return totemExecStatus_Break(totemExecStatus_RegisterOverflow);
	}

	TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, (src1Gc->NumRegisters + src2Gc->NumRegisters), &gc));
    
    totemRegister *newRegs = gc->Data.Registers;
    totemRegister *source1Regs = src1Gc->Data.Registers;
    
    for (uint32_t i = 0; i < src1Gc->NumRegisters; i++)
    {
        totemRegister_Set(&newRegs[i], &source1Regs[i]);
    }
    
    totemRegister *source2Regs = src2Gc->Data.Registers;
	for (uint32_t i = src1Gc->NumRegisters; i < gc->NumRegisters; i++)
    {
        totemRegister_Set(&newRegs[i], &source2Regs[i - src1Gc->NumRegisters]);
    }
    
    totemRegister_SetArray(dst, gc);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_StringToFunction(totemExecState *state, totemRuntimeStringValue *src, totemRegister *dst)
{
    totemRuntimeStringMapKey key;
    key.Key = *src;
    
    if (totemRuntimeStringMap_Get(&state->Runtime->NativeFunctionsLookup, &key))
    {
        totemNativeFunction *nativeFunction = totemNativeFunctionDynBuffer_At(&state->Runtime->NativeFunctions, key.Value);
        if (!nativeFunction)
        {
            return totemExecStatus_Break(totemExecStatus_FunctionNotFound);
        }
        
        totemRegister_SetNativeFunction(dst, nativeFunction);
    }
    else
    {
        if (totemRuntimeStringMap_Get(&state->CallStack->Instance->Ptr.Instance->Script->FunctionNameLookup, &key))
        {
			totemInstanceFunction *func = &state->CallStack->Instance->Ptr.Instance->LocalFunctions[key.Value];
            if (!func)
            {
                return totemExecStatus_Break(totemExecStatus_FunctionNotFound);
            }
            
            totemRegister_SetInstanceFunction(dst, func);
        }
        else
        {
            return totemExecStatus_Break(totemExecStatus_FunctionNotFound);
        }
    }
    
    return totemExecStatus_Continue;
}
