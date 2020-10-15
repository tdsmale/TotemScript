//
//  exec_vm.c
//  TotemScript
//
//  Created by Timothy Smale on 10/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <TotemScript/dispatch.h>
#include <string.h>
#include <limits.h>

#if TOTEM_DEBUGOPT_PRINT_VM_ACTIVITY
#define TOTEM_INSTRUCTION_PRINT_DEBUG(vm, state) totemVMState_PrintInstructionDetailed(vm, state, stdout)
#else
#define TOTEM_INSTRUCTION_PRINT_DEBUG(vm, state)
#endif

#define TOTEM_VM_GET_REG(x) totemDispatchArgument_FromRegister((x))
#define TOTEM_VM_GET_XU(x) totemDispatchArgument_FromOperandXUnsigned((x))
#define TOTEM_VM_GET_X(x) totemDispatchArgument_FromOperandXSigned((x))
#define TOTEM_VM_ZERO TOTEM_VM_GET_REG(NULL)
#define TOTEM_VM_GET_A3(instruction) TOTEM_VM_GET_X(TOTEM_INSTRUCTION_GET_A3(instruction))
#define TOTEM_VM_GET_A3U(instruction) TOTEM_VM_GET_XU(TOTEM_INSTRUCTION_GET_A3U(instruction))
#define TOTEM_VM_GET_B2(instruction) TOTEM_VM_GET_X(TOTEM_INSTRUCTION_GET_B2(instruction))
#define TOTEM_VM_GET_B2U(instruction) TOTEM_VM_GET_XU(TOTEM_INSTRUCTION_GET_B2U(instruction))
#define TOTEM_VM_GET_B1(instruction) TOTEM_VM_GET_X(TOTEM_INSTRUCTION_GET_B1(instruction))
#define TOTEM_VM_GET_C1(instruction) TOTEM_VM_GET_X(TOTEM_INSTRUCTION_GET_C1(instruction))
#define TOTEM_VM_GET_C1U(instruction) TOTEM_VM_GET_XU(TOTEM_INSTRUCTION_GET_C1U(instruction))

#if TOTEM_VMOPT_GLOBAL_OPERANDS

typedef struct totemVMState
{
	totemInstruction *InsPtr;
	totemRegister *Base[2];
	totemInstruction Ins;
	totemOperationType Op;
}
totemVMState;

static TOTEM_DEBUG_INLINE void totemVMState_Init(totemVMState *vm, totemExecState *state)
{
	vm->InsPtr = state->CallStack->ResumeAt;
	vm->Base[totemOperandType_GlobalRegister] = state->GlobalRegisters;
	vm->Base[totemOperandType_LocalRegister] = state->LocalRegisters;
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetA(totemVMState *vm)
{
	return TOTEM_VM_GET_REG(&vm->Base[(TOTEM_INSTRUCTION_GET_REGISTERA_SCOPE(vm->Ins))][(TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(vm->Ins))]);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetB(totemVMState *vm)
{
	return TOTEM_VM_GET_REG(&vm->Base[(TOTEM_INSTRUCTION_GET_REGISTERB_SCOPE(vm->Ins))][(TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(vm->Ins))]);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetC(totemVMState *vm)
{
	return TOTEM_VM_GET_REG(&vm->Base[(TOTEM_INSTRUCTION_GET_REGISTERC_SCOPE(vm->Ins))][(TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(vm->Ins))]);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetGlobal(totemVMState *vm, totemOperandXUnsigned index)
{
	return TOTEM_VM_GET_REG(&vm->Base[totemOperandType_GlobalRegister][index]);
}

#else

typedef struct
{
	totemInstruction *InsPtr;
	totemRegister *Base, *Globals;
	totemInstruction Ins;
	totemOperationType Op;
}
totemVMState;

static TOTEM_DEBUG_INLINE void totemVMState_Init(totemVMState *vm, totemExecState *state)
{
	vm->InsPtr = state->CallStack->ResumeAt;
	vm->Base = state->GlobalRegisters;
	vm->Globals = state->LocalRegisters;
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetA(totemVMState *vm)
{
	return TOTEM_VM_GET_REG(&vm->Base[(TOTEM_INSTRUCTION_GET_REGISTERA_INDEX(vm->Ins))]);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetB(totemVMState *vm)
{
	return TOTEM_VM_GET_REG(&vm->Base[(TOTEM_INSTRUCTION_GET_REGISTERB_INDEX(vm->Ins))]);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetC(totemVMState *vm)
{
	return TOTEM_VM_GET_REG(&vm->Base[(TOTEM_INSTRUCTION_GET_REGISTERC_INDEX(vm->Ins))]);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetGlobal(totemVMState *vm, totemOperandXUnsigned index)
{
	return TOTEM_VM_GET_REG(&vm->Globals[index]);
}

#endif

static TOTEM_DEBUG_INLINE void totemVMState_FetchInstruction(totemVMState *vm, totemExecState *state)
{
	vm->Ins = *vm->InsPtr;
	TOTEM_INSTRUCTION_PRINT_DEBUG(vm, state);
}

static TOTEM_DEBUG_INLINE void totemVMState_Prologue(totemVMState *vm, totemExecState *state)
{
	totemVMState_FetchInstruction(vm, state);
	vm->Op = TOTEM_INSTRUCTION_GET_OP(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetA3(totemVMState *vm)
{
	return TOTEM_VM_GET_A3(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetA3u(totemVMState *vm)
{
	return TOTEM_VM_GET_A3U(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetB2(totemVMState *vm)
{
	return TOTEM_VM_GET_B2(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetB2u(totemVMState *vm)
{
	return TOTEM_VM_GET_B2U(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetB1(totemVMState *vm)
{
	return TOTEM_VM_GET_B1(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetC1(totemVMState *vm)
{
	return TOTEM_VM_GET_C1(vm->Ins);
}

static TOTEM_DEBUG_INLINE totemDispatchArgument totemVMState_GetC1u(totemVMState *vm)
{
	return TOTEM_VM_GET_C1U(vm->Ins);
}

#if TOTEM_VMOPT_THREADED_DISPATCH
#define TOTEM_VM_NEXT(vm, state) \
    totemVMState_Prologue((vm), (state)); \
    goto *s_opcodes[(vm)->Op];

#define TOTEM_VM_LOOP(vm, state) TOTEM_VM_NEXT((vm), (state))
#define TOTEM_VM_TARGET_LABEL(name) totem_vm_target_##name
#define TOTEM_VM_TARGET(name) TOTEM_VM_TARGET_LABEL(name):
#define TOTEM_VM_DEFAULT_TARGET_LABEL TOTEM_VM_TARGET_LABEL(badop)
#define TOTEM_VM_DEFAULT_TARGET() TOTEM_VM_DEFAULT_TARGET_LABEL:

#define TOTEM_VM_OPCODE_FORMAT_TABLE(x, y) &&TOTEM_VM_TARGET_LABEL(val),
#define TOTEM_VM_DEFINE_TARGET_TABLE() static const void *s_opcodes[UINT8_MAX + 1] = \
    { \
        TOTEM_EMIT_OPCODES(TOTEM_VM_OPCODE_FORMAT_TABLE) \
        [TOTEM_NUM_OPTYPES ... 255] = &&TOTEM_VM_DEFAULT_TARGET_LABEL \
    }

#else
#define TOTEM_VM_NEXT(vm, state) goto totem_vm_loop_entry;
#define TOTEM_VM_LOOP(vm, state) \
    totem_vm_loop_entry: \
    totemVMState_Prologue((vm), (state)); \
    switch ((vm)->Op)

#define TOTEM_VM_TARGET(name) case totemOperationType_ ## name:
#define TOTEM_VM_DEFAULT_TARGET() default:
#define TOTEM_VM_DEFINE_TARGET_TABLE()
#endif

#define TOTEM_EMIT_OPCODE_A(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, A, behaviour)
#define TOTEM_EMIT_OPCODE_A3(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, A3, behaviour)
#define TOTEM_EMIT_OPCODE_A3U(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, A3u, behaviour)
#define TOTEM_EMIT_OPCODE_AB(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, Ab, behaviour)
#define TOTEM_EMIT_OPCODE_AB2(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, Ab2, behaviour)
#define TOTEM_EMIT_OPCODE_AB2U(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Ab2u, behaviour)
#define TOTEM_EMIT_OPCODE_AB2UG(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Ab2ug, behaviour)
#define TOTEM_EMIT_OPCODE_ABC(_, op, behaviour)		TOTEM_EMIT_OPCODE(_, op, Abc, behaviour)
#define TOTEM_EMIT_OPCODE_ABC1(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Abc1, behaviour)
#define TOTEM_EMIT_OPCODE_ABC1U(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Abc1u, behaviour)
#define TOTEM_EMIT_OPCODE_AB1C1(_, op, behaviour)	TOTEM_EMIT_OPCODE(_, op, Ab1c1, behaviour)

// dispatch targets for each instruction type
#define TOTEM_VM_DISPATCH_A(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), TOTEM_VM_ZERO, TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_A3(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA3(vm), TOTEM_VM_ZERO, TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_A3u(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA3u(vm), TOTEM_VM_ZERO, TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_Ab(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB(vm), TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_Ab2(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB2(vm), TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_Ab2u(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB2u(vm), TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_Ab2ug(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetGlobal(vm, totemVMState_GetB2u(vm).XUnsigned), TOTEM_VM_ZERO)

#define TOTEM_VM_DISPATCH_Abc(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC(vm))

#define TOTEM_VM_DISPATCH_Abc1(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC1(vm))

#define TOTEM_VM_DISPATCH_Abc1u(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC1u(vm))

#define TOTEM_VM_DISPATCH_Abc1u(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC1u(vm))

#define TOTEM_VM_DISPATCH_Ab1c1(vm, state, op) \
	TOTEM_DISPATCH_NAME(op) (state, totemVMState_GetA(vm), totemVMState_GetB1(vm), totemVMState_GetC1(vm))

#define TOTEM_VM_DISPATCH(vm, state, op, type) TOTEM_VM_DISPATCH_ ## type (vm, state, op)

void totemVMState_PrintInstructionDetailed(totemVMState *vm, totemExecState *state, FILE *file)
{
    totemInstruction_Print(file, vm->Ins);
    totemInstructionType type = totemOperationType_GetInstructionType(vm->Op);
    switch (type)
    {
		case totemInstructionType_A:
		case totemInstructionType_Ab1c1:
		{
			totemDispatchArgument a = totemVMState_GetA(vm);
			fprintf(file, "a:");
			totemExecState_PrintRegister(state, file, a.Reg);
			break;
		}		

		case totemInstructionType_Ab:
		{
			totemDispatchArgument a = totemVMState_GetA(vm);
			totemDispatchArgument b = totemVMState_GetB(vm);
			fprintf(file, "a:");
			totemExecState_PrintRegister(state, file, a.Reg);
			fprintf(file, "b:");
			totemExecState_PrintRegister(state, file, b.Reg);
			break;
		}
            
        case totemInstructionType_Abc:
        {
			totemDispatchArgument a = totemVMState_GetA(vm);
			totemDispatchArgument b = totemVMState_GetB(vm);
			totemDispatchArgument c = totemVMState_GetC(vm);
            fprintf(file, "a:");
            totemExecState_PrintRegister(state, file, a.Reg);
            fprintf(file, "b:");
            totemExecState_PrintRegister(state, file, b.Reg);
            fprintf(file, "c:");
            totemExecState_PrintRegister(state, file, c.Reg);
            break;
        }

        case totemInstructionType_Ab2:
		case totemInstructionType_Ab2u:
        {
			totemDispatchArgument a = totemVMState_GetA(vm);
            fprintf(file, "a:");
            totemExecState_PrintRegister(state, file, a.Reg);   
            break;
        }

		case totemInstructionType_Ab2ug:
		{
			totemDispatchArgument a = totemVMState_GetA(vm);
			totemDispatchArgument g = totemVMState_GetGlobal(vm, totemVMState_GetB2u(vm).XUnsigned);
			fprintf(file, "a:");
			totemExecState_PrintRegister(state, file, a.Reg);
			fprintf(file, "g:");
			totemExecState_PrintRegister(state, file, g.Reg);
			break;
		}	

		case totemInstructionType_Abc1:
		case totemInstructionType_Abc1u:
		{
			totemDispatchArgument a = totemVMState_GetA(vm);
			totemDispatchArgument b = totemVMState_GetB(vm);
			fprintf(file, "a:");
			totemExecState_PrintRegister(state, file, a.Reg);
			fprintf(file, "b:");
			totemExecState_PrintRegister(state, file, b.Reg);
			break;
		}

		default:
			break;
    }
    
    fprintf(file, "\n");
}

void totemExecState_ExecuteInstructions(totemExecState *state)
{
	totemVMState vm;
    TOTEM_VM_DEFINE_TARGET_TABLE();

	totemVMState_Init(&vm, state);
	TOTEM_VM_LOOP(&vm, state)
	{
		// this should all be optimised away by the pre-processor & compiler to just the bare-minimum per-optype
#define TOTEM_VM_EMIT_TARGET(op, type, flags) \
		TOTEM_VM_TARGET(op) \
			{ \
			const static totemOpcodeBehaviourFlag flag = totemOpcodeBehaviourFlag_ ## flags; \
			if(TOTEM_HASBITS(flag, totemOpcodeBehaviourFlag_Control)) \
			{ \
				vm.InsPtr += TOTEM_VM_DISPATCH(&vm, state, op, type); \
			} \
			else \
			{ \
				vm.InsPtr++; \
				TOTEM_VM_DISPATCH(&vm, state, op, type); \
			} \
			if(TOTEM_HASBITS(flag, totemOpcodeBehaviourFlag_Return)) \
			{ \
				return; \
			} \
			else \
			{ \
				TOTEM_VM_NEXT(&vm, state); \
			} \
		}

		TOTEM_EMIT_OPCODES(TOTEM_VM_EMIT_TARGET)

        TOTEM_VM_DEFAULT_TARGET()
        {
			TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnrecognisedOperation);
        }
    }
    
    // should never reach this
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_InvalidDispatch);
}

static TOTEM_DEBUG_INLINE totemExecStatus totemExecState_Start(totemExecState *state, void *arg, totemBootstrapFunctionCb cb)
{
	totemFunctionCall *retain = state->CallStack;

	totemJmpNode node;
	node.Status = totemExecStatus_Continue;
	node.Prev = state->JmpNode;
	state->JmpNode = &node;

	totemRegister dummyReturn;
	TOTEM_ZERO(&dummyReturn);

	TOTEM_JMP_TRY(&node.Buffer)
	{
		cb(state, arg, &dummyReturn);
	}
	TOTEM_JMP_CATCH(node.Buffer)
	{
		totemExecState_CollectGarbage(state, totemBool_True);

		// rollback calls
		for (totemFunctionCall *call = state->CallStack; call != retain; call = state->CallStack)
		{
			totemExecState_FinalizeRoutine(state);
		}
	}

	state->JmpNode = node.Prev;
	totemExecState_AssignNull(state, &dummyReturn);

	return node.Status;
}

static void totemExecState_BootstrapNative(totemExecState *state, void *arg, totemRegister *dummyReturn)
{
	totemNativeFunction *function = (totemNativeFunction*)arg;
	totemExecState_PreInvokeNative(state, function, 0);
	totemExecState_Invoke(state, dummyReturn);
}

static void totemExecState_BootstrapInstance(totemExecState *state, void *arg, totemRegister *dummyReturn)
{
	totemInstanceFunction *function = (totemInstanceFunction*)arg;
	totemExecState_PreInvokeInstance(state, function, 0);
	totemExecState_Invoke(state, dummyReturn);
}

totemExecStatus totemExecState_StartNative(totemExecState *state, totemNativeFunction *function)
{
	return totemExecState_Start(state, function, totemExecState_BootstrapNative);
}

totemExecStatus totemExecState_StartInstance(totemExecState *state, totemInstanceFunction *function)
{
	return totemExecState_Start(state, function, totemExecState_BootstrapInstance);
}