//
//  exec_jit.c
//  TotemScript
//
//  Created by Timothy Smale on 07/10/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <TotemScript/dispatch.h>

#ifdef TOTEM_JIT

typedef enum
{
	totemJitEmitStatus_Success,
	totemJitEmitStatus_OutOfMemory,
	totemJitEmitStatus_Unsupported
}
totemJitEmitStatus;

totemJitEmitStatus totemJitEmitStatus_Break(totemJitEmitStatus status)
{
	return status;
}

void totemJit_EmitEpilogue()
{
	totemJit_RestoreRegisters();
	totemJit_Return();
}

void totemJit_EmitPrologue()
{
	totemJit_SaveRegisters();
	totemJit_RetrieveGlobals();
	totemJit_RetrieveLocals();
	totemJit_RetrieveInstructionStart();
	totemJit_CheckInstructionStart();
	totemJit_JumpToStart();
}

totemJitEmitStatus totemJit_Emit(totemScriptFunction *func)
{
#ifndef TOTEM_JIT
	return totemJitEmitStatus_Break(totemJitEmitStatus_Unsupported);
#else

	/*
	memory layout:

	1. function prologue
		a. setup stack
		b. determine first instruction
		c. if not valid bail out
		d. jump to first instruction
	2. goto table - pointer per instruction
	3. call per instruction
	4. function epilogue
		a. cleanup stack
		b. return
	*/

	/*
	1. determine memory needed
		a. prologue
		b. call per instruction
		c. epilogue
	2. allocate via mmap
	3. emit prologue
	4. emit instruction calls
	5. emit epilogue
	6. change to read/execute
	
	*/

	size_t memNeeded = countMemNeeded(func);
	void *alloc = mmap(memNeeded);
	if (!alloc)
	{
		return totemJitEmitStatus_Break(totemJitEmitStatus_OutOfMemory);
	}

	totemJit_EmitPrologue();

	for (size_t i = 0; i < func->NumInstructions; i++)
	{
		gotoTable[i] = top;

		totemInstruction ins = func->InstructionsStart[i];
		totemOperationType op = TOTEM_INSTRUCTION_GET_OP(ins);
		
		if (op == totemOperationType_Goto)
		{
			__emitDirectGoto();
		}
		else
		{
			totemInstructionType insType = totemOperationType_GetInstructionType(op);

			switch (insType)
			{
				case totemInstructionType_A:
					___assignAParameters();
					break;

				case totemInstructionType_Ax:
					___assignAxParameters();
					break;

				case totemInstructionType_Axu:
					___assignAxuParameters();
					break;

				case totemInstructionType_Ab:
					___assignAbParameters();
					break;

				case totemInstructionType_Abx:
					___assignAbxParameters();
					break;

				case totemInstructionType_Abxu:
					___assignAbxuParameters();
					break;

				case totemInstructionType_Abc:
					___assignAbcParameters();
					break;

				case totemInstructionType_Abcx:
					___assignAbcxParameters();
					break;

				case totemInstructionType_Abcxu:
					___assignAbcxuParameters();
					break;

				default:
					badOp();
					break;
			}
			
			___call();

			if (op == totemOperationType_ConditionalGoto)
			{
				__performConditonalDirectGoto();
			}
			else if (op == totemOperationType_Return)
			{
				totemJit_EmitEpilogue();
			}
		}
	}

	// fix conditional gotos & direct gotos
	
	setToReadExec();

	return totemJitEmitStatus_Success;
#endif
}

#endif