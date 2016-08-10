//
//  eval_register.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>

totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    totemOperandXUnsigned index = 0;
    totemRegisterPrototype *reg = NULL;
    totemBool fromFreeList = totemBool_False;
    size_t freelistSize = totemMemoryBuffer_GetNumObjects(&list->RegisterFreeList);
    size_t numRegisters = totemMemoryBuffer_GetNumObjects(&list->Registers);
    
    if (freelistSize > 0)
    {
        totemOperandXUnsigned *indexPtr = totemMemoryBuffer_Top(&list->RegisterFreeList);
        index = *indexPtr;
        totemMemoryBuffer_Pop(&list->RegisterFreeList, 1);
        reg = totemMemoryBuffer_Get(&list->Registers, index);
        fromFreeList = totemBool_True;
    }
    else
    {
        size_t max = list->ScopeType == totemOperandType_GlobalRegister ? TOTEM_MAX_GLOBAL_REGISTERS : TOTEM_MAX_LOCAL_REGISTERS;
        
        if (numRegisters >= max)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyRegisters);
        }
        
        index = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&list->Registers);
        
        reg = totemMemoryBuffer_Secure(&list->Registers, 1);
        if (!reg)
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    reg->Int = 0;
    reg->RefCount = 1;
    reg->GlobalCache = 0;
    reg->DataType = totemPublicDataType_Null;
    reg->Flags = totemRegisterPrototypeFlag_IsTemporary | totemRegisterPrototypeFlag_IsUsed;
    
    operand->RegisterIndex = index;
    operand->RegisterScopeType = list->ScopeType;
    
    //printf("add %p %i %i %i %i %i %i %i\n", list, freelistSize, numRegisters, list->ScopeType, a, fromFreeList, operand->RegisterIndex, operand->RegisterScopeType);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    totem_assert(list->ScopeType == operand->RegisterScopeType);
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
    
    if (reg->Flags != totemRegisterPrototypeFlag_None)
    {
        if (!totemMemoryBuffer_Insert(&list->RegisterFreeList, &operand->RegisterIndex, 1))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        //printf("free %i %i\n", operand->RegisterIndex, operand->RegisterScopeType);
        
        reg->Flags = totemRegisterPrototypeFlag_None;
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags &= ~flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags |= flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag *flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *flags = reg->Flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType *type)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *type = reg->DataType;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType type)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->DataType = type;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_DecRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *countOut)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    if (reg->RefCount > 0)
    {
        reg->RefCount--;
    }
    
    if (countOut)
    {
        *countOut = reg->RefCount;
    }
    
    return totemBool_True;
}

totemBool totemRegisterListPrototype_IncRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *countOut)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->RefCount++;
    if (countOut)
    {
        *countOut = reg->RefCount;
    }
    
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *countOut)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    if (countOut)
    {
        *countOut = reg->RefCount;
    }
    
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterGlobalCache(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned assoc)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->GlobalCache = assoc;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterGlobalCache(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned *assoc)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *assoc = reg->GlobalCache;
    return totemBool_True;
}

totemEvalStatus totemRegisterListPrototype_AddType(totemRegisterListPrototype *list, totemPublicDataType type, totemOperandRegisterPrototype *op)
{
    if (type >= totemPublicDataType_Max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InvalidDataType);
    }
    
    if (list->HasDataType[type])
    {
        op->RegisterIndex = list->DataTypes[type];
        op->RegisterScopeType = list->ScopeType;
        totemRegisterListPrototype_IncRegisterRefCount(list, op->RegisterIndex, NULL);
        return totemEvalStatus_Success;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(list, op));
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, op->RegisterIndex);
    reg->DataType = totemPublicDataType_Type;
    reg->TypeValue = type;
    
    list->DataTypes[type] = op->RegisterIndex;
    list->HasDataType[type] = totemBool_True;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *str, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Strings, str->Value, str->Length);
    
    totemRegisterPrototype *reg = NULL;
    if (searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->ScopeType;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if (status != totemEvalStatus_Success)
        {
            return status;
        }
        
        reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        reg->DataType = totemPublicDataType_String;
        reg->String = str;
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
        
        if (!totemHashMap_Insert(&list->Strings, str->Value, str->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand, totemBool currentOnly)
{
    for (totemRegisterListPrototypeScope *scope = list->Scope;
         scope;
         scope = scope->Prev)
    {
        totemHashMapEntry *searchResult = totemHashMap_Find(&scope->Variables, name->Value, name->Length);
        
        if (searchResult != NULL)
        {
            operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
            operand->RegisterScopeType = list->ScopeType;
            
            totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
            
            return totemBool_True;
        }
        
        if (currentOnly)
        {
            break;
        }
    }
    
    return totemBool_False;
}

totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype)
{
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, prototype);
    
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    if (!totemHashMap_Insert(&list->Scope->Variables, name->Value, name->Length, prototype->RegisterIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, prototype->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsVariable);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandRegisterPrototype *op)
{
    if (list->HasNull)
    {
        op->RegisterIndex = list->Null;
        op->RegisterScopeType = list->ScopeType;
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, op);
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, op->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    reg->DataType = totemPublicDataType_Null;
    list->Null = op->RegisterIndex;
    list->HasNull = totemBool_True;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddBoolean(totemRegisterListPrototype *list, totemBool val, totemOperandRegisterPrototype *op)
{
    val = val != totemBool_False;
    
    if (list->HasBoolean[val])
    {
        op->RegisterIndex = list->Boolean[val];
        op->RegisterScopeType = list->ScopeType;
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, op);
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, op->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    reg->DataType = totemPublicDataType_Boolean;
    reg->Boolean = val;
    
    list->Boolean[val] = op->RegisterIndex;
    list->HasBoolean[val] = totemBool_True;
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Numbers, number->Value, number->Length);
    if (searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->ScopeType;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if (status != totemEvalStatus_Success)
        {
            return status;
        }
        
        if (!totemHashMap_Insert(&list->Numbers, number->Value, number->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        
        if (memchr(number->Value, '.', number->Length) != NULL)
        {
            reg->Float = atof(number->Value);
            reg->DataType = totemPublicDataType_Float;
        }
        else
        {
            reg->Int = atoi(number->Value);
            reg->DataType = totemPublicDataType_Int;
        }
        
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemFunctionPointerPrototype *value, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *result = totemHashMap_Find(&list->FunctionPointers, value, sizeof(totemFunctionPointerPrototype));
    if (result)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)result->Value;
        operand->RegisterScopeType = list->ScopeType;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    if (!totemHashMap_Insert(&list->FunctionPointers, value, sizeof(totemFunctionPointerPrototype), operand->RegisterIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
    reg->FunctionPointer = *value;
    reg->DataType = totemPublicDataType_Function;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

void totemRegisterListPrototypeScope_Init(totemRegisterListPrototypeScope *scope)
{
    totemHashMap_Init(&scope->MoveToLocalVars);
    totemHashMap_Init(&scope->Variables);
    scope->Prev = NULL;
}

void totemRegisterListPrototypeScope_Reset(totemRegisterListPrototypeScope *scope)
{
    totemHashMap_Reset(&scope->MoveToLocalVars);
    totemHashMap_Reset(&scope->Variables);
    scope->Prev = NULL;
}

void totemRegisterListPrototypeScope_Cleanup(totemRegisterListPrototypeScope *scope)
{
    totemHashMap_Cleanup(&scope->MoveToLocalVars);
    totemHashMap_Cleanup(&scope->Variables);
    scope->Prev = NULL;
}

totemEvalStatus totemRegisterListPrototype_EnterScope(totemRegisterListPrototype *list)
{
    totemRegisterListPrototypeScope *newScope = totem_CacheMalloc(sizeof(totemRegisterListPrototypeScope));
    if (!newScope)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    totemRegisterListPrototypeScope_Init(newScope);
    
    newScope->Prev = list->Scope;
    list->Scope = newScope;
    return totemEvalStatus_Success;
}

void totemRegisterListPrototype_FreeScope(totemRegisterListPrototype *list)
{
    totemRegisterListPrototypeScope *scope = list->Scope;
    list->Scope = scope->Prev;
    totemRegisterListPrototypeScope_Cleanup(scope);
    totem_CacheFree(scope, sizeof(totemRegisterListPrototypeScope));
}

totemEvalStatus totemRegisterListPrototypeScope_FreeGlobalCache(totemRegisterListPrototypeScope *scope, totemRegisterListPrototype *list)
{
    for (size_t i = 0; i < scope->MoveToLocalVars.NumBuckets; i++)
    {
        totemHashMapEntry *bucket = scope->MoveToLocalVars.Buckets[i];
        while (bucket)
        {
            totemOperandRegisterPrototype operand;
            operand.RegisterIndex = (totemOperandXUnsigned)bucket->Value;
            operand.RegisterScopeType = list->ScopeType;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(list, &operand));
            
            bucket = bucket->Next;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_ExitScope(totemRegisterListPrototype *list)
{
    for (size_t i = 0; i < list->Scope->Variables.NumBuckets; i++)
    {
        totemHashMapEntry *bucket = list->Scope->Variables.Buckets[i];
        while (bucket)
        {
            totemOperandRegisterPrototype operand;
            operand.RegisterIndex = (totemOperandXUnsigned)bucket->Value;
            operand.RegisterScopeType = list->ScopeType;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(list, &operand));
            
            bucket = bucket->Next;
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototypeScope_FreeGlobalCache(list->Scope, list));
    
    totemRegisterListPrototype_FreeScope(list);
    return totemEvalStatus_Success;
}

void totemRegisterListPrototype_Init(totemRegisterListPrototype *list, totemOperandType scope)
{
    list->ScopeType = scope;
    memset(list->DataTypes, 0, sizeof(list->DataTypes));
    memset(list->HasDataType, 0, sizeof(list->HasDataType));
    memset(list->Boolean, 0, sizeof(list->Boolean));
    memset(list->HasBoolean, 0, sizeof(list->HasBoolean));
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemHashMap_Init(&list->Numbers);
    totemHashMap_Init(&list->Strings);
    totemHashMap_Init(&list->FunctionPointers);
    totemMemoryBuffer_Init(&list->Registers, sizeof(totemRegisterPrototype));
    totemMemoryBuffer_Init(&list->RegisterFreeList, sizeof(totemOperandXUnsigned));
    list->Scope = NULL;
}

void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list)
{
    while (list->Scope)
    {
        totemRegisterListPrototype_FreeScope(list);
    }
    
    memset(list->DataTypes, 0, sizeof(list->DataTypes));
    memset(list->HasDataType, 0, sizeof(list->HasDataType));
    memset(list->Boolean, 0, sizeof(list->Boolean));
    memset(list->HasBoolean, 0, sizeof(list->HasBoolean));
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemHashMap_Reset(&list->Numbers);
    totemHashMap_Reset(&list->Strings);
    totemHashMap_Reset(&list->FunctionPointers);
    totemMemoryBuffer_Reset(&list->Registers);
    totemMemoryBuffer_Reset(&list->RegisterFreeList);
}

void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list)
{
    while (list->Scope)
    {
        totemRegisterListPrototype_FreeScope(list);
    }
    
    memset(list->DataTypes, 0, sizeof(list->DataTypes));
    memset(list->HasDataType, 0, sizeof(list->HasDataType));
    memset(list->Boolean, 0, sizeof(list->Boolean));
    memset(list->HasBoolean, 0, sizeof(list->HasBoolean));
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemHashMap_Cleanup(&list->Numbers);
    totemHashMap_Cleanup(&list->Strings);
    totemHashMap_Cleanup(&list->FunctionPointers);
    totemMemoryBuffer_Cleanup(&list->Registers);
    totemMemoryBuffer_Cleanup(&list->RegisterFreeList);
}