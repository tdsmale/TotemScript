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
    totemOperandXu index = 0;
    totemRegisterPrototype *reg = NULL;
    size_t freelistSize = totemOperandXuDynBuffer_Length(&list->RegisterFreeList);
    size_t numRegisters = totemRegisterPrototypeDynBuffer_Length(&list->Registers);
    
    if (freelistSize > 0)
    {
		totemOperandXu *indexPtr = totemOperandXuDynBuffer_Top(&list->RegisterFreeList);
        index = *indexPtr;
		totemOperandXuDynBuffer_Pop(&list->RegisterFreeList, 1);
        reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    }
    else
    {
        size_t max = TOTEM_MAX_LOCAL_REGISTERS;
        if (list->ScopeType == totemOperandType_GlobalRegister)
        {
            max = TOTEM_MAX_GLOBAL_REGISTERS;
        }
        
        if (numRegisters >= max)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyRegisters);
        }
        
        index = (totemOperandXu)totemRegisterPrototypeDynBuffer_Length(&list->Registers);
        
        reg = totemRegisterPrototypeDynBuffer_Secure(&list->Registers, 1);
        
        if (!reg)
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    reg->Value.Int = 0;
    reg->DataType = totemPublicDataType_Max;
    reg->Flags = totemRegisterPrototypeFlag_IsTemporary | totemRegisterPrototypeFlag_IsUsed;
    
    operand->RegisterIndex = index;
    
    TOTEM_LOG_V(Eval, Register, Debug, "Adding reg %i %p", operand->RegisterIndex, list->Scope);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, operand->RegisterIndex);
    
    TOTEM_LOG_V(Eval, Register, Debug, "Freeing reg %i %p", operand->RegisterIndex, list->Scope);
    
    if (reg->Flags != totemRegisterPrototypeFlag_None)
    {
		if (!totemOperandXuDynBuffer_Push(&list->RegisterFreeList, &operand->RegisterIndex, 1))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        reg->Flags = totemRegisterPrototypeFlag_None;
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags &= ~flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags |= flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeFlag *flags)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *flags = reg->Flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXu index, totemPublicDataType *type)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *type = reg->DataType;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXu index, totemPublicDataType type)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->DataType = type;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterValue(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeValue *valueOut)
{
	totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
	if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
	{
		return totemBool_False;
	}

	*valueOut = reg->Value;
	return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterValue(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeValue *valueOut)
{
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }

    reg->Value = *valueOut;
    return totemBool_True;
}


totemBool totemRegisterListPrototype_GetRegisterValueAndType(totemRegisterListPrototype* list, totemOperandXu index, totemRegisterPrototypeValue* valueOut, totemPublicDataType* typeOut)
{
    return (totemRegisterListPrototype_GetRegisterValue(list, index, valueOut) && totemRegisterListPrototype_GetRegisterType(list, index, typeOut));
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
        return totemEvalStatus_Success;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(list, op));
    
	totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, op->RegisterIndex);
    reg->DataType = totemPublicDataType_Type;
	reg->Value.Type = type;
    
    list->DataTypes[type] = op->RegisterIndex;
    list->HasDataType[type] = totemBool_True;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    TOTEM_LOG_V(Eval, Register, Debug, "adding type %s to index %i", totemPublicDataType_Describe(type), op->RegisterIndex);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemParseString *str, totemOperandPrototype *operand)
{
    totemRegisterPrototype *reg = NULL;
    totemEvalStrHashMapKey key;
    key.Key = str;
    if (totemEvalStrHashMap_Get(&list->Strings, &key))
    {
        operand->RegisterIndex = key.Value;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if (status != totemEvalStatus_Success)
        {
            return status;
        }
        
		reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, operand->RegisterIndex);
        reg->DataType = totemPublicDataType_String;
		reg->Value.String = str;
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
        
        key.Value = operand->RegisterIndex;
        if (!totemEvalStrHashMap_Insert(&list->Strings, &key))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        TOTEM_LOG_BEGIN(Eval, Register, Debug, logger)
            totemLog_Write(logger, "adding string constant ");
            totemParseString_Print(key.Key, logger);
            totemLog_Write(logger, " to index %i", operand->RegisterIndex);
        TOTEM_LOG_END()
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_GetIdentifier(totemRegisterListPrototype *list, totemParseString *name, totemOperandPrototype *operand, totemBool currentOnly)
{
    for (totemRegisterListPrototypeScope *scope = list->Scope;
         scope;
         scope = scope->Prev)
    {
        totemEvalStrHashMapKey key;
        key.Key = name;
        if (totemEvalStrHashMap_Get(&scope->Identifiers, &key))
        {
            operand->RegisterIndex = key.Value;
            
            return totemBool_True;
        }
        
        if (currentOnly)
        {
            break;
        }
    }
    
    return totemBool_False;
}

totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemParseString *name, totemOperandPrototype *operand, totemBool currentOnly)
{
    if (totemRegisterListPrototype_GetIdentifier(list, name, operand, currentOnly))
    {
        totemRegisterPrototypeFlag flags = totemRegisterPrototypeFlag_None;
        totemRegisterListPrototype_GetRegisterFlags(list, operand->RegisterIndex, &flags);
        return TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsVariable);
    }
    
    return totemBool_False;
}

totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemParseString *name, totemOperandPrototype *prototype)
{
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, prototype);
    
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
    totemEvalStrHashMapKey key;
    key.Key = name;
    key.Value = prototype->RegisterIndex;
    if (!totemEvalStrHashMap_Insert(&list->Scope->Identifiers, &key))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
	totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, prototype->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsVariable);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    TOTEM_LOG_BEGIN(Eval, Register, Debug, logger)
        totemLog_Write(logger, "adding variable ");
        totemParseString_Print(key.Key, logger);
        totemLog_Write(logger, " to index %i (scope %i)", prototype->RegisterIndex, list->ScopeType);
    TOTEM_LOG_END()
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandPrototype *op)
{
    if (list->HasNull)
    {
        op->RegisterIndex = list->Null;
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, op);
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
	totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, op->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    reg->DataType = totemPublicDataType_Null;
    list->Null = op->RegisterIndex;
    list->HasNull = totemBool_True;
    
    TOTEM_LOG_V(Eval, Register, Debug, "adding null to index %i", op->RegisterIndex);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddBool(totemRegisterListPrototype *list, totemBool val, totemOperandPrototype *op)
{
    val = val != totemBool_False;
    
    if (list->HasBool[val])
    {
        op->RegisterIndex = list->Bool[val];
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, op);
    if (status != totemEvalStatus_Success)
    {
        return status;
    }
    
	totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, op->RegisterIndex);
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    reg->DataType = totemPublicDataType_Boolean;
	reg->Value.Bool = val;
    
    list->Bool[val] = op->RegisterIndex;
    list->HasBool[val] = totemBool_True;
    
    TOTEM_LOG_V(Eval, Register, Debug, "adding bool %i to index %i", val, op->RegisterIndex);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddIntString(totemRegisterListPrototype *list, totemParseString *number, totemOperandPrototype *operand)
{
    char buffer[256];
    if (totemParseString_GetLength(number) >= sizeof(buffer))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }

    totemParseString_ExtractValue(number, buffer, sizeof(buffer));
    totemInt numVal = totem_strtoInt(buffer);
    return totemRegisterListPrototype_AddIntConstant(list, numVal, operand);
}

totemEvalStatus totemRegisterListPrototype_AddIntConstant(totemRegisterListPrototype *list, totemInt number, totemOperandPrototype *operand)
{
    totemEvalIntHashMapKey key;
    key.Key = number;
    if (totemEvalIntHashMap_Get(&list->Ints, &key))
    {
        operand->RegisterIndex = key.Value;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if (status != totemEvalStatus_Success)
        {
            return status;
        }
        
        key.Key = number;
        key.Value = operand->RegisterIndex;
        if (!totemEvalIntHashMap_Insert(&list->Ints, &key))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, operand->RegisterIndex);
        reg->Value.Int = number;
        reg->DataType = totemPublicDataType_Int;
        
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
        
        TOTEM_LOG_V(Eval, Register, Debug, "adding int constant %"TOTEM_INT_PRINTF" to index %i", number, operand->RegisterIndex);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddFloatString(totemRegisterListPrototype *list, totemParseString *number, totemOperandPrototype *operand)
{
    char buffer[256];
    if (totemParseString_GetLength(number) >= sizeof(buffer))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }

    totemParseString_ExtractValue(number, buffer, sizeof(buffer));
    totemFloat numVal = totem_strtoFloat(buffer, NULL);
    return totemRegisterListPrototype_AddFloatConstant(list, numVal, operand);
}

totemEvalStatus totemRegisterListPrototype_AddFloatConstant(totemRegisterListPrototype *list, totemFloat number, totemOperandPrototype *operand)
{
    totemEvalFloatHashMapKey key;
    key.Key = number;
    if (totemEvalFloatHashMap_Get(&list->Floats, &key))
    {
        operand->RegisterIndex = key.Value;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if (status != totemEvalStatus_Success)
        {
            return status;
        }
        
        key.Key = number;
        key.Value = operand->RegisterIndex;
        if (!totemEvalFloatHashMap_Insert(&list->Floats, &key))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
		totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, operand->RegisterIndex);
        reg->Value.Float = number;
        reg->DataType = totemPublicDataType_Float;
        
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
        
        TOTEM_LOG_V(Eval, Register, Debug, "adding float constant %f to index %i", number, operand->RegisterIndex);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemParseString *name, totemFunctionPointerPrototype *value, totemOperandPrototype *operand)
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
        totemEvalStrHashMapKey key;
        key.Key = name;
        key.Value = operand->RegisterIndex;
        if (!totemEvalStrHashMap_Insert(&list->Scope->Identifiers, &key))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    totemRegisterPrototype *reg = totemRegisterPrototypeDynBuffer_At(&list->Registers, operand->RegisterIndex);
	reg->Value.FunctionPointer = *value;
    reg->DataType = totemPublicDataType_Function;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    TOTEM_LOG_BEGIN(Eval, Register, Debug, logger)
        totemLog_Write(logger, "adding function ptr ");
        if(name)
        {
            totemParseString_Print(name, logger);
        }
        totemLog_Write(logger, " to index %i", operand->RegisterIndex);
    TOTEM_LOG_END()
    
    return totemEvalStatus_Success;
}

void totemRegisterListPrototypeScope_Init(totemRegisterListPrototypeScope *scope)
{
    totemEvalOperandHashMap_Init(&scope->MoveToLocalVars);
    totemEvalStrHashMap_Init(&scope->Identifiers);
    scope->Prev = NULL;
}

void totemRegisterListPrototypeScope_Reset(totemRegisterListPrototypeScope *scope)
{
    totemEvalOperandHashMap_Reset(&scope->MoveToLocalVars);
    totemEvalStrHashMap_Reset(&scope->Identifiers);
    scope->Prev = NULL;
}

void totemRegisterListPrototypeScope_Cleanup(totemRegisterListPrototypeScope *scope)
{
    totemEvalOperandHashMap_Cleanup(&scope->MoveToLocalVars);
    totemEvalStrHashMap_Cleanup(&scope->Identifiers);
    scope->Prev = NULL;
}

totemEvalStatus totemRegisterListPrototype_EnterScope(totemRegisterListPrototype *list)
{
    totemRegisterListPrototypeScope *newScope = TOTEM_MALLOC(sizeof(totemRegisterListPrototypeScope));
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
    TOTEM_FREE(scope, sizeof(totemRegisterListPrototypeScope));
}

totemEvalStatus totemRegisterListPrototypeScope_FreeGlobalCache(totemRegisterListPrototypeScope *scope, totemRegisterListPrototype *list)
{
	for (uint32_t i = 0; i < scope->MoveToLocalVars.NumBuckets; i++)
    {
        totemHashMapEntry *bucket = scope->MoveToLocalVars.Buckets[i];
        while (bucket)
        {
            totemEvalOperandHashMapEntry* e = (totemEvalOperandHashMapEntry*)bucket;
            totemOperandPrototype operand;
            operand.RegisterIndex = e->Data.Value;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(list, &operand));
            
            bucket = bucket->Next;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_ExitScope(totemRegisterListPrototype *list)
{
	for (uint32_t i = 0; i < list->Scope->Identifiers.NumBuckets; i++)
    {
        totemHashMapEntry *bucket = list->Scope->Identifiers.Buckets[i];
        while (bucket)
        {
            totemEvalStrHashMapEntry *ae = (totemEvalStrHashMapEntry*)bucket;
            
            TOTEM_LOG_BEGIN(Eval, Register, Debug, logger);
                totemLog_Write(logger, "Freeing identifier from scope: ");
                totemParseString_Print(ae->Data.Key, logger);
            TOTEM_LOG_END();

            totemOperandPrototype operand;
            operand.RegisterIndex = ae->Data.Value;
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
	TOTEM_ZERO(list->DataTypes);
	TOTEM_ZERO(list->HasDataType);
	TOTEM_ZERO(list->Bool);
	TOTEM_ZERO(list->HasBool);
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemEvalFloatHashMap_Init(&list->Floats);
    totemEvalIntHashMap_Init(&list->Ints);
    totemEvalStrHashMap_Init(&list->Strings);
	totemRegisterPrototypeDynBuffer_Init(&list->Registers);
	totemOperandXuDynBuffer_Init(&list->RegisterFreeList);
    list->Scope = NULL;
}

void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list)
{
    while (list->Scope)
    {
        totemRegisterListPrototype_FreeScope(list);
    }
    
	TOTEM_ZERO(list->DataTypes);
	TOTEM_ZERO(list->HasDataType);
	TOTEM_ZERO(list->Bool);
	TOTEM_ZERO(list->HasBool);
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemEvalFloatHashMap_Reset(&list->Floats);
    totemEvalIntHashMap_Reset(&list->Ints);
    totemEvalStrHashMap_Reset(&list->Strings);
	totemRegisterPrototypeDynBuffer_Reset(&list->Registers);
	totemOperandXuDynBuffer_Reset(&list->RegisterFreeList);
}

void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list)
{
    while (list->Scope)
    {
        totemRegisterListPrototype_FreeScope(list);
    }
    
	TOTEM_ZERO(list->DataTypes);
	TOTEM_ZERO(list->HasDataType);
	TOTEM_ZERO(list->Bool);
	TOTEM_ZERO(list->HasBool);
    list->HasNull = totemBool_False;
    list->Null = 0;
    totemEvalFloatHashMap_Cleanup(&list->Floats);
    totemEvalIntHashMap_Cleanup(&list->Ints);
    totemEvalStrHashMap_Cleanup(&list->Strings);
	totemRegisterPrototypeDynBuffer_Cleanup(&list->Registers);
	totemOperandXuDynBuffer_Cleanup(&list->RegisterFreeList);
}

/*
 todo - what is this for? it is not used anywhere...
totemEvalStatus totemBuildPrototype_ResolveOperandIndex(totemBuildPrototype* build, totemOperandPrototype* op)
{
    if (TOTEM_HASANYBITS(op->Flags, totemOperandPrototypeFlag_Allocated))
    {
        return totemEvalStatus_Success;
    }

    if (TOTEM_HASANYBITS(op->Flags, totemOperandPrototypeFlag_IsVariable))
    {
        // variables should be allocated already...
        return totemEvalStatus_Break(totemEvalStatus_);
    }

    switch (op->ValueType)
    {
    case totemPublicDataType_Boolean:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddBool(&build->GlobalRegisters, op->Value.Bool, ));
        break;

    case totemPublicDataType_Int:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddInt(&build->GlobalRegisters, op->Value.Int, ));
        break;

    case totemPublicDataType_Float:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddFloatString(&build->GlobalRegisters, op->Value.Float, ));
        break;

    case totemPublicDataType_String:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddStringConstant(&build->GlobalRegisters, op->Value.String, ));
        break;

    case totemPublicDataType_Null:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNull(&build->GlobalRegisters, ));
        break;

    case totemPublicDataType_Type:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddType(&build->GlobalRegisters, op->Value.Type, ));
        break;

    case totemPublicDataType_Function:
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddFunctionPointer(&build->GlobalRegisters, op->Value.FunctionPointer, ));
        break;
        
    case totemPublicDataType_Array:
        todo;
        break;

    case totemPublicDataType_Object:
        todo;
        break;

    default:
        return totemEvalStatus_Break(totemEvalStatus_);
    }

    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_GlobalCacheCheck(build, op, ));
    return totemEvalStatus_Success;
}
*/
