//
//  eval.c
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <string.h>

#define TOTEM_EVAL_CHECKRETURN(exp) { totemEvalStatus status = exp; if(status != totemEvalStatus_Success) return status; }
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

totemEvalStatus totemEvalLoopPrototype_SetCondition(totemEvalLoopPrototype *loop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemExpressionPrototype *condition)
{
    totemOperandRegisterPrototype conditionOp;
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(condition, build, scope, globals, &conditionOp, totemEvalVariableFlag_None));
    
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
        if(child->ConditionFailIndex != numInstructions - 1)
        {
            totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->ConditionIndex);
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
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(loop, loop, build));
    
    for(totemEvalLoopPrototype *child = loop->Next; child != NULL; child = child->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(loop, child, build));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalStatus_Break(totemEvalStatus status)
{
    return status;
}

totemEvalStatus totemInstruction_SetRegister(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope, uint32_t start)
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

totemEvalStatus totemInstruction_SetRegisterA(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_A);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterB(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_B);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterC(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope)
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
    size_t index = totemMemoryBuffer_GetNumObjects(&list->Registers);
    
    if(index + 1 >= TOTEM_MAX_REGISTERS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyRegisters);
    }

    totemRegisterPrototype *reg = totemMemoryBuffer_Secure(&list->Registers, 1);
    
    if(!reg)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    reg->Value.Data = 0;
    reg->DataType = totemDataType_Null;
    reg->Flags = totemRegisterPrototypeFlag_None;
    
    operand->RegisterIndex = index;
    operand->RegisterScopeType = list->Scope;
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg)
    {
        return totemBool_False;
    }
    
    reg->Flags |= flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag *flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg)
    {
        return totemBool_False;
    }
    
    *flags = reg->Flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemRegisterIndex index, totemDataType *type)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg)
    {
        return totemBool_False;
    }
    
    *type = reg->DataType;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemRegisterIndex index, totemDataType type)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg)
    {
        return totemBool_False;
    }
    
    reg->DataType = type;
    return totemBool_True;
}

totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    if(list->HasNull)
    {
        operand->RegisterIndex = list->NullIndex;
        operand->RegisterScopeType = list->Scope;
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
    if(status != totemEvalStatus_Success)
    {
        return status;
    }
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
    reg->DataType = totemDataType_Null;
    TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    list->HasNull = totemBool_True;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *str, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Strings, str->Value, str->Length);
    
    totemRegisterPrototype *reg = NULL;
    if(searchResult != NULL)
    {
        operand->RegisterIndex = searchResult->Value;
        operand->RegisterScopeType = list->Scope;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if(status != totemEvalStatus_Success)
        {
            return status;
        }
        
        reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        reg->DataType = totemDataType_InternedString;
        reg->Value.InternedString = (void*)str;
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
        
        if(!totemHashMap_Insert(&list->Strings, str->Value, str->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        // add to global string register list for later linking
        size_t *globalRegisterStrIndex = totemMemoryBuffer_Secure(&list->GlobalRegisterStrings, 1);
        if(!globalRegisterStrIndex)
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        *globalRegisterStrIndex = operand->RegisterIndex;
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Variables, name->Value, name->Length);
    if(searchResult != NULL)
    {
        operand->RegisterIndex = searchResult->Value;
        operand->RegisterScopeType = list->Scope;
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype)
{
    if(!totemRegisterListPrototype_GetVariable(list, name, prototype))
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
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddInteger(totemRegisterListPrototype *list, totemInt number, totemOperandRegisterPrototype *operand)
{
    int length = snprintf(NULL, 0, "%lld", number) + 1;
    char *numStr = totem_CacheMalloc(length);
    if(!numStr)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    snprintf(numStr, length, "%lld", number);
    numStr[length] = 0;
    
    totemString numberLiteral;
    numberLiteral.Length = length;
    numberLiteral.Value = numStr;

    totemEvalStatus result = totemRegisterListPrototype_AddNumberConstant(list, &numberLiteral, operand);
    totem_CacheFree(numStr, length);
    return result;
}

totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Numbers, number->Value, number->Length);
    if(searchResult != NULL)
    {
        operand->RegisterIndex = searchResult->Value;
        operand->RegisterScopeType = list->Scope;
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
        
        if(strnstr(number->Value, ".", number->Length) != NULL)
        {
            reg->Value.Float = atof(number->Value);
            reg->DataType = totemDataType_Float;
        }
        else
        {
            reg->Value.Int = atoi(number->Value);
            reg->DataType = totemDataType_Int;
        }
        
        TOTEM_SETBITS(reg->Flags, totemRegisterPrototypeFlag_IsValue);
    }
    
    return totemEvalStatus_Success;
}

void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list, totemRegisterScopeType scope)
{
    totemHashMap_Reset(&list->Numbers);
    totemHashMap_Reset(&list->Strings);
    totemHashMap_Reset(&list->Variables);
    totemMemoryBuffer_Reset(&list->Registers, sizeof(totemRegisterPrototype));
    totemMemoryBuffer_Reset(&list->GlobalRegisterStrings, sizeof(totemRegisterIndex));
    list->Scope = scope;
    list->NullIndex = 0;
    list->HasNull = totemBool_False;
}

void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list)
{
    totemHashMap_Cleanup(&list->Numbers);
    totemHashMap_Cleanup(&list->Strings);
    totemHashMap_Cleanup(&list->Variables);
    totemMemoryBuffer_Cleanup(&list->Registers);
}

void totemBuildPrototype_Init(totemBuildPrototype *build)
{
    totemBuildPrototype_Reset(build);
}

void totemBuildPrototype_Reset(totemBuildPrototype *build)
{
    totemHashMap_Reset(&build->FunctionLookup);
    totemHashMap_Reset(&build->NativeFunctionNamesLookup);
    totemMemoryBuffer_Reset(&build->Functions, sizeof(totemFunction));
    totemRegisterListPrototype_Reset(&build->GlobalRegisters, totemRegisterScopeType_Global);
    totemMemoryBuffer_Reset(&build->Instructions, sizeof(totemInstruction));
    totemMemoryBuffer_Reset(&build->NativeFunctionCallInstructions, sizeof(size_t));
    totemMemoryBuffer_Reset(&build->NativeFunctionNames, sizeof(totemString));
}

void totemBuildPrototype_Cleanup(totemBuildPrototype *build)
{
    totemHashMap_Cleanup(&build->FunctionLookup);
    totemHashMap_Cleanup(&build->NativeFunctionNamesLookup);
    totemMemoryBuffer_Cleanup(&build->Functions);
    totemRegisterListPrototype_Cleanup(&build->GlobalRegisters);
    totemMemoryBuffer_Cleanup(&build->Instructions);
    totemMemoryBuffer_Cleanup(&build->NativeFunctionCallInstructions);
    totemMemoryBuffer_Cleanup(&build->NativeFunctionNames);
}

totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype)
{
    totemFunction *globalFunction;

    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &globalFunction));
    globalFunction->InstructionsStart = 0;
    totemString_FromLiteral(&globalFunction->Name, "__initGlobals");
    
    // globals
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(block->Statement, build, &build->GlobalRegisters, &build->GlobalRegisters));
                break;
                
            default:
                break;
        }
    }
    
    globalFunction->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.Registers);
    totemBuildPrototype_EvalReturn(build, NULL);
    
    // all other functions
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
                TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(block->FuncDec, build, &build->GlobalRegisters));
                break;
                
            default:
                break;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemFunction **functionOut)
{
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

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemRegisterListPrototype *globals)
{
    totemRegisterListPrototype localRegisters;
    memset(&localRegisters, 0, sizeof(totemRegisterListPrototype));
    totemRegisterListPrototype_Reset(&localRegisters, totemRegisterScopeType_Local);
    
    if(totemMemoryBuffer_GetNumObjects(&build->Functions) == TOTEM_MAX_SCRIPTFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
    }
    
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->Functions); i++)
    {
        totemFunction *existingFunc = totemMemoryBuffer_Get(&build->Functions, i);
        if(existingFunc != NULL && totemString_Equals(&existingFunc->Name, function->Identifier))
        {
            build->ErrorContext = function;
            return totemEvalStatus_Break(totemEvalStatus_ScriptFunctionAlreadyDefined);
        }
    }
    
    size_t functionIndex = totemMemoryBuffer_GetNumObjects(&build->Functions);
    totemFunction *functionPrototype;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
    memcpy(&functionPrototype->Name, function->Identifier, sizeof(totemString));
    
    if(!totemHashMap_Insert(&build->FunctionLookup, function->Identifier->Value, function->Identifier->Length, functionIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    functionPrototype->InstructionsStart = totemMemoryBuffer_GetNumObjects(&build->Instructions);

    // parameters
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemVariablePrototype_Eval(parameter, &localRegisters, globals, &dummy, totemEvalVariableFlag_LocalOnly));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, &localRegisters, globals));
    }
    
    functionPrototype->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&localRegisters.Registers);
    
    // do we need an implicit return?
    totemInstruction *lastInstruction = totemMemoryBuffer_Get(&build->Instructions, totemMemoryBuffer_GetNumObjects(&build->Instructions) - 1);
    if(lastInstruction == NULL || TOTEM_INSTRUCTION_GET_OP(*lastInstruction) != totemOperationType_Return)
    {
        totemBuildPrototype_EvalReturn(build, NULL);
    }

    return totemEvalStatus_Success;
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype result;

    switch(statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemDoWhileLoopPrototype_Eval(statement->DoWhileLoop, build, scope, globals));
            break;
            
        case totemStatementType_ForLoop:
            TOTEM_EVAL_CHECKRETURN(totemForLoopPrototype_Eval(statement->ForLoop, build, scope, globals));
            break;
            
        case totemStatementType_IfBlock:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_Eval(statement->IfBlock, build, scope, globals));
            break;

        case totemStatementType_WhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemWhileLoopPrototype_Eval(statement->WhileLoop, build, scope, globals));
            break;
            
        case totemStatementType_Simple:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Expression, build, scope, globals, &result, totemEvalVariableFlag_None));
            break;
            
        case totemStatementType_Return:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Return, build, scope, globals, &result, totemEvalVariableFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, &result));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *result, totemEvalVariableFlag flags)
{
    totemOperandRegisterPrototype lValueSrc;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValueArgument, build, scope, globals, &lValueSrc, flags));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValueExpression, build, scope, globals, &lValueSrc, flags));
            break;
    }
    
    totemBool mutatedLValueRegisterUnary = totemBool_False;
    totemBool mutatedLValueRegisterBinary = totemBool_False;
    totemDataType lValueType;
    totemBool isArrayAccess = totemBool_False;
    
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
            isArrayAccess = totemBool_True;
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
        totemRegisterListPrototype *list = NULL;
        
        switch(lValueSrc.RegisterScopeType)
        {
            case totemRegisterScopeType_Global:
                list = globals;
                break;
                
            case totemRegisterScopeType_Local:
                list = scope;
                break;
        }
        
        // if already assigned, and is const, throw an error
        totemRegisterPrototypeFlag flags;
        if(totemRegisterListPrototype_GetRegisterFlags(list, lValueSrc.RegisterIndex, &flags))
        {
            if(TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsAssigned | totemRegisterPrototypeFlag_IsConst))
            {
                return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
            }
        }
        
        // assignment expressions can only happen on mutable lValues (variables)
        // TODO: enforce all of this grammatically, rather than here
        if(totemRegisterListPrototype_GetRegisterType(list, lValueSrc.RegisterIndex, &lValueType))
        {
            if(!TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsVariable) && !isArrayAccess)
            {
                return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
            }
            
            // ONLY vars can be const
            if(TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsVariable | totemRegisterPrototypeFlag_IsConst))
            {
                return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueCannotBeConst);
            }

            // ensure the array src is valid
            if(isArrayAccess)
            {
                if(TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsValue))
                {
                    return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
                }
            }
        }
        
        // this lvalue is now assigned
        totemRegisterListPrototype_SetRegisterFlags(list, lValueSrc.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
    }
    
    // when lValue is an array member, we must retrieve it first and place it in a register before any other op
    totemOperandRegisterPrototype lValue;
    totemOperandRegisterPrototype arrIndex;
    memcpy(&lValue, &lValueSrc, sizeof(totemOperandRegisterPrototype));
    
    if(isArrayAccess)
    {
        for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
        {
            if(op->Type == totemPostUnaryOperatorType_ArrayAccess)
            {
                memcpy(&lValueSrc, &lValue, sizeof(totemOperandRegisterPrototype));
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &lValue));
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->ArrayAccess, build, scope, globals, &arrIndex, totemEvalVariableFlag_MustBeDefined));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValueSrc, &arrIndex, totemOperationType_ArrayGet));
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
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                totemString_FromLiteral(&preUnaryNumber, "0");
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Equals));
                // A = B == C(0)
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
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
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &postUnaryNumber, &postUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &postUnaryNumber, &postUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
                // already done above
                break;
                
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemOperandRegisterPrototype rValue;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, scope, globals, &rValue, flags));
        
        switch(expression->BinaryOperator)
        {
            case totemBinaryOperatorType_Plus:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Add));
                break;
                
            case totemBinaryOperatorType_PlusAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Add));
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                break;
                
            case totemBinaryOperatorType_Minus:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Subtract));
                break;
                
            case totemBinaryOperatorType_MinusAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Subtract));
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                break;
                
            case totemBinaryOperatorType_Multiply:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Multiply));
                break;
                
            case totemBinaryOperatorType_MultiplyAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Multiply));
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                break;
                
            case totemBinaryOperatorType_Divide:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Divide));
                break;
                
            case totemBinaryOperatorType_DivideAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Divide));
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                break;
                                
            case totemBinaryOperatorType_Assign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &rValue, &rValue, totemOperationType_Move));
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                break;
                
            case totemBinaryOperatorType_Equals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Equals));
                break;
                
            case totemBinaryOperatorType_MoreThan:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_MoreThan));
                break;
                
            case totemBinaryOperatorType_LessThan:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LessThan));
                break;
                
            case totemBinaryOperatorType_MoreThanEquals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_MoreThanEquals));
                break;
                
            case totemBinaryOperatorType_LessThanEquals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LessThanEquals));
                break;
                
            case totemBinaryOperatorType_LogicalAnd:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LogicalAnd));
                break;
                
            case totemBinaryOperatorType_LogicalOr:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LogicalOr));
                break;
                
            case totemBinaryOperatorType_None:
                break;
        }
    }
    else
    {
        // no binary operation - result is lValue
        memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
    }
    
    // finish the array-access if the value was mutated
    if(isArrayAccess && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary))
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValueSrc, &arrIndex, &lValue, totemOperationType_ArraySet));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
            build->ErrorContext = argument;
            totemEvalStatus status = totemVariablePrototype_Eval(argument->Variable, scope, globals, value, flags);
            if(status == totemEvalStatus_Success)
            {
                build->ErrorContext = NULL;
            }
            
            return status;
            
        case totemArgumentType_Null:
            return totemRegisterListPrototype_AddNull(globals, value);
            
        case totemArgumentType_String:
            return totemRegisterListPrototype_AddStringConstant(globals, argument->String, value);
            
        case totemArgumentType_Number:
            return totemRegisterListPrototype_AddNumberConstant(globals, argument->Number, value);
            
        case totemArgumentType_FunctionCall:
            return totemFunctionCallPrototype_Eval(argument->FunctionCall, build, scope, globals, value);
            
        case totemArgumentType_NewArray:
            return totemNewArrayPrototype_Eval(argument->NewArray, build, scope, globals, value);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value)
{
    totemOperandRegisterPrototype arraySize, c;
    memset(&c, 0, sizeof(totemOperandRegisterPrototype));
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(newArray->Accessor, build, scope, globals, &arraySize, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &arraySize, &c, totemOperationType_NewArray));
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *index, totemEvalVariableFlag flags)
{
    totemEvalStatus result = totemEvalStatus_Success;
    totemBool justCreated = totemBool_False;
    totemRegisterListPrototype *list = NULL;
    
    if(!totemRegisterListPrototype_GetVariable(globals, &variable->Identifier, index))
    {
        if(TOTEM_GETBITS(flags, totemEvalVariableFlag_MustBeDefined))
        {
            if(!totemRegisterListPrototype_GetVariable(scope, &variable->Identifier, index))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableNotDefined);
            }
        }
        else
        {
            justCreated = totemBool_True;
            result = totemRegisterListPrototype_AddVariable(scope, &variable->Identifier, index);
        }
        
        list = scope;
    }
    else if(TOTEM_GETBITS(flags, totemEvalVariableFlag_LocalOnly))
    {
        return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
    }
    else
    {
        list = globals;
    }
    
    if(variable->IsConst)
    {
        if(!justCreated)
        {
            // can't declare a var const if it already exists
            return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
        }
        else
        {
            switch(index->RegisterScopeType)
            {
                case totemRegisterScopeType_Global:
                    totemRegisterListPrototype_SetRegisterFlags(globals, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
                    break;
                    
                case totemRegisterScopeType_Local:
                    totemRegisterListPrototype_SetRegisterFlags(scope, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
                    break;
            }
        }
    }
    
    // set register as variable
    totemRegisterListPrototype_SetRegisterFlags(list, index->RegisterIndex, totemRegisterPrototypeFlag_IsVariable);
    
    return result;
}

/*
 for
 ---
 _Init (loop)
 Initialization statement
 _SetLoopStartPosition
 _SetCondition
 statements
 afterthought
 _SetConditionFailPosition
 _End
 */
totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype initialisation, afterThought;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, scope, globals, &initialisation, totemEvalVariableFlag_None));
    
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, scope, globals, forLoop->Condition));
    
    for(totemStatementPrototype *statement = forLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->AfterThought, build, scope, globals, &afterThought, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    return totemEvalStatus_Success;
}

/*
if
--
_Init (no loop)
_SetLoopStartPosition
_SetCondition
statements
_AddBreakChild
_SetConditionFailPosition
.. do again for each if...else...
_End
 */
totemEvalStatus totemIfBlockPrototype_EvalBlock(totemIfBlockPrototype *ifBlock, totemEvalLoopPrototype *loop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype_Begin(loop, build);
    totemEvalLoopPrototype_SetStartPosition(loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(loop, build, scope, globals, ifBlock->Expression));
    
    for(totemStatementPrototype *statement = ifBlock->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_EvalChild(totemEvalLoopPrototype *root, totemEvalLoopPrototype *parent, totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalBlock(ifBlock, &loop, build, scope, globals));
    
    totemEvalLoopPrototype_AddBreakChild(parent, &loop, build);
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    
    switch(ifBlock->ElseType)
    {
        case totemIfElseBlockType_Else:
            for(totemStatementPrototype *statement = ifBlock->ElseBlock->StatementsStart; statement != NULL; statement = statement->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
            }
            
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
            
        case totemIfElseBlockType_ElseIf:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(root, parent, ifBlock->IfElseBlock, build, scope, globals));
            break;
            
        case totemIfElseBlockType_None:
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalBlock(ifBlock, &loop, build, scope, globals));
    if(ifBlock->IfElseBlock != NULL)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_AddBreak(&loop, build));
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
        TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(&loop, &loop, ifBlock->IfElseBlock, build, scope, globals));
    }
    else
    {
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    }

    return totemEvalStatus_Success;
}

/*
dowhile
-------
_Init (loop)
_SetLoopStartPosition
statements
_SetCondition
_SetConditionFailPosition
_End*/
totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    for(totemStatementPrototype *statement = doWhileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, scope, globals, doWhileLoop->Expression));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    
    return totemEvalStatus_Success;
}

/*
 while
 -----
 _Init (loop)
 _SetLoopStartPosition
 _SetCondition
 statements
 _SetConditionFailPosition
 _End*/
totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, scope, globals, whileLoop->Expression));
    
    for(totemStatementPrototype *statement = whileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
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

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *dest)
{
    totemOperandXUnsigned option = totemReturnOption_Register;
    totemOperandRegisterPrototype def;
    def.RegisterScopeType = totemRegisterScopeType_Local;
    def.RegisterIndex = 0;
    
    // implicit return
    if(dest == NULL)
    {
        dest = &def;
        option = totemReturnOption_Implicit;
    }
    
    return totemBuildPrototype_EvalAbxInstructionUnsigned(build, dest, option, totemOperationType_Return);
}

totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *result)
{
    size_t address = 0;
    totemOperationType opType = totemOperationType_NativeFunction;

    // did we define this function in the script?
    totemHashMapEntry *searchResult = totemHashMap_Find(&build->FunctionLookup, functionCall->Identifier.Value, functionCall->Identifier.Length);
    if(searchResult)
    {
        address = searchResult->Value;
        opType = totemOperationType_ScriptFunction;
    }
    else
    {
        // add native function name
        totemHashMapEntry *result = totemHashMap_Find(&build->NativeFunctionNamesLookup, functionCall->Identifier.Value, functionCall->Identifier.Length);
        if(result)
        {
            address = result->Value;
        }
        else
        {
            size_t index = totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames);
            
            if(index >= TOTEM_MAX_NATIVEFUNCTIONS)
            {
                return totemEvalStatus_Break(totemEvalStatus_TooManyNativeFunctions);
            }
            
            if(totemMemoryBuffer_Insert(&build->NativeFunctionNames, &functionCall->Identifier, 1) == NULL)
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
            
            if(!totemHashMap_Insert(&build->NativeFunctionNamesLookup, functionCall->Identifier.Value, functionCall->Identifier.Length, index))
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
        }
        
        // add to native function list for later linking
        size_t instructionIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
        if(!totemMemoryBuffer_Insert(&build->NativeFunctionCallInstructions, &instructionIndex, 1))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, result, (totemOperandXUnsigned)address, opType));
    
    for(totemExpressionPrototype *parameter = functionCall->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype operand;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, scope, globals, &operand, totemEvalVariableFlag_MustBeDefined));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &operand, 0, totemOperationType_FunctionArg));
    }
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
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
    }
}