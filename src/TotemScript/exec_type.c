//
//  exec_type.c
//  TotemScript
//
//  Created by Timothy Smale on 03/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>

#define TOTEM_EXEC_CHECKRETURN(x) { totemExecStatus status = x; if(status != totemExecStatus_Continue) return status; }

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

void totemExecState_Assign(totemExecState *state, totemRegister *dst, totemRegister *src)
{
    totemExecState_DecRefCount(state, dst);
    *dst = *src;
    totemExecState_IncRefCount(state, dst);
}

void totemExecState_AssignNull(totemExecState *state, totemRegister *dst)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Null;
    dst->Value.Data = 0;
}

void totemExecState_AssignNewInt(totemExecState *state, totemRegister *dst, totemInt newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Int;
    dst->Value.Int = newVal;
}

void totemExecState_AssignNewFloat(totemExecState *state, totemRegister *dst, totemFloat newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Float;
    dst->Value.Float = newVal;
}

void totemExecState_AssignNewType(totemExecState *state, totemRegister *dst, totemPublicDataType newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Type;
    dst->Value.DataType = newVal;
}

void totemExecState_AssignNewNativeFunction(totemExecState *state, totemRegister *dst, totemNativeFunction *func)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_NativeFunction;
    dst->Value.NativeFunction = func;
}

void totemExecState_AssignNewInstanceFunction(totemExecState *state, totemRegister *dst, totemInstanceFunction *func)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_InstanceFunction;
    dst->Value.InstanceFunction = func;
}

void totemExecState_AssignNewArray(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Array;
    dst->Value.GCObject = newVal;
}

void totemExecState_AssignNewCoroutine(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Coroutine;
    dst->Value.GCObject = newVal;
}

void totemExecState_AssignNewObject(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Object;
    dst->Value.GCObject = newVal;
}

void totemExecState_AssignNewUserdata(totemExecState *state, totemRegister *dst, totemGCObject *newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->DataType = totemPrivateDataType_Userdata;
    dst->Value.GCObject = newVal;
}

void totemExecState_AssignNewString(totemExecState *state, totemRegister *dst, totemRegister *src)
{
    totemExecState_DecRefCount(state, dst);
    *dst = *src;
}

void totemExecState_AssignNewBoolean(totemExecState *state, totemRegister *dst, totemBool newVal)
{
    totemExecState_DecRefCount(state, dst);
    dst->Value.Data = newVal != totemBool_False;
    dst->DataType = totemPrivateDataType_Boolean;
}

totemExecStatus totemExecState_Add(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewInt(state, destination, source1->Value.Int + source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, ((totemFloat)source1->Value.Int) + source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float + source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float + ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Array, totemPrivateDataType_Array):
            return totemExecState_ConcatArrays(state, source1, source2, destination);
            break;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_MiniString, totemPrivateDataType_MiniString):
        case TOTEM_TYPEPAIR(totemPrivateDataType_InternedString, totemPrivateDataType_MiniString):
        case TOTEM_TYPEPAIR(totemPrivateDataType_MiniString, totemPrivateDataType_InternedString):
        case TOTEM_TYPEPAIR(totemPrivateDataType_InternedString, totemPrivateDataType_InternedString):
            return totemExecState_ConcatStrings(state, source1, source2, destination);
            break;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_Subtract(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewInt(state, destination, source1->Value.Int - source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float - source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float - ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, ((totemFloat)source1->Value.Int) - source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_Multiply(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewInt(state, destination, source1->Value.Int * source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float * source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float * ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, ((totemFloat)source1->Value.Int) * source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_Divide(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    if (source2->Value.Int == 0)
    {
        return totemExecStatus_Break(totemExecStatus_DivideByZero);
    }
    
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewInt(state, destination, source1->Value.Int / source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float / source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewFloat(state, destination, source1->Value.Float / ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewFloat(state, destination, ((totemFloat)source1->Value.Int) / source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_LessThan(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Int < source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float < source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float < ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, ((totemFloat)source1->Value.Float) < source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_LessThanEquals(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Int <= source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float <= source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float <= ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, ((totemFloat)source1->Value.Float) <= source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_MoreThan(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Int > source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float > source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float > ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, ((totemFloat)source1->Value.Float) > source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_MoreThanEquals(totemExecState *state, totemRegister *destination, totemRegister *source1, totemRegister *source2)
{
    switch (TOTEM_TYPEPAIR(source1->DataType, source2->DataType))
    {
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Int >= source2->Value.Int);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float >= source2->Value.Float);
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Float, totemPrivateDataType_Int):
            totemExecState_AssignNewBoolean(state, destination, source1->Value.Float >= ((totemFloat)source2->Value.Int));
            return totemExecStatus_Continue;
            
        case TOTEM_TYPEPAIR(totemPrivateDataType_Int, totemPrivateDataType_Float):
            totemExecState_AssignNewBoolean(state, destination, ((totemFloat)source1->Value.Float) >= source2->Value.Float);
            return totemExecStatus_Continue;
            
        default:
            return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
}

totemExecStatus totemExecState_InternStringChar(totemExecState *state, totemRegister *src, totemInt index, totemRegister *dst)
{
    const char *str = totemRegister_GetStringValue(src);
    totemStringLength len = totemRegister_GetStringLength(src);
    
    if (index < 0 || index >= (totemInt)len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemString toIntern;
    toIntern.Length = 1;
    toIntern.Value = str + index;
    
    return totemExecState_InternString(state, &toIntern, dst);
}

totemExecStatus totemExecState_ArrayShift(totemExecState *state, totemRegister *arr, size_t len, totemInt index, totemRegister *dst)
{
    if (index < 0 || ((size_t)index) >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemRegister *src = &arr[index];
    
    totemExecState_Assign(state, dst, src);
    totemExecState_AssignNull(state, src);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArraySet(totemExecState *state, totemRegister *arr, size_t len, totemInt index, totemRegister *src)
{
    if (index < 0 || ((size_t)index) >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecState_Assign(state, &arr[index], src);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ArrayGet(totemExecState *state, totemRegister *arr, size_t len, totemInt index, totemRegister *dst)
{
    if (index < 0 || ((size_t)index) >= len)
    {
        return totemExecStatus_Break(totemExecStatus_IndexOutOfBounds);
    }
    
    totemExecState_Assign(state, dst, &arr[index]);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ObjectShift(totemExecState *state, totemGCObject *obj, totemRegister *key, totemRegister *dst)
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
    
    totemHash hash = totemRegister_GetStringHash(&actualKey);
    
    totemHashMapEntry *result = totemHashMap_RemovePrecomputed(obj->Object, &actualKey, sizeof(actualKey), hash);
    if (!result)
    {
        totemExecState_AssignNull(state, dst);
    }
    else
    {
        totemExecState_Assign(state, dst, obj->Registers + result->Value);
    }
    
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
    
    totemHash hash = totemRegister_GetStringHash(&actualKey);
    
    totemHashMapEntry *result = totemHashMap_FindPrecomputed(obj->Object, &actualKey, sizeof(actualKey), hash);
    if (!result)
    {
        totemExecState_AssignNull(state, dst);
    }
    else
    {
        totemExecState_Assign(state, dst, obj->Registers + result->Value);
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
    
    totemHash hash = totemRegister_GetStringHash(&actualKey);
    totemHashValue registerIndex = 0;
    totemHashMapEntry *entry = totemHashMap_FindPrecomputed(obj->Object, &actualKey, sizeof(actualKey), hash);
    
    if (entry)
    {
        // already in hash map
        registerIndex = entry->Value;
    }
    else
    {
        totemBool expanded = totemBool_False;
        
        // we need to insert a new lookup value
        if (obj->Object->FreeList)
        {
            // we can reuse the register index sitting on top of the freelist
            registerIndex = obj->Object->FreeList->Value;
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
        
        if (!totemHashMap_InsertPrecomputedWithoutSearch(obj->Object, &actualKey, sizeof(actualKey), registerIndex, hash))
        {
            return totemExecStatus_Break(totemExecStatus_OutOfMemory);
        }
        
        if (expanded)
        {
            state->GCNumBytes += sizeof(totemRegister) + sizeof(totemHashMapEntry);
        }
    }
    
    totemRegister *newReg = obj->Registers + registerIndex;
    totemExecState_Assign(state, newReg, src);
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_Cast(totemExecState *state, totemRegister *dst, totemRegister *src, totemRegister *typeReg)
{
    totemPublicDataType srcType = totemPrivateDataType_ToPublic(src->DataType);
    totemPublicDataType toType = typeReg->Value.DataType;
    
    if (typeReg->DataType != totemPrivateDataType_Type)
    {
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    // type
    if (toType == totemPublicDataType_Type)
    {
        totemExecState_AssignNewType(state, dst, srcType);
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
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArrayFromExisting(state, src->Value.GCObject->Registers, src->Value.GCObject->NumRegisters, &gc));
                totemExecState_AssignNewArray(state, dst, gc);
                break;
            }
                
                // explode string into array
            case totemPublicDataType_String:
            {
                const char *str = totemRegister_GetStringValue(src);
                totemGCObject *gc = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, (uint32_t)totemRegister_GetStringLength(src), &gc));
                
                totemRegister *regs = gc->Registers;
                for (size_t i = 0; i < gc->NumRegisters; i++)
                {
                    totemString newStr;
                    newStr.Length = 1;
                    newStr.Value = &str[i];
                    
                    if (totemRuntime_InternString(state->Runtime, &newStr, &regs[i].Value, &regs[i].DataType) != totemLinkStatus_Success)
                    {
                        return totemExecStatus_Break(totemExecStatus_OutOfMemory);
                    }
                }
                
                totemExecState_AssignNewArray(state, dst, gc);
                break;
            }
                
            default:
            {
                totemGCObject *gc = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArrayFromExisting(state, src, 1, &gc));
                totemExecState_AssignNewArray(state, dst, gc);
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
                totemExecState_AssignNewInt(state, dst, src->Value.Int);
                break;
                
                // int as float
            case TOTEM_TYPEPAIR(totemPublicDataType_Int, totemPublicDataType_Float):
                totemExecState_AssignNewFloat(state, dst, (totemFloat)src->Value.Int);
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
                totemExecState_AssignNewFloat(state, dst, src->Value.Float);
                break;
                
                // float as int
            case TOTEM_TYPEPAIR(totemPublicDataType_Float, totemPublicDataType_Int):
                totemExecState_AssignNewInt(state, dst, (totemInt)src->Value.Float);
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
                totemExecState_AssignNewInt(state, dst, (totemInt)src->Value.GCObject->NumRegisters);
                break;
                
                // array as float (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_Float):
                totemExecState_AssignNewFloat(state, dst, (totemFloat)src->Value.GCObject->NumRegisters);
                break;
                
                // array as string (implode)
            case TOTEM_TYPEPAIR(totemPublicDataType_Array, totemPublicDataType_String):
                TOTEM_EXEC_CHECKRETURN(totemExecState_ArrayToString(state, src->Value.GCObject->Registers, src->Value.GCObject->NumRegisters, dst));
                break;
                
                /*
                 * types
                 */
                
                // type as type
            case TOTEM_TYPEPAIR(totemPublicDataType_Type, totemPublicDataType_Type):
                totemExecState_AssignNewType(state, dst, src->Value.DataType);
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
                totemExecState_AssignNewInt(state, dst, val);
                break;
            }
                
                // string as float (attempt atof)
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Float):
            {
                const char *str = totemRegister_GetStringValue(src);
                totemFloat val = strtod(str, NULL);
                totemExecState_AssignNewFloat(state, dst, val);
                break;
            }
                
                // string as string
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_String):
                totemExecState_AssignNewString(state, dst, src);
                break;
                
                // lookup function pointer by name
            case TOTEM_TYPEPAIR(totemPublicDataType_String, totemPublicDataType_Function):
                TOTEM_EXEC_CHECKRETURN(totemExecState_StringToFunction(state, src, dst));
                break;
                
                /*
                 * functions
                 */
                
                // function as string
            case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_String):
            {
                switch (src->DataType)
                {
                    case totemPrivateDataType_NativeFunction:
                        TOTEM_EXEC_CHECKRETURN(totemExecState_NativeFunctionToString(state, src->Value.NativeFunction, dst));
                        break;
                        
                    case totemPrivateDataType_InstanceFunction:
                        TOTEM_EXEC_CHECKRETURN(totemExecState_InstanceFunctionToString(state, src->Value.InstanceFunction, dst));
                        break;
                }
                break;
            }
                
                // function as function
            case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_Function):
                totemExecState_AssignNewInstanceFunction(state, dst, src->Value.InstanceFunction);
                break;
                
                // create coroutine
            case TOTEM_TYPEPAIR(totemPublicDataType_Function, totemPublicDataType_Coroutine):
            {
                if (src->DataType != totemPrivateDataType_InstanceFunction)
                {
                    return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
                }
                
                totemGCObject *obj = NULL;
                totemInstanceFunction *function = totemMemoryBuffer_Get(&state->CallStack->Instance->Instance->LocalFunctions, src->Value.InstanceFunction->Function->Address);
                if (!function)
                {
                    return totemExecStatus_Break(totemExecStatus_InstanceFunctionNotFound);
                }
                
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateCoroutine(state, function, &obj));
                totemExecState_AssignNewCoroutine(state, dst, obj);
                break;
            }
                
                /*
                 * coroutine
                 */
                
                // coroutine as string
            case TOTEM_TYPEPAIR(totemPublicDataType_Coroutine, totemPublicDataType_String):
            {
                totemRegister val;
                memset(&val, 0, sizeof(val));
                totemInstanceFunction *func = src->Value.GCObject->Coroutine->InstanceFunction;
                
                if (!totemScript_GetFunctionName(func->Instance->Instance->Script, func->Function->Address, &val.Value, &val.DataType))
                {
                    return totemExecStatus_Break(totemExecStatus_InstanceFunctionNotFound);
                }
                
                totemExecState_AssignNewString(state, dst, &val);
                break;
            }
                
                // clone coroutine
            case TOTEM_TYPEPAIR(totemPublicDataType_Coroutine, totemPublicDataType_Coroutine):
            {
                totemGCObject *obj = NULL;
                TOTEM_EXEC_CHECKRETURN(totemExecState_CreateCoroutine(state, src->Value.GCObject->Coroutine->InstanceFunction, &obj));
                totemExecState_AssignNewCoroutine(state, dst, obj);
                break;
            }
                
                // extract function pointer from coroutine
            case TOTEM_TYPEPAIR(totemPublicDataType_Coroutine, totemPublicDataType_Function):
                totemExecState_AssignNewInstanceFunction(state, dst, src->Value.GCObject->Coroutine->InstanceFunction);
                break;
                
                /*
                 * objects
                 */
                
                // object as int (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Object, totemPublicDataType_Int):
                totemExecState_AssignNewInt(state, dst, src->Value.GCObject->Object->NumKeys);
                break;
                
                // object as float (length)
            case TOTEM_TYPEPAIR(totemPublicDataType_Object, totemPublicDataType_Float):
                totemExecState_AssignNewFloat(state, dst, (totemFloat)(src->Value.GCObject->Object->NumKeys));
                break;
                
            default:
                return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
        }
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemExecState_ConcatStrings(totemExecState *state, totemRegister *str1, totemRegister *str2, totemRegister *strOut)
{
    totemStringLength len1 = totemRegister_GetStringLength(str1);
    totemStringLength len2 = totemRegister_GetStringLength(str2);
    const char *str1Val = totemRegister_GetStringValue(str1);
    const char *str2Val = totemRegister_GetStringValue(str2);
    
    if (len1 + len2 <= TOTEM_MINISTRING_MAXLENGTH)
    {
        totemExecState_DecRefCount(state, strOut);
        strOut->DataType = totemPrivateDataType_MiniString;
        memset(strOut->Value.MiniString.Value, 0, sizeof(strOut->Value.MiniString.Value));
        memcpy(strOut->Value.MiniString.Value, str1Val, len1);
        memcpy(strOut->Value.MiniString.Value + len1, str2Val, len2);
        
        return totemExecStatus_Continue;
    }
    
    char *buffer = totemExecState_Alloc(state, len1 + len2);
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

totemExecStatus totemExecState_ToString(totemExecState *state, totemRegister *src, totemRegister *dst)
{
    switch (src->DataType)
    {
        case totemPrivateDataType_Int:
            return totemExecState_IntToString(state, src->Value.Int, dst);
            
        case totemPrivateDataType_Type:
            return totemExecState_TypeToString(state, src->Value.DataType, dst);
            
        case totemPrivateDataType_Array:
            return totemExecState_ArrayToString(state, src->Value.GCObject->Registers, src->Value.GCObject->NumRegisters, dst);
            
        case totemPrivateDataType_Float:
            return totemExecState_FloatToString(state, src->Value.Float, dst);
            
        case totemPrivateDataType_InternedString:
        case totemPrivateDataType_MiniString:
            memcpy(dst, src, sizeof(totemRegister));
            return totemExecStatus_Continue;
            
        case totemPrivateDataType_Coroutine:
            return totemExecState_InstanceFunctionToString(state, src->Value.GCObject->Coroutine->InstanceFunction, dst);
            
        case totemPrivateDataType_NativeFunction:
            return totemExecState_NativeFunctionToString(state, src->Value.NativeFunction, dst);
            
        case totemPrivateDataType_InstanceFunction:
            return totemExecState_InstanceFunctionToString(state, src->Value.InstanceFunction, dst);
            
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
    int result = totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "%"PRIi64, val);
    
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
    
    if (!totemScript_GetFunctionName(func->Instance->Instance->Script, func->Function->Address, &val.Value, &val.DataType))
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
    
    memset(strings, 0, sizeof(totemRegister) * numRegisters);
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
        const char *val = totemRegister_GetStringValue(&strings[i]);
        
        memcpy(current, val, len);
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
    TOTEM_EXEC_CHECKRETURN(totemExecState_CreateArray(state, src1->Value.GCObject->NumRegisters + src2->Value.GCObject->NumRegisters, &gc));
    
    totemRegister *newRegs = gc->Registers;
    totemRegister *source1Regs = src1->Value.GCObject->Registers;
    
    for (size_t i = 0; i < src1->Value.GCObject->NumRegisters; i++)
    {
        totemExecState_Assign(state, &newRegs[i], &source1Regs[i]);
    }
    
    totemRegister *source2Regs = src2->Value.GCObject->Registers;
    for (size_t i = src1->Value.GCObject->NumRegisters; i < gc->NumRegisters; i++)
    {
        totemExecState_Assign(state, &newRegs[i], &source2Regs[i - src1->Value.GCObject->NumRegisters]);
    }
    
    totemExecState_AssignNewArray(state, dst, gc);
    return totemExecStatus_Continue;
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
        totemHashMapEntry *entry = totemHashMap_Find(&state->CallStack->Instance->Instance->Script->FunctionNameLookup, lookup.Value, lookup.Length);
        if (entry)
        {
            totemInstanceFunction *func = totemMemoryBuffer_Get(&state->CallStack->Instance->Instance->LocalFunctions, (size_t)entry->Value);
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
