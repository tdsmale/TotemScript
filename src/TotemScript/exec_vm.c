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

typedef struct
{
	totemInstruction *InsPtr;
	totemRegister *Base, *Globals;
	totemInstruction Ins;
	totemOperationType Op;
}
totemVMState;

void totemVMState_PrintInstructionDetailed(totemLog *file, totemVMState *vm, totemExecState *state);

static TOTEM_DEBUG_INLINE void totemVMState_Init(totemVMState *vm, totemExecState *state)
{
	vm->InsPtr = state->CallStack->ResumeAt;
	vm->Base = state->LocalRegisters;
	vm->Globals = state->GlobalRegisters;
}

static TOTEM_DEBUG_INLINE totemRegister *totemVMState_GetA(totemVMState *vm)
{
    totemRegister *r = &vm->Base[(TOTEM_INSTRUCTION_GET_A(vm->Ins))];
    return r;
}

static TOTEM_DEBUG_INLINE totemRegister* totemVMState_GetB(totemVMState *vm)
{
    totemRegister *r = &vm->Base[(TOTEM_INSTRUCTION_GET_B(vm->Ins))];
    return r;
}

static TOTEM_DEBUG_INLINE totemRegister* totemVMState_GetC(totemVMState *vm)
{
    totemRegister *r = &vm->Base[(TOTEM_INSTRUCTION_GET_C(vm->Ins))];
    return r;
}

static TOTEM_DEBUG_INLINE totemRegister* totemVMState_GetGlobal(totemVMState *vm, totemOperandXu index)
{
    totemRegister *r = &vm->Globals[index];
    return r;
}

static TOTEM_DEBUG_INLINE void totemVMState_FetchInstruction(totemVMState *vm, totemExecState *state)
{
	vm->Ins = *vm->InsPtr;
}

static TOTEM_DEBUG_INLINE void totemVMState_Prologue(totemVMState *vm, totemExecState *state)
{
	totemVMState_FetchInstruction(vm, state);
	vm->Op = TOTEM_INSTRUCTION_GET_OP(vm->Ins);
    TOTEM_LOG_DO(Exec, VM, Debug, dst, totemVMState_PrintInstructionDetailed(dst, vm, state));
}

static TOTEM_DEBUG_INLINE totemOperand3 totemVMState_GetA3(totemVMState *vm)
{
    totemOperand3 o = TOTEM_INSTRUCTION_GET_A3(vm->Ins);;
    return o;
}

static TOTEM_DEBUG_INLINE totemOperand2 totemVMState_GetB2(totemVMState *vm)
{
    totemOperand2 o = TOTEM_INSTRUCTION_GET_B2(vm->Ins);
    return o;
}

static TOTEM_DEBUG_INLINE totemOperand2u totemVMState_GetB2u(totemVMState *vm)
{
    totemOperand2u o = TOTEM_INSTRUCTION_GET_B2U(vm->Ins);
    return o;
}

static TOTEM_DEBUG_INLINE totemOperand1u totemVMState_GetC1u(totemVMState *vm)
{
    totemOperand1u o = TOTEM_INSTRUCTION_GET_C1U(vm->Ins);
    return o;
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

#define TOTEM_VM_OPCODE_FORMAT_TABLE(x, y, z) &&TOTEM_VM_TARGET_LABEL(x),
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

#define TOTEM_EMIT_STATE_PARAM_Pure TOTEM_NONE
#define TOTEM_EMIT_STATE_PARAM_None state, 
#define TOTEM_EMIT_STATE_PARAM(x) TOTEM_EMIT_STATE_PARAM_ ## x

// dispatch targets for each instruction type
#define TOTEM_VM_DISPATCH_A(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm))

#define TOTEM_VM_DISPATCH_A3(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA3(vm))

#define TOTEM_VM_DISPATCH_A3u(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA3u(vm))

#define TOTEM_VM_DISPATCH_Ab(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB(vm))

#define TOTEM_VM_DISPATCH_Ab2(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB2(vm))

#define TOTEM_VM_DISPATCH_Ab2u(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB2u(vm))

#define TOTEM_VM_DISPATCH_Ab2ug(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetGlobal(vm, totemVMState_GetB2u(vm)))

#define TOTEM_VM_DISPATCH_Abc(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC(vm))

#define TOTEM_VM_DISPATCH_Abc1(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC1(vm))

#define TOTEM_VM_DISPATCH_Abc1u(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB(vm), totemVMState_GetC1u(vm))

#define TOTEM_VM_DISPATCH_Ab1c1(vm, state, op, isPure) \
	TOTEM_DISPATCH_NAME(op) (TOTEM_EMIT_STATE_PARAM(isPure) totemVMState_GetA(vm), totemVMState_GetB1(vm), totemVMState_GetC1(vm))

#define TOTEM_VM_DISPATCH(vm, state, op, type, isPure) TOTEM_VM_DISPATCH_ ## type (vm, state, op, isPure)

#define TOTEM_VM_BEHAVIOUR_Pure(op, type) \
	vm.InsPtr++; \
	TOTEM_VM_DISPATCH(&vm, state, op, type, Pure); \
	TOTEM_VM_NEXT(&vm, state);

#define TOTEM_VM_BEHAVIOUR_PureControl(op, type) \
	vm.InsPtr += TOTEM_VM_DISPATCH(&vm, state, op, type, Pure); \
	TOTEM_VM_NEXT(&vm, state);

#define TOTEM_VM_BEHAVIOUR_Return(op, type) \
	vm.InsPtr++; \
	TOTEM_VM_DISPATCH(&vm, state, op, type, None); \
	return;

#define TOTEM_VM_BEHAVIOUR_None(op, type) \
	vm.InsPtr++; \
	TOTEM_VM_DISPATCH(&vm, state, op, type, None); \
	TOTEM_VM_NEXT(&vm, state);

void totemVMState_PrintInstructionDetailed(totemLog *file, totemVMState *vm, totemExecState *state)
{
    totemInstruction_Print(file, vm->Ins);
    totemInstructionType type = totemOperationType_GetInstructionType(vm->Op);
    switch (type)
    {
		case totemInstructionType_A:
        {
            totemRegister* a = totemVMState_GetA(vm);
            totemLog_Write(file, "a:");
            totemRuntime_PrintRegister(state->Runtime, file, a, 0, totemBool_False);
            break;
        }
            
		case totemInstructionType_Ab:
		{
			totemRegister* a = totemVMState_GetA(vm);
			totemRegister* b = totemVMState_GetB(vm);
            totemLog_Write(file, "a:");
			totemRuntime_PrintRegister(state->Runtime, file, a, 0, totemBool_False);
            totemLog_Write(file, "b:");
            totemRuntime_PrintRegister(state->Runtime, file, b, 0, totemBool_False);
			break;
		}
            
        case totemInstructionType_Abc:
        {
			totemRegister* a = totemVMState_GetA(vm);
			totemRegister* b = totemVMState_GetB(vm);
			totemRegister* c = totemVMState_GetC(vm);
            totemLog_Write(file, "a:");
            totemRuntime_PrintRegister(state->Runtime, file, a, 0, totemBool_False);
            totemLog_Write(file, "b:");
            totemRuntime_PrintRegister(state->Runtime, file, b, 0, totemBool_False);
            totemLog_Write(file, "c:");
            totemRuntime_PrintRegister(state->Runtime, file, c, 0, totemBool_False);
            break;
        }

        case totemInstructionType_Ab2:
		case totemInstructionType_Ab2u:
        {
			totemRegister* a = totemVMState_GetA(vm);
            totemLog_Write(file, "a:");
            totemRuntime_PrintRegister(state->Runtime, file, a, 0, totemBool_False);
            break;
        }

		case totemInstructionType_Ab2ug:
		{
			totemRegister* a = totemVMState_GetA(vm);
			totemRegister* g = totemVMState_GetGlobal(vm, totemVMState_GetB2u(vm));
            totemLog_Write(file, "a:");
            totemRuntime_PrintRegister(state->Runtime, file, a, 0, totemBool_False);
            totemLog_Write(file, "g:");
            totemRuntime_PrintRegister(state->Runtime, file, g, 0, totemBool_False);
			break;
		}	

		case totemInstructionType_Abc1:
		case totemInstructionType_Abc1u:
		{
			totemRegister* a = totemVMState_GetA(vm);
			totemRegister* b = totemVMState_GetB(vm);
            totemLog_Write(file, "a:");
            totemRuntime_PrintRegister(state->Runtime, file, a, 0, totemBool_False);
            totemLog_Write(file, "b:");
            totemRuntime_PrintRegister(state->Runtime, file, b, 0, totemBool_False);
			break;
		}

		default:
			break;
    }
}

void totemExecState_ExecuteInstructions(totemExecState *state)
{
	totemVMState vm;
    TOTEM_VM_DEFINE_TARGET_TABLE();

	totemVMState_Init(&vm, state);
	TOTEM_VM_LOOP(&vm, state)
	{
		// this should all be optimised away by the pre-processor & compiler to just the bare-minimum per-optype
#define TOTEM_VM_EMIT_TARGET(op, type, behaviour) \
		TOTEM_VM_TARGET(op) \
		{ \
			TOTEM_VM_BEHAVIOUR_ ## behaviour (op, type); \
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
	TOTEM_ZERO(dummyReturn);

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
			totemDispatch_FinalizeRoutine(state);
		}
	}

	state->JmpNode = node.Prev;

	return node.Status;
}

static void totemExecState_BootstrapNative(totemExecState *state, void *arg, totemRegister *dummyReturn)
{
	totemNativeFunction *function = (totemNativeFunction*)arg;
	totemDispatch_PreInvokeNativeFunc(state, function, 0);
	TOTEM_DISPATCH_NAME(Invoke_Subroutine)(state, dummyReturn);
}

static void totemExecState_BootstrapInstance(totemExecState *state, void *arg, totemRegister *dummyReturn)
{
	totemInstanceFunction *function = (totemInstanceFunction*)arg;
	totemDispatch_PreInvokeInstanceFunc(state, function, 0);
	TOTEM_DISPATCH_NAME(Invoke_Subroutine)(state, dummyReturn);
}

totemExecStatus totemExecState_StartNative(totemExecState *state, totemNativeFunction *function)
{
	return totemExecState_Start(state, function, totemExecState_BootstrapNative);
}

totemExecStatus totemExecState_StartInstance(totemExecState *state, totemInstanceFunction *function)
{
	return totemExecState_Start(state, function, totemExecState_BootstrapInstance);
}
