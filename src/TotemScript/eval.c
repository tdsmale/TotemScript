//
//  eval.c
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>

#define TOTEM_EVAL_CHECKRETURN(exp) { totemEvalStatus status = exp; if(status != totemEvalStatus_Success) return status; }
#define TOTEM_EVAL_CHECKRETURN_EXTRA(exp, extra) { totemEvalStatus status = exp; if(status != totemEvalStatus_Success) { extra; return status; } }
#define TOTEM_EVAL_SETINSTRUCTIONBITS(ins, val, min, max, start) \
if ((val) > (max) || (val) < (min)) \
{ \
    return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow); \
} \
TOTEM_SETBITS_OFFSET(ins, val, start);

typedef enum
{
    totemEvalLoopFlag_None = 0,
    totemEvalLoopFlag_StartingCondition = 1
}
totemEvalLoopFlag;

typedef struct totemEvalLoopPrototype
{
    size_t LoopBeginIndex;
    size_t ConditionFailIndex;
    size_t ConditionIndex;
    size_t BreakIndex;
    struct totemEvalLoopPrototype *Next;
    totemEvalLoopFlag Flags;
}
totemEvalLoopPrototype;

void totemEvalLoopPrototype_Begin(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->Next = NULL;
    loop->ConditionFailIndex = 0;
    loop->ConditionIndex = 0;
    loop->Flags = totemEvalLoopFlag_None;
    loop->LoopBeginIndex = 0;
    loop->BreakIndex = 0;
}

void totemEvalLoopPrototype_SetStartPosition(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->LoopBeginIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
}

totemEvalStatus totemEvalLoopPrototype_SetCondition(totemEvalLoopPrototype *loop, totemBuildPrototype *build, totemExpressionPrototype *condition)
{
    totemOperandRegisterPrototype conditionOp;
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(condition, build, NULL, &conditionOp, totemEvalVariableFlag_None));
    
    loop->ConditionIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionSigned(build, &conditionOp, 0, totemOperationType_ConditionalGoto));
    TOTEM_SETBITS(loop->Flags, totemEvalLoopFlag_StartingCondition);
    
    return totemEvalStatus_Success;
}

void totemEvalLoopPrototype_SetConditionFailPosition(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->ConditionFailIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
}

totemEvalStatus totemEvalLoopPrototype_AddBreak(totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    // add goto to edit later
    child->BreakIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstructionSigned(build, 0, totemOperationType_Goto));
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_AddBreakChild(totemEvalLoopPrototype *parent, totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    if(parent->Next != NULL)
    {
        child->Next = parent->Next;
    }
    
    parent->Next = child;
    
    return totemEvalLoopPrototype_AddBreak(child, build);
}

totemEvalStatus totemEvalLoopPrototype_Loop(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    size_t numInstructions = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    totemOperandXSigned resetLoopOffset = (totemOperandXSigned)loop->LoopBeginIndex - (totemOperandXSigned)numInstructions;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstructionSigned(build, resetLoopOffset, totemOperationType_Goto));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_EndChild(totemEvalLoopPrototype *loop, totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    size_t numInstructions = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    
    if(TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
    {
        totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->ConditionIndex);
        
        if(child->ConditionFailIndex != numInstructions - 1)
        {
            totemOperandXUnsigned offset = (totemOperandXUnsigned)child->ConditionFailIndex - (totemOperandXUnsigned)child->ConditionIndex;
            TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(gotoInstruction, offset));
        }
    }
    
    if(child->BreakIndex != 0)
    {
        totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->BreakIndex);
        totemOperandXUnsigned offset = (totemOperandXUnsigned)numInstructions - (totemOperandXUnsigned)child->BreakIndex;
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxSigned(gotoInstruction, offset));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_End(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    for(totemEvalLoopPrototype *child = loop; child != NULL; child = child->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(loop, child, build));
    }
    
    // clean up condition registers after all loops have been evaluated
    for(totemEvalLoopPrototype *child = loop; child != NULL; child = child->Next)
    {
        if(TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
        {
            totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->ConditionIndex);
            totemOperandRegisterPrototype condition;
            condition.RegisterScopeType = TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(*gotoInstruction);
            condition.RegisterIndex = TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(*gotoInstruction);
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &condition));
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalStatus_Break(totemEvalStatus status)
{
    return status;
}

totemEvalStatus totemInstruction_SetRegister(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope, uint32_t start)
{
    uint32_t reg = scope;
    if(scope > 1)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    TOTEM_SETBITS_OFFSET(reg, index, 1);
    TOTEM_SETBITS_OFFSET(*instruction, reg, start);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetSignedValue(totemInstruction *instruction, int32_t value, uint32_t start, uint32_t numBits)
{
    uint32_t mask = 0;
    uint32_t isNegative = value < 0;
    uint32_t unsignedValue = *((uint32_t*)(&value));
    
    if(isNegative)
    {
        unsignedValue = ~unsignedValue;
        unsignedValue++;
    }
    
    // value
    TOTEM_SETBITS(mask, (unsignedValue & TOTEM_BITMASK(0, numBits - 1)));
    
    // signed bit
    TOTEM_SETBITS_OFFSET(mask, isNegative, numBits - 1);
    
    // add to instruction
    TOTEM_SETBITS_OFFSET(*instruction, mask, start);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetRegisterA(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_A);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterB(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_B);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterC(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope)
{
    totemEvalStatus status =  totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_C);
    return status;
}

totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op)
{
    TOTEM_EVAL_SETINSTRUCTIONBITS(*instruction, op, TOTEM_MINVAL_UNSIGNED(totemInstructionSize_Op), TOTEM_MAXVAL_UNSIGNED(totemInstructionSize_Op), totemInstructionStart_Op);
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetBxUnsigned(totemInstruction *instruction, totemOperandXUnsigned bx)
{
    TOTEM_EVAL_SETINSTRUCTIONBITS(*instruction, bx, TOTEM_MINVAL_UNSIGNED(totemInstructionSize_Bx), TOTEM_MAXVAL_UNSIGNED(totemInstructionSize_Bx), totemInstructionStart_B);
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetBxSigned(totemInstruction *instruction, totemOperandXSigned bx)
{
    totemOperandXSigned min = TOTEM_MINVAL_SIGNED(totemInstructionSize_Bx);
    totemOperandXSigned max = TOTEM_MAXVAL_SIGNED(totemInstructionSize_Bx);
    
    if(bx < min || bx > max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    return totemInstruction_SetSignedValue(instruction, bx, totemInstructionStart_B, totemInstructionSize_Bx);
}

totemEvalStatus totemInstruction_SetAxSigned(totemInstruction *instruction, totemOperandXSigned ax)
{
    totemOperandXSigned min = TOTEM_MINVAL_SIGNED(totemInstructionSize_Ax);
    totemOperandXSigned max = TOTEM_MAXVAL_SIGNED(totemInstructionSize_Ax);
    
    if(ax < min || ax > max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    return totemInstruction_SetSignedValue(instruction, ax, totemInstructionStart_A, totemInstructionSize_Ax);
}

totemEvalStatus totemInstruction_SetAxUnsigned(totemInstruction *instruction, totemOperandXUnsigned ax)
{
    TOTEM_EVAL_SETINSTRUCTIONBITS(*instruction, ax, TOTEM_MINVAL_SIGNED(totemInstructionSize_Ax), TOTEM_MAXVAL_SIGNED(totemInstructionSize_Ax), totemInstructionStart_A);
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    totemOperandXUnsigned index = 0;
    totemRegisterPrototype *reg = NULL;
    totemBool isFromFreeList = totemBool_False;
    
    if(totemMemoryBuffer_GetNumObjects(&list->RegisterFreeList) > 0)
    {
        totemOperandXUnsigned *indexPtr = totemMemoryBuffer_Top(&list->RegisterFreeList);
        index = *indexPtr;
        totemMemoryBuffer_Pop(&list->RegisterFreeList, 1);
        reg = totemMemoryBuffer_Get(&list->Registers, index);
        isFromFreeList = totemBool_True;
    }
    else
    {
        size_t max = list->Scope == totemOperandType_GlobalRegister ? TOTEM_MAX_GLOBAL_REGISTERS : TOTEM_MAX_LOCAL_REGISTERS;
        
        if(totemMemoryBuffer_GetNumObjects(&list->Registers) >= max)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyRegisters);
        }
        
        index = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&list->Registers);
        
        reg = totemMemoryBuffer_Secure(&list->Registers, 1);
        if(!reg)
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    reg->Value.Data = 0;
    reg->RefCount = 1;
    reg->GlobalAssoc = 0;
    reg->DataType = totemPublicDataType_Int;
    reg->Flags = totemRegisterPrototypeFlag_IsTemporary | totemRegisterPrototypeFlag_IsUsed;
    
    operand->RegisterIndex = index;
    operand->RegisterScopeType = list->Scope;
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    if(!totemMemoryBuffer_Insert(&list->RegisterFreeList, &operand->RegisterIndex, 1))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
    
    reg->Flags = totemRegisterPrototypeFlag_None;
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags &= ~flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->Flags |= flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag *flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *flags = reg->Flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType *type)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *type = reg->DataType;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType type)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->DataType = type;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_DecRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *countOut)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    if(reg->RefCount > 0)
    {
        reg->RefCount--;
    }
    
    if(countOut)
    {
        *countOut = reg->RefCount;
    }
    
    return totemBool_True;
}

totemBool totemRegisterListPrototype_IncRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *countOut)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->RefCount++;
    if(countOut)
    {
        *countOut = reg->RefCount;
    }
    
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *countOut)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    if(countOut)
    {
        *countOut = reg->RefCount;
    }
    
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterGlobalAssoc(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned assoc)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    reg->GlobalAssoc = assoc;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterGlobalAssoc(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned *assoc)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if (!reg || !TOTEM_HASBITS(reg->Flags, totemRegisterPrototypeFlag_IsUsed))
    {
        return totemBool_False;
    }
    
    *assoc = reg->GlobalAssoc;
    return totemBool_True;
}

totemEvalStatus totemRegisterListPrototype_AddType(totemRegisterListPrototype *list, totemPublicDataType type, totemOperandRegisterPrototype *op)
{
    if(type >= totemPublicDataType_Max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InvalidDataType);
    }
    
    if(list->HasDataType[type])
    {
        op->RegisterIndex = list->DataTypes[type];
        op->RegisterScopeType = list->Scope;
        totemRegisterListPrototype_IncRegisterRefCount(list, op->RegisterIndex, NULL);
        return totemEvalStatus_Success;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(list, op));
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, op->RegisterIndex);
    reg->DataType = totemPublicDataType_Type;
    reg->Value.DataType = type;
    
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
    if(searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->Scope;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if(status != totemEvalStatus_Success)
        {
            return status;
        }
        
        reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        reg->DataType = totemPublicDataType_String;
        reg->Value.InternedString = (void*)str;
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
        
        if(!totemHashMap_Insert(&list->Strings, str->Value, str->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Variables, name->Value, name->Length);
    if(searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->Scope;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype)
{
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, prototype);
    if(status != totemEvalStatus_Success)
    {
        return status;
    }
    
    if(!totemHashMap_Insert(&list->Variables, name->Value, name->Length, prototype->RegisterIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, prototype->RegisterIndex);
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsVariable);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Numbers, number->Value, number->Length);
    if(searchResult != NULL)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)searchResult->Value;
        operand->RegisterScopeType = list->Scope;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if(status != totemEvalStatus_Success)
        {
            return status;
        }
        
        if(!totemHashMap_Insert(&list->Numbers, number->Value, number->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        
        if(memchr(number->Value, '.', number->Length) != NULL)
        {
            reg->Value.Float = atof(number->Value);
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

totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemFunctionPointer *value, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *result = totemHashMap_Find(&list->FunctionPointers, value, sizeof(totemFunctionPointer));
    if(result)
    {
        operand->RegisterIndex = (totemOperandXUnsigned)result->Value;
        operand->RegisterScopeType = list->Scope;
        totemRegisterListPrototype_IncRegisterRefCount(list, operand->RegisterIndex, NULL);
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
    if(status != totemEvalStatus_Success)
    {
        return status;
    }
    
    if(!totemHashMap_Insert(&list->FunctionPointers, value, sizeof(totemFunctionPointer), operand->RegisterIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
    reg->Value.FunctionPointer = *value;
    reg->DataType = totemPublicDataType_Function;
    
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    TOTEM_UNSETBITS(reg->Flags, totemRegisterPrototypeFlag_IsTemporary);
    
    return totemEvalStatus_Success;
}

void totemRegisterListPrototype_Init(totemRegisterListPrototype *list, totemOperandType scope)
{
    memset(list->DataTypes, 0, sizeof(list->DataTypes));
    memset(list->HasDataType, 0, sizeof(list->HasDataType));
    totemHashMap_Init(&list->Numbers);
    totemHashMap_Init(&list->Strings);
    totemHashMap_Init(&list->Variables);
    totemHashMap_Init(&list->MoveToLocalVars);
    totemHashMap_Init(&list->FunctionPointers);
    totemMemoryBuffer_Init(&list->Registers, sizeof(totemRegisterPrototype));
    totemMemoryBuffer_Init(&list->RegisterFreeList, sizeof(totemOperandXUnsigned));
    list->Scope = scope;
}

void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list)
{
    memset(list->DataTypes, 0, sizeof(list->DataTypes));
    memset(list->HasDataType, 0, sizeof(list->HasDataType));
    totemHashMap_Reset(&list->Numbers);
    totemHashMap_Reset(&list->Strings);
    totemHashMap_Reset(&list->Variables);
    totemHashMap_Reset(&list->MoveToLocalVars);
    totemHashMap_Reset(&list->FunctionPointers);
    totemMemoryBuffer_Reset(&list->Registers);
    totemMemoryBuffer_Reset(&list->RegisterFreeList);
}

void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list)
{
    memset(list->DataTypes, 0, sizeof(list->DataTypes));
    memset(list->HasDataType, 0, sizeof(list->HasDataType));
    totemHashMap_Cleanup(&list->Numbers);
    totemHashMap_Cleanup(&list->Strings);
    totemHashMap_Cleanup(&list->Variables);
    totemHashMap_Cleanup(&list->MoveToLocalVars);
    totemHashMap_Cleanup(&list->FunctionPointers);
    totemMemoryBuffer_Cleanup(&list->Registers);
    totemMemoryBuffer_Cleanup(&list->RegisterFreeList);
}

void totemBuildPrototype_Init(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Init(&build->GlobalRegisters, totemOperandType_GlobalRegister);
    totemRegisterListPrototype_Init(&build->LocalRegisters, totemOperandType_LocalRegister);
    totemHashMap_Init(&build->FunctionLookup);
    totemHashMap_Init(&build->NativeFunctionNamesLookup);
    totemHashMap_Init(&build->AnonymousFunctions);
    totemMemoryBuffer_Init(&build->Functions, sizeof(totemScriptFunctionPrototype));
    totemMemoryBuffer_Init(&build->Instructions, sizeof(totemInstruction));
    totemMemoryBuffer_Init(&build->NativeFunctionNames, sizeof(totemString));
    totemMemoryBuffer_Init(&build->FunctionArguments, sizeof(totemOperandRegisterPrototype));
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
}

void totemBuildPrototype_Reset(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
    totemRegisterListPrototype_Reset(&build->LocalRegisters);
    totemHashMap_Reset(&build->FunctionLookup);
    totemHashMap_Reset(&build->NativeFunctionNamesLookup);
    totemHashMap_Reset(&build->AnonymousFunctions);
    totemMemoryBuffer_Reset(&build->Functions);
    totemMemoryBuffer_Reset(&build->Instructions);
    totemMemoryBuffer_Reset(&build->NativeFunctionNames);
    totemMemoryBuffer_Reset(&build->FunctionArguments);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
}

void totemBuildPrototype_Cleanup(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Cleanup(&build->GlobalRegisters);
    totemRegisterListPrototype_Cleanup(&build->LocalRegisters);
    totemHashMap_Cleanup(&build->FunctionLookup);
    totemHashMap_Cleanup(&build->NativeFunctionNamesLookup);
    totemHashMap_Cleanup(&build->AnonymousFunctions);
    totemMemoryBuffer_Cleanup(&build->Functions);
    totemMemoryBuffer_Cleanup(&build->Instructions);
    totemMemoryBuffer_Cleanup(&build->NativeFunctionNames);
    totemMemoryBuffer_Cleanup(&build->FunctionArguments);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
}

totemEvalStatus totemBuildPrototype_EvalFunctionArgumentsBegin(totemBuildPrototype *build, totemExpressionPrototype *parametersStart)
{
    // we need to eval the func-args before we call the function
    // 1. count num args
    totemOperandXUnsigned numArgs = 0;
    for(totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        if(numArgs == TOTEM_OPERANDX_UNSIGNED_MAX - 1)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyFunctionArguments);
        }
        
        numArgs++;
    }
    
    // 2. alloc mem
    totemMemoryBuffer_Reset(&build->FunctionArguments);
    totemOperandRegisterPrototype *funcArgs = totemMemoryBuffer_Secure(&build->FunctionArguments, numArgs);
    if(!funcArgs)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    // 3. eval args
    totemOperandXUnsigned currentArg = 0;
    for(totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype *operand = &funcArgs[currentArg];
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, NULL, operand, totemEvalVariableFlag_MustBeDefined));
        currentArg++;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalFunctionArgumentsEnd(totemBuildPrototype *build)
{
    totemOperandXUnsigned numObjects = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->FunctionArguments);
    
    for(size_t i = 0; i < numObjects; i++)
    {
        totemOperandRegisterPrototype *operand = totemMemoryBuffer_Get(&build->FunctionArguments, i);
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, operand, numObjects, totemOperationType_FunctionArg));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
    }
    
    return totemEvalStatus_Success;
}

totemRegisterListPrototype *totemBuildPrototype_GetRegisterList(totemBuildPrototype *build, totemOperandType scope)
{
    if(scope == totemOperandType_GlobalRegister)
    {
        return &build->GlobalRegisters;
    }
    
    return totemBuildPrototype_GetLocalScope(build);
}

totemRegisterListPrototype *totemBuildPrototype_GetLocalScope(totemBuildPrototype *build)
{
    /*
     if(build->LocalRegisters)
     {
     return build->LocalRegisters;
     }
     
     return &build->GlobalRegisters;*/
    
    return &build->LocalRegisters;
}

totemEvalStatus totemBuildPrototype_FreeGlobalAssocs(totemBuildPrototype *build, size_t max)
{
    size_t numFreed = 0;
    
    // free moved global registers if any still exist in local scope
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    for (size_t i = 0; i < localScope->MoveToLocalVars.NumBuckets; i++)
    {
        for (totemHashMapEntry *entry = localScope->MoveToLocalVars.Buckets[i]; entry != NULL;)
        {
            size_t count = 0;
            totemOperandXUnsigned globalAssoc = 0;
            
            //printf("free %lu\n", entry->Value);
            
            if(totemRegisterListPrototype_GetRegisterRefCount(localScope, (totemOperandXUnsigned)entry->Value, &count)
               && (count == 0 || max == 0)
               && totemRegisterListPrototype_GetRegisterGlobalAssoc(localScope, (totemOperandXUnsigned)entry->Value, &globalAssoc))
            {
                totemOperandRegisterPrototype dummy;
                memset(&dummy, 0, sizeof(totemOperandRegisterPrototype));
                dummy.RegisterIndex = (totemOperandXUnsigned)entry->Value;
                dummy.RegisterScopeType = totemOperandType_LocalRegister;
                
                // remove
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &dummy, globalAssoc, totemOperationType_MoveToGlobal));
                
                totemRegisterListPrototype_SetRegisterFlags(localScope, dummy.RegisterIndex, totemRegisterPrototypeFlag_IsTemporary);
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &dummy));
                
                entry = entry->Next;
                totemHashMap_Remove(&localScope->MoveToLocalVars, (const char*)&dummy, sizeof(totemOperandRegisterPrototype));
                
                numFreed++;
            }
            else
            {
                entry = entry->Next;
            }
        }
        
        if(max != 0 && numFreed >= max)
        {
            break;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_GlobalAssocCheck(totemBuildPrototype *build, totemOperandRegisterPrototype *opOut)
{
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    // if we can't access this in a normal instruction, move to local-scope
    if(opOut->RegisterScopeType == totemOperandType_GlobalRegister && opOut->RegisterIndex >= TOTEM_MAX_LOCAL_REGISTERS && TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EvalGlobalAssocs))
    {
        // is this register already in local scope?
        totemHashMapEntry *entry = totemHashMap_Find(&localScope->MoveToLocalVars, (const char*)opOut, sizeof(totemOperandRegisterPrototype));
        if(entry != NULL)
        {
            opOut->RegisterIndex = (totemOperandXUnsigned)entry->Value;
            opOut->RegisterScopeType = totemOperandType_LocalRegister;
            totemRegisterListPrototype_IncRegisterRefCount(localScope, opOut->RegisterIndex, NULL);
            return totemEvalStatus_Success;
        }
        
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(localScope, &dummy));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &dummy, opOut->RegisterIndex, totemOperationType_MoveToLocal));
        
        totemRegisterListPrototype_SetRegisterGlobalAssoc(localScope, dummy.RegisterIndex, opOut->RegisterIndex);
        
        //printf("add %d %d\n", dummy.RegisterIndex, opOut->RegisterIndex);
        
        // add to lookup
        if (!totemHashMap_Insert(&localScope->MoveToLocalVars, (const char*)opOut, sizeof(totemOperandRegisterPrototype), dummy.RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        totemRegisterPrototypeFlag flags;
        totemRegisterListPrototype_GetRegisterFlags(&build->GlobalRegisters, opOut->RegisterIndex, &flags);
        
        opOut->RegisterIndex = dummy.RegisterIndex;
        opOut->RegisterScopeType = totemOperandType_LocalRegister;
        
        totemRegisterListPrototype_UnsetRegisterFlags(localScope, opOut->RegisterIndex, totemRegisterPrototypeFlag_IsTemporary);
        totemRegisterListPrototype_SetRegisterFlags(localScope, opOut->RegisterIndex, flags | totemRegisterPrototypeFlag_IsGlobalAssoc);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandRegisterPrototype *opOut)
{
    totemEvalStatus status = totemEvalStatus_Success;
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    switch(preferredScope)
    {
        case totemOperandType_LocalRegister:
            status = totemRegisterListPrototype_AddRegister(localScope, opOut);
            if(status == totemEvalStatus_TooManyRegisters)
            {
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_FreeGlobalAssocs(build, 1));
                status = totemRegisterListPrototype_AddRegister(localScope, opOut);
            }
            break;
            
        case totemOperandType_GlobalRegister:
        {
            status = totemRegisterListPrototype_AddRegister(&build->GlobalRegisters, opOut);
            if(status == totemEvalStatus_Success)
            {
                status = totemBuildPrototype_GlobalAssocCheck(build, opOut);
            }
            
            break;
        }
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_RecycleRegister(totemBuildPrototype *build, totemOperandRegisterPrototype *op)
{
    totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, op->RegisterScopeType);
    
    totemRegisterPrototypeFlag flags;
    if(totemRegisterListPrototype_GetRegisterFlags(scope, op->RegisterIndex, &flags) && TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsUsed))
    {
        size_t refCount = 0;
        if(totemRegisterListPrototype_DecRegisterRefCount(scope, op->RegisterIndex, &refCount) && refCount == 0)
        {
            // free if temp
            if(TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsTemporary))
            {
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(scope, op));
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemString *number, totemOperandRegisterPrototype *operand)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNumberConstant(&build->GlobalRegisters, number, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, operand);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemString *buffer, totemOperandRegisterPrototype *operand)
{
    totemEvalStatus status = totemRegisterListPrototype_AddStringConstant(&build->GlobalRegisters, buffer, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, operand);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalFunctionName(totemBuildPrototype *build, totemString *name, totemFunctionPointer *func)
{
    // did we define this function in the script?
    totemHashMapEntry *searchResult = totemHashMap_Find(&build->FunctionLookup, name->Value, name->Length);
    if(searchResult)
    {
        func->Address = (totemOperandXUnsigned)searchResult->Value;
        func->Type = totemFunctionType_Script;
    }
    else
    {
        func->Type = totemFunctionType_Native;
        
        // add native function name
        totemHashMapEntry *result = totemHashMap_Find(&build->NativeFunctionNamesLookup, name->Value, name->Length);
        if(result)
        {
            func->Address = (totemOperandXUnsigned)result->Value;
        }
        else
        {
            if(totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames) >= TOTEM_MAX_NATIVEFUNCTIONS)
            {
                return totemEvalStatus_Break(totemEvalStatus_TooManyNativeFunctions);
            }
            
            func->Address = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames);
            
            if(totemMemoryBuffer_Insert(&build->NativeFunctionNames, name, 1) == NULL)
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
            
            if(!totemHashMap_Insert(&build->NativeFunctionNamesLookup, name->Value, name->Length, func->Address))
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemFunctionPointer *value, totemOperandRegisterPrototype *op)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFunctionPointer(&build->GlobalRegisters, value, op);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, op);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandRegisterPrototype *op)
{
    totemFunctionPointer ptr;
    ptr.Type = totemFunctionType_Script;
    
    totemHashMapEntry *result = totemHashMap_Find(&build->AnonymousFunctions, &func, sizeof(totemFunctionDeclarationPrototype*));
    if(result)
    {
        ptr.Address = (totemOperandXUnsigned)result->Value;
    }
    else
    {
        // increment script function count
        if(totemMemoryBuffer_GetNumObjects(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
        }
        
        totemOperandXUnsigned funcIndex = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->Functions);
        totemScriptFunctionPrototype *functionPrototype;
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
        
        if(!totemHashMap_Insert(&build->AnonymousFunctions, &func, sizeof(totemFunctionDeclarationPrototype*), funcIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        ptr.Address = funcIndex;
    }
    
    // enqueue function declaration to be evaluated after the current one
    func->Next = NULL;
    if(build->AnonymousFunctionHead)
    {
        build->AnonymousFunctionTail->Next = func;
    }
    else
    {
        build->AnonymousFunctionHead = func;
    }
    
    build->AnonymousFunctionTail = func;
    
    return totemBuildPrototype_EvalFunctionPointer(build, &ptr, op);
}

totemEvalStatus totemBuildPrototype_EvalNamedFunctionPointer(totemBuildPrototype *build, totemString *name, totemOperandRegisterPrototype *op)
{
    totemFunctionPointer value;
    memset(&value, 0, sizeof(totemFunctionPointer));
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionName(build, name, &value));
    return totemBuildPrototype_EvalFunctionPointer(build, &value, op);
}

totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandRegisterPrototype *operand)
{
    totemEvalStatus status = totemRegisterListPrototype_AddType(&build->GlobalRegisters, type, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, operand);
    }
    
    return status;
}

totemEvalStatus totemFunctionCallPrototype_EvalValues(totemFunctionCallPrototype *call, totemBuildPrototype *build)
{
    for(totemExpressionPrototype *exp = call->ParametersStart; exp != NULL; exp = exp->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(exp, build, totemEvalVariableFlag_MustBeDefined));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_EvalValues(totemVariablePrototype *var, totemBuildPrototype *build, totemEvalVariableFlag varFlags)
{
    totemOperandRegisterPrototype dummy;
    if(!totemRegisterListPrototype_GetVariable(&build->GlobalRegisters, &var->Identifier, &dummy))
    {
        if(TOTEM_HASBITS(varFlags, totemEvalVariableFlag_MustBeDefined))
        {
            return totemEvalStatus_Break(totemEvalStatus_VariableNotDefined);
        }
        
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(&build->GlobalRegisters, &var->Identifier, &dummy));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_EvalValues(totemArgumentPrototype *arg, totemBuildPrototype *build, totemEvalVariableFlag varFlags)
{
    totemOperandRegisterPrototype dummy;
    
    switch(arg->Type)
    {
        case totemArgumentType_Number:
            return totemBuildPrototype_EvalNumber(build, arg->Number, &dummy);
            
        case totemArgumentType_String:
            return totemBuildPrototype_EvalString(build, arg->String, &dummy);
            
        case totemArgumentType_FunctionCall:
            return totemFunctionCallPrototype_EvalValues(arg->FunctionCall, build);
            
        case totemArgumentType_FunctionPointer:
            return totemBuildPrototype_EvalNamedFunctionPointer(build, arg->FunctionPointer, &dummy);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, arg->FunctionDeclaration, &dummy);
            
        case totemArgumentType_Type:
            return totemBuildPrototype_EvalType(build, arg->DataType, &dummy);
            
        case totemArgumentType_Variable:
            if(TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EvalVariables))
            {
                return totemVariablePrototype_EvalValues(arg->Variable, build, varFlags);
            }
            break;
            
        case totemArgumentType_NewArray:
        case totemArgumentType_NewObject:
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_EvalValues(totemExpressionPrototype *expression, totemBuildPrototype *build, totemEvalVariableFlag varFlags)
{
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalValues(expression->LValueArgument, build, varFlags));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->LValueExpression, build, varFlags));
            break;
    }
    
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        totemString preUnaryNumber;
        memset(&preUnaryNumber, 0, sizeof(totemString));
        
        totemOperandRegisterPrototype preUnaryRegister;
        memset(&preUnaryNumber, 0, sizeof(totemOperandRegisterPrototype));
        
        switch(op->Type)
        {
            case totemPreUnaryOperatorType_Dec:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                totemString_FromLiteral(&preUnaryNumber, "0");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                // A = B == C(0)
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                // A = B * -1
                break;
                
            case totemPreUnaryOperatorType_None:
                break;
        }
    }
    
    for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        totemString postUnaryNumber;
        memset(&postUnaryNumber, 0, sizeof(totemString));
        
        totemOperandRegisterPrototype postUnaryRegister;
        memset(&postUnaryNumber, 0, sizeof(totemOperandRegisterPrototype));
        
        switch(op->Type)
        {
            case totemPostUnaryOperatorType_Dec:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->ArrayAccess, build, NULL, &postUnaryRegister, totemEvalVariableFlag_MustBeDefined));
                break;
                
            case totemPostUnaryOperatorType_Invocation:
                for(totemExpressionPrototype *parameter = op->InvocationParametersStart; parameter != NULL; parameter = parameter->Next)
                {
                    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(parameter, build, totemEvalVariableFlag_MustBeDefined));
                }
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    if(expression->RValue)
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->RValue, build, totemEvalVariableFlag_MustBeDefined));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemDoWhileLoopPrototype_EvalValues(totemDoWhileLoopPrototype *loop, totemBuildPrototype *build)
{
    for(totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    return totemExpressionPrototype_EvalValues(loop->Expression, build, totemEvalVariableFlag_None);
}

totemEvalStatus totemForLoopPrototype_EvalValues(totemForLoopPrototype *loop, totemBuildPrototype *build)
{
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Initialisation, build, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Condition, build, totemEvalVariableFlag_None));
    
    for(totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    return totemExpressionPrototype_EvalValues(loop->AfterThought, build, totemEvalVariableFlag_None);
}

totemEvalStatus totemIfBlockPrototype_EvalValues(totemIfBlockPrototype *loop, totemBuildPrototype *build)
{
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Expression, build, totemEvalVariableFlag_None));
    
    for(totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    switch(loop->ElseType)
    {
        case totemIfElseBlockType_ElseIf:
            return totemIfBlockPrototype_EvalValues(loop->IfElseBlock, build);
            
        case totemIfElseBlockType_Else:
            for(totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
            }
            return totemEvalStatus_Success;
            
        default:
            return totemEvalStatus_Success;
    }
}

totemEvalStatus totemWhileLoopPrototype_EvalValues(totemWhileLoopPrototype *loop, totemBuildPrototype *build)
{
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Expression, build, totemEvalVariableFlag_None));
    
    for(totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemStatementPrototype_EvalValues(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    switch(statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            return totemDoWhileLoopPrototype_EvalValues(statement->DoWhileLoop, build);
            
        case totemStatementType_ForLoop:
            return totemForLoopPrototype_EvalValues(statement->ForLoop, build);
            
        case totemStatementType_IfBlock:
            return totemIfBlockPrototype_EvalValues(statement->IfBlock, build);
            
        case totemStatementType_WhileLoop:
            return totemWhileLoopPrototype_EvalValues(statement->WhileLoop, build);
            
        case totemStatementType_Simple:
            return totemExpressionPrototype_EvalValues(statement->Simple, build, totemEvalVariableFlag_None);
            
        case totemStatementType_Return:
            return totemExpressionPrototype_EvalValues(statement->Return, build, totemEvalVariableFlag_MustBeDefined);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype)
{
    // global function is always at index 0
    totemScriptFunctionPrototype *globalFunction;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &globalFunction));
    
    // eval global values
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
    build->Flags = totemBuildPrototypeFlag_EvalVariables;
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(block->Statement, build));
                break;
                
            default:
                break;
        }
    }
    
    // eval values from functions into global scope
    build->Flags = totemBuildPrototypeFlag_None;
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; /* nada */)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                totemBool isAnonymous = totemBool_False;
                totemFunctionDeclarationPrototype *func = block->FuncDec;
                
                // if an anonynous functions needs evaluating, do that one instead
                if(build->AnonymousFunctionHead)
                {
                    func = build->AnonymousFunctionHead;
                    build->AnonymousFunctionHead = build->AnonymousFunctionHead->Next;
                    isAnonymous = totemBool_True;
                }
                
                if(!isAnonymous)
                {
                    // named function cannot already exist
                    if(totemHashMap_Find(&build->FunctionLookup, func->Identifier->Value, func->Identifier->Length) != NULL)
                    {
                        build->ErrorContext = func;
                        return totemEvalStatus_Break(totemEvalStatus_ScriptFunctionAlreadyDefined);
                    }
                    
                    // secure named function - anonymous functions are secured when evaluated as arguments
                    size_t functionIndex = totemMemoryBuffer_GetNumObjects(&build->Functions);
                    totemScriptFunctionPrototype *functionPrototype;
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
                    memcpy(&functionPrototype->Name, func->Identifier, sizeof(totemString));
                    
                    if(!totemHashMap_Insert(&build->FunctionLookup, func->Identifier->Value, func->Identifier->Length, functionIndex))
                    {
                        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
                    }
                }
                
                for(totemStatementPrototype *stmt = func->StatementsStart; stmt != NULL; stmt = stmt->Next)
                {
                    TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
                }
                
                if(!isAnonymous)
                {
                    block = block->Next;
                }
                
                break;
            }
                
            default:
                block = block->Next;
                break;
        }
    }
    
    // eval "global" function instructions first
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->Flags = totemBuildPrototypeFlag_EvalVariables | totemBuildPrototypeFlag_EvalGlobalAssocs;
    totemRegisterListPrototype_Reset(&build->LocalRegisters);
    globalFunction->InstructionsStart = 0;
    
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(block->Statement, build));
                break;
            }
                
            default:
                break;
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalImplicitReturn(build));
    globalFunction->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&build->LocalRegisters.Registers);
    
    // now eval all other function instructions
    totemOperandXUnsigned funcIndex = 1;
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; /* nada */)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                totemBool isAnonymous = totemBool_False;
                totemFunctionDeclarationPrototype *func = block->FuncDec;
                
                // if an anonynous functions needs evaluating, do that one instead
                if(build->AnonymousFunctionHead)
                {
                    func = build->AnonymousFunctionHead;
                    build->AnonymousFunctionHead = build->AnonymousFunctionHead->Next;
                    isAnonymous = totemBool_True;
                }
                
                totemScriptFunctionPrototype *functionPrototype = totemMemoryBuffer_Get(&build->Functions, funcIndex);
                functionPrototype->InstructionsStart = totemMemoryBuffer_GetNumObjects(&build->Instructions);
                
                totemRegisterListPrototype_Reset(&build->LocalRegisters);
                TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(func, build));
                
                functionPrototype->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&build->LocalRegisters.Registers);
                
                if(!isAnonymous)
                {
                    block = block->Next;
                }
                
                funcIndex++;
                break;
            }
                
            default:
                block = block->Next;
                break;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut)
{
    if(totemMemoryBuffer_GetNumObjects(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
    }
    
    *functionOut = totemMemoryBuffer_Secure(&build->Functions, 1);
    if(*functionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemBuildPrototype_AllocInstruction(totemBuildPrototype *build, totemInstruction **instructionOut)
{
    *instructionOut = totemMemoryBuffer_Secure(&build->Instructions, 1);
    if(*instructionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build)
{
    // parameters
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemVariablePrototype_Eval(parameter, build, &dummy, totemEvalVariableFlag_LocalOnly));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    return totemBuildPrototype_EvalImplicitReturn(build);
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    totemOperandRegisterPrototype result;
    memset(&result, 0, sizeof(totemOperandRegisterPrototype));
    
    switch(statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemDoWhileLoopPrototype_Eval(statement->DoWhileLoop, build));
            break;
            
        case totemStatementType_ForLoop:
            TOTEM_EVAL_CHECKRETURN(totemForLoopPrototype_Eval(statement->ForLoop, build));
            break;
            
        case totemStatementType_IfBlock:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_Eval(statement->IfBlock, build));
            break;
            
        case totemStatementType_WhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemWhileLoopPrototype_Eval(statement->WhileLoop, build));
            break;
            
        case totemStatementType_Simple:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Simple, build, NULL, &result, totemEvalVariableFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
            break;
            
        case totemStatementType_Return:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Return, build, NULL, &result, totemEvalVariableFlag_MustBeDefined));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, &result));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalArrayAccessEnd(totemBuildPrototype *build, totemOperandRegisterPrototype *lValue, totemOperandRegisterPrototype *lValueSrc, totemOperandRegisterPrototype *arrIndex)
{
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, lValueSrc, arrIndex, lValue, totemOperationType_ArraySet));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, arrIndex));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, lValueSrc));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandRegisterPrototype *lValueHint, totemOperandRegisterPrototype *result, totemEvalVariableFlag varFlags)
{
    totemOperandRegisterPrototype lValueSrc;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValueArgument, build, lValueHint, &lValueSrc, varFlags));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValueExpression, build, lValueHint, &lValueSrc, varFlags));
            break;
    }
    
    totemBool mutatedLValueRegisterUnary = totemBool_False;
    totemBool mutatedLValueRegisterBinary = totemBool_False;
    totemBool lValueRegisterIsArrayMember = totemBool_False;
    size_t numlValueArrayAccesses = 0;
    totemRegisterPrototypeFlag lValueSrcFlags = totemRegisterPrototypeFlag_None;
    totemRegisterListPrototype *lValueSrcScope = totemBuildPrototype_GetRegisterList(build, lValueSrc.RegisterScopeType);
    totemRegisterListPrototype_GetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, &lValueSrcFlags);
    
    // check the operators used against this lvalue to see if it gets mutated
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        if(op->Type == totemPreUnaryOperatorType_Dec || op->Type == totemPreUnaryOperatorType_Inc)
        {
            mutatedLValueRegisterUnary = totemBool_True;
            break;
        }
    }
    
    for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        if(op->Type == totemPostUnaryOperatorType_Dec || op->Type == totemPostUnaryOperatorType_Inc)
        {
            mutatedLValueRegisterUnary = totemBool_True;
        }
        
        if(op->Type == totemPostUnaryOperatorType_ArrayAccess)
        {
            numlValueArrayAccesses++;
            lValueRegisterIsArrayMember = totemBool_True;
        }
    }
    
    switch(expression->BinaryOperator)
    {
        case totemBinaryOperatorType_Assign:
        case totemBinaryOperatorType_DivideAssign:
        case totemBinaryOperatorType_MinusAssign:
        case totemBinaryOperatorType_MultiplyAssign:
        case totemBinaryOperatorType_PlusAssign:
            mutatedLValueRegisterBinary = totemBool_True;
            break;
            
        default:
            break;
    }
    
    if(mutatedLValueRegisterBinary | mutatedLValueRegisterUnary)
    {
        // array accesses cannot be modified
        if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsConst) && lValueRegisterIsArrayMember)
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // if already assigned, and is const, throw an error
        if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsAssigned | totemRegisterPrototypeFlag_IsConst))
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // assignment expressions can only happen on mutable lValues (variables)
        if(!TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsVariable) && !lValueRegisterIsArrayMember)
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // ONLY vars can be const
        if(!TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsVariable) && TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsConst))
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueCannotBeConst);
        }
        
        // ensure the array src is valid
        if(lValueRegisterIsArrayMember && TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsValue))
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // this lvalue is now assigned
        totemRegisterListPrototype_SetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
    }
    
    // when lValue is an array member, we must retrieve it first and place it in a register before any other op
    totemOperandRegisterPrototype lValue;
    totemOperandRegisterPrototype arrIndex;
    memcpy(&lValue, &lValueSrc, sizeof(totemOperandRegisterPrototype));
    totemRegisterPrototypeFlag lValueFlags = lValueSrcFlags;
    totemRegisterListPrototype *lValueScope = lValueSrcScope;
    
    if(lValueRegisterIsArrayMember)
    {
        size_t arrayAccessesPerformed = 0;
        
        for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
        {
            if(op->Type == totemPostUnaryOperatorType_ArrayAccess)
            {
                // we don't need to hang onto all the array srcs, just the last one
                if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsTemporary))
                {
                    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(lValueSrcScope, &lValueSrc));
                }
                
                // lvalue is now the src
                memcpy(&lValueSrc, &lValue, sizeof(totemOperandRegisterPrototype));
                lValueSrcFlags = lValueFlags;
                lValueSrcScope = lValueScope;
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->ArrayAccess, build, NULL, &arrIndex, totemEvalVariableFlag_MustBeDefined));
                arrayAccessesPerformed++;
                
                if(arrayAccessesPerformed == numlValueArrayAccesses)
                {
                    // if this is a move, we can skip this array_get - we'll be grabbing the new value and performing a direct array_set further below
                    if(expression->BinaryOperator == totemBinaryOperatorType_Assign)
                    {
                        break;
                    }
                    
                    // if caller has given us a hint, we need to use it for the last array access
                    if(lValueHint)
                    {
                        memcpy(&lValue, lValueHint, sizeof(totemOperandRegisterPrototype));
                    }
                    else
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                    }
                }
                else
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                }
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValueSrc, &arrIndex, totemOperationType_ArrayGet));
                
                lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
                totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
                
                if(arrayAccessesPerformed == numlValueArrayAccesses)
                {
                    break;
                }
            }
        }
    }
    
    totemOperandRegisterPrototype preUnaryRegister, preUnaryLValue;
    totemString preUnaryNumber;
    totemString_FromLiteral(&preUnaryNumber, "0");
    
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
            case totemPreUnaryOperatorType_Dec:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                totemString_FromLiteral(&preUnaryNumber, "0");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Equals));
                // A = B == C(0)
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Multiply));
                // A = B * -1
                break;
                
            case totemPreUnaryOperatorType_None:
                break;
        }
    }
    
    totemOperandRegisterPrototype postUnaryRegister;
    totemString postUnaryNumber;
    totemString_FromLiteral(&postUnaryNumber, "0");
    
    for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
            case totemPostUnaryOperatorType_Dec:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
                // already done above
                break;
                
            case totemPostUnaryOperatorType_Invocation:
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsValue))
                {
                    totemPublicDataType dataType = 0;
                    if(totemRegisterListPrototype_GetRegisterType(lValueScope, lValue.RegisterIndex, &dataType))
                    {
                        if(dataType != totemPublicDataType_Function)
                        {
                            return totemEvalStatus_Break(totemEvalStatus_InvalidDataType);
                        }
                    }
                }
                
                // retain pointer address
                totemOperandRegisterPrototype pointer;
                memcpy(&pointer, &lValue, sizeof(totemOperandRegisterPrototype));
                
                // replace lvalue, we're done with it
                if(lValueRegisterIsArrayMember && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary))
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
                }
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                lValueRegisterIsArrayMember = totemBool_False;
                lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
                totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsBegin(build, op->InvocationParametersStart));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &pointer, &pointer, totemOperationType_FunctionPointer));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsEnd(build));
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemOperandRegisterPrototype rValue;
        memset(&rValue, 0, sizeof(totemOperandRegisterPrototype));
        totemBool recycleRValue = totemBool_False;
        totemBool recycleLValue = totemBool_False;
        
        if(expression->BinaryOperator == totemBinaryOperatorType_Assign)
        {
            if(lValueRegisterIsArrayMember || TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsTemporary))
            {
                // result of rValue expression can now be stored directly in lValue
                if(!lValueRegisterIsArrayMember && TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsTemporary))
                {
                    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(lValueScope, &lValue));
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &lValue, totemEvalVariableFlag_MustBeDefined));
            }
            else
            {
                // otherwise we need the move op
                recycleRValue = totemBool_True;
                totemOperandRegisterPrototype *lValueToUse = NULL;
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsVariable))
                {
                    lValueToUse = &lValue;
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, lValueToUse, &rValue, totemEvalVariableFlag_MustBeDefined));
                
                // expression was evaled directly to lValue, so we no longer need the move
                if(lValueToUse == NULL || memcmp(lValueToUse, &rValue, sizeof(totemOperandRegisterPrototype)))
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &rValue, &rValue, totemOperationType_Move));
                }
                
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
            }
        }
        else
        {
            recycleRValue = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &rValue, totemEvalVariableFlag_MustBeDefined));
            
            switch(expression->BinaryOperator)
            {
                case totemBinaryOperatorType_Plus:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Add));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_PlusAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Add));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Minus:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Subtract));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MinusAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Subtract));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Multiply:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Multiply));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MultiplyAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Multiply));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Divide:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Divide));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_DivideAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Divide));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Assign:
                    // already dealt with
                    break;
                    
                case totemBinaryOperatorType_Equals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Equals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_NotEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_NotEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MoreThan:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_MoreThan));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LessThan:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LessThan));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MoreThanEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_MoreThanEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LessThanEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LessThanEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LogicalAnd:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LogicalAnd));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LogicalOr:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LogicalOr));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_IsType:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Is));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_AsType:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_As));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_None:
                    break;
            }
        }
        
        if(recycleRValue)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &rValue));
        }
        
        if(recycleLValue)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
        }
    }
    else
    {
        // no binary operation - result is lValue
        memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
    }
    
    // finish the array-access if the value was mutated
    if(lValueRegisterIsArrayMember && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary))
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandRegisterPrototype *hint,  totemOperandRegisterPrototype *value, totemEvalVariableFlag flags)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
        {
            totemEvalStatus status = totemVariablePrototype_Eval(argument->Variable, build, value, flags);
            if(status != totemEvalStatus_Success)
            {
                build->ErrorContext = argument;
            }
            
            return status;
        }
            
        case totemArgumentType_String:
            return totemBuildPrototype_EvalString(build, argument->String, value);
            
        case totemArgumentType_Number:
            return totemBuildPrototype_EvalNumber(build, argument->Number, value);
            
        case totemArgumentType_Type:
            return totemBuildPrototype_EvalType(build, argument->DataType, value);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, argument->FunctionDeclaration, value);
            
        case totemArgumentType_FunctionPointer:
            return totemBuildPrototype_EvalNamedFunctionPointer(build, argument->FunctionPointer, value);
            
        case totemArgumentType_FunctionCall:
            return totemFunctionCallPrototype_Eval(argument->FunctionCall, build, hint, value);
            
        case totemArgumentType_NewArray:
            return totemNewArrayPrototype_Eval(argument->NewArray, build, hint, value);
            
        case totemArgumentType_NewObject:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, value));
            return totemBuildPrototype_EvalAbcInstruction(build, value, value, value, totemOperationType_NewObject);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *value)
{
    totemOperandRegisterPrototype arraySize, c;
    memset(&c, 0, sizeof(totemOperandRegisterPrototype));
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(newArray->Accessor, build, NULL, &arraySize, totemEvalVariableFlag_None));
    
    if(hint)
    {
        memcpy(value, hint, sizeof(totemOperandRegisterPrototype));
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, value));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &arraySize, &c, totemOperationType_NewArray));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &arraySize));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandRegisterPrototype *index, totemEvalVariableFlag flags)
{
    totemBool justCreated = totemBool_False;
    
    // check local scope first
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    if(!totemRegisterListPrototype_GetVariable(localScope, &variable->Identifier, index))
    {
        // check global second
        if(!TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsLocal) && totemRegisterListPrototype_GetVariable(&build->GlobalRegisters, &variable->Identifier, index))
        {
            // HAS to be local? eep.
            if(TOTEM_GETBITS(flags, totemEvalVariableFlag_LocalOnly))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
            }
        }
        else
        {
            // create local
            justCreated = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(localScope, &variable->Identifier, index));
        }
    }
    
    /*
    if(TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsLocal) || !totemRegisterListPrototype_GetVariable(&build->GlobalRegisters, &variable->Identifier, index))
    {
        totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
        
        if(!totemRegisterListPrototype_GetVariable(localScope, &variable->Identifier, index))
        {
            if(TOTEM_GETBITS(flags, totemEvalVariableFlag_MustBeDefined))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableNotDefined);
            }
            
            justCreated = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(localScope, &variable->Identifier, index));
        }
    }
    else if(TOTEM_GETBITS(flags, totemEvalVariableFlag_LocalOnly))
    {
        return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
    }*/
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_GlobalAssocCheck(build, index));
    
    totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, index->RegisterScopeType);
    
    if(TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsConst))
    {
        if(!justCreated)
        {
            // can't declare a var const if it already exists
            return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
        }
        else
        {
            totemRegisterListPrototype_SetRegisterFlags(scope, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build)
{
    totemOperandRegisterPrototype initialisation, afterThought;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, NULL, &initialisation, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &initialisation));
    
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, forLoop->Condition));
    
    for(totemStatementPrototype *statement = forLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->AfterThought, build, NULL, &afterThought, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &afterThought));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_EvalBlock(totemIfBlockPrototype *ifBlock, totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    totemEvalLoopPrototype_Begin(loop, build);
    totemEvalLoopPrototype_SetStartPosition(loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(loop, build, ifBlock->Expression));
    
    for(totemStatementPrototype *statement = ifBlock->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_EvalChild(totemEvalLoopPrototype *root, totemEvalLoopPrototype *parent, totemIfBlockPrototype *ifBlock, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalBlock(ifBlock, &loop, build));
    
    totemEvalLoopPrototype_AddBreakChild(parent, &loop, build);
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    
    switch(ifBlock->ElseType)
    {
        case totemIfElseBlockType_Else:
            for(totemStatementPrototype *statement = ifBlock->ElseBlock->StatementsStart; statement != NULL; statement = statement->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
            }
            
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
            
        case totemIfElseBlockType_ElseIf:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(root, parent, ifBlock->IfElseBlock, build));
            break;
            
        case totemIfElseBlockType_None:
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalBlock(ifBlock, &loop, build));
    if(ifBlock->IfElseBlock != NULL)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_AddBreak(&loop, build));
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
        TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(&loop, &loop, ifBlock->IfElseBlock, build));
    }
    else
    {
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    for(totemStatementPrototype *statement = doWhileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, doWhileLoop->Expression));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, whileLoop->Expression));
    
    for(totemStatementPrototype *statement = whileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstructionSigned(totemBuildPrototype *build, totemOperandXSigned ax, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxSigned(instruction, ax));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstructionUnsigned(totemBuildPrototype *build, totemOperandXUnsigned ax, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxUnsigned(instruction, ax));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionSigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXSigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxSigned(instruction, bx));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXUnsigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(instruction, bx));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, b->RegisterIndex, b->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterC(instruction, c->RegisterIndex, c->RegisterScopeType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build)
{
    totemInstruction *lastInstruction = totemMemoryBuffer_Top(&build->Instructions);
    if(lastInstruction == NULL || TOTEM_INSTRUCTION_GET_OP(*lastInstruction) != totemOperationType_Return)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, NULL));
        lastInstruction = totemMemoryBuffer_Top(&build->Instructions);
    }
    
    totemOperandXUnsigned flags = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*lastInstruction);
    totemInstruction_SetBxUnsigned(lastInstruction, flags | totemReturnFlag_Last);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *dest)
{
    // free moved global registers if any still exist in local scope
    totemBuildPrototype_FreeGlobalAssocs(build, 0);
    
    totemOperandXUnsigned flags = totemReturnFlag_Register;
    totemOperandRegisterPrototype def;
    def.RegisterScopeType = totemOperandType_LocalRegister;
    def.RegisterIndex = 0;
    
    // implicit return
    if(dest == NULL)
    {
        dest = &def;
        flags = totemReturnFlag_None;
    }
    
    return totemBuildPrototype_EvalAbxInstructionUnsigned(build, dest, flags, totemOperationType_Return);
}

totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *result)
{
    // lookup function
    totemFunctionPointer funcInfo;
    totemOperandRegisterPrototype funcPtrReg;
    memset(&funcInfo, 0, sizeof(totemFunctionPointer));
    memset(&funcPtrReg, 0, sizeof(totemOperandRegisterPrototype));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionName(build, &functionCall->Identifier, &funcInfo));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionPointer(build, &funcInfo, &funcPtrReg));
    
    if(hint)
    {
        memcpy(result, hint, sizeof(totemOperandRegisterPrototype));
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsBegin(build, functionCall->ParametersStart));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &funcPtrReg, &funcPtrReg, totemOperationType_FunctionPointer));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsEnd(build));
    
    if(!hint)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, result));
    }
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InvalidDataType);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_ScriptFunctionAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_NativeFunctionAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_FunctionNotDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_Success);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyRegisters);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableNotDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_AssignmentLValueCannotBeConst);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_AssignmentLValueNotMutable);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableAlreadyAssigned);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyNativeFunctions);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyScriptFunctions);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyFunctionArguments);
    }
    
    return "UNKNOWN";
}