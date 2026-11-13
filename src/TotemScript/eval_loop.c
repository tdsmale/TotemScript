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

typedef enum totemEvalLoopFlag
{
    totemEvalLoopFlag_None = 0,
    totemEvalLoopFlag_StartingCondition = 1
}
totemEvalLoopFlag;

typedef struct totemEvalLoopBreak
{
    size_t Index;
    struct totemEvalLoopBreak* Next;
}
totemEvalLoopBreak;

typedef struct totemEvalLoopPrototype
{
    size_t LoopBeginIndex;
    size_t ConditionFailIndex;
    size_t ConditionIndex;
    totemEvalLoopBreak *Breaks;
    struct totemEvalLoopPrototype *Next;
    totemEvalLoopFlag Flags;
    totemBool ConstCondition, ConstConditionFail;
}
totemEvalLoopPrototype;

static void totemEvalLoopPrototype_Begin(totemEvalLoopPrototype *loop)
{
    loop->Next = NULL;
    loop->ConditionFailIndex = 0;
    loop->ConditionIndex = 0;
    loop->ConstCondition = totemBool_False;
    loop->ConstConditionFail = totemBool_False;
    loop->Flags = totemEvalLoopFlag_None;
    loop->LoopBeginIndex = 0;
    loop->Breaks = NULL;
}

static void totemEvalLoopPrototype_SetStartPosition(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->LoopBeginIndex = totemInstructionDynBuffer_Length(&build->Instructions);
}

static totemEvalStatus totemEvalLoopPrototype_SetCondition(totemEvalLoopPrototype *loop, totemBuildPrototype *build, totemExpressionPrototype *condition)
{
    totemOperandPrototype conditionOp;
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(condition, build, NULL, &conditionOp));
    loop->ConditionIndex = totemInstructionDynBuffer_Length(&build->Instructions);
    
    totemRegisterListPrototype *localRegList = totemBuildPrototype_GetLocalRegisters(build);
    
    totemRegisterPrototypeValue valueOut;
    totemPublicDataType valueType = totemPublicDataType_Max;
    if (totemRegisterListPrototype_GetRegisterValue(localRegList, conditionOp.RegisterIndex, &valueOut) && totemRegisterListPrototype_GetRegisterType(localRegList, conditionOp.RegisterIndex, &valueType))
    {
        if (valueType != totemPublicDataType_Max)
        {
            // if this has a constant value, exit the loop if it evals to false
            // otherwise we don't need a conditional goto
            loop->ConstCondition = valueOut.Bool;
            loop->ConstConditionFail = !valueOut.Bool;
            return totemEvalStatus_Success;
        }
    }

    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_ConditionalGoto(build, &conditionOp, 0));
    TOTEM_SETBITS(loop->Flags, totemEvalLoopFlag_StartingCondition);
    
    return totemEvalStatus_Success;
}

static void totemEvalLoopPrototype_SetConditionFailPosition(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->ConditionFailIndex = totemInstructionDynBuffer_Length(&build->Instructions);
}

static totemEvalStatus totemEvalLoopPrototype_AddBreak(totemEvalLoopPrototype *child, totemBuildPrototype *build, totemEvalLoopBreak *br)
{
    // add goto to edit later
    br->Index = totemInstructionDynBuffer_Length(&build->Instructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Goto(build, 0));
    br->Next = child->Breaks;
    child->Breaks = br;
    br->Next = NULL;
    return totemEvalStatus_Success;
}

static totemEvalStatus totemEvalLoopPrototype_AddBreakChild(totemEvalLoopPrototype *parent, totemEvalLoopPrototype *child, totemBuildPrototype *build, totemEvalLoopBreak* br)
{
    if (parent->Next != NULL)
    {
        child->Next = parent->Next;
    }
    
    parent->Next = child;
    
    return totemEvalLoopPrototype_AddBreak(child, build, br);
}

static totemEvalStatus totemEvalLoopPrototype_Loop(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    size_t numInstructions = totemInstructionDynBuffer_Length(&build->Instructions);
    totemOperandX resetLoopOffset = (totemOperandX)loop->LoopBeginIndex - (totemOperandX)numInstructions;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Goto(build, resetLoopOffset));
    
    return totemEvalStatus_Success;
}

static totemEvalStatus totemEvalLoopPrototype_EndChild(totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    size_t numInstructions = totemInstructionDynBuffer_Length(&build->Instructions);

    if (TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
    {
        // fixup the conditional goto
        totemInstruction *gotoInstruction = totemInstructionDynBuffer_At(&build->Instructions, child->ConditionIndex);
        totemOperandX offset = (totemOperandX)child->ConditionFailIndex - (totemOperandX)child->ConditionIndex;
        totemOperandPrototype condRegister;
        condRegister.RegisterIndex = TOTEM_INSTRUCTION_GET_A(*gotoInstruction);
        
        TOTEM_ASSERT(totemOperationType_GetInstructionType(totemOperationType_ConditionalGoto) == totemInstructionType_Ab2);
        TOTEM_EVAL_CHECKRETURN(totemInstruction_As_ConditionalGoto(gotoInstruction, &condRegister, offset));
    }
    
    while(child->Breaks)
    {
        // fixup the goto
        totemEvalLoopBreak* br = child->Breaks;
		totemInstruction *gotoInstruction = totemInstructionDynBuffer_At(&build->Instructions, br->Index);
        totemOperandXu offset = (totemOperandXu)numInstructions - (totemOperandXu)br->Index;
        TOTEM_EVAL_CHECKRETURN(totemInstruction_As_Goto(gotoInstruction, offset));
        child->Breaks = child->Breaks->Next;
    }
    
    return totemEvalStatus_Success;
}

static totemEvalStatus totemEvalLoopPrototype_End(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    for (totemEvalLoopPrototype *child = loop; child != NULL; child = child->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(child, build));
    }
    
    // clean up condition registers after all loops have been evaluated
    for (totemEvalLoopPrototype *child = loop; child != NULL; child = child->Next)
    {
        if (TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
        {
            totemInstruction *gotoInstruction = totemInstructionDynBuffer_At(&build->Instructions, child->ConditionIndex);
            totemOperandPrototype condition;
            condition.RegisterIndex = TOTEM_INSTRUCTION_GET_A(*gotoInstruction);
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &condition));
        }
    }
    
    return totemEvalStatus_Success;
}

static totemEvalStatus totemIfBlockPrototype_EvalIfBlock(totemIfBlockPrototype *ifBlock, totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    totemEvalLoopPrototype_Begin(loop);
    totemEvalLoopPrototype_SetStartPosition(loop, build);
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(loop, build, ifBlock->Expression));
    
    if (!loop->ConstConditionFail)
    {
        for (totemStatementPrototype* statement = ifBlock->StatementsStart; statement != NULL; statement = statement->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
    
    return totemEvalStatus_Success;
}

static totemEvalStatus totemIfBlockPrototype_EvalElseBlock(totemElseBlockPrototype *elseBlock, totemBuildPrototype *build)
{
    for (totemStatementPrototype *statement = elseBlock->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    return totemEvalStatus_Success;
}

static totemEvalStatus totemIfBlockPrototype_EvalChild(totemEvalLoopPrototype *root, totemEvalLoopPrototype *parent, totemIfBlockPrototype *ifBlock, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalIfBlock(ifBlock, &loop, build));

    if (loop.ConstCondition)
    {
        // ignore the rest
        return totemEvalLoopPrototype_End(&loop, build);
    }

    totemEvalLoopBreak br;
    totemEvalLoopPrototype_AddBreakChild(parent, &loop, build, &br);
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);

    switch (ifBlock->ElseType)
    {
        case totemIfElseBlockType_Else:
			TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalElseBlock(ifBlock->Next.ElseBlock, build));
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
            
        case totemIfElseBlockType_ElseIf:
			TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(root, parent, ifBlock->Next.IfElseBlock, build));
            break;
            
        case totemIfElseBlockType_None:
        default:
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build)
{
	totemOperandPrototype initialisation, afterThought;

	TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
	TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, NULL, &initialisation));
	TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &initialisation));

	totemEvalLoopPrototype loop;
	totemEvalLoopPrototype_Begin(&loop);
	totemEvalLoopPrototype_SetStartPosition(&loop, build);
	TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, forLoop->Condition));

    if (!loop.ConstConditionFail)
    {
        for (totemStatementPrototype* statement = forLoop->StatementsStart; statement != NULL; statement = statement->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
        }

        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->AfterThought, build, NULL, &afterThought));
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    }
	
	TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
	TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &afterThought));
	TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));

	return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalIfBlock(ifBlock, &loop, build));
    
    totemEvalLoopBreak br;

    switch (ifBlock->ElseType)
    {
        case totemIfElseBlockType_ElseIf:
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_AddBreak(&loop, build, &br));
            totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(&loop, &loop, ifBlock->Next.IfElseBlock, build));
            break;
            
        case totemIfElseBlockType_Else:
            totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
			TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalElseBlock(ifBlock->Next.ElseBlock, build));
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
            break;
            
        case totemIfElseBlockType_None:
            totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemLoopPrototype_Eval(totemStatementPrototype* loopStart, totemBuildPrototype* build)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));

    for (totemStatementPrototype* statement = loopStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }

    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    
    for (totemStatementPrototype *statement = doWhileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, doWhileLoop->Expression));

    if (!loop.ConstConditionFail)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    }

    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, whileLoop->Expression));

    if (!loop.ConstConditionFail)
    {
        for (totemStatementPrototype* statement = whileLoop->StatementsStart; statement != NULL; statement = statement->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
        }

        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
    
    return totemEvalStatus_Success;
}
