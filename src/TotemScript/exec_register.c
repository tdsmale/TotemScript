//
//  exec_register.c
//  TotemScript
//
//  Created by Timothy Smale on 27/08/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

#if TOTEM_DEBUGOPT_ASSERT_REGISTER_VALUES
#define totemRegister_Assert totemRegister_AssertValue
#else
#define totemRegister_Assert(x)
#endif

#if TOTEM_VMOPT_NANBOXING

#define TOTEM_FLOAT_QUIET_NAN_MASK TOTEM_BITMASK(uint64_t, 51, 12)
#define TOTEM_FLOAT_MANTISSA_MASK TOTEM_BITMASK(uint64_t, 0, 48)
#define TOTEM_REGISTER_TYPE_MASK TOTEM_BITMASK(uint64_t, 48, 16)
#define TOTEM_REGISTER_NAN_VALUE(type, val) ((((uint64_t)(type)) << 48) | ((val) & TOTEM_BITMASK(uint64_t, 0, 48)))

void totemRegister_AssertValue(totemRegister *reg)
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
        totem_printBits(stdout, TOTEM_BITCAST(uint64_t, *reg), 64, 0);
        printf("\n");
        totem_printBits(stdout, totemPrivateDataType_Float, 64, 0);
        printf(" float\n");
        totem_printBits(stdout, totemPrivateDataType_Int, 64, 0);
        printf(" int\n");
        totem_printBits(stdout, totemPrivateDataType_Array, 64, 0);
        printf(" array\n");
        totem_printBits(stdout, totemPrivateDataType_Boolean, 64, 0);
        printf(" bool\n");
        totem_printBits(stdout, totemPrivateDataType_Coroutine, 64, 0);
        printf(" coroutine\n");
        totem_printBits(stdout, totemPrivateDataType_InstanceFunction, 64, 0);
        printf(" instance func\n");
        totem_printBits(stdout, totemPrivateDataType_InternedString, 64, 0);
        printf(" interned string\n");
        totem_printBits(stdout, totemPrivateDataType_MiniString, 64, 0);
        printf(" mini string\n");
        totem_printBits(stdout, totemPrivateDataType_NativeFunction, 64, 0);
        printf(" native func\n");
        totem_printBits(stdout, totemPrivateDataType_Null, 64, 0);
        printf(" null\n");
        totem_printBits(stdout, totemPrivateDataType_Object, 64, 0);
        printf(" object\n");
        totem_printBits(stdout, totemPrivateDataType_Type, 64, 0);
        printf(" type\n");
        totem_printBits(stdout, totemPrivateDataType_Userdata, 64, 0);
        printf(" userdata\n");
        
        totem_assert(totemBool_False);
    }
}

totemPrivateDataType totemRegister_GetType(totemRegister *reg)
{
    uint16_t type = reg->AsTagVal.Tag;
    uint16_t isNaN = TOTEM_HASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK);
    
    // if is not NaN, type is floating point - ensure all bits are set to 0
    return type & (-isNaN);
}

uint64_t totemRegister_GetMantissaValue(totemRegister *reg)
{
    return TOTEM_GETBITS(reg->AsBits, TOTEM_FLOAT_MANTISSA_MASK);
}

totemInt totemRegister_GetInt(totemRegister *reg)
{
    return reg->AsInt.LSW;
}

totemFloat totemRegister_GetFloat(totemRegister * reg)
{
    return reg->AsFloat;
}

totemInternedStringHeader *totemRegister_GetInternedString(totemRegister *reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemInternedStringHeader*, val);
}

struct totemGCObject *totemRegister_GetGCObject( totemRegister *reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(struct totemGCObject*, val);
}

totemBool totemRegister_IsZero(totemRegister *reg)
{
    return
    reg->AsBits == 0 ||
    reg->AsBits == totemPrivateDataType_Null ||
    (TOTEM_HASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK) && !TOTEM_HASANYBITS(reg->AsBits, TOTEM_FLOAT_MANTISSA_MASK));
}

totemBool totemRegister_IsNotZero(totemRegister *reg)
{
    return
    reg->AsBits != 0 &&
    reg->AsBits != totemPrivateDataType_Null &&
    (!TOTEM_HASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK) || TOTEM_HASANYBITS(reg->AsBits, TOTEM_FLOAT_MANTISSA_MASK));
}

totemBool totemRegister_IsType(totemRegister *reg, totemPrivateDataType type)
{
    return reg->AsTagVal.Tag == type;
}

totemBool totemRegister_IsFloat(totemRegister *reg)
{
    return TOTEM_NHASBITS(reg->AsBits, TOTEM_FLOAT_QUIET_NAN_MASK);
}

totemBool totemRegister_IsGarbageCollected(totemRegister *reg)
{
    return reg->AsTagVal.Tag >= totemPrivateDataType_Array;
}

totemBool totemRegister_Equals(totemRegister *a, totemRegister *b)
{
    return a->AsBits == b->AsBits;
}

totemNativeFunction *totemRegister_GetNativeFunction(totemRegister *reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemNativeFunction*, val);
}

totemInstanceFunction *totemRegister_GetInstanceFunction(totemRegister *reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemInstanceFunction*, val);
}

totemPublicDataType totemRegister_GetTypeValue(totemRegister *reg)
{
    uint64_t val = totemRegister_GetMantissaValue(reg);
    return TOTEM_BITCAST(totemPublicDataType, val);
}

void totemRegister_GetMiniString(totemRegister *reg, char *valOut)
{
    memcpy(valOut, reg->AsMiniString.MiniString, TOTEM_MINISTRING_MAXLENGTH + 1);
}

void totemRegister_InitList(totemRegister *reg, size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        totemRegister_SetNull(&reg[i]);
    }
}

void totemRegister_SetNull(totemRegister *dst)
{
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Null, 0);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Null);
    totem_assert(totemRegister_IsNull(dst));
}

void totemRegister_SetInt(totemRegister *dst, totemInt val)
{
    dst->AsInt.MSW = totemPrivateDataType_Int << 16;
    dst->AsInt.LSW = val;
    
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Int);
    totem_assert(totemRegister_GetInt(dst) == val);
    totem_assert(totemRegister_IsInt(dst));
}

void totemRegister_SetFloat(totemRegister *dst, totemFloat val)
{
    dst->AsFloat = val;
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Float);
    totem_assert(totemRegister_GetFloat(dst) == val);
    totem_assert(totemRegister_IsFloat(dst));
}

void totemRegister_SetTypeValue(totemRegister *dst, totemPublicDataType val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Type, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Type);
    totem_assert(totemRegister_GetTypeValue(dst) == val);
    totem_assert(totemRegister_IsTypeValue(dst));
}

void totemRegister_SetNativeFunction(totemRegister *dst, totemNativeFunction *val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_NativeFunction, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_NativeFunction);
    totem_assert(totemRegister_GetNativeFunction(dst) == val);
    totem_assert(totemRegister_IsNativeFunction(dst));
}

void totemRegister_SetInstanceFunction(totemRegister *dst, totemInstanceFunction *val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_InstanceFunction, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_InstanceFunction);
    totem_assert(totemRegister_GetInstanceFunction(dst) == val);
    totem_assert(totemRegister_IsInstanceFunction(dst));
}

void totemRegister_SetArray(totemRegister *dst, totemGCObject *val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Array, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Array);
    totem_assert(totemRegister_GetGCObject(dst) == val);
    totem_assert(totemRegister_IsArray(dst));
}

void totemRegister_SetObject(totemRegister *dst, totemGCObject *val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Object, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Object);
    totem_assert(totemRegister_GetGCObject(dst) == val);
    totem_assert(totemRegister_IsObject(dst));
}

void totemRegister_SetCoroutine(totemRegister *dst, totemGCObject *val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Coroutine, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Coroutine);
    totem_assert(totemRegister_GetGCObject(dst) == val);
    totem_assert(totemRegister_IsCoroutine(dst));
}

void totemRegister_SetUserdata(totemRegister *dst, totemGCObject *val)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, val);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Userdata, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Userdata);
    totem_assert(totemRegister_GetGCObject(dst) == val);
    totem_assert(totemRegister_IsUserdata(dst));
}

void totemRegister_SetBoolean(totemRegister *dst, totemBool val)
{
    uint64_t b = val != totemBool_False;
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_Boolean, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_Boolean);
    totem_assert(totemRegister_IsNotZero(dst) == val);
    totem_assert(totemRegister_IsBoolean(dst));
}

void totemRegister_SetInternedString(totemRegister *dst, totemInternedStringHeader *hdr)
{
    uint64_t b = TOTEM_BITCAST(uint64_t, hdr);
    dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_InternedString, b);
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_InternedString);
    totem_assert(totemRegister_GetInternedString(dst) == hdr);
    totem_assert(totemRegister_IsInternedString(dst));
}

void totemRegister_SetMiniString(totemRegister *dst, char *val)
{
    /*
     uint64_t b = 0;
     memcpy(&b, val, TOTEM_MINISTRING_MAXLENGTH + 1);
     dst->AsBits = TOTEM_REGISTER_NAN_VALUE(totemPrivateDataType_MiniString, b);
     */
    
    dst->AsTagVal.Tag = totemPrivateDataType_MiniString;
    memcpy(dst->AsMiniString.MiniString, val, TOTEM_MINISTRING_MAXLENGTH + 1);
    
#if TOTEM_DEBUG
    char test[TOTEM_MINISTRING_MAXLENGTH + 1];
    totem_assert(totemRegister_GetType(dst) == totemPrivateDataType_MiniString);
    totemRegister_GetMiniString(dst, test);
    totem_assert(memcmp(val, test, TOTEM_MINISTRING_MAXLENGTH + 1) == 0);
    totem_assert(totemRegister_IsMiniString(dst));
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
    return reg->Value.Data == 0;
}

totemBool totemRegister_IsNotZero(totemRegister *reg)
{
    return reg->Value.Data != 0;
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
    return a->DataType == b->DataType && a->Value.Data == b->Value.Data;
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
    memset(reg, 0, sizeof(totemRegister) * num);
}

void totemRegister_SetNull(totemRegister *dst)
{
    dst->DataType = totemPrivateDataType_Null;
    dst->Value.Data = 0;
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

void totemRegister_SetBoolean(totemRegister *dst, totemBool val)
{
    dst->DataType = totemPrivateDataType_Boolean;
    dst->Value.Data = val != 0;
}

void totemRegister_SetInternedString(totemRegister *dst, totemInternedStringHeader *val)
{
    dst->DataType = totemPrivateDataType_InternedString;
    dst->Value.InternedString = val;
}

void totemRegister_SetMiniString(totemRegister *dst, char *val)
{
    dst->DataType = totemPrivateDataType_MiniString;
    memcpy(dst->Value.MiniString, val, TOTEM_MINISTRING_MAXLENGTH + 1);
}

#endif

totemBool totemRegister_IsNull(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Null);
}

totemBool totemRegister_IsString(totemRegister *reg)
{
    return totemRegister_IsInternedString(reg) || totemRegister_IsMiniString(reg);
}

totemBool totemRegister_IsTypeValue(totemRegister *reg)
{
    return totemRegister_IsType(reg, totemPrivateDataType_Type);
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

void totemExecState_Assign(totemExecState *state, totemRegister *dst, totemRegister *src)
{
    totemExecState_DecRefCount(state, dst);
    *dst = *src;
    totemExecState_IncRefCount(state, dst);
}

void totemExecState_AssignNull(totemExecState *state, totemRegister *dst)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetNull(dst);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewInt(totemExecState *state, totemRegister *dst, totemInt newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetInt(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewFloat(totemExecState *state, totemRegister *dst, totemFloat newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetFloat(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewType(totemExecState *state, totemRegister *dst, totemPublicDataType newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetTypeValue(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewNativeFunction(totemExecState *state, totemRegister *dst, totemNativeFunction *func)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetNativeFunction(dst, func);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewInstanceFunction(totemExecState *state, totemRegister *dst, totemInstanceFunction *func)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetInstanceFunction(dst, func);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewArray(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetArray(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewCoroutine(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetCoroutine(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetObject(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewUserdata(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetUserdata(dst, newVal);
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewString(totemExecState *state, totemRegister *dst, totemRuntimeStringValue *src)
{
    totemExecState_DecRefCount(state, dst);
    
    memset(dst, 0, sizeof(totemRegister)); // we use string-registers as keys, ensure any added bytes get zeroed
    
    switch (src->Type)
    {
        case totemPrivateDataType_InternedString:
            totemRegister_SetInternedString(dst, src->InternedString);
            break;
            
        case totemPrivateDataType_MiniString:
            totemRegister_SetMiniString(dst, src->MiniString);
            break;
            
        default:
            break;
    }
    
    totemRegister_Assert(dst);
}

void totemExecState_AssignNewBoolean(totemExecState *state, totemRegister *dst, totemBool newVal)
{
    totemExecState_DecRefCount(state, dst);
    totemRegister_SetBoolean(dst, newVal);
    totemRegister_Assert(dst);
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

totemHash totemRegister_GetStringHash(totemRegister *reg)
{
    if (totemRegister_IsInternedString(reg))
    {
        return totemRegister_GetInternedString(reg)->Hash;
    }
    else if (totemRegister_IsMiniString(reg))
    {
        char val[TOTEM_MINISTRING_MAXLENGTH + 1];
        memset(val, 0, sizeof(val));
        totemRegister_GetMiniString(reg, val);
        return totem_Hash(val, totemMiniString_GetLength(val));
    }
    else
    {
        return 0;
    }
}

void totemRegister_GetStringValue(totemRegister *src, totemRuntimeStringValue *val)
{
    if (totemRegister_IsInternedString(src))
    {
        val->Type = totemPrivateDataType_InternedString;
        val->InternedString = totemRegister_GetInternedString(src);
        val->Value = val->InternedString->Data;
    }
    else if (totemRegister_IsMiniString(src))
    {
        val->Type = totemPrivateDataType_MiniString;
        totemRegister_GetMiniString(src, val->MiniString);
        val->Value = val->MiniString;
    }
    else
    {
        val->Value = NULL;
    }
}

void totemExecState_PrintRegisterRecursive(totemExecState *state, FILE *file, totemRegister *reg, size_t indent)
{
    if (indent > 50)
    {
        return;
    }
    
    totemPrivateDataType type = totemRegister_GetType(reg);
    
    switch (type)
    {
        case totemPrivateDataType_NativeFunction:
        {
            totemNativeFunction *func = totemRegister_GetNativeFunction(reg);
            totemRuntimeStringValue val;
            if (totemRuntime_GetNativeFunctionName(state->Runtime, func->Address, &val))
            {
                fprintf(file, "%s: %s\n", totemPrivateDataType_Describe(type), val.Value);
            }
            break;
        }
            
        case totemPrivateDataType_InstanceFunction:
        {
            totemInstanceFunction *func = totemRegister_GetInstanceFunction(reg);
            totemRuntimeStringValue val;
            if (totemScript_GetFunctionName(func->Instance->Instance->Script, func->Function->Address, &val))
            {
                fprintf(file, "%s: %s\n", totemPrivateDataType_Describe(type), val.Value);
            }
            break;
        }
            
        case totemPrivateDataType_Coroutine:
        {
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            totemFunctionCall *cr = gc->Coroutine;
            totemRuntimeStringValue val;
            if (totemScript_GetFunctionName(cr->InstanceFunction->Instance->Instance->Script, cr->InstanceFunction->Function->Address, &val))
            {
                fprintf(file, "%s: %s\n", totemPrivateDataType_Describe(type), val.Value);
            }
            break;
        }
            
        case totemPrivateDataType_Type:
            fprintf(file, "%s: %s\n", totemPrivateDataType_Describe(type), totemPublicDataType_Describe(totemRegister_GetTypeValue(reg)));
            break;
            
        case totemPrivateDataType_InternedString:
        case totemPrivateDataType_MiniString:
        {
            totemRuntimeStringValue val;
            totemRegister_GetStringValue(reg, &val);
            
            fprintf(file, "%s \"%s\" \n",
                    totemPrivateDataType_Describe(type),
                    val.Value);
            break;
        }
            
        case totemPrivateDataType_Object:
        {
            indent += 5;
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            totemHashMap *obj = gc->Object;
            
            fprintf(file, "object {\n");
            
            for (size_t i = 0; i < obj->NumBuckets; i++)
            {
                totemHashMapEntry *entry = obj->Buckets[i];
                
                while (entry)
                {
                    for (size_t i = 0; i < indent; i++)
                    {
                        fprintf(file, " ");
                    }
                    
                    fprintf(file, "\"%.*s\": ", (int)entry->KeyLen, (char*)entry->Key);
                    totemExecState_PrintRegisterRecursive(state, file, gc->Registers + entry->Value, indent);
                    
                    entry = entry->Next;
                }
            }
            
            indent -= 5;
            
            for (size_t i = 0; i < indent; i++)
            {
                fprintf(file, " ");
            }
            
            fprintf(file, "}\n");
            
            break;
        }
            
        case totemPrivateDataType_Array:
        {
            indent += 5;
            totemGCObject *gc = totemRegister_GetGCObject(reg);
            
            fprintf(file, "array[%"PRISize"] {\n", gc->NumRegisters);
            
            for (size_t i = 0; i < gc->NumRegisters; ++i)
            {
                for (size_t j = 0; j < indent; j++)
                {
                    fprintf(file, " ");
                }
                
                fprintf(file, "%"PRISize": ", i);
                
                totemRegister *val = &gc->Registers[i];
                totemExecState_PrintRegisterRecursive(state, file, val, indent);
            }
            
            indent -= 5;
            
            for (size_t i = 0; i < indent; i++)
            {
                fprintf(file, " ");
            }
            
            fprintf(file, "}\n");
            break;
        }
            
        case totemPrivateDataType_Float:
            fprintf(file, "%s %f\n", totemPrivateDataType_Describe(type), totemRegister_GetFloat(reg));
            break;
            
        case totemPrivateDataType_Int:
            fprintf(file, "%s %"TOTEM_INT_PRINTF"\n", totemPrivateDataType_Describe(type), totemRegister_GetInt(reg));
            break;
            
        case totemPrivateDataType_Boolean:
            fprintf(file, "%s %s\n", totemPrivateDataType_Describe(type), totemRegister_IsZero(reg) ? "false" : "true");
            break;
            
        case totemPrivateDataType_Null:
        case totemPrivateDataType_Userdata:
            fprintf(file, "%s\n", totemPrivateDataType_Describe(type));
            break;
            
        default:
            fprintf(file, "%s %d\n", totemPrivateDataType_Describe(type), type);
            break;
    }
}

void totemExecState_PrintRegister(totemExecState *state, FILE *file, totemRegister *reg)
{
    totemExecState_PrintRegisterRecursive(state, file, reg, 0);
}

void totemExecState_PrintRegisterList(totemExecState *state, FILE *file, totemRegister *regs, size_t numRegisters)
{
    for (size_t i = 0; i < numRegisters; i++)
    {
        totemExecState_PrintRegister(state, file, &regs[i]);
    }
}