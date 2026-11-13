//
//  dispatch.h
//  TotemScript
//
//  Created by Timothy Smale on 24/09/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_DISPATCH_H
#define TOTEMSCRIPT_DISPATCH_H

#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <math.h>

#define TOTEM_DISPATCH_PARAMS_A		totemRegister* a
#define TOTEM_DISPATCH_PARAMS_A3	totemOperandX a3
#define TOTEM_DISPATCH_PARAMS_A3u	totemOperandXu a3u
#define TOTEM_DISPATCH_PARAMS_Ab	totemRegister* a, totemRegister* b
#define TOTEM_DISPATCH_PARAMS_Ab2	totemRegister* a, totemOperandX b2
#define TOTEM_DISPATCH_PARAMS_Ab2u	totemRegister* a, totemOperandXu b2u
#define TOTEM_DISPATCH_PARAMS_Ab2ug	totemRegister* a, totemRegister *bg
#define TOTEM_DISPATCH_PARAMS_Abc	totemRegister* a, totemRegister* b, totemRegister* c
#define TOTEM_DISPATCH_PARAMS_Abc1	totemRegister* a, totemRegister* b, totemOperandX c1
#define TOTEM_DISPATCH_PARAMS_Abc1u	totemRegister* a, totemRegister* b, totemOperandXu c1u
#define TOTEM_DISPATCH_PARAMS_Ab1c1	totemRegister* a, totemOperandX b1, totemOperandX c1

typedef void totemDispatchReturnType_None;
typedef void totemDispatchReturnType_Return;
typedef void totemDispatchReturnType_Pure;
typedef totemOperandX totemDispatchReturnType_PureControl;

#define EMIT_OP(op, type, behaviour) typedef totemDispatchReturnType_ ## behaviour totemDispatchReturnType_ ## op;
TOTEM_EMIT_OPCODES(EMIT_OP)
#undef EMIT_OP

#define TOTEM_DISPATCH_NAME(op) totemOperationType_ ## op ## _Dispatch

#ifdef TOTEM_X86
#define TOTEM_DISPATCH_FUNC_SIGNATURE TOTEM_DEBUG_INLINE TOTEM_CDECL
#else
#define TOTEM_DISPATCH_FUNC_SIGNATURE TOTEM_DEBUG_INLINE 
#endif

#define TOTEM_DISPATCH_FUNC(op, ins) \
	TOTEM_STATIC_ASSERT(totemInstructionType_ ## ins == (totemInstructionType)totemOperationInstructionType_ ## op, "given op type must be valid"); \
	TOTEM_DISPATCH_FUNC_SIGNATURE totemDispatchReturnType_ ## op TOTEM_DISPATCH_NAME(op) ( totemExecState *state, TOTEM_DISPATCH_PARAMS_ ## ins )

#define TOTEM_DISPATCH_FUNC_PURE(op, ins) \
	TOTEM_STATIC_ASSERT(totemInstructionType_ ## ins == (totemInstructionType)totemOperationInstructionType_ ## op, "given op type must be valid"); \
	TOTEM_DISPATCH_FUNC_SIGNATURE totemDispatchReturnType_ ## op TOTEM_DISPATCH_NAME(op) ( TOTEM_DISPATCH_PARAMS_ ## ins )

#define TOTEM_DISPATCH_FUNC_A(op) TOTEM_DISPATCH_FUNC(op, A)
#define TOTEM_DISPATCH_FUNC_A3(op) TOTEM_DISPATCH_FUNC(op, A3)
#define TOTEM_DISPATCH_FUNC_A3u(op) TOTEM_DISPATCH_FUNC(op, A3u)
#define TOTEM_DISPATCH_FUNC_Ab(op) TOTEM_DISPATCH_FUNC(op, Ab)
#define TOTEM_DISPATCH_FUNC_Ab2(op) TOTEM_DISPATCH_FUNC(op, Ab2)
#define TOTEM_DISPATCH_FUNC_Ab2u(op) TOTEM_DISPATCH_FUNC(op, Ab2u)
#define TOTEM_DISPATCH_FUNC_Ab2ug(op) TOTEM_DISPATCH_FUNC(op, Ab2ug)
#define TOTEM_DISPATCH_FUNC_Abc(op) TOTEM_DISPATCH_FUNC(op, Abc)
#define TOTEM_DISPATCH_FUNC_Abc1(op) TOTEM_DISPATCH_FUNC(op, Abc1)
#define TOTEM_DISPATCH_FUNC_Abc1u(op) TOTEM_DISPATCH_FUNC(op, Abc1u)
#define TOTEM_DISPATCH_FUNC_Ab1c1(op) TOTEM_DISPATCH_FUNC(op, Ab1c1)

#define TOTEM_DISPATCH_FUNC_A_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, A)
#define TOTEM_DISPATCH_FUNC_A3_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, A3)
#define TOTEM_DISPATCH_FUNC_A3u_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, A3u)
#define TOTEM_DISPATCH_FUNC_Ab_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Ab)
#define TOTEM_DISPATCH_FUNC_Ab2_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Ab2)
#define TOTEM_DISPATCH_FUNC_Ab2u_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Ab2u)
#define TOTEM_DISPATCH_FUNC_Ab2ug_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Ab2ug)
#define TOTEM_DISPATCH_FUNC_Abc_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Abc)
#define TOTEM_DISPATCH_FUNC_Abc1_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Abc1)
#define TOTEM_DISPATCH_FUNC_Abc1u_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Abc1u)
#define TOTEM_DISPATCH_FUNC_Ab1c1_PURE(op) TOTEM_DISPATCH_FUNC_PURE(op, Ab1c1)

#define TOTEM_DISPATCH_ERROR(state, status) \
    state->JmpNode->Status = totemExecStatus_Break(status); \
    TOTEM_JMP_THROW(&state->JmpNode->Buffer);

#define TOTEM_DISPATCH_ASSERT(x, state, status) if (!(x)) { TOTEM_DISPATCH_ERROR(state, status); }

#define TOTEM_DISPATCH_BREAK(x, state) \
	{ \
        totemExecStatus status = (x); \
        if (status != totemExecStatus_Continue) \
		{ \
            TOTEM_DISPATCH_ERROR(state, status); \
		} \
	}

#define TOTEM_DISPATCH_FUNCS_ARITH_INT(type, op) \
	TOTEM_DISPATCH_FUNC_Abc_PURE(type ## _IntInt) \
	{ \
		totemInt _a = totemRegister_GetInt(b); \
		totemInt _b = totemRegister_GetInt(c); \
		totemInt _c = _a op _b; \
		totemRegister_SetInt(a, _c); \
	} \
	\
	TOTEM_DISPATCH_FUNC_Abc1u_PURE(type ## _IntIntConst) \
	{ \
		totemInt _a = totemRegister_GetInt(b); \
		totemInt _b = c1u; \
		totemInt _c = _a op _b; \
		totemRegister_SetInt(a, _c); \
	} \

#define TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(type, op) \
	TOTEM_DISPATCH_FUNC_Abc_PURE(type ## _FloatFloat) \
	{ \
		totemFloat _a = totemRegister_GetFloat(b); \
		totemFloat _b = totemRegister_GetFloat(c); \
		totemFloat _c = _a op _b; \
		totemRegister_SetFloat(a, _c); \
	} \

#define TOTEM_DISPATCH_FUNCS_ARITH_INT_DIVIDE(type, op) \
	TOTEM_DISPATCH_FUNC_Abc_PURE(type ## Unsafe_IntInt) \
	{ \
		totemInt _a = totemRegister_GetInt(b); \
		totemInt _b = totemRegister_GetInt(c); \
		totemInt _c = _a op _b; \
		totemRegister_SetInt(a, _c); \
	} \
	\
	TOTEM_DISPATCH_FUNC_Abc(type ## _IntInt) \
	{ \
		TOTEM_DISPATCH_ASSERT(!totemRegister_IsZero(c), state, totemExecStatus_DivideByZero); \
		TOTEM_DISPATCH_NAME(type ## Unsafe_IntInt)(a, b, c); \
	} \
	\
	TOTEM_DISPATCH_FUNC_Abc1u_PURE(type ## _IntIntConst) \
	{ \
		totemInt _a = totemRegister_GetInt(b); \
		totemInt _b = c1u; \
		totemInt _c = _a op _b; \
		totemRegister_SetInt(a, _c); \
	} \

#define TOTEM_DISPATCH_COMMON_ARITH_INT(state, dst, src1, src2, op) \
	if (totemRegister_IsInt(src1)) \
	{ \
		if (totemRegister_IsInt(src2)) \
		{ \
			TOTEM_DISPATCH_NAME(op ## _IntInt)(dst, src1, src2); \
			return; \
		} \
        else if (totemRegister_IsFloat(src2)) \
        { \
            totemRegister newSrc; \
            totemRegister_IntFromFloat(&newSrc, src1); \
            TOTEM_DISPATCH_NAME(op ## _FloatFloat)(dst, &newSrc, src2); \
            return; \
        } \
		\
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
	}

#define TOTEM_DISPATCH_COMMON_ARITH_INT_ONLY(state, dst, src1, src2, op) \
    if (totemRegister_IsInt(src1)) \
    { \
        if (totemRegister_IsInt(src2)) \
        { \
            TOTEM_DISPATCH_NAME(op ## _IntInt)(dst, src1, src2); \
            return; \
        } \
        \
        TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
    }

#define TOTEM_DISPATCH_COMMON_ARITH_FLOAT(state, dst, src1, src2, op) \
	if (totemRegister_IsFloat(src1)) \
	{ \
		if (totemRegister_IsFloat(src2)) \
		{ \
			TOTEM_DISPATCH_NAME(op ## _FloatFloat)(dst, src1, src2); \
			return; \
		} \
        else if (totemRegister_IsInt(src2)) \
        { \
            totemRegister newSrc; \
            totemRegister_IntFromFloat(&newSrc, src2); \
            TOTEM_DISPATCH_NAME(op ## _FloatFloat)(dst, src1, &newSrc); \
            return; \
        } \
        \
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
	}

#define TOTEM_DISPATCH_COMMON_ARITH(state, dst, src1, src2, op) \
	TOTEM_DISPATCH_COMMON_ARITH_INT(state, dst, src1, src2, op) \
	TOTEM_DISPATCH_COMMON_ARITH_FLOAT(state, dst, src1, src2, op) \

#define TOTEM_DISPATCH_COMMON_ARITH_DIVIDE(state, dst, src1, src2, op) \
	if (totemRegister_IsInt(src1)) \
	{ \
		if (totemRegister_IsInt(src2)) \
		{ \
			TOTEM_DISPATCH_NAME(op ## _IntInt)(state, dst, src1, src2); \
			return; \
		} \
        else if (totemRegister_IsFloat(src2)) \
        { \
            totemRegister newSrc; \
            totemRegister_IntFromFloat(&newSrc, src1); \
            TOTEM_DISPATCH_NAME(op ## _FloatFloat)(state, dst, &newSrc, src2); \
            return; \
        } \
		\
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
	} \
	if (totemRegister_IsFloat(src1)) \
	{ \
		if (totemRegister_IsFloat(src2)) \
		{ \
			TOTEM_DISPATCH_NAME(op ## _FloatFloat)(state, dst, src1, src2); \
			return; \
        }\
        else if (totemRegister_IsInt(src2)) \
        { \
            totemRegister newSrc; \
            totemRegister_IntFromFloat(&newSrc, src2); \
            TOTEM_DISPATCH_NAME(op ## _FloatFloat)(state, dst, src1, &newSrc); \
            return; \
        } \
		\
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
	}

TOTEM_DEBUG_INLINE totemGCObject* totemDispatch_GetGCObject(totemExecState * state, totemRegister * from)
{
	totemGCObject* gc = totemRegister_GetGCObject(from);
	TOTEM_DISPATCH_ASSERT(gc != NULL, state, totemExecStatus_NullPointer);
	return gc;
}

TOTEM_DEBUG_INLINE totemFunctionCall *totemExecState_CreateSubroutine(totemExecState *state, uint32_t numRegisters, totemGCObject *instance, totemRegister *returnReg, totemRuntimeFunctionCb cb, void *function)
{
	totemFunctionCall *call = totemExecState_SecureFunctionCall(state);
	TOTEM_DISPATCH_ASSERT(call, state, totemExecStatus_OutOfMemory);

	if (state->NextFreeRegister + numRegisters > state->LastFreeRegister)
	{
		call->FrameStart = (totemRegister*)totemExecState_Alloc(state, sizeof(totemRegister) * numRegisters);
		if (!call->FrameStart)
		{
			totemExecState_FreeFunctionCall(state, call);
			TOTEM_DISPATCH_ERROR(state, totemExecStatus_OutOfMemory);
		}

		TOTEM_SETBITS(call->Flags, totemFunctionCallFlag_FreeStack);
	}
	else
	{
		call->FrameStart = state->NextFreeRegister;
		state->NextFreeRegister += numRegisters;
	}

	// reset registers to be used
	totemRegister_InitList(call->FrameStart, numRegisters);

	call->Instance = instance;
	call->ReturnRegister = returnReg;
	call->Function.Void = function;
	call->NumRegisters = numRegisters;
	call->Invoke = cb;
	call->NumArguments = 0;

	return call;
}

TOTEM_DEBUG_INLINE void totemDispatch_PushRoutine(totemExecState *state, totemFunctionCall *call, totemRegister *globals, totemInstruction *startAt)
{
	call->PreviousFrameStart = state->LocalRegisters;
	call->PreviousGlobals = state->GlobalRegisters;
	call->ResumeAt = startAt;
	call->Prev = state->CallStack;
	state->CallStack = call;
	state->LocalRegisters = call->FrameStart;
	state->GlobalRegisters = globals;
}

TOTEM_DEBUG_INLINE void totemDispatch_PopRoutine(totemExecState *state)
{
	totemExecState_CollectGarbage(state, totemBool_False);

	totemFunctionCall *call = state->CallStack;
	
	state->LocalRegisters = call->PreviousFrameStart;
	state->GlobalRegisters = call->PreviousGlobals;
	state->CallStack = call->Prev;
}

TOTEM_DEBUG_INLINE void totemDispatch_FinalizeSubroutine(totemExecState* state)
{
	totemFunctionCall* call = state->CallStack;

	if (TOTEM_HASBITS(call->Flags, totemFunctionCallFlag_FreeStack))
	{
        TOTEM_FREE(call->FrameStart, sizeof(totemRegister) * call->NumRegisters);
	}
	else
	{
		state->NextFreeRegister -= call->NumRegisters;
	}

	totemDispatch_PopRoutine(state);
	totemExecState_FreeFunctionCall(state, call);
}

TOTEM_DEBUG_INLINE void totemDispatch_FinalizeCoroutine(totemExecState* state)
{
	totemDispatch_PopRoutine(state);
}

TOTEM_DEBUG_INLINE void totemDispatch_FinalizeRoutine(totemExecState *state)
{
	if (TOTEM_HASBITS(state->CallStack->Flags, totemFunctionCallFlag_IsCoroutine))
	{
		totemDispatch_FinalizeCoroutine(state);
	}
	else
	{
		totemDispatch_FinalizeSubroutine(state);
	}
}

TOTEM_DEBUG_INLINE void totemDispatch_CastFunctionToCoroutine(totemExecState* state, totemRegister* dst, totemInstanceFunction* function)
{
	totemGCObject* obj = NULL;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateCoroutine(state, function, &obj), state);
	totemRegister_SetCoroutine(dst, obj);
}

TOTEM_DEBUG_INLINE void totemDispatch_PreInvokeNativeFunc(totemExecState* state, totemNativeFunction* func, totemOperandXu numArgs)
{
	totemFunctionCall* call = totemExecState_CreateSubroutine(
		state,
		numArgs,
		state->CallStack->Instance,
		NULL,
		func->Callback,
		func);

	totemDispatch_PushRoutine(state, call, NULL, NULL);
}

TOTEM_DEBUG_INLINE void totemDispatch_PreInvokeInstanceFunc(totemExecState* state, totemInstanceFunction* func, totemOperandXu numArgs)
{
	totemFunctionCall* call = totemExecState_CreateSubroutine(
		state,
		numArgs <= func->Function->RegistersNeeded ? func->Function->RegistersNeeded : numArgs,
		func->Instance,
		NULL,
		func->Function->Invoke,
		func);

	totemDispatch_PushRoutine(state, call, call->Instance->Data.Registers, func->Function->InstructionsStart);
}

TOTEM_DEBUG_INLINE void totemDispatch_PushFunctionArg(totemExecState *state, totemRegister *arg)
{
	totemRegister_Set(&state->CallStack->FrameStart[state->CallStack->NumArguments++], arg);
}

TOTEM_DEBUG_INLINE void totemDispatch_Invoke(totemExecState* state, totemRegister* a)
{
	state->CallStack->ReturnRegister = a;
	state->CallStack->Invoke(state);
}

TOTEM_DEBUG_INLINE void totemDispatch_CreateArray(totemExecState* state, uint32_t size, totemRegister* dst)
{
	totemGCObject* gc;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateArray(state, size, &gc), state);
	totemRegister_SetArray(dst, gc);
}

TOTEM_DEBUG_INLINE void totemDispatch_CreateObject(totemExecState* state, uint32_t size, totemRegister* dst)
{
	totemGCObject* gc;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateObject(state, size, &gc), state);
	totemRegister_SetObject(dst, gc);
}

TOTEM_DEBUG_INLINE void totemDispatch_Is(totemPublicDataType against, totemRegister* src, totemRegister* dst)
{
	totemPrivateDataType typeAp = totemRegister_GetType(src);
	totemPublicDataType typeA = totemPrivateDataType_ToPublic(typeAp);
	totemBool result = typeA == against;
	totemRegister_SetBool(dst, result);
}

TOTEM_DEBUG_INLINE void totemDispatch_ComplexGet_Instance_WithString(totemExecState *state, totemRegister *dst, totemRegister *ins, totemRuntimeStringValue *key)
{
    totemGCObject* src = totemDispatch_GetGCObject(state, ins);
    totemScript *script = src->Ptr.Instance->Script;
    
    totemRuntimeStringMapKey lookup;
    lookup.Key = *key;
    
    if(!totemRuntimeStringMap_Get(&script->FunctionNameLookup, &lookup))
    {
        // following the same procedure as objects
        totemRegister_SetNull(dst);
    }
    else
    {
        totemInstanceFunction *func = &src->Ptr.Instance->LocalFunctions[lookup.Value];
        totemRegister_SetInstanceFunction(dst, func);
    }
}

TOTEM_DEBUG_INLINE void totemDispatch_ComplexGet_Instance_WithInternedString(totemExecState *state, totemRegister *dst, totemRegister *inst, totemRegister *keyReg)
{
    totemRuntimeStringValue key;
    totemInternedStringHeader* hdr = totemRegister_GetInternedString(keyReg);
    totemRuntimeStringValue_FromInternedString(hdr, &key);
    totemDispatch_ComplexGet_Instance_WithString(state, dst, inst, &key);
}

TOTEM_DEBUG_INLINE void totemDispatch_ComplexGet_Instance_WithMiniString(totemExecState *state, totemRegister *dst, totemRegister *inst, totemRegister *keyReg)
{
    totemRuntimeStringValue key;
    totemRuntimeStringValue_FromMiniStringRegister(keyReg, &key);
    totemDispatch_ComplexGet_Instance_WithString(state, dst, inst, &key);
}

TOTEM_DEBUG_INLINE void totemDispatch_ComplexGet_Instance(totemExecState *state, totemRegister *dst, totemRegister *ins, totemRegister *key)
{
    if(totemRegister_IsInternedString(key))
    {
        totemDispatch_ComplexGet_Instance_WithInternedString(state, dst, ins, key);
    }
    else if(totemRegister_IsMiniString(key))
    {
        totemDispatch_ComplexGet_Instance_WithMiniString(state, dst, ins, key);
    }
    else
    {
        TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
    }
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Move)
{
	totemRegister_Set(a, b);
}

TOTEM_DISPATCH_FUNC_A_PURE(Move_Null)
{
	totemRegister_SetNull(a);
}

TOTEM_DISPATCH_FUNC_Ab2u_PURE(Move_Int)
{
	totemInt val = totem_toInt(b2u);
	totemRegister_SetInt(a, val);
}

TOTEM_DISPATCH_FUNC_Ab2u_PURE(Move_Bool)
{
	totemRegister_SetBool(a, b2u);
}

TOTEM_DISPATCH_FUNC_Ab2u_PURE(Move_Type)
{
	totemRegister_SetTypeValue(a, b2u);
}

TOTEM_DISPATCH_FUNC_Ab2ug_PURE(MoveToLocal)
{
	totemRegister_Set(a, bg);
}

TOTEM_DISPATCH_FUNC_Ab2ug(MoveToGlobal)
{
	totemRegister_Set(bg, a);
	totemExecState_WriteBarrier(state, state->CallStack->Instance);
}
TOTEM_DISPATCH_FUNC_Abc(Add_ArrayArray)
{
	TOTEM_DISPATCH_BREAK(totemExecState_ConcatArrays(state, b, c, a), state);
}

TOTEM_DISPATCH_FUNC_Abc(Add_StringString)
{
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_ConcatStrings(state, b, c, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Add, +);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Add, +);

TOTEM_DISPATCH_FUNC_Abc(Add)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, Add);
	
	if (totemRegister_IsArray(b) && totemRegister_IsArray(c))
	{
		TOTEM_DISPATCH_NAME(Add_ArrayArray)(state, a, b, c);
	}
	else if (totemRegister_IsString(c) && totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(Add_StringString)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Subtract, -);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Subtract, -);

TOTEM_DISPATCH_FUNC_Abc(Subtract)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, Subtract);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}
TOTEM_DISPATCH_FUNCS_ARITH_INT(Multiply, *);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Multiply, *);

TOTEM_DISPATCH_FUNC_Abc(Multiply)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, Multiply);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT_DIVIDE(Divide, / );
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(DivideUnsafe, / );

TOTEM_DISPATCH_FUNC_Abc(Divide_FloatFloat)
{
	TOTEM_DISPATCH_ASSERT(!totemRegister_IsZero(c), state, totemExecStatus_DivideByZero);
	TOTEM_DISPATCH_NAME(DivideUnsafe_FloatFloat)(a, b, c);
}

TOTEM_DISPATCH_FUNC_Abc(Divide)
{
	TOTEM_DISPATCH_COMMON_ARITH_DIVIDE(state, a, b, c, Divide);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT_DIVIDE(Modulus, %);

TOTEM_DISPATCH_FUNC_Abc_PURE(ModulusUnsafe_FloatFloat)
{
	totemFloat _b = totemRegister_GetFloat(b);
	totemFloat _c = totemRegister_GetFloat(c);
	totemFloat _a = totem_toFloat(fmod((double)_b, (double)_c));
	totemRegister_SetFloat(a, _a);
}

TOTEM_DISPATCH_FUNC_Abc(Modulus_FloatFloat)
{
	TOTEM_DISPATCH_ASSERT(!totemRegister_IsZero(c), state, totemExecStatus_DivideByZero);
	TOTEM_DISPATCH_NAME(ModulusUnsafe_FloatFloat)(a, b, c);
}

TOTEM_DISPATCH_FUNC_Abc(Modulus)
{
	TOTEM_DISPATCH_COMMON_ARITH_DIVIDE(state, a, b, c, Modulus);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryLeftShift, << );

TOTEM_DISPATCH_FUNC_Abc(BinaryLeftShift)
{
	TOTEM_DISPATCH_COMMON_ARITH_INT_ONLY(state, a, b, c, BinaryLeftShift);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryRightShift, >> );

TOTEM_DISPATCH_FUNC_Abc(BinaryRightShift)
{
	TOTEM_DISPATCH_COMMON_ARITH_INT_ONLY(state, a, b, c, BinaryRightShift);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryAnd, &);

TOTEM_DISPATCH_FUNC_Abc(BinaryAnd)
{
	TOTEM_DISPATCH_COMMON_ARITH_INT_ONLY(state, a, b, c, BinaryAnd);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryOr, | );

TOTEM_DISPATCH_FUNC_Abc(BinaryOr)
{
	TOTEM_DISPATCH_COMMON_ARITH_INT_ONLY(state, a, b, c, BinaryOr);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryXor, ^);

TOTEM_DISPATCH_FUNC_Abc(BinaryXor)
{
	TOTEM_DISPATCH_COMMON_ARITH_INT_ONLY(state, a, b, c, BinaryXor);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(BinaryNot_Int)
{
	totemInt _a = totemRegister_GetInt(b);
	totemInt _c = ~_a;
	totemRegister_SetInt(a, _c);
}

TOTEM_DISPATCH_FUNC_Ab(BinaryNot)
{
	if (totemRegister_IsInt(b))
	{
		TOTEM_DISPATCH_NAME(BinaryNot_Int)(a, b);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(LessThan, < );
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(LessThan, < );

TOTEM_DISPATCH_FUNC_Abc(LessThan)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, LessThan);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(LessThanEquals, <= );
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(LessThanEquals, <= );

TOTEM_DISPATCH_FUNC_Abc(LessThanEquals)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, LessThanEquals);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(MoreThan, > );
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(MoreThan, > );

TOTEM_DISPATCH_FUNC_Abc(MoreThan)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, MoreThan);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(MoreThanEquals, >= );
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(MoreThanEquals, >= );

TOTEM_DISPATCH_FUNC_Abc(MoreThanEquals)
{
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, MoreThanEquals);
	TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
}

TOTEM_DISPATCH_FUNC_Abc_PURE(Equals)
{
	totemBool r = totemRegister_Equals(b, c);
	totemRegister_SetBool(a, r);
}

TOTEM_DISPATCH_FUNC_Abc_PURE(NotEquals)
{
	totemBool r = totemRegister_Equals(b, c);
	r = !r;
	totemRegister_SetBool(a, r);
}

TOTEM_DISPATCH_FUNC_Abc_PURE(LogicalOr)
{
	totemBool r = totemRegister_IsNotZero(b) || totemRegister_IsNotZero(c);
	totemRegister_SetBool(a, r);
}

TOTEM_DISPATCH_FUNC_Abc_PURE(LogicalAnd)
{
	totemBool r = totemRegister_IsNotZero(b) && totemRegister_IsNotZero(c);
	totemRegister_SetBool(a, r);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(LogicalNegate)
{
	totemRegister_SetBool(a, totemRegister_IsZero(b));
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Negate_Float)
{
	totemRegister_SetFloat(a, -totemRegister_GetFloat(b));
}

TOTEM_DISPATCH_FUNC_Ab(Negate)
{
	if (totemRegister_IsFloat(b))
	{
		TOTEM_DISPATCH_NAME(Negate_Float)(a, b);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab2_PURE(ConditionalGoto)
{
	return totemRegister_IsZero(a) ? b2 : 1;
}

TOTEM_DISPATCH_FUNC_A3_PURE(Goto)
{
	return a3;
}

TOTEM_DISPATCH_FUNC_A(Return)
{
	totemFunctionCall *call = state->CallStack;
	call->ResumeAt = NULL;
	totemRegister_Set(call->ReturnRegister, a);
}

TOTEM_DISPATCH_FUNC_Ab2u(Yield)
{
	totemFunctionCall *call = state->CallStack;
	call->ResumeAt = call->Function.Instance->Function->InstructionsStart + b2u;
	totemRegister_Set(call->ReturnRegister, a);
}

TOTEM_DISPATCH_FUNC_Ab(NewArray_WithInt)
{
	uint32_t size = (uint32_t)totemRegister_GetInt(b);
	totemDispatch_CreateArray(state, size, a);
}

TOTEM_DISPATCH_FUNC_Ab(NewArray_WithFloat)
{
	uint32_t size = (uint32_t)totemRegister_GetFloat(b);
	totemDispatch_CreateArray(state, size, a);
}

TOTEM_DISPATCH_FUNC_Ab2u(NewArray_WithIntConst)
{
	totemDispatch_CreateArray(state, b2u, a);
}

TOTEM_DISPATCH_FUNC_Ab(NewArray)
{
	if (totemRegister_IsInt(b))
	{
		TOTEM_DISPATCH_NAME(NewArray_WithInt)(state, a, b);
	}
	else if (totemRegister_IsFloat(b))
	{
		TOTEM_DISPATCH_NAME(NewArray_WithFloat)(state, a, b);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab(NewObject_WithInt)
{
	totemInt size = totemRegister_GetInt(b);
	totemDispatch_CreateObject(state, (uint32_t)size, a);
}

TOTEM_DISPATCH_FUNC_Ab(NewObject_WithFloat)
{
	totemFloat size = totemRegister_GetFloat(b);
	totemDispatch_CreateObject(state, (uint32_t)size, a);
}

TOTEM_DISPATCH_FUNC_Ab2u(NewObject_WithIntConst)
{
	totemDispatch_CreateObject(state, b2u, a);
}

TOTEM_DISPATCH_FUNC_Ab(NewObject)
{
	if (totemRegister_IsInt(b))
	{
		TOTEM_DISPATCH_NAME(NewObject_WithInt)(state, a, b);
	}
	else if (totemRegister_IsFloat(b))
	{
		TOTEM_DISPATCH_NAME(NewObject_WithFloat)(state, a, b);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab_PURE(LengthOf_Array)
{
	totemGCObject *obj = totemRegister_GetGCObject(b);
	totemRegister_SetFloat(a, obj ? obj->NumRegisters : 0);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(LengthOf_Object)
{
	totemGCObject* obj = totemRegister_GetGCObject(b);
	totemRegister_SetFloat(a, obj ? obj->Ptr.Object->NumKeys : 0);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(LengthOf_String)
{
	totemRegister_SetFloat(a, totem_toFloat(totemRegister_GetStringLength(b)));
}

TOTEM_DISPATCH_FUNC_Ab_PURE(LengthOf)
{
	if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(LengthOf_Array)(a, b);
	}
	else if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(LengthOf_Object)(a, b);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(LengthOf_String)(a, b);
	}
	else
	{
		totemRegister_SetFloat(a, !totemRegister_IsNull(b));
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_String_WithInt)
{
	totemInt index = totemRegister_GetInt(c);
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_InternStringChar(state, b, (uint32_t)index, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_String_WithFloat)
{
	totemFloat index = totemRegister_GetFloat(c);
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_InternStringChar(state, b, (uint32_t)index, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Abc1u(ComplexGet_String_WithIntConst)
{
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_InternStringChar(state, b, c1u, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_String)
{
	if (totemRegister_IsInt(c))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_String_WithInt)(state, a, b, c);
	}
	else if (totemRegister_IsFloat(c))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_String_WithFloat)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_Array_WithInt)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, b);
	totemInt index = totemRegister_GetInt(c);
	TOTEM_DISPATCH_BREAK(totemExecState_ArrayGet(state, gc, (uint32_t)index, a), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_Array_WithFloat)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, b);
	totemFloat index = totemRegister_GetFloat(c);
	TOTEM_DISPATCH_BREAK(totemExecState_ArrayGet(state, gc, (uint32_t)index, a), state);
}

TOTEM_DISPATCH_FUNC_Abc1u(ComplexGet_Array_WithIntConst)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, b);
	TOTEM_DISPATCH_BREAK(totemExecState_ArrayGet(state, gc, c1u, a), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_Array)
{
	if (totemRegister_IsInt(c))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Array_WithInt)(state, a, b, c);
	}
	else if (totemRegister_IsFloat(c))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Array_WithFloat)(state, a, b, c);
	}
	else 
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_Object_WithInternedString)
{
	totemRuntimeStringValue key;
	totemInternedStringHeader* hdr = totemRegister_GetInternedString(c);
	totemRuntimeStringValue_FromInternedString(hdr, &key);
	totemGCObject* src = totemDispatch_GetGCObject(state, b);
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectGet(state, src, &key, a), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_Object_WithMiniString)
{
	totemRuntimeStringValue key;
	totemRuntimeStringValue_FromMiniStringRegister(c, &key);
	totemGCObject* src = totemDispatch_GetGCObject(state, b);
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectGet(state, src, &key, a), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_Object)
{
	if (totemRegister_IsMiniString(c))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Object_WithMiniString)(state, a, b, c);
	}
	else if (totemRegister_IsInternedString(c))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Object_WithInternedString)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_WithFloat)
{
	if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Array_WithFloat)(state, a, b, c);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_String_WithFloat)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_WithInt)
{
	if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Array_WithInt)(state, a, b, c);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_String_WithInt)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc1u(ComplexGet_WithIntConst)
{
	if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Array_WithIntConst)(state, a, b, c1u);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_String_WithIntConst)(state, a, b, c1u);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_WithInternedString)
{
	if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Object_WithInternedString)(state, a, b, c);
	}
    else if(totemRegister_IsInstance(b))
    {
        totemDispatch_ComplexGet_Instance_WithInternedString(state, a, b, c);
    }
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet_WithMiniString)
{
	if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Object_WithMiniString)(state, a, b, c);
	}
    else if (totemRegister_IsInstance(b))
    {
        totemDispatch_ComplexGet_Instance_WithMiniString(state, a, b, c);
    }
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexGet)
{
	if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_String)(state, a, b, c);
	}
	else if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Array)(state, a, b, c);
	}
	else if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(ComplexGet_Object)(state, a, b, c);
	}
    else if (totemRegister_IsInstance(b))
    {
        totemDispatch_ComplexGet_Instance(state, a, b, c);
    }
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_Array_WithInt)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, a);
	uint32_t index = (uint32_t)totemRegister_GetInt(b);
	TOTEM_DISPATCH_BREAK(totemExecState_ArraySet(state, gc, index, c), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_Array_WithFloat)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, a);
	uint32_t index = (uint32_t)totemRegister_GetFloat(b);
	TOTEM_DISPATCH_BREAK(totemExecState_ArraySet(state, gc, index, c), state);
}

TOTEM_DISPATCH_FUNC_Abc1u(ComplexSet_Array_WithIntConst)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, a);
	TOTEM_DISPATCH_BREAK(totemExecState_ArraySet(state, gc, c1u, b), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_Array)
{
	if (totemRegister_IsInt(b))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Array_WithInt)(state, a, b, c);
	}
	else if (totemRegister_IsFloat(b))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Array_WithFloat)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_Object_WithInternedString)
{
	totemRuntimeStringValue key;
	totemInternedStringHeader *hdr = totemRegister_GetInternedString(b);
	totemRuntimeStringValue_FromInternedString(hdr, &key);
	totemGCObject* dst = totemDispatch_GetGCObject(state, a);
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectSet(state, dst, &key, c), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_Object_WithMiniString)
{
	totemRuntimeStringValue key;
	totemRuntimeStringValue_FromMiniStringRegister(b, &key);
	totemGCObject* dst = totemDispatch_GetGCObject(state, a);
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectSet(state, dst, &key, c), state);
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_Object)
{
	if (totemRegister_IsMiniString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Object_WithMiniString)(state, a, b, c);
	}
	else if (totemRegister_IsInternedString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Object_WithInternedString)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_WithInt)
{
	if (totemRegister_IsArray(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Array_WithInt)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc1u(ComplexSet_WithIntConst)
{
	if (totemRegister_IsArray(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Array_WithIntConst)(state, a, b, c1u);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_WithFloat)
{
	if (totemRegister_IsArray(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Array_WithFloat)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_WithInternedString)
{
	if (totemRegister_IsObject(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Object_WithInternedString)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet_WithMiniString)
{
	if (totemRegister_IsObject(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Object_WithMiniString)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc(ComplexSet)
{
	if (totemRegister_IsArray(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Array)(state, a, b, c);
	}
	else if (totemRegister_IsObject(a))
	{
		TOTEM_DISPATCH_NAME(ComplexSet_Object)(state, a, b, c);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab(ComplexUnset_Object_WithInternedString)
{
	totemRuntimeStringMapKey key;
	totemInternedStringHeader* hdr = totemRegister_GetInternedString(b);
	totemRuntimeStringValue_FromInternedString(hdr, &key.Key);
	totemGCObject* dst = totemDispatch_GetGCObject(state, a);
	totemRuntimeStringMap_Remove(dst->Ptr.Object, &key);
}

TOTEM_DISPATCH_FUNC_Ab(ComplexUnset_Object_WithMiniString)
{
	totemRuntimeStringMapKey key;
	totemRuntimeStringValue_FromMiniStringRegister(b, &key.Key);
	totemGCObject* dst = totemDispatch_GetGCObject(state, a);
	totemRuntimeStringMap_Remove(dst->Ptr.Object, &key);
}

TOTEM_DISPATCH_FUNC_Ab(ComplexUnset_Object)
{
	if (totemRegister_IsMiniString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexUnset_Object_WithMiniString)(state, a, b);
	}
	else if (totemRegister_IsInternedString(b))
	{
		TOTEM_DISPATCH_NAME(ComplexUnset_Object_WithInternedString)(state, a, b);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Abc_PURE(Is)
{
	totemPublicDataType typeC;

	if (totemRegister_IsTypeValue(c))
	{
		typeC = totemRegister_GetTypeValue(c);
	}
	else
	{
		totemPrivateDataType typeCp = totemRegister_GetType(c);
		typeC = totemPrivateDataType_ToPublic(typeCp);
	}

	totemDispatch_Is(typeC, b, a);
}

TOTEM_DISPATCH_FUNC_Abc1u_PURE(IsType)
{
	totemPublicDataType typeB = (totemPublicDataType)c1u;
	totemDispatch_Is(typeB, b, a);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_ToBool)
{
	totemRegister_SetBool(a, totemRegister_IsNotZero(b));
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_Float_ToInt)
{
	totemRegister_SetInt(a, (totemInt)totemRegister_GetFloat(b));
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_String_ToInt)
{
	// string to int
	totemRuntimeStringValue str;
	totemRegister_GetString(b, &str);
    const char *val = totemRuntimeStringValue_GetVal(&str);
	totemInt result = totem_strtoInt(val);
	totemRegister_SetInt(a, result);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_Array_ToInt)
{
	// array to int
	totemGCObject* arr = totemRegister_GetGCObject(b);
	totemRegister_SetInt(a, arr ? arr->NumRegisters : 0);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_Object_ToInt)
{
	// object to int
	totemGCObject* obj = totemRegister_GetGCObject(b);
	totemRegister_SetInt(a, obj ? obj->Ptr.Object->NumKeys : 0);
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_Int_ToFloat)
{
	totemRegister_SetFloat(a, totem_toFloat(totemRegister_GetInt(b)));
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_String_ToFloat)
{
	totemRuntimeStringValue str;
	totemRegister_GetString(b, &str);
    const char *val = totemRuntimeStringValue_GetVal(&str);
	totemFloat result = totem_strtoFloat(val, NULL);
	totemRegister_SetFloat(a, result);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Int_ToString)
{
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_IntToString(state, totemRegister_GetInt(b), &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Type_ToString)
{
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_TypeToString(state, totemRegister_GetInt(b), &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Array_ToString)
{
	totemGCObject* obj = totemDispatch_GetGCObject(state, b);
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_ArrayToString(state, obj->Data.Registers, obj->NumRegisters, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Object_ToString)
{
	totemGCObject* obj = totemDispatch_GetGCObject(state, b);
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectToString(state, obj, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Float_ToString)
{
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_FloatToString(state, totemRegister_GetFloat(b), &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Coroutine_ToString)
{
	totemGCObject* obj = totemDispatch_GetGCObject(state, b);
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_InstanceFunctionToString(state, obj->Ptr.Coroutine->Function.Instance, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Function_ToString)
{
	totemRuntimeStringValue val;

	if (totemRegister_IsNativeFunction(b))
	{
		TOTEM_DISPATCH_BREAK(totemExecState_NativeFunctionToString(state, totemRegister_GetNativeFunction(b), &val), state);
	}
	else
	{
		TOTEM_DISPATCH_BREAK(totemExecState_InstanceFunctionToString(state, totemRegister_GetInstanceFunction(b), &val), state);
	}

	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_ToString)
{
	totemRuntimeStringValue val;
	TOTEM_DISPATCH_BREAK(totemExecState_ToString(state, b, &val), state);
	totemRegister_SetString(a, &val);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_String_ToArray)
{
	totemGCObject* gc = NULL;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateArray(state, (uint32_t)totemRegister_GetStringLength(b), &gc), state);

	totemRuntimeStringValue val;
	totemRegister_GetString(b, &val);
    const char *valPtr = totemRuntimeStringValue_GetVal(&val);

	totemRegister* regs = gc->Data.Registers;
	for (uint32_t i = 0; i < gc->NumRegisters; i++)
	{
		totemRuntimeStringValue strVal;
		totemString newStr;
		newStr.Length = 1;
        newStr.Value = valPtr + i;

		TOTEM_DISPATCH_BREAK(totemExecState_InternString(state, &newStr, &strVal), state);
		totemRegister_SetString(regs + i, &strVal);
	}

	totemRegister_SetArray(a, gc);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Object_ToArray)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, b);
	TOTEM_DISPATCH_BREAK(totemExecState_CreateArrayFromExisting(state, gc->Data.Registers, gc->NumRegisters, &gc), state);
	totemRegister_SetArray(a, gc);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_String_ToFunction)
{
	totemRuntimeStringValue val;
	totemRegister_GetString(b, &val);
	TOTEM_DISPATCH_BREAK(totemExecState_StringToFunction(state, &val, a), state);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Coroutine_ToFunction)
{
	totemGCObject* cr = totemDispatch_GetGCObject(state, b);
	totemRegister_SetInstanceFunction(a, cr->Ptr.Coroutine->Function.Instance);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_ToFunction)
{
	if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(Cast_String_ToFunction)(state, a, b);
	}
	else if (totemRegister_IsCoroutine(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Coroutine_ToFunction)(state, a, b);
	}
	else if (totemRegister_IsInstanceFunction(b))
	{
		totemRegister_SetInstanceFunction(a, totemRegister_GetInstanceFunction(b));
	}
	else if (totemRegister_IsNativeFunction(b))
	{
		totemRegister_SetNativeFunction(a, totemRegister_GetNativeFunction(b));
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab(Cast_ToCoroutine)
{
	totemInstanceFunction* function = NULL;

	if (totemRegister_IsInstanceFunction(b))
	{
		function = totemRegister_GetInstanceFunction(b);
	}
	else if (totemRegister_IsString(b))
	{
		totemRuntimeStringValue val;
		totemRegister_GetString(b, &val);
		TOTEM_DISPATCH_BREAK(totemExecState_StringToFunction(state, &val, a), state);

		if (!totemRegister_IsInstanceFunction(a))
		{
			TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
		}

		function = totemRegister_GetInstanceFunction(a);
	}
	else if (totemRegister_IsCoroutine(b))
	{
		totemGCObject* cr = totemDispatch_GetGCObject(state, b);
		function = cr->Ptr.Coroutine->Function.Instance;
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}

	totemDispatch_CastFunctionToCoroutine(state, a, function);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Function_ToCoroutine)
{
	totemInstanceFunction* func = totemRegister_GetInstanceFunction(b);
	totemDispatch_CastFunctionToCoroutine(state, a, func);
}

TOTEM_DISPATCH_FUNC_Ab(Cast_Array_ToObject)
{
	totemGCObject* srcGc = totemDispatch_GetGCObject(state, b);

	totemGCObject* gc = NULL;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateObject(state, srcGc->NumRegisters, &gc), state);
	totemRegister_SetObject(a, gc);
	
	for (uint32_t i = 0; i < srcGc->NumRegisters; i++)
	{
		totemRuntimeStringValue key;
		TOTEM_DISPATCH_BREAK(totemExecState_IntToString(state, (totemInt)i, &key), state);
		TOTEM_DISPATCH_BREAK(totemExecState_ObjectSet(state, gc, &key, srcGc->Data.Registers + i), state);
	}
}

TOTEM_DISPATCH_FUNC_Ab(Cast_String_ToObject)
{
	totemRuntimeStringValue str;
	totemRegister_GetString(b, &str);

	totemGCObject* gc = NULL;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateObject(state, (uint32_t)str.Length, &gc), state);
	totemRegister_SetObject(a, gc);

	for (totemStringLength i = 0; i < str.Length; i++)
	{
        const char *strPtr = totemRuntimeStringValue_GetVal(&str);
		totemString cha;
		cha.Length = 1;
		cha.Value = strPtr + i;

		totemRuntimeStringValue key;
		totemRuntimeStringValue value;
		totemRegister valueR;
		TOTEM_DISPATCH_BREAK(totemExecState_IntToString(state, (totemInt)i, &key), state);
		TOTEM_DISPATCH_BREAK(totemExecState_InternString(state, &cha, &value), state);
		totemRegister_SetString(&valueR, &value);
		TOTEM_DISPATCH_BREAK(totemExecState_ObjectSet(state, gc, &key, &valueR), state);
	}
}

TOTEM_DISPATCH_FUNC_Ab(Cast_ToArray)
{
	if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Object_ToArray)(state, a, b);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(Cast_String_ToArray)(state, a, b);
	}
    else if (totemRegister_IsArray(b))
    {
        totemGCObject *newArr = NULL, *src = totemRegister_GetGCObject(b);
        TOTEM_DISPATCH_BREAK(totemExecState_CloneArray(state, src, &newArr), state);
        totemRegister_SetArray(a, newArr);
    }
	else
	{
        totemGCObject *newArr = NULL;
        TOTEM_DISPATCH_BREAK(totemExecState_CreateArray(state, 1, &newArr), state);
        totemRegister_Set(&newArr->Data.Registers[0], b);
        totemRegister_SetArray(a, newArr);
	}
}

TOTEM_DISPATCH_FUNC_Ab(Cast_ToObject)
{
	if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Array_ToObject)(state, a, b);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(Cast_String_ToObject)(state, a, b);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_ToType)
{
	totemRegister_SetTypeValue(a, totemPrivateDataType_ToPublic(totemRegister_GetType(b)));
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_ToInt)
{
	if (totemRegister_IsBoolean(b))
	{
		totemRegister_SetInt(a, totemRegister_IsNotZero(b));
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(Cast_String_ToInt)(a, b);
	}
	else if (totemRegister_IsInt(b))
	{
		totemRegister_Set(a, b);
	}
	else if (totemRegister_IsFloat(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Float_ToInt)(a, b);
	}
	else if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Array_ToInt)(a, b);
	}
	else if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Object_ToInt)(a, b);
	}
	else
	{
		totemRegister_SetInt(a, 0);
	}
}

TOTEM_DISPATCH_FUNC_Ab_PURE(Cast_ToFloat)
{
	if (totemRegister_IsBoolean(b))
	{
		totemRegister_SetFloat(a, totemRegister_IsNotZero(b));
	}
	else if (totemRegister_IsInt(b))
	{
		TOTEM_DISPATCH_NAME(Cast_Int_ToFloat)(a, b);
	}
	else if (totemRegister_IsFloat(b))
	{
		totemRegister_Set(a, b);
	}
	else if (totemRegister_IsString(b))
	{
		TOTEM_DISPATCH_NAME(Cast_String_ToFloat)(a, b);
	}
	else if (totemRegister_IsArray(b))
	{
		TOTEM_DISPATCH_NAME(LengthOf_Array)(a, b);
	}
	else if (totemRegister_IsObject(b))
	{
		TOTEM_DISPATCH_NAME(LengthOf_Object)(a, b);
	}
	else
	{
		totemRegister_SetFloat(a, 0);
	}
}

TOTEM_DISPATCH_FUNC_Abc(Cast)
{
	totemPublicDataType toType;

	if (totemRegister_IsTypeValue(c))
	{
		toType = totemRegister_GetTypeValue(c);
	}
	else
	{
		toType = totemPrivateDataType_ToPublic(totemRegister_GetType(c));
	}

	switch (toType)
	{
	case totemPublicDataType_Null:
		totemRegister_SetNull(a);
		break;

	case totemPublicDataType_Int:
		TOTEM_DISPATCH_NAME(Cast_ToInt)(a, b);
		break;

	case totemPublicDataType_Float:
		TOTEM_DISPATCH_NAME(Cast_ToFloat)(a, b);
		break;

	case totemPublicDataType_String:
		TOTEM_DISPATCH_NAME(Cast_ToString)(state, a, b);
		break;

	case totemPublicDataType_Array:
		TOTEM_DISPATCH_NAME(Cast_ToArray)(state, a, b);
		break;

	case totemPublicDataType_Type:
		totemRegister_SetTypeValue(a, toType);
		break;

	case totemPublicDataType_Function:
		TOTEM_DISPATCH_NAME(Cast_ToFunction)(state, a, b);
		break;

	case totemPublicDataType_Coroutine:
		TOTEM_DISPATCH_NAME(Cast_ToCoroutine)(state, a, b);
		break;

	case totemPublicDataType_Boolean:
		TOTEM_DISPATCH_NAME(Cast_ToBool)(a, b);
		break;

	default:
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_Ab2u(PreInvoke_Native)
{
	totemNativeFunction* func = totemRegister_GetNativeFunction(a);
	totemDispatch_PreInvokeNativeFunc(state, func, b2u);
}

TOTEM_DISPATCH_FUNC_Ab2u(PreInvoke_Instance)
{
	totemInstanceFunction* func = totemRegister_GetInstanceFunction(a);
	totemDispatch_PreInvokeInstanceFunc(state, func, b2u);
}

TOTEM_DISPATCH_FUNC_Ab2u(PreInvoke_Coroutine)
{
	totemGCObject* gc = totemDispatch_GetGCObject(state, a);

	TOTEM_DISPATCH_ASSERT(b2u <= gc->Ptr.Coroutine->NumRegisters, state, totemExecStatus_RegisterOverflow);

	totemFunctionCall* call = gc->Ptr.Coroutine;
	totemInstruction* startAt = call->ResumeAt;

	if (!startAt)
	{
		startAt = call->Function.Instance->Function->InstructionsStart;
	}

	totemDispatch_PushRoutine(state, call, call->Instance->Data.Registers, startAt);
}

TOTEM_DISPATCH_FUNC_Ab2u(PreInvoke)
{
	if (totemRegister_IsNativeFunction(a))
	{
		TOTEM_DISPATCH_NAME(PreInvoke_Native)(state, a, b2u);
	}
	else if (totemRegister_IsInstanceFunction(a))
	{
		TOTEM_DISPATCH_NAME(PreInvoke_Instance)(state, a, b2u);
	}
	else if (totemRegister_IsCoroutine(a))
	{
		TOTEM_DISPATCH_NAME(PreInvoke_Coroutine)(state, a, b2u);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC_A(Invoke)
{
	totemDispatch_Invoke(state, a);
	totemDispatch_FinalizeRoutine(state);
}

TOTEM_DISPATCH_FUNC_A(Invoke_Coroutine)
{
	totemDispatch_Invoke(state, a);
	totemDispatch_FinalizeCoroutine(state);
}

TOTEM_DISPATCH_FUNC_A(Invoke_Subroutine)
{
	totemDispatch_Invoke(state, a);
	totemDispatch_FinalizeSubroutine(state);
}

TOTEM_DISPATCH_FUNC_Abc(PreInvoke_2Args)
{
	TOTEM_DISPATCH_NAME(PreInvoke)(state, a, 2);
	totemDispatch_PushFunctionArg(state, b);
	totemDispatch_PushFunctionArg(state, c);
}

TOTEM_DISPATCH_FUNC_Abc(PreInvoke_2Args_Native)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Native)(state, a, 2);
	totemDispatch_PushFunctionArg(state, b);
	totemDispatch_PushFunctionArg(state, c);
}

TOTEM_DISPATCH_FUNC_Abc(PreInvoke_2Args_Instance)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Instance)(state, a, 2);
	totemDispatch_PushFunctionArg(state, b);
	totemDispatch_PushFunctionArg(state, c);
}

TOTEM_DISPATCH_FUNC_Abc(PreInvoke_2Args_Coroutine)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Coroutine)(state, a, 2);
	totemDispatch_PushFunctionArg(state, b);
	totemDispatch_PushFunctionArg(state, c);
}

TOTEM_DISPATCH_FUNC_Ab(FunctionArg_Invoke)
{
	totemDispatch_PushFunctionArg(state, a);
	TOTEM_DISPATCH_NAME(Invoke)(state, b);
}

TOTEM_DISPATCH_FUNC_Abc(FunctionArg_2_Invoke)
{
	totemDispatch_PushFunctionArg(state, a);
	totemDispatch_PushFunctionArg(state, b);
	TOTEM_DISPATCH_NAME(Invoke)(state, c);
}

TOTEM_DISPATCH_FUNC_Abc(FunctionArg_3)
{
	totemDispatch_PushFunctionArg(state, a);
	totemDispatch_PushFunctionArg(state, b);
	totemDispatch_PushFunctionArg(state, c);
}

TOTEM_DISPATCH_FUNC_Ab(FullInvoke_NoArgs)
{
	TOTEM_DISPATCH_NAME(PreInvoke)(state, b, 0);
	TOTEM_DISPATCH_NAME(Invoke)(state, a);
}

TOTEM_DISPATCH_FUNC_Ab(FullInvoke_NoArgs_Native)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Native)(state, b, 0);
	TOTEM_DISPATCH_NAME(Invoke_Subroutine)(state, a);
}

TOTEM_DISPATCH_FUNC_Ab(FullInvoke_NoArgs_Instance)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Instance)(state, b, 0);
	TOTEM_DISPATCH_NAME(Invoke_Subroutine)(state, a);
}

TOTEM_DISPATCH_FUNC_Ab(FullInvoke_NoArgs_Coroutine)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Coroutine)(state, b, 0);
	TOTEM_DISPATCH_NAME(Invoke_Coroutine)(state, a);
}

TOTEM_DISPATCH_FUNC_Abc(FullInvoke_1Arg)
{
	TOTEM_DISPATCH_NAME(PreInvoke)(state, b, 1);
	totemDispatch_PushFunctionArg(state, c);
	TOTEM_DISPATCH_NAME(Invoke)(state, a);
}

TOTEM_DISPATCH_FUNC_Abc(FullInvoke_1Arg_Native)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Native)(state, b, 1);
	totemDispatch_PushFunctionArg(state, c);
	TOTEM_DISPATCH_NAME(Invoke_Subroutine)(state, a);
}

TOTEM_DISPATCH_FUNC_Abc(FullInvoke_1Arg_Instance)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Instance)(state, b, 0);
	totemDispatch_PushFunctionArg(state, c);
	TOTEM_DISPATCH_NAME(Invoke_Subroutine)(state, a);
}

TOTEM_DISPATCH_FUNC_Abc(FullInvoke_1Arg_Coroutine)
{
	TOTEM_DISPATCH_NAME(PreInvoke_Coroutine)(state, b, 1);
	totemDispatch_PushFunctionArg(state, c);
	TOTEM_DISPATCH_NAME(Invoke_Coroutine)(state, a);
}

#endif
