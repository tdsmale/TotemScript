//
//  eval_instruction.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>

static totemEvalStatus totemInstruction_SetUnsignedValue(totemInstruction *ins, uint32_t val, uint32_t min, uint32_t maxi, uint32_t start, uint32_t numBits)
{
	if (val > maxi || val < min)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
	TOTEM_UNSETBITS(*ins, TOTEM_BITMASK(uint32_t, start, numBits));
    TOTEM_SETBITS_OFFSET(*ins, val, start);
    return totemEvalStatus_Success;
}

static totemEvalStatus totemInstruction_SetSignedValue(totemInstruction *instruction, int32_t value, int32_t min, int32_t max, uint32_t start, uint32_t numBits)
{
    if (value < min || value > max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    uint32_t mask = 0;
    uint32_t isNegative = value < 0;
    uint32_t unsignedValue = TOTEM_BITCAST(uint32_t, value);
    
    if (isNegative)
    {
        unsignedValue = ~unsignedValue;
        unsignedValue++;
    }
    
    // value
    TOTEM_SETBITS(mask, (unsignedValue & TOTEM_BITMASK(uint32_t, 0, numBits - 1)));
    
    // signed bit
    TOTEM_SETBITS_OFFSET(mask, isNegative, numBits - 1);
    
    // add to instruction
	TOTEM_UNSETBITS(*instruction, TOTEM_BITMASK(uint32_t, start, numBits));
    TOTEM_SETBITS_OFFSET(*instruction, mask, start);
    
    return totemEvalStatus_Success;
}

static totemEvalStatus totemInstruction_SetRegister(totemInstruction *instruction, totemOperandXu index, uint32_t start)
{
	if (index > TOTEM_MAX_LOCAL_REGISTERS)
	{
		return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
	}

	totemInstruction_SetUnsignedValue(
		instruction,
		index, 
		TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_A), 
		TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_A), 
		start, 
		totemInstructionSize_A);

	return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetA(totemInstruction *instruction, totemOperandPrototype* index)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index->RegisterIndex, totemInstructionStart_A);
    return status;
}

totemEvalStatus totemInstruction_SetB(totemInstruction *instruction, totemOperandPrototype* index)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index->RegisterIndex, totemInstructionStart_B);
    return status;
}

totemEvalStatus totemInstruction_SetC(totemInstruction *instruction, totemOperandPrototype* index)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index->RegisterIndex, totemInstructionStart_C);
    return status;
}

totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
        instruction,
        op,
        TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_Op),
        TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_Op),
        totemInstructionStart_Op,
	    totemInstructionSize_Op);
	
    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_OP(*instruction) == op);
    return status;
}

totemEvalStatus totemInstruction_SetB1(totemInstruction* instruction, totemOperandX bx)
{
    totemEvalStatus status = totemInstruction_SetSignedValue(
        instruction,
        bx,
        TOTEM_MINVAL_SIGNED(int32_t, totemInstructionSize_B1),
        TOTEM_MAXVAL_SIGNED(int32_t, totemInstructionSize_B1),
        totemInstructionStart_B,
        totemInstructionSize_B1);

    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_B1(*instruction) == bx);
    return status;
}

totemEvalStatus totemInstruction_SetB2u(totemInstruction *instruction, totemOperandXu bx)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
        instruction,
        bx,
        TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_B2),
        TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_B2),
        totemInstructionStart_B,
		totemInstructionSize_B2);
    
    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_B2U(*instruction) == bx);
    return status;
}

totemEvalStatus totemInstruction_SetB2(totemInstruction *instruction, totemOperandX bx)
{
    totemEvalStatus status = totemInstruction_SetSignedValue(
        instruction,
        bx,
        TOTEM_MINVAL_SIGNED(int32_t, totemInstructionSize_B2),
        TOTEM_MAXVAL_SIGNED(int32_t, totemInstructionSize_B2),
        totemInstructionStart_B,
        totemInstructionSize_B2);
    
    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_B2(*instruction) == bx);
    return status;
}

totemEvalStatus totemInstruction_SetA3(totemInstruction *instruction, totemOperandX ax)
{
    totemEvalStatus status = totemInstruction_SetSignedValue(
        instruction,
        ax,
        TOTEM_MINVAL_SIGNED(int32_t, totemInstructionSize_A3),
        TOTEM_MAXVAL_SIGNED(int32_t, totemInstructionSize_A3),
        totemInstructionStart_A,
        totemInstructionSize_A3);
    
    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_A3(*instruction) == ax);
    return status;
}

totemEvalStatus totemInstruction_SetA3u(totemInstruction *instruction, totemOperandXu ax)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
        instruction,
        ax,
        TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_A3),
        TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_A3),
        totemInstructionStart_A,
		totemInstructionSize_A3);
    
    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_A3U(*instruction) == ax);
    return status;
}

totemEvalStatus totemInstruction_SetC1(totemInstruction *instruction, totemOperandX cx)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
        instruction,
        cx,
        TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_C1),
        TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_C1),
        totemInstructionStart_C,
		totemInstructionSize_C1);
    
    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_C1(*instruction) == cx);
    return status;
}

totemEvalStatus totemInstruction_SetC1u(totemInstruction* instruction, totemOperandXu cx)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
        instruction,
        cx,
        TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_C1),
        TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_C1),
        totemInstructionStart_C,
        totemInstructionSize_C1);

    TOTEM_ASSERT(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_C1U(*instruction) == cx);
    return status;
}

totemEvalStatus totemBuildPrototype_AllocInstruction(totemBuildPrototype *build, totemInstruction **instructionOut)
{
    *instructionOut = totemInstructionDynBuffer_Secure(&build->Instructions, 1);
    if (*instructionOut)
    {
        return totemEvalStatus_Success;
    }

	*instructionOut = 0;
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemInstruction_AsA(totemInstruction *instruction, EMIT_PARAMS_A, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_A);
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));

	return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsA3(totemInstruction *instruction, EMIT_PARAMS_A3, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_A3);
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA3(instruction, a3));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAb(totemInstruction *instruction, EMIT_PARAMS_Ab, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Ab);
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB(instruction, b));

	return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAb2(totemInstruction *instruction, EMIT_PARAMS_Ab2, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Ab2);
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB2(instruction, b2));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAb2u(totemInstruction *instruction, EMIT_PARAMS_Ab2u, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Ab2u);
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB2u(instruction, b2u));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAb2ug(totemInstruction* instruction, EMIT_PARAMS_Ab2ug, totemOperationType operationType)
{
    TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Ab2ug);
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB2u(instruction, b2ug));

    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAbc(totemInstruction *instruction, EMIT_PARAMS_Abc, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Abc);
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB(instruction, b));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetC(instruction, c));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAbc1(totemInstruction* instruction, EMIT_PARAMS_Abc1, totemOperationType operationType)
{
    TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Abc1);
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB(instruction, b));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetC1u(instruction, c1));

    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_AsAbc1u(totemInstruction *instruction, EMIT_PARAMS_Abc1u, totemOperationType operationType)
{
	TOTEM_ASSERT(totemOperationType_GetInstructionType(operationType) == totemInstructionType_Abc1u);
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetA(instruction, a));
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetB(instruction, b));
	TOTEM_EVAL_CHECKRETURN(totemInstruction_SetC1u(instruction, c1u));

	return totemEvalStatus_Success;
}

#define EMIT_TYPELESS_PARAMS_A		instruction, a
#define EMIT_TYPELESS_PARAMS_A3		instruction, a3
#define EMIT_TYPELESS_PARAMS_A3u	instruction, a3u
#define EMIT_TYPELESS_PARAMS_Ab		instruction, a, b
#define EMIT_TYPELESS_PARAMS_Ab2	instruction, a, b2
#define EMIT_TYPELESS_PARAMS_Ab2u	instruction, a, b2u
#define EMIT_TYPELESS_PARAMS_Ab2ug	instruction, a, b2ug
#define EMIT_TYPELESS_PARAMS_Abc	instruction, a, b, c
#define EMIT_TYPELESS_PARAMS_Abc1	instruction, a, b, c1
#define EMIT_TYPELESS_PARAMS_Abc1u	instruction, a, b, c1u
#define EMIT_TYPELESS_PARAMS_Ab1c1	instruction, a, b1, c1

#define EMIT(ins) \
totemEvalStatus totemBuildPrototype_Eval ## ins ## Instruction(totemBuildPrototype *build, EMIT_PARAMS_ ## ins, totemOperationType op) \
    { \
        totemInstruction *instruction = NULL; \
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction)); \
        TOTEM_EVAL_CHECKRETURN(totemInstruction_As ## ins (EMIT_TYPELESS_PARAMS_ ## ins, op)); \
        return totemEvalStatus_Success; \
    }

    TOTEM_EMIT_INSTRUCTION_TYPES(EMIT)
#undef EMIT

#define EMIT(op, type, behaviour) \
    totemEvalStatus totemInstruction_As_ ## op ( totemInstruction *instruction, EMIT_PARAMS_ ## type ) \
    { \
        return totemInstruction_As ## type(EMIT_TYPELESS_PARAMS_ ## type, totemOperationType_ ## op); \
    }  \
    \
    totemEvalStatus totemBuildPrototype_AddInstruction_ ## op ( totemBuildPrototype *build, EMIT_PARAMS_ ## type ) \
    { \
        totemInstruction *instruction = NULL; \
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction)); \
        TOTEM_EVAL_CHECKRETURN(totemInstruction_As_ ## op ( EMIT_TYPELESS_PARAMS_ ## type )); \
        return totemEvalStatus_Success; \
    }
    TOTEM_EMIT_OPCODES(EMIT)
#undef EMIT
