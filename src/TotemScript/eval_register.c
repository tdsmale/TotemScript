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

totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand)
{
    totemOperandXUnsigned index = 0;
    totemRegisterPrototype *reg = NULL;
    size_t freelistSize = totemOperandXUnsignedBuffer_Num(&list->RegisterFreeList);
    size_t numRegisters = totemRegisterPrototypeBuffer_Num(&list->Registers);
    
    if (freelistSize > 0)
    {
		totemOperandXUnsigned *indexPtr = totemOperandXUnsignedBuffer_Top(&list->RegisterFreeList);
        index = *indexPtr;
		totemOperandXUnsignedBuffer_Pop(&list->RegisterFreeList, 1);
        reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    }
    else
    {
        size_t max = list->ScopeType == totemOperandType_GlobalRegister ? TOTEM_MAX_GLOBAL_REGISTERS : TOTEM_MAX_LOCAL_REGISTERS;
        
        if (numRegisters >= max)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyRegisters);
        }
        
        index = (totemOperandXUnsigned)totemRegisterPrototypeBuffer_Num(&list->Registers);
        
        reg = totemRegisterPrototypeBuffer_Secure(&list->Registers, 1);
        if (!reg)
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    reg->Value.Int = 0;
    reg->GlobalCache = 0;
    reg->DataType = totemPublicDataType_Null;
    reg->Flags = totemRegisterPrototypeFlag_IsTemporary | totemRegisterPrototypeFlag_IsUsed;
    
    operand->RegisterIndex = index;
    operand->RegisterScopeType = list->ScopeType;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand)
{
    TOTEM_DEBUG_ASSERT(list->ScopeType == operand->RegisterScopeType);
    
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, operand->RegisterIndex);
    
    if (reg->Flags != totemRegisterPrototypeFlag_None)
    {
		if (!totemOperandXUnsignedBuffer_Insert(&list->RegisterFreeList, &operand->RegisterIndex, 1))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        reg->Flags = totemRegisterPrototypeFlag_None;
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags &= ~flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags |= flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag *flags)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *flags = reg->Flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType *type)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *type = reg->DataType;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType type)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->DataType = type;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterGlobalCache(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned assoc)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->GlobalCache = assoc;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterGlobalCache(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned *assoc)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *assoc = reg->GlobalCache;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterValue(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeValue *valueOut)
{
	totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, index);
	if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
	{
		return totemBool_False;
	}

	*valueOut = reg->Value;
	return totemBool_True;
}

totemEvalStatus totemRegisterListPrototype_AddType(totemRegisterListPrototype *list, totemPublicDataType type, totemOperandPrototype *op)
{
    if (type >= totemPublicDataType_Max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InvalidDataType);
    }
    
    if (list->HasDataType[type])
    {
        op->RegisterIndex = list->DataTypes[type];
        op->RegisterScopeType = list->ScopeType;
        return totemEvalStatus_Success;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(list, op));
    
	totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, op->RegisterIndex);
    reg->DataType = totemPublicDataType_Type;
	reg->Value.Type = type;
    
    list->DataTypes[type] = op->RegisterIndex;
    list->HasDataType[type] = totemBool_True;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *str, totemOperandPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Strings, str->Value, str->Length);
    
    totemRegisterPrototype *reg = NULL;
    if (searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->ScopeType;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if (status != totemEvalStatus_Success)
        {
            return status;
        }
        
		reg = totemRegisterPrototypeBuffer_At(&list->Registers, operand->RegisterIndex);
        reg->DataType = totemPublicDataType_String;
		reg->Value.String = str;
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
        
        if (!totemHashMap_Insert(&list->Strings, str->Value, str->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_GetIdentifier(totemRegisterListPrototype *list, totemString *name, totemOperandPrototype *operand, totemBool currentOnly)
{
    for (totemRegisterListPrototypeScope *scope = list->Scope;
         scope;
         scope = scope->Prev)
    {
        totemHashMapEntry *searchResult = totemHashMap_Find(&scope->Identifiers, name->Value, name->Length);
        
        if (searchResult != NULL)
        {
            operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
            operand->RegisterScopeType = list->ScopeType;
            
            return totemBool_True;
        }
        
        if (currentOnly)
        {
            break;
        }
    }
    
    return totemBool_False;
}

totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandPrototype *operand, totemBool currentOnly)
{
    if (totemRegisterListPrototype_GetIdentifier(list, name, operand, currentOnly))
    {
        totemRegisterPrototypeFlag flags = totemRegisterPrototypeFlag_None;
        totemRegisterListPrototype_GetRegisterFlags(list, operand->RegisterIndex, &flags);
        return TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsVariable);
    }
    
    return totemBool_False;
}

totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandPrototype *prototype)
{
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, prototype);
    
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    if (!totemHashMap_Insert(&list->Scope->Identifiers, name->Value, name->Length, prototype->RegisterIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
	totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, prototype->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsVariable);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandPrototype *op)
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
    
	totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, op->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    reg->DataType = totemPublicDataType_Null;
    list->Null = op->RegisterIndex;
    list->HasNull = totemBool_True;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddBoolean(totemRegisterListPrototype *list, totemBool val, totemOperandPrototype *op)
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
    
	totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, op->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    reg->DataType = totemPublicDataType_Boolean;
	reg->Value.Boolean = val;
    
    list->Boolean[val] = op->RegisterIndex;
    list->HasBoolean[val] = totemBool_True;
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Numbers, number->Value, number->Length);
    if (searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->ScopeType;
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
        
		totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, operand->RegisterIndex);
        
		if (memchr(number->Value, '.', number->Length) != NULL || 
			memchr(number->Value, 'e', number->Length) != NULL || 
			memchr(number->Value, 'E', number->Length) != NULL)
        {
			reg->Value.Float = totem_strtoFloat(number->Value, NULL);
            reg->DataType = totemPublicDataType_Float;
        }
        else
        {
			reg->Value.Int = atoi(number->Value);
            reg->DataType = totemPublicDataType_Int;
        }
        
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemString *name, totemFunctionPointerPrototype *value, totemOperandPrototype *operand)
{
    if (name)
    {
        if (totemRegisterListPrototype_GetIdentifier(list, name, operand, totemBool_False))
        {
            return totemEvalStatus_Success;
        }
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    if (name)
    {
        if (!totemHashMap_Insert(&list->Scope->Identifiers, name->Value, name->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    totemRegisterPrototype *reg = totemRegisterPrototypeBuffer_At(&list->Registers, operand->RegisterIndex);
	reg->Value.FunctionPointer = *value;
    reg->DataType = totemPublicDataType_Function;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

void totemRegisterListPrototypeScope_Init(totemRegisterListPrototypeScope *scope)
{
    totemHashMap_Init(&scope->MoveToLocalVars);
    totemHashMap_Init(&scope->Identifiers);
    scope->Prev = NULL;
}

void totemRegisterListPrototypeScope_Reset(totemRegisterListPrototypeScope *scope)
{
    totemHashMap_Reset(&scope->MoveToLocalVars);
    totemHashMap_Reset(&scope->Identifiers);
    scope->Prev = NULL;
}

void totemRegisterListPrototypeScope_Cleanup(totemRegisterListPrototypeScope *scope)
{
    totemHashMap_Cleanup(&scope->MoveToLocalVars);
    totemHashMap_Cleanup(&scope->Identifiers);
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
	for (totemInt i = 0; i < scope->MoveToLocalVars.NumBuckets; i++)
    {
        totemHashMapEntry *bucket = scope->MoveToLocalVars.Buckets[i];
        while (bucket)
        {
            totemOperandPrototype operand;
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
	for (totemInt i = 0; i < list->Scope->Identifiers.NumBuckets; i++)
    {
        totemHashMapEntry *bucket = list->Scope->Identifiers.Buckets[i];
        while (bucket)
        {
            totemOperandPrototype operand;
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
	TOTEM_ZERO(&list->DataTypes);
	TOTEM_ZERO(&list->HasDataType);
	TOTEM_ZERO(&list->Boolean);
	TOTEM_ZERO(&list->HasBoolean);
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemHashMap_Init(&list->Numbers);
    totemHashMap_Init(&list->Strings);
	totemRegisterPrototypeBuffer_Init(&list->Registers);
	totemOperandXUnsignedBuffer_Init(&list->RegisterFreeList);
    list->Scope = NULL;
}

void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list)
{
    while (list->Scope)
    {
        totemRegisterListPrototype_FreeScope(list);
    }
    
	TOTEM_ZERO(&list->DataTypes);
	TOTEM_ZERO(&list->HasDataType);
	TOTEM_ZERO(&list->Boolean);
	TOTEM_ZERO(&list->HasBoolean);
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemHashMap_Reset(&list->Numbers);
    totemHashMap_Reset(&list->Strings);
	totemRegisterPrototypeBuffer_Reset(&list->Registers);
	totemOperandXUnsignedBuffer_Reset(&list->RegisterFreeList);
}

void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list)
{
    while (list->Scope)
    {
        totemRegisterListPrototype_FreeScope(list);
    }
    
	TOTEM_ZERO(&list->DataTypes);
	TOTEM_ZERO(&list->HasDataType);
	TOTEM_ZERO(&list->Boolean);
	TOTEM_ZERO(&list->HasBoolean);
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemHashMap_Cleanup(&list->Numbers);
    totemHashMap_Cleanup(&list->Strings);
	totemRegisterPrototypeBuffer_Cleanup(&list->Registers);
	totemOperandXUnsignedBuffer_Cleanup(&list->RegisterFreeList);
}
