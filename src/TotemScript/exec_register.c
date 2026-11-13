//
//  exec_register.c
//  TotemScript
//
//  Created by Timothy Smale on 27/08/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <TotemScript/log.h>
#include <string.h>

static void totemRegister_SetMiniStringValue(char* buffer, const char* value, uint8_t valueLenIn)
{
    // buffer is assumed to be TOTEM_MINISTRING_MAXLENGTH + 1
    if (valueLenIn > TOTEM_MINISTRING_MAXLENGTH)
    {
        valueLenIn = TOTEM_MINISTRING_MAXLENGTH;
    }

    memcpy(buffer, value, valueLenIn);
    buffer[valueLenIn] = 0;
    uint8_t left = TOTEM_MINISTRING_MAXLENGTH - valueLenIn;
    memset(buffer + valueLenIn + 1, 0, left);
}

#if TOTEM_VMOPT_NANBOXING

#define TOTEM_FLOAT_QUIET_NAN_MASK TOTEM_BITMASK(uint64_t, 51, 12)
#define TOTEM_FLOAT_MANTISSA_MASK TOTEM_BITMASK(uint64_t, 0, 48)
#define TOTEM_REGISTER_TYPE_MASK TOTEM_BITMASK(uint64_t, 48, 16)
#define TOTEM_REGISTER_NAN_VALUE(type, val) ((((uint64_t)(type)) << 48) | ((val) & TOTEM_BITMASK(uint64_t, 0, 48)))

static void totemRegister_AssertValueOutput(totemLog *dst, totemRegister *reg)
{
    totem_PrintBits(dst, TOTEM_BITCAST(uint64_t, *reg), 64, 0);
    totemLog_Write(dst, "\n");
    totem_PrintBits(dst, totemPrivateDataType_Float, 64, 0);
    totemLog_Write(dst, " float\n");
    totem_PrintBits(dst, totemPrivateDataType_Int, 64, 0);
    totemLog_Write(dst, " int\n");
    totem_PrintBits(dst, totemPrivateDataType_Array, 64, 0);
    totemLog_Write(dst, " array\n");
    totem_PrintBits(dst, totemPrivateDataType_Boolean, 64, 0);
    totemLog_Write(dst, " bool\n");
    totem_PrintBits(dst, totemPrivateDataType_Coroutine, 64, 0);
    totemLog_Write(dst, " coroutine\n");
    totem_PrintBits(dst, totemPrivateDataType_InstanceFunction, 64, 0);
    totemLog_Write(dst, " instance func\n");
    totem_PrintBits(dst, totemPrivateDataType_InternedString, 64, 0);
    totemLog_Write(dst, " interned string\n");
    totem_PrintBits(dst, totemPrivateDataType_MiniString, 64, 0);
    totemLog_Write(dst, " mini string\n");
    totem_PrintBits(dst, totemPrivateDataType_NativeFunction, 64, 0);
    totemLog_Write(dst, " native func\n");
    totem_PrintBits(dst, totemPrivateDataType_Null, 64, 0);
    totemLog_Write(dst, " null\n");
    totem_PrintBits(dst, totemPrivateDataType_Object, 64, 0);
    totemLog_Write(dst, " object\n");
    totem_PrintBits(dst, totemPrivateDataType_Type, 64, 0);
    totemLog_Write(dst, " type\n");
    totem_PrintBits(dst, totemPrivateDataType_Userdata, 64, 0);
    totemLog_Write(dst, " userdata\n");
}

void totemRegister_AssertValue(totemRegister* reg)
{
    totemPrivateDataType type = totemRegister_GetType(reg);

    totemBool goodType = (type == totemPrivateDataType_Array ||
        type == totemPrivateDataType_Boolean ||
        type == totemPrivateDataType_Coroutine ||
        type == totemPrivateDataType_Float ||
        type == totemPrivateDataType_InstanceFunction ||
        type == totemPrivateDataType_Int ||
        type == totemPrivateDataType_InternedString ||
        type == totemPrivateDataType_MiniString ||
        type == totemPrivateDataType_NativeFunction ||
        type == totemPrivateDataType_Null ||
        type == totemPrivateDataType_Object ||
        type == totemPrivateDataType_Type ||
        type == totemPrivateDataType_Userdata);

    if (!goodType)
    {
        

        TOTEM_ASSERT(totemBool_False);
    }
}

totemPrivateDataType totemRegister_GetType(totemRegister* reg)
{
    uint16_t type = reg->AsTagVal.Tag;
    uint16_t isNaN = TOTEM_HASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK);

    // if is not NaN, type is floating point - ensure all bits are set to 0
    return type & (-isNaN);
}

uint64_t totemRegister_GetMantissaValue(totemRegister* reg)
{
    return TOTEM_GETBITS(reg->AsBits, TOTEM_FLOAT_MANTISSA_MASK);
}

totemInt totemRegister_GetInt(totemRegister* reg)
{
    return reg->AsInt.LSW;
}

totemFloat totemRegister_GetFloat(totemRegister* reg)
{
    return reg->AsFloat;
}

totemInternedStringHeader* totemRegister_GetInternedString(totemRegister* reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemInternedStringHeader*, val);
}

struct totemGCObject* totemRegister_GetGCObject(totemRegister* reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(struct totemGCObject*, val);
}

totemBool totemRegister_IsZero(totemRegister* reg)
{
    return
        reg->AsBits == 0 ||
        reg->AsBits == totemPrivateDataType_Null ||
        (TOTEM_HASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK) && !TOTEM_HASANYBITS(reg->AsBits, TOTEM_FLOAT_MANTISSA_MASK));
}

totemBool totemRegister_IsNotZero(totemRegister* reg)
{
    return
        reg->AsBits != 0 &&
        reg->AsBits != totemPrivateDataType_Null &&
        (!TOTEM_HASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK) || TOTEM_HASANYBITS(reg->AsBits, TOTEM_FLOAT_MANTISSA_MASK));
}

totemBool totemRegister_IsType(totemRegister* reg, totemPrivateDataType type)
{
    return reg->AsTagVal.Tag == type;
}

totemBool totemRegister_IsFloat(totemRegister* reg)
{
    return TOTEM_NHASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK);
}

totemBool totemRegister_IsGarbageCollected(totemRegister* reg)
{
    return reg->AsTagVal.Tag >= totemPrivateDataType_Array;
}

totemBool totemRegister_Equals(totemRegister* a, totemRegister* b)
{
    return a->AsBits == b->AsBits;
}

totemNativeFunction* totemRegister_GetNativeFunction(totemRegister* reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemNativeFunction*, val);
}

totemInstanceFunction* totemRegister_GetInstanceFunction(totemRegister* reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemInstanceFunction*, val);
}

totemPublicDataType totemRegister_GetTypeValue(totemRegister* reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemPublicDataType, val);
}

void totemRegister_GetMiniString(totemRegister* reg, char* valOut)
{
    memcpy(valOut, reg->AsMiniString.MiniString, TOTEM_MINISTRING_MAXLENGTH + 1);
}

char* totemRegister_GetMiniStringPtr(totemRegister* reg)
{
    return reg->AsMiniString.MiniString;
}

void totemRegister_InitList(totemRegister* reg, size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        totemRegister_SetNull(&reg[i]);
    }
}

void totemRegister_SetNull(totemRegister* dst)
{
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Null, 0);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Null);
    TOTEM_ASSERT(totemRegister_IsNull(dst));
}

void totemRegister_SetInt(totemRegister* dst, totemInt val)
{
    dst->AsInt.MSW = totemPrivateDataType_Int << 16;
    dst->AsInt.LSW = val;

    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Int);
    TOTEM_ASSERT(totemRegister_GetInt(dst) == val);
    TOTEM_ASSERT(totemRegister_IsInt(dst));
}

void totemRegister_SetFloat(totemRegister* dst, totemFloat val)
{
    dst->AsFloat = val;
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Float);
    TOTEM_ASSERT(totemRegister_GetFloat(dst) == val);
    TOTEM_ASSERT(totemRegister_IsFloat(dst));
}

void totemRegister_SetTypeValue(totemRegister* dst, totemPublicDataType val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Type, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Type);
    TOTEM_ASSERT(totemRegister_GetTypeValue(dst) == val);
    TOTEM_ASSERT(totemRegister_IsTypeValue(dst));
}

void totemRegister_SetNativeFunction(totemRegister* dst, totemNativeFunction* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_NativeFunction, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_NativeFunction);
    TOTEM_ASSERT(totemRegister_GetNativeFunction(dst) == val);
    TOTEM_ASSERT(totemRegister_IsNativeFunction(dst));
}

void totemRegister_SetInstanceFunction(totemRegister* dst, totemInstanceFunction* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_InstanceFunction, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_InstanceFunction);
    TOTEM_ASSERT(totemRegister_GetInstanceFunction(dst) == val);
    TOTEM_ASSERT(totemRegister_IsInstanceFunction(dst));
}

void totemRegister_SetArray(totemRegister* dst, totemGCObject* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Array, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Array);
    TOTEM_ASSERT(totemRegister_GetGCObject(dst) == val);
    TOTEM_ASSERT(totemRegister_IsArray(dst));
}

void totemRegister_SetObject(totemRegister* dst, totemGCObject* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Object, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Object);
    TOTEM_ASSERT(totemRegister_GetGCObject(dst) == val);
    TOTEM_ASSERT(totemRegister_IsObject(dst));
}

void totemRegister_SetCoroutine(totemRegister* dst, totemGCObject* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Coroutine, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Coroutine);
    TOTEM_ASSERT(totemRegister_GetGCObject(dst) == val);
    TOTEM_ASSERT(totemRegister_IsCoroutine(dst));
}

void totemRegister_SetUserdata(totemRegister* dst, totemGCObject* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Userdata, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Userdata);
    TOTEM_ASSERT(totemRegister_GetGCObject(dst) == val);
    TOTEM_ASSERT(totemRegister_IsUserdata(dst));
}

void totemRegister_SetInstance(totemRegister* dst, totemGCObject* val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Instance, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Instance);
    TOTEM_ASSERT(totemRegister_GetGCObject(dst) == val);
    TOTEM_ASSERT(totemRegister_IsInstance(dst));
}

void totemRegister_SetBool(totemRegister* dst, totemBool val)
{
    uint64_t b = val != totemBool_False;
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Boolean, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_Boolean);
    TOTEM_ASSERT(totemRegister_IsNotZero(dst) == val);
    TOTEM_ASSERT(totemRegister_IsBoolean(dst));
}

void totemRegister_SetInternedString(totemRegister* dst, totemInternedStringHeader* hdr)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, hdr);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_InternedString, b);
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_InternedString);
    TOTEM_ASSERT(totemRegister_GetInternedString(dst) == hdr);
    TOTEM_ASSERT(totemRegister_IsInternedString(dst));
}

void totemRegister_SetMiniString(totemRegister* dst, char* val, uint8_t len)
{
    dst->AsTagVal.Tag = totemPrivateDataType_MiniString;
    totemRegister_SetMiniStringValue(dst->AsMiniString.MiniString, val, len);

#if TOTEM_DEBUG
    char test[sizeof(dst->AsMiniString.MiniString)];
    TOTEM_ASSERT(totemRegister_GetType(dst) == totemPrivateDataType_MiniString);
    totemRegister_GetMiniString(dst, test);
    TOTEM_ASSERT(memcmp(dst->AsMiniString.MiniString, test, sizeof(test)) == 0);
    TOTEM_ASSERT(memcmp(val, test, len) == 0);
    TOTEM_ASSERT(totemRegister_IsMiniString(dst));
#endif

}

#else

totemPrivateDataType totemRegister_GetType(totemRegister *reg)
{
    return reg->DataType;
}

totemInt totemRegister_GetInt(totemRegister *reg)
{
    return reg->Value.Int;
}

totemFloat totemRegister_GetFloat(totemRegister * reg)
{
    return reg->Value.Float;
}

totemInternedStringHeader *totemRegister_GetInternedString(totemRegister *reg)
{
    return reg->Value.InternedString;
}

void totemRegister_GetMiniString(totemRegister *dst, char *val)
{
    memcpy(val, dst->Value.MiniString, TOTEM_MINISTRING_MAXLENGTH + 1);
}

struct totemGCObject *totemRegister_GetGCObject( totemRegister *reg)
{
    return reg->Value.GCObject;
}

totemBool totemRegister_IsZero(totemRegister *reg)
{
    return reg->Value.Int == 0;
}

totemBool totemRegister_IsNotZero(totemRegister *reg)
{
    return reg->Value.Int != 0;
}

totemBool totemRegister_IsGarbageCollected(totemRegister *reg)
{
    totemPrivateDataType type = totemRegister_GetType(reg);
    return type >= totemPrivateDataType_Array && type <= totemPrivateDataType_Userdata;
}

totemBool totemRegister_IsType(totemRegister *reg, totemPrivateDataType type)
{
    return reg->DataType == type;
}

totemBool totemRegister_IsFloat(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Float);
}

totemBool totemRegister_Equals(totemRegister *a, totemRegister *b)
{
    return a->DataType == b->DataType && a->Value.Int == b->Value.Int;
}

totemNativeFunction *totemRegister_GetNativeFunction(totemRegister *reg)
{
    return reg->Value.NativeFunction;
}

totemInstanceFunction *totemRegister_GetInstanceFunction(totemRegister *reg)
{
    return reg->Value.InstanceFunction;
}

totemPublicDataType totemRegister_GetTypeValue(totemRegister *reg)
{
    return reg->Value.DataType;
}

void totemRegister_InitList(totemRegister *reg, size_t num)
{
	TOTEM_ZERO_N(reg, num);
}

void totemRegister_SetNull(totemRegister *dst)
{
    dst->DataType = totemPrivateDataType_Null;
    dst->Value.Int = 0;
}

void totemRegister_SetInt(totemRegister *dst, totemInt val)
{
    dst->DataType = totemPrivateDataType_Int;
    dst->Value.Int = val;
}

void totemRegister_SetFloat(totemRegister *dst, totemFloat val)
{
    dst->DataType = totemPrivateDataType_Float;
    dst->Value.Float = val;
}

void totemRegister_SetTypeValue(totemRegister *dst, totemPublicDataType val)
{
    dst->DataType = totemPrivateDataType_Type;
    dst->Value.DataType = val;
}

void totemRegister_SetNativeFunction(totemRegister *dst, totemNativeFunction *val)
{
    dst->DataType = totemPrivateDataType_NativeFunction;
    dst->Value.NativeFunction = val;
}

void totemRegister_SetInstanceFunction(totemRegister *dst, totemInstanceFunction *val)
{
    dst->DataType = totemPrivateDataType_InstanceFunction;
    dst->Value.InstanceFunction = val;
}

void totemRegister_SetArray(totemRegister *dst, totemGCObject *val)
{
    dst->DataType = totemPrivateDataType_Array;
    dst->Value.GCObject = val;
}

void totemRegister_SetObject(totemRegister *dst, totemGCObject *val)
{
    dst->DataType = totemPrivateDataType_Object;
    dst->Value.GCObject = val;
}

void totemRegister_SetCoroutine(totemRegister *dst, totemGCObject *val)
{
    dst->DataType = totemPrivateDataType_Coroutine;
    dst->Value.GCObject = val;
}

void totemRegister_SetUserdata(totemRegister *dst, totemGCObject *val)
{
    dst->DataType = totemPrivateDataType_Userdata;
    dst->Value.GCObject = val;
}

void totemRegister_SetInstance(totemRegister *dst, totemGCObject *val)
{
    dst->DataType = totemPrivateDataType_Instance;
    dst->Value.GCObject = val;
}

void totemRegister_SetBool(totemRegister *dst, totemBool val)
{
    dst->DataType = totemPrivateDataType_Boolean;
    dst->Value.Int = val == totemBool_True;
}

void totemRegister_SetInternedString(totemRegister *dst, totemInternedStringHeader *val)
{
    dst->DataType = totemPrivateDataType_InternedString;
    dst->Value.InternedString = val;
}

void totemRegister_SetMiniString(totemRegister *dst, char *val, uint8_t len)
{
    dst->DataType = totemPrivateDataType_MiniString;
    totemRegister_SetMiniStringValue(dst->Value.MiniString, val, len);
}

char* totemRegister_GetMiniStringPtr(totemRegister* reg)
{
    return reg->Value.MiniString;
}

#endif

void totemRegister_IntFromFloat(totemRegister *dst, totemRegister *src)
{
    totemInt num = totemRegister_GetInt(src);
    totemFloat newNum = (totemFloat)num;
    totemRegister_SetFloat(dst, newNum);
}

totemBool totemRegister_IsNull(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Null);
}

totemBool totemRegister_IsString(totemRegister *reg)
{
    return totemRegister_IsInternedString(reg) || totemRegister_IsMiniString(reg);
}

totemBool totemRegister_IsFunction(totemRegister *reg)
{
	return totemRegister_IsInstanceFunction(reg) || totemRegister_IsNativeFunction(reg);
}

totemBool totemRegister_IsTypeValue(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Type);
}

totemBool totemRegister_IsInstance(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Instance);
}

totemBool totemRegister_IsInt(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Int);
}

totemBool totemRegister_IsArray(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Array);
}

totemBool totemRegister_IsObject(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Object);
}

totemBool totemRegister_IsInstanceFunction(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_InstanceFunction);
}

totemBool totemRegister_IsNativeFunction(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_NativeFunction);
}

totemBool totemRegister_IsCoroutine(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Coroutine);
}

totemBool totemRegister_IsBoolean(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Boolean);
}

totemBool totemRegister_IsUserdata(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Userdata);
}

totemBool totemRegister_IsInternedString(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_InternedString);
}

totemBool totemRegister_IsMiniString(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_MiniString);
}

void totemRegister_Set(totemRegister *dst, totemRegister *src)
{
    *dst = *src;
}

void totemRegister_SetString(totemRegister *dst, totemRuntimeStringValue *src)
{
    TOTEM_ZERO(*dst); // we use string-registers as keys, ensure any added bytes get zeroed
    
    switch (src->Type)
    {
        case totemPrivateDataType_InternedString:
			totemRegister_SetInternedString(dst, src->Src.InternedString);
            break;
            
        case totemPrivateDataType_MiniString:
			totemRegister_SetMiniString(dst, src->Src.MiniString, (uint8_t)src->Length);
            break;
            
        default:
            break;
    }
}

totemStringLength totemRegister_GetStringLength(totemRegister *reg)
{
    if (totemRegister_IsInternedString(reg))
    {
        return totemRegister_GetInternedString(reg)->Length;
    }
    else if (totemRegister_IsMiniString(reg))
    {
        char val[TOTEM_MINISTRING_MAXLENGTH + 1];
        totemRegister_GetMiniString(reg, val);
        return totemMiniString_GetLength(val);
    }
    else
    {
        return 0;
    }
}

totemHashValue totemRegister_GetStringHash(totemRegister *reg)
{
    if (totemRegister_IsInternedString(reg))
    {
        return totemRegister_GetInternedString(reg)->Hash;
    }
    else if (totemRegister_IsMiniString(reg))
    {
        return totemMiniString_GetHash(totemRegister_GetMiniStringPtr(reg));
    }
    else
    {
        return 0;
    }
}

totemBool totemRegister_GetString(totemRegister *src, totemRuntimeStringValue *val)
{
    if (totemRegister_IsInternedString(src))
    {
        totemRuntimeStringValue_FromInternedString(totemRegister_GetInternedString(src), val);
        return totemBool_True;
    }
    else if (totemRegister_IsMiniString(src))
    {
        totemRuntimeStringValue_FromMiniStringRegister(src, val);
        return totemBool_True;
    }
    else
    {
        return totemBool_False;
    }
}

void totemRuntime_PrintRegisterRecursive(totemRuntime *runtime, totemLog *file, totemRegister *reg, size_t indent, size_t index, totemBool isMultiple)
{
    if (indent > 50)
    {
        return;
    }

    totemPrivateDataType type = totemRegister_GetType(reg);
    
    if(isMultiple)
    {
        totemLog_Write(file, "%"PRISize": ", index);
    }
    
    switch (type)
    {
        case totemPrivateDataType_NativeFunction:
        {
            totemNativeFunction *func = totemRegister_GetNativeFunction(reg);
            
            totemRuntimeStringValue* val = totemRuntimeStringValueDynBuffer_At(&runtime->NativeFunctionNames, func->Address);
            if (val)
            {
                const char *ptr = totemRuntimeStringValue_GetVal(val);
                totemLog_Write(file, "%s: %s\n", totemPrivateDataType_Describe(type), ptr);
            }
            break;
        }
            
        case totemPrivateDataType_InstanceFunction:
        {
            totemInstanceFunction *func = totemRegister_GetInstanceFunction(reg);
            if(func)
            {
                totemRuntimeStringValue val;
                if (totemScript_GetFunctionName(func->Instance->Ptr.Instance->Script, func->Function->Address, &val))
                {
                    const char *ptr = totemRuntimeStringValue_GetVal(&val);
                    totemLog_Write(file, "%s: %s\n", totemPrivateDataType_Describe(type), ptr);
                }
            }
            else
            {
                totemLog_Write(file, "NULL\n");
            }
            
            break;
        }
            
        case totemPrivateDataType_Coroutine:
        {
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            totemFunctionCall *cr = gc->Ptr.Coroutine;
            totemRuntimeStringValue val;
			if (totemScript_GetFunctionName(cr->Function.Instance->Instance->Ptr.Instance->Script, cr->Function.Instance->Function->Address, &val))
            {
                const char *ptr = totemRuntimeStringValue_GetVal(&val);
                totemLog_Write(file, "%s: %s\n", totemPrivateDataType_Describe(type), ptr);
            }
            break;
        }
            
        case totemPrivateDataType_Type:
            totemLog_Write(file, "%s: %s\n", totemPrivateDataType_Describe(type), totemPublicDataType_Describe(totemRegister_GetTypeValue(reg)));
            break;
            
        case totemPrivateDataType_InternedString:
        case totemPrivateDataType_MiniString:
        {
            totemRuntimeStringValue val;
            totemRegister_GetString(reg, &val);
            const char *ptr = totemRuntimeStringValue_GetVal(&val);
            
            totemLog_Write(file, "%s \"%s\" \n", totemPrivateDataType_Describe(type), ptr);
            break;
        }
            
        case totemPrivateDataType_Object:
        {
            indent += 5;
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            totemRuntimeStringMap* obj = gc->Ptr.Object;
            
            totemLog_Write(file, "object {\n");
            
            for (uint32_t i = 0; i < obj->NumBuckets; i++)
            {
                totemHashMapEntry *entry = obj->Buckets[i];
                
                while (entry)
                {
                    for (size_t j = 0; j < indent; j++)
                    {
                        totemLog_Write(file, " ");
                    }
                    
                    totemRuntimeStringMapEntry* e = (totemRuntimeStringMapEntry*)entry;
                    const char *ptr = totemRuntimeStringValue_GetVal(&e->Data.Key);
                    
                    totemLog_Write(file, "\"%.*s\": ", (int)e->Data.Key.Length, ptr);
                    totemRuntime_PrintRegisterRecursive(runtime, file, gc->Data.Registers + e->Data.Value, indent, 0, totemBool_False);
                    
                    entry = entry->Next;
                }
            }
            
            indent -= 5;
            
            for (size_t i = 0; i < indent; i++)
            {
                totemLog_Write(file, " ");
            }
            
            totemLog_Write(file, "}\n");
            
            break;
        }
            
        case totemPrivateDataType_Array:
        {
            indent += 5;
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            
            totemLog_Write(file, "array[%u] {\n", gc->NumRegisters);
            
            for (uint32_t i = 0; i < gc->NumRegisters; ++i)
            {
                for (size_t j = 0; j < indent; j++)
                {
                    totemLog_Write(file, " ");
                }
                
                totemLog_Write(file, "%u: ", i);
                
                totemRegister *val = &gc->Data.Registers[i];
                totemRuntime_PrintRegisterRecursive(runtime, file, val, indent, i, gc->NumRegisters > 1);
            }
            
            indent -= 5;
            
            for (size_t i = 0; i < indent; i++)
            {
                totemLog_Write(file, " ");
            }
            
            totemLog_Write(file, "}\n");
            break;
        }
            
        case totemPrivateDataType_Float:
            totemLog_Write(file, "%s %f\n", totemPrivateDataType_Describe(type), totemRegister_GetFloat(reg));
            break;
            
        case totemPrivateDataType_Int:
            totemLog_Write(file, "%s %"TOTEM_INT_PRINTF"\n", totemPrivateDataType_Describe(type), totemRegister_GetInt(reg));
            break;
            
        case totemPrivateDataType_Boolean:
            totemLog_Write(file, "%s %s\n", totemPrivateDataType_Describe(type), totemRegister_IsZero(reg) ? "false" : "true");
            break;
            
        case totemPrivateDataType_Null:
            totemLog_Write(file, "%s\n", totemPrivateDataType_Describe(type));
            break;
            
        case totemPrivateDataType_Userdata:
        {
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            totemLog_Write(file, "%s %p\n", totemPrivateDataType_Describe(type), gc->Data.Userdata);
            break;
        }
            
        case totemPrivateDataType_Instance:
        {
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            totemLog_Write(file, "%s %p\n", totemPrivateDataType_Describe(type), gc->Ptr.Instance);
            break;
        }
            
        default:
            totemLog_Write(file, "%s %"TOTEM_INT_PRINTF"\n", totemPrivateDataType_Describe(type), type);
            break;
    }
}

void totemRuntime_PrintRegister(totemRuntime *runtime, totemLog *file, totemRegister *reg, size_t index, totemBool isMultiple)
{
    totemRuntime_PrintRegisterRecursive(runtime, file, reg, 0, index, isMultiple);
}

void totemRuntime_PrintRegisterList(totemRuntime *runtime, totemLog *file, totemRegister *regs, size_t numRegisters)
{
    for (size_t i = 0; i < numRegisters; i++)
    {
        totemRuntime_PrintRegister(runtime, file, &regs[i], i, numRegisters > 1);
    }
}
