//
//  eval_loop.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>

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
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(condition, build, NULL, &conditionOp, totemEvalVariableFlag_None, totemEvalExpressionFlag_None));
    
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
    if (parent->Next != NULL)
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
    
    if (TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
    {
        totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->ConditionIndex);
        totemOperandXUnsigned offset = (totemOperandXUnsigned)child->ConditionFailIndex - (totemOperandXUnsigned)child->ConditionIndex;
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(gotoInstruction, offset));
    }
    
    if (child->BreakIndex != 0)
    {
        totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->BreakIndex);
        totemOperandXUnsigned offset = (totemOperandXUnsigned)numInstructions - (totemOperandXUnsigned)child->BreakIndex;
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxSigned(gotoInstruction, offset));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_End(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    for (totemEvalLoopPrototype *child = loop; child != NULL; child = child->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(loop, child, build));
    }
    
    // clean up condition registers after all loops have been evaluated
    for (totemEvalLoopPrototype *child = loop; child != NULL; child = child->Next)
    {
        if (TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
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

totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build)
{
    totemOperandRegisterPrototype initialisation, afterThought;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, NULL, &initialisation, totemEvalVariableFlag_None, totemEvalExpressionFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &initialisation));
    
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, forLoop->Condition));
    
    for (totemStatementPrototype *statement = forLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->AfterThought, build, NULL, &afterThought, totemEvalVariableFlag_None, totemEvalExpressionFlag_None));
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
    
    for (totemStatementPrototype *statement = ifBlock->StatementsStart; statement != NULL; statement = statement->Next)
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
    
    switch (ifBlock->ElseType)
    {
        case totemIfElseBlockType_Else:
            for (totemStatementPrototype *statement = ifBlock->ElseBlock->StatementsStart; statement != NULL; statement = statement->Next)
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
    if (ifBlock->IfElseBlock != NULL)
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
    
    for (totemStatementPrototype *statement = doWhileLoop->StatementsStart; statement != NULL; statement = statement->Next)
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
    
    for (totemStatementPrototype *statement = whileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    return totemEvalStatus_Success;
}

totemEvalStatus totemDoWhileLoopPrototype_EvalValues(totemDoWhileLoopPrototype *loop, totemBuildPrototype *build)
{
    for (totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    return totemExpressionPrototype_EvalValues(loop->Expression, build, totemEvalVariableFlag_None);
}

totemEvalStatus totemForLoopPrototype_EvalValues(totemForLoopPrototype *loop, totemBuildPrototype *build)
{
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Initialisation, build, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Condition, build, totemEvalVariableFlag_None));
    
    for (totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    return totemExpressionPrototype_EvalValues(loop->AfterThought, build, totemEvalVariableFlag_None);
}

totemEvalStatus totemIfBlockPrototype_EvalValues(totemIfBlockPrototype *loop, totemBuildPrototype *build)
{
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(loop->Expression, build, totemEvalVariableFlag_None));
    
    for (totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    switch (loop->ElseType)
    {
        case totemIfElseBlockType_ElseIf:
            return totemIfBlockPrototype_EvalValues(loop->IfElseBlock, build);
            
        case totemIfElseBlockType_Else:
            for (totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
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
    
    for (totemStatementPrototype *stmt = loop->StatementsStart; stmt != NULL; stmt = stmt->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
    }
    
    return totemEvalStatus_Success;
}