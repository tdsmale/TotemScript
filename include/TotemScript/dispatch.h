//
//  dispatch.h
//  TotemScript
//
//  Created by Timothy Smale on 24/09/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef TotemScript_dispatch_h
#define TotemScript_dispatch_h

#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <math.h>

typedef union totemDispatchArgument
{
	totemRegister *Reg;
	totemOperandXUnsigned XUnsigned;
	totemOperandXSigned XSigned;
}
totemDispatchArgument;

TOTEM_STATIC_ASSERT(sizeof(totemDispatchArgument) == sizeof(void*), "");

TOTEM_DEBUG_INLINE totemDispatchArgument totemDispatchArgument_FromOperandXUnsigned(totemOperandXUnsigned x)
{
	totemDispatchArgument a;
	a.XUnsigned = x;
	return a;
}

TOTEM_DEBUG_INLINE totemDispatchArgument totemDispatchArgument_FromOperandXSigned(totemOperandXSigned x)
{
	totemDispatchArgument a;
	a.XUnsigned = x;
	return a;
}

TOTEM_DEBUG_INLINE totemDispatchArgument totemDispatchArgument_FromRegister(totemRegister *reg)
{
	totemDispatchArgument a;
	a.Reg = reg;
	return a;
}

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

#define TOTEM_DISPATCH_NAME(op) totemOperationType_ ## op ## _Dispatch

#ifdef TOTEM_X86
#define TOTEM_DISPATCH_FUNC_SIGNATURE TOTEM_DEBUG_INLINE TOTEM_CDECL
#else
#define TOTEM_DISPATCH_FUNC_SIGNATURE TOTEM_DEBUG_INLINE 
#endif

#define TOTEM_DISPATCH_FUNC(op) \
	TOTEM_STATIC_ASSERT(totemOperationType_ ## op < TOTEM_MAX_OPTYPES, "given op type must exist"); \
	totemOperandXSigned TOTEM_DISPATCH_FUNC_SIGNATURE TOTEM_DISPATCH_NAME(op) (totemExecState *state, totemDispatchArgument a, totemDispatchArgument b, totemDispatchArgument c)

#define TOTEM_DISPATCH_FUNCS_ARITH_INT(type, op) \
	TOTEM_DISPATCH_FUNC(type ## _IntInt) \
	{ \
		totemInt _a = totemRegister_GetInt(b.Reg); \
		totemInt _b = totemRegister_GetInt(c.Reg); \
		totemInt _c = _a op _b; \
		totemExecState_AssignNewInt(state, a.Reg, _c); \
	} \
	\
	TOTEM_DISPATCH_FUNC(type ## _IntIntConst) \
	{ \
		totemInt _a = totemRegister_GetInt(b.Reg); \
		totemInt _b = c.XSigned; \
		totemInt _c = _a op _b; \
		totemExecState_AssignNewInt(state, a.Reg, _c); \
	} \
	\
	TOTEM_DISPATCH_FUNC(type ## _IntConstIntConst) \
	{ \
		totemInt _a = b.XSigned; \
		totemInt _b = c.XSigned; \
		totemInt _c = _a op _b; \
		totemExecState_AssignNewInt(state, a.Reg, _c); \
	} \

#define TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(type, op) \
	TOTEM_DISPATCH_FUNC(type ## _FloatFloat) \
	{ \
		totemFloat _a = totemRegister_GetFloat(b.Reg); \
		totemFloat _b = totemRegister_GetFloat(c.Reg); \
		totemFloat _c = _a op _b; \
		totemExecState_AssignNewFloat(state, a.Reg, _c); \
	} \

#define TOTEM_DISPATCH_COMMON_ARITH_INT(state, dst, src1, src2, _int, _float) \
	if (totemRegister_IsInt(src2.Reg)) \
	{ \
		totemInt _a = totemRegister_GetInt(src1.Reg); \
		totemInt _b = totemRegister_GetInt(src2.Reg); \
		totemInt _c = _int(_a, _b); \
		totemExecState_AssignNewInt(state, dst.Reg, _c); \
	} \
	else if (totemRegister_IsFloat(src2.Reg)) \
	{ \
		totemFloat _a = (totemFloat)totemRegister_GetInt(src1.Reg); \
		totemFloat _b = totemRegister_GetFloat(src2.Reg); \
		totemFloat _c = _float(_a, _b); \
		totemExecState_AssignNewFloat(state, dst.Reg, _c); \
    } \
	else \
    { \
        TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
    }

#define TOTEM_DISPATCH_COMMON_ARITH_FLOAT(state, dst, src1, src2, _int, _float) \
	if (totemRegister_IsFloat(src2.Reg)) \
	{ \
		totemFloat _a = totemRegister_GetFloat(src1.Reg); \
		totemFloat _b = totemRegister_GetFloat(src2.Reg); \
		totemFloat _c = _float(_a, _b); \
		totemExecState_AssignNewFloat(state, dst.Reg, _c); \
	} \
	else if (totemRegister_IsInt(src2.Reg)) \
	{ \
		totemFloat _a = totemRegister_GetFloat(src1.Reg); \
		totemFloat _b = (totemFloat)totemRegister_GetInt(src2.Reg); \
		totemFloat _c = _float(_a, _b); \
		totemExecState_AssignNewFloat(state, dst.Reg, _c); \
	} \
	else \
	{ \
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType); \
	}

#define TOTEM_DISPATCH_COMMON_ARITH(state, dst, src1, src2, _int, _float) \
	if (totemRegister_IsInt(src1.Reg)) \
	{ \
		TOTEM_DISPATCH_COMMON_ARITH_INT(state, dst, src1, src2, _int, _float) \
	} \
	else if (totemRegister_IsFloat(src1.Reg)) \
	{ \
		TOTEM_DISPATCH_COMMON_ARITH_FLOAT(state, dst, src1, src2, _int, _float) \
	} \
    else

#define TOTEM_DISPATCH_COMMON_BITWISE(state, dst, src1, src2, op) \
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(src1), state, totemExecStatus_UnexpectedDataType); \
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(src2), state, totemExecStatus_UnexpectedDataType); \
	totemInt _a = totemRegister_GetInt(src1); \
	totemInt _b = totemRegister_GetInt(src2); \
	totemInt _c = _a op _b; \
	totemExecState_AssignNewInt(state, dst, _c);

TOTEM_DEBUG_INLINE totemFunctionCall *totemExecState_CreateSubroutine(
	totemExecState *state, totemInt numRegisters, totemGCObject *instance, totemRegister *returnReg, totemRuntimeFunctionCb cb, void *function)
{
	totemFunctionCall *call = totemExecState_SecureFunctionCall(state);
	TOTEM_DISPATCH_ASSERT(call, state, totemExecStatus_OutOfMemory);

	if (state->NextFreeRegister + numRegisters > state->LastFreeRegister)
	{
		call->FrameStart = totemExecState_Alloc(state, sizeof(totemRegister) * numRegisters);
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

TOTEM_DEBUG_INLINE void totemExecState_PushRoutine(totemExecState *state, totemFunctionCall *call, totemRegister *globals, totemInstruction *startAt)
{
	call->PreviousFrameStart = state->LocalRegisters;
	call->PreviousGlobals = state->GlobalRegisters;
	call->ResumeAt = startAt;
	call->Prev = state->CallStack;
	state->CallStack = call;
	state->LocalRegisters = call->FrameStart;
	state->GlobalRegisters = globals;
}

TOTEM_DEBUG_INLINE void totemExecState_PopRoutine(totemExecState *state)
{
	totemExecState_CollectGarbage(state, totemBool_False);

	totemFunctionCall *call = state->CallStack;
	
	state->LocalRegisters = call->PreviousFrameStart;
	state->GlobalRegisters = call->PreviousGlobals;
	state->CallStack = call->Prev;
}

TOTEM_DEBUG_INLINE void totemExecState_FinalizeSubroutine(totemExecState *state)
{
	totemFunctionCall *call = state->CallStack;
	totemExecState_CleanupRegisterList(state, call->FrameStart, call->NumRegisters);

	if (TOTEM_HASBITS(call->Flags, totemFunctionCallFlag_FreeStack))
	{
		totem_CacheFree(call->FrameStart, sizeof(totemRegister) * call->NumRegisters);
	}
	else
	{
		state->NextFreeRegister -= call->NumRegisters;
	}

	totemExecState_PopRoutine(state);
	totemExecState_FreeFunctionCall(state, call);
}

TOTEM_DEBUG_INLINE void totemExecState_FinalizeCoroutine(totemExecState *state)
{
	totemExecState_PopRoutine(state);
}

TOTEM_DEBUG_INLINE void totemExecState_FinalizeRoutine(totemExecState *state)
{
	if (TOTEM_HASBITS(state->CallStack->Flags, totemFunctionCallFlag_IsCoroutine))
	{
		totemExecState_FinalizeCoroutine(state);
	}
	else
	{
		totemExecState_FinalizeSubroutine(state);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_CastBoolToInt(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewInt(state, dst, totemRegister_IsNotZero(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CastStringToInt(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	// string to int
	totemRuntimeStringValue str;
	totemRegister_GetStringValue(src, &str);
	totemInt val = totem_strtoInt(str.Value, NULL, 10);
	totemExecState_AssignNewInt(state, dst, val);
}

TOTEM_DEBUG_INLINE void totemExecState_CastFloatToBoolean(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewBoolean(state, dst, totemRegister_IsZero(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CastFloatToInt(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewInt(state, dst, (totemInt)totemRegister_GetFloat(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CastArrayToInt(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	// array to int
	totemGCObject *arr = totemRegister_GetGCObject(src);
	totemExecState_AssignNewInt(state, dst, arr->NumRegisters);
}

TOTEM_DEBUG_INLINE void totemExecState_CastObjectToInt(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	// object to int
	totemGCObject *obj = totemRegister_GetGCObject(src);
	totemExecState_AssignNewInt(state, dst, obj->Ptr.Object->NumKeys);
}

TOTEM_DEBUG_INLINE void totemExecState_CastToInt(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	if (totemRegister_IsBoolean(src))
	{
		totemExecState_CastBoolToInt(state, dst, src);
	}
	else if (totemRegister_IsString(src))
	{
		totemExecState_CastStringToInt(state, dst, src);
	}
	else if (totemRegister_IsInt(src))
	{
		totemExecState_Assign(state, dst, src);
	}
	else if (totemRegister_IsFloat(src))
	{
		totemExecState_CastFloatToInt(state, dst, src);
	}
	else if (totemRegister_IsArray(src))
	{
		totemExecState_CastArrayToInt(state, dst, src);
	}
	else if (totemRegister_IsObject(src))
	{
		totemExecState_CastObjectToInt(state, dst, src);
	}
	else
	{
		totemExecState_AssignNewInt(state, dst, 0);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_CastBoolToFloat(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewFloat(state, dst, (totemFloat)totemRegister_IsNotZero(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CastIntToFloat(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewFloat(state, dst, (totemFloat)totemRegister_GetInt(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CastStringToFloat(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	// string to float
	totemRuntimeStringValue str;
	totemRegister_GetStringValue(src, &str);
	totemFloat val = totem_strtoFloat(str.Value, NULL);
	totemExecState_AssignNewFloat(state, dst, val);
}

TOTEM_DEBUG_INLINE void totemExecState_CastArrayToFloat(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	// array to float
	totemGCObject *arr = totemRegister_GetGCObject(src);
	totemExecState_AssignNewFloat(state, dst, (totemFloat)arr->NumRegisters);
}

TOTEM_DEBUG_INLINE void totemExecState_CastObjectToFloat(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	// object to float
	totemGCObject *obj = totemRegister_GetGCObject(src);
	totemExecState_AssignNewFloat(state, dst, (totemFloat)(obj->Ptr.Object->NumKeys));
}

TOTEM_DEBUG_INLINE void totemExecState_CastToFloat(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	if (totemRegister_IsBoolean(src))
	{
		totemExecState_CastFloatToBoolean(state, dst, src);
	}
	else if (totemRegister_IsInt(src))
	{
		totemExecState_CastFloatToInt(state, dst, src);
	}
	else if (totemRegister_IsFloat(src))
	{
		// float to float
		totemExecState_AssignNewFloat(state, dst, totemRegister_GetFloat(src));
	}
	else if (totemRegister_IsString(src))
	{
		totemExecState_CastStringToFloat(state, dst, src);
	}
	else if (totemRegister_IsArray(src))
	{
		totemExecState_CastArrayToFloat(state, dst, src);
	}
	else if (totemRegister_IsObject(src))
	{
		totemExecState_CastObjectToFloat(state, dst, src);
	}
	else
	{
		totemExecState_AssignNewFloat(state, dst, 0);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_CastToString(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	TOTEM_DISPATCH_BREAK(totemExecState_ToString(state, src, dst), state);
}

TOTEM_DEBUG_INLINE void totemExecState_CastToArray(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	if (totemRegister_IsArray(src))
	{
		totemGCObject *gc = NULL;
		totemGCObject *arr = totemRegister_GetGCObject(src);
		TOTEM_DISPATCH_BREAK(totemExecState_CreateArrayFromExisting(state, arr->Data.Registers, arr->NumRegisters, &gc), state);
		totemExecState_AssignNewArray(state, dst, gc);
	}
	else if (totemRegister_IsString(src))
	{
		totemGCObject *gc = NULL;
		TOTEM_DISPATCH_BREAK(totemExecState_CreateArray(state, (uint32_t)totemRegister_GetStringLength(src), &gc), state);

		totemRuntimeStringValue val;
		totemRegister_GetStringValue(src, &val);

		totemRegister *regs = gc->Data.Registers;
		for (totemInt i = 0; i < gc->NumRegisters; i++)
		{
			totemRuntimeStringValue strVal;
			totemString newStr;
			newStr.Length = 1;
			newStr.Value = &val.Value[i];

			if (totemRuntime_InternString(state->Runtime, &newStr, &strVal) != totemLinkStatus_Success)
			{
				TOTEM_DISPATCH_ERROR(state, totemExecStatus_OutOfMemory);
			}

			totemExecState_AssignNewString(state, regs + i, &strVal);
		}

		totemExecState_AssignNewArray(state, dst, gc);
	}
	else
	{
		totemGCObject *gc = NULL;
		TOTEM_DISPATCH_BREAK(totemExecState_CreateArrayFromExisting(state, src, 1, &gc), state);
		totemExecState_AssignNewArray(state, dst, gc);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_CastToTypeValue(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewType(state, dst, totemPrivateDataType_ToPublic(totemRegister_GetType(src)));
}

TOTEM_DEBUG_INLINE void totemExecState_CastToFunction(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	if (totemRegister_IsString(src))
	{
		TOTEM_DISPATCH_BREAK(totemExecState_StringToFunction(state, src, dst), state);
	}
	else if (totemRegister_IsCoroutine(src))
	{
		totemGCObject *cr = totemRegister_GetGCObject(src);
		totemExecState_AssignNewInstanceFunction(state, dst, cr->Ptr.Coroutine->Function.Instance);
	}
	else if (totemRegister_IsInstanceFunction(src))
	{
		totemExecState_AssignNewInstanceFunction(state, dst, totemRegister_GetInstanceFunction(src));
	}
	else if (totemRegister_IsNativeFunction(src))
	{
		totemExecState_AssignNewNativeFunction(state, dst, totemRegister_GetNativeFunction(src));
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_CastToCoroutine(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemInstanceFunction *function = NULL;
	
	if (totemRegister_IsInstanceFunction(src))
	{
		function = totemRegister_GetInstanceFunction(src);
	}
	else if (totemRegister_IsString(src))
	{
		TOTEM_DISPATCH_BREAK(totemExecState_StringToFunction(state, src, dst), state);

		if (!totemRegister_IsInstanceFunction(dst))
		{
			TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
		}

		function = totemRegister_GetInstanceFunction(dst);
	}
	else if (totemRegister_IsCoroutine(src))
	{
		totemGCObject *cr = totemRegister_GetGCObject(src);
		function = cr->Ptr.Coroutine->Function.Instance;
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}

	totemGCObject *obj = NULL;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateCoroutine(state, function, &obj), state);
	totemExecState_AssignNewCoroutine(state, dst, obj);
}

TOTEM_DEBUG_INLINE void totemExecState_CastToBoolean(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemExecState_AssignNewBoolean(state, dst, totemRegister_IsNotZero(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CastToNull(totemExecState *state, totemRegister *dst)
{
	totemExecState_AssignNull(state, dst);
}

TOTEM_DEBUG_INLINE void totemExecState_Cast(totemExecState *state, totemRegister *dst, totemRegister *src, totemRegister *typeReg)
{
	totemPublicDataType toType;

	if (totemRegister_IsTypeValue(typeReg))
	{
		toType = totemRegister_GetTypeValue(typeReg);
	}
	else
	{
		toType = totemPrivateDataType_ToPublic(totemRegister_GetType(typeReg));
	}

	switch (toType)
	{
		case totemPublicDataType_Null:
			totemExecState_CastToNull(state, dst);
			break;

		case totemPublicDataType_Int:
			totemExecState_CastToInt(state, dst, src);
			break;

		case totemPublicDataType_Float:
			totemExecState_CastToFloat(state, dst, src);
			break;

		case totemPublicDataType_String:
			totemExecState_CastToString(state, dst, src);
			break;

		case totemPublicDataType_Array:
			totemExecState_CastToArray(state, dst, src);
			break;

		case totemPublicDataType_Type:
			totemExecState_CastToTypeValue(state, dst, src);
			break;

		case totemPublicDataType_Function:
			totemExecState_CastToFunction(state, dst, src);
			break;

		case totemPublicDataType_Coroutine:
			totemExecState_CastToCoroutine(state, dst, src);
			break;

		case totemPublicDataType_Boolean:
			totemExecState_CastToBoolean(state, dst, src);
			break;

		default:
			TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_PreInvokeNative(totemExecState *state, totemRegister *src, totemOperandXUnsigned numArgs)
{
	totemNativeFunction *func = totemRegister_GetNativeFunction(src);

	totemFunctionCall *call = totemExecState_CreateSubroutine(
		state,
		numArgs,
		state->CallStack->Instance,
		NULL,
		func->Callback,
		func);

	totemExecState_PushRoutine(state, call, NULL, NULL);
}

TOTEM_DEBUG_INLINE void totemExecState_PreInvokeInstance(totemExecState *state, totemRegister *src, totemOperandXUnsigned numArgs)
{
	totemInstanceFunction *func = totemRegister_GetInstanceFunction(src);

	totemFunctionCall *call = totemExecState_CreateSubroutine(
		state,
		numArgs <= func->Function->RegistersNeeded ? func->Function->RegistersNeeded : numArgs,
		func->Instance,
		NULL,
		func->Function->Invoke,
		func);

	totemExecState_PushRoutine(state, call, call->Instance->Data.Registers, func->Function->InstructionsStart);
}

TOTEM_DEBUG_INLINE void totemExecState_PreInvokeCoroutine(totemExecState *state, totemRegister *src, totemOperandXUnsigned numArgs)
{	
	totemGCObject *gc = totemRegister_GetGCObject(src);

	TOTEM_DISPATCH_ASSERT(numArgs <= gc->Ptr.Coroutine->NumRegisters, state, totemExecStatus_RegisterOverflow);

	totemFunctionCall *call = gc->Ptr.Coroutine;
	totemInstruction *startAt = call->ResumeAt;

	if (!startAt)
	{
		totemExecState_CleanupRegisterList(state, call->FrameStart, call->NumRegisters);
		startAt = call->Function.Instance->Function->InstructionsStart;
	}
	
	totemExecState_PushRoutine(state, call, call->Instance->Data.Registers, startAt);
}

TOTEM_DEBUG_INLINE void totemExecState_PushFunctionArg(totemExecState *state, totemRegister *arg)
{
	totemExecState_Assign(state, &state->CallStack->FrameStart[state->CallStack->NumArguments++], arg);
}

TOTEM_DEBUG_INLINE void totemExecState_PreInvoke(totemExecState *state, totemRegister *src, totemOperandXUnsigned numArgs)
{
	if (totemRegister_IsNativeFunction(src))
	{
		totemExecState_PreInvokeNative(state, src, numArgs);
	}
	else if (totemRegister_IsInstanceFunction(src))
	{
		totemExecState_PreInvokeInstance(state, src, numArgs);
	}
	else if (totemRegister_IsCoroutine(src))
	{
		totemExecState_PreInvokeCoroutine(state, src, numArgs);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DEBUG_INLINE void totemExecState_Invoke(totemExecState *state, totemRegister *ret)
{
	state->CallStack->ReturnRegister = ret;
	state->CallStack->Invoke(state);
	totemExecState_FinalizeRoutine(state);
}

TOTEM_DEBUG_INLINE void totemExecState_CloneGCRegister(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemGCObject *obj;
	TOTEM_DISPATCH_BREAK(totemExecState_CloneGCObject(state, totemRegister_GetGCObject(src), &obj), state);
	totemExecState_AssignNewGCObject(state, dst, obj, totemRegister_GetType(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CloneArrayRegister(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemGCObject *obj;
	TOTEM_DISPATCH_BREAK(totemExecState_CloneArray(state, totemRegister_GetGCObject(src), &obj), state);
	totemExecState_AssignNewGCObject(state, dst, obj, totemRegister_GetType(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CloneObjectRegister(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemGCObject *obj;
	TOTEM_DISPATCH_BREAK(totemExecState_CloneObject(state, totemRegister_GetGCObject(src), &obj), state);
	totemExecState_AssignNewGCObject(state, dst, obj, totemRegister_GetType(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CloneUserdataRegister(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemGCObject *obj;
	TOTEM_DISPATCH_BREAK(totemExecState_CloneUserdata(state, totemRegister_GetGCObject(src), &obj), state);
	totemExecState_AssignNewGCObject(state, dst, obj, totemRegister_GetType(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CloneCoroutineRegister(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemGCObject *obj;
	TOTEM_DISPATCH_BREAK(totemExecState_CloneCoroutine(state, totemRegister_GetGCObject(src), &obj), state);
	totemExecState_AssignNewGCObject(state, dst, obj, totemRegister_GetType(src));
}

TOTEM_DEBUG_INLINE void totemExecState_CloneInstanceRegister(totemExecState *state, totemRegister *dst, totemRegister *src)
{
	totemGCObject *obj;
	TOTEM_DISPATCH_BREAK(totemExecState_CloneInstance(state, totemRegister_GetGCObject(src), &obj), state);
	totemExecState_AssignNewGCObject(state, dst, obj, totemRegister_GetType(src));
}

/*
 * DISPATCH FUNCS BEGIN
 */

TOTEM_SUPPRESS_NO_RETURN_WARNING

TOTEM_DISPATCH_FUNC(Move)
{
	totemExecState_Assign(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(MoveNull)
{
	totemExecState_AssignNull(state, a.Reg);
}

TOTEM_DISPATCH_FUNC(MoveInt)
{
	totemExecState_AssignNewInt(state, a.Reg, (totemInt)b.XSigned);
}

TOTEM_DISPATCH_FUNC(MoveTrue)
{
	totemExecState_AssignNewBoolean(state, a.Reg, totemBool_True);
}

TOTEM_DISPATCH_FUNC(MoveFalse)
{
	totemExecState_AssignNewBoolean(state, a.Reg, totemBool_False);
}

TOTEM_DISPATCH_FUNC(MoveTypeNull)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Null);
}

TOTEM_DISPATCH_FUNC(MoveTypeInt)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Int);
}

TOTEM_DISPATCH_FUNC(MoveTypeFloat)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Float);
}

TOTEM_DISPATCH_FUNC(MoveTypeString)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_String);
}

TOTEM_DISPATCH_FUNC(MoveTypeArray)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Array);
}

TOTEM_DISPATCH_FUNC(MoveTypeType)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Type);
}

TOTEM_DISPATCH_FUNC(MoveTypeFunction)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Function);
}

TOTEM_DISPATCH_FUNC(MoveTypeCoroutine)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Coroutine);
}

TOTEM_DISPATCH_FUNC(MoveTypeObject)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Object);
}

TOTEM_DISPATCH_FUNC(MoveTypeUserdata)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Userdata);
}

TOTEM_DISPATCH_FUNC(MoveTypeBoolean)
{
	totemExecState_AssignNewType(state, a.Reg, totemPublicDataType_Boolean);
}

TOTEM_DISPATCH_FUNC(MoveToLocal)
{
	totemExecState_Assign(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(MoveToGlobal)
{
	totemExecState_Assign(state, b.Reg, a.Reg);
#if !TOTEM_VMOPT_GLOBAL_OPERANDS
	totemExecState_WriteBarrier(state, state->CallStack->Instance);
#endif
}

TOTEM_DISPATCH_FUNC(Add)
{
#define _OP(a, b) a + b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		if (totemRegister_IsArray(b.Reg) && totemRegister_IsArray(c.Reg))
		{
			totemExecState_ConcatArrays(state, b.Reg, c.Reg, a.Reg);
		}
		else if (totemRegister_IsString(c.Reg) && totemRegister_IsString(b.Reg))
		{
			totemExecState_ConcatStrings(state, b.Reg, c.Reg, a.Reg);
		}
		else
		{
			TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
		}
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Add, +);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Add, +);

TOTEM_DISPATCH_FUNC(AddArrayArray)
{
	totemExecState_ConcatArrays(state, b.Reg, c.Reg, a.Reg);
}

TOTEM_DISPATCH_FUNC(AddStringString)
{
	totemExecState_ConcatStrings(state, b.Reg, c.Reg, a.Reg);
}

TOTEM_DISPATCH_FUNC(Subtract)
{
#define _OP(a, b) a - b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Subtract, -);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Subtract, -);

TOTEM_DISPATCH_FUNC(Multiply)
{
#define _OP(a, b) a * b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Multiply, *);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Multiply, *);

TOTEM_DISPATCH_FUNC(Divide)
{
	TOTEM_DISPATCH_ASSERT(totemRegister_IsNotZero(c.Reg), state, totemExecStatus_DivideByZero);
#define _OP(a, b) a / b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Divide, /);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(Divide, /);

TOTEM_DISPATCH_FUNC(Modulus)
{
	TOTEM_DISPATCH_ASSERT(totemRegister_IsNotZero(c.Reg), state, totemExecStatus_DivideByZero);
#define _INTOP(a, b) a % b
#define _FLOATOP(a, b) fmod(a, b)
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _INTOP, _FLOATOP)
#undef _INTOP
#undef _FLOATOP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(Modulus, % );

TOTEM_DISPATCH_FUNC(Modulus_FloatFloat)
{
	totemFloat _a = totemRegister_GetFloat(b.Reg);
	totemFloat _b = totemRegister_GetFloat(c.Reg);
	totemFloat _c = (totemFloat)fmod((double)_a, (double)_b);
	totemExecState_AssignNewFloat(state, a.Reg, _c);
}

TOTEM_DISPATCH_FUNC(BinaryLeftShift)
{
	TOTEM_DISPATCH_COMMON_BITWISE(state, a.Reg, b.Reg, c.Reg, << );
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryLeftShift, << );

TOTEM_DISPATCH_FUNC(BinaryRightShift)
{
	TOTEM_DISPATCH_COMMON_BITWISE(state, a.Reg, b.Reg, c.Reg, >> );
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryRightShift, >> );

TOTEM_DISPATCH_FUNC(BinaryAnd)
{
	TOTEM_DISPATCH_COMMON_BITWISE(state, a.Reg, b.Reg, c.Reg, & );
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryAnd, & );

TOTEM_DISPATCH_FUNC(BinaryOr)
{
	TOTEM_DISPATCH_COMMON_BITWISE(state, a.Reg, b.Reg, c.Reg, | );
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryOr, |);

TOTEM_DISPATCH_FUNC(BinaryXor)
{
	TOTEM_DISPATCH_COMMON_BITWISE(state, a.Reg, b.Reg, c.Reg, ^ );
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(BinaryXor, ^ );

TOTEM_DISPATCH_FUNC(BinaryNot)
{
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(b.Reg), state, totemExecStatus_UnexpectedDataType);
	totemInt _a = totemRegister_GetInt(b.Reg);
	totemInt _c = ~_a;
	totemExecState_AssignNewInt(state, a.Reg, _c);
}

TOTEM_DISPATCH_FUNC(BinaryNot_Int)
{
	totemInt _a = totemRegister_GetInt(b.Reg);
	totemInt _c = ~_a;
	totemExecState_AssignNewInt(state, a.Reg, _c);
}

TOTEM_DISPATCH_FUNC(BinaryNot_IntConst)
{
	totemInt _a = b.XSigned;
	totemInt _c = ~_a;
	totemExecState_AssignNewInt(state, a.Reg, _c);
}

TOTEM_DISPATCH_FUNC(LessThan)
{
#define _OP(a, b) a < b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(LessThan, <);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(LessThan, <);

TOTEM_DISPATCH_FUNC(LessThanEquals)
{
#define _OP(a, b) a <= b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(LessThanEquals, <=);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(LessThanEquals, <=);

TOTEM_DISPATCH_FUNC(MoreThan)
{
#define _OP(a, b) a > b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(MoreThan, > );
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(MoreThan, > );

TOTEM_DISPATCH_FUNC(MoreThanEquals)
{
#define _OP(a, b) a >= b
	TOTEM_DISPATCH_COMMON_ARITH(state, a, b, c, _OP, _OP)
#undef _OP
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNCS_ARITH_INT(MoreThanEquals, >=);
TOTEM_DISPATCH_FUNCS_ARITH_FLOAT(MoreThanEquals, >=);

TOTEM_DISPATCH_FUNC(Equals)
{
	totemExecState_AssignNewBoolean(state, a.Reg, totemRegister_Equals(b.Reg, c.Reg));
}

TOTEM_DISPATCH_FUNC(NotEquals)
{
	totemExecState_AssignNewBoolean(state, a.Reg, !totemRegister_Equals(b.Reg, c.Reg));
}

TOTEM_DISPATCH_FUNC(LogicalOr)
{
	totemExecState_AssignNewBoolean(state, a.Reg, totemRegister_IsNotZero(b.Reg) || totemRegister_IsNotZero(c.Reg));
}

TOTEM_DISPATCH_FUNC(LogicalAnd)
{
	totemExecState_AssignNewBoolean(state, a.Reg, totemRegister_IsNotZero(b.Reg) && totemRegister_IsNotZero(c.Reg));
}

TOTEM_DISPATCH_FUNC(LogicalNegate)
{
	totemExecState_AssignNewBoolean(state, a.Reg, totemRegister_IsZero(b.Reg));
}

TOTEM_DISPATCH_FUNC(Negate)
{
	if (totemRegister_IsInt(b.Reg))
	{
		totemExecState_AssignNewInt(state, a.Reg, -totemRegister_GetInt(b.Reg));
	}
	else if (totemRegister_IsFloat(b.Reg))
	{
		totemExecState_AssignNewFloat(state, a.Reg, -totemRegister_GetFloat(b.Reg));
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC(Negate_Int)
{
	totemExecState_AssignNewInt(state, a.Reg, -b.XSigned);
}

TOTEM_DISPATCH_FUNC(Negate_IntConst)
{
	totemExecState_AssignNewInt(state, a.Reg, -b.XSigned);
}

TOTEM_DISPATCH_FUNC(Negate_Float)
{
	totemExecState_AssignNewFloat(state, a.Reg, -totemRegister_GetFloat(b.Reg));
}

TOTEM_DISPATCH_FUNC(ConditionalGoto)
{
	return totemRegister_IsZero(a.Reg) ? b.XSigned : 1;
}

TOTEM_DISPATCH_FUNC(Goto)
{
	return a.XSigned;
}

TOTEM_DISPATCH_FUNC(Return)
{
	totemFunctionCall *call = state->CallStack;
	call->ResumeAt = NULL;
	totemExecState_Assign(state, call->ReturnRegister, a.Reg);
}

TOTEM_DISPATCH_FUNC(Yield)
{
	totemFunctionCall *call = state->CallStack;
	call->ResumeAt = call->Function.Instance->Function->InstructionsStart + b.XUnsigned;
	totemExecState_Assign(state, call->ReturnRegister, a.Reg);
}

TOTEM_DISPATCH_FUNC(NewArray)
{
	totemGCObject *gc;
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(b.Reg), state, totemExecStatus_UnexpectedDataType);
	TOTEM_DISPATCH_BREAK(totemExecState_CreateArray(state, totemRegister_GetInt(b.Reg), &gc), state);
	totemExecState_AssignNewArray(state, a.Reg, gc);
}

TOTEM_DISPATCH_FUNC(NewArrayWithLength)
{
	totemGCObject *gc;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateArray(state, b.XUnsigned, &gc), state);
	totemExecState_AssignNewArray(state, a.Reg, gc);
}

TOTEM_DISPATCH_FUNC(NewObject)
{
	totemGCObject *gc;
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(b.Reg), state, totemExecStatus_UnexpectedDataType);
	TOTEM_DISPATCH_BREAK(totemExecState_CreateObject(state, totemRegister_GetInt(b.Reg), &gc), state);
	totemExecState_AssignNewObject(state, a.Reg, gc);
}

TOTEM_DISPATCH_FUNC(NewObjectWithLength)
{
	totemGCObject *gc;
	TOTEM_DISPATCH_BREAK(totemExecState_CreateObject(state, b.XUnsigned, &gc), state);
	totemExecState_AssignNewObject(state, a.Reg, gc);
}

TOTEM_DISPATCH_FUNC(LengthOf)
{
	if (totemRegister_IsArray(b.Reg))
	{
		totemExecState_AssignNewInt(state, a.Reg, totemRegister_GetGCObject(b.Reg)->NumRegisters);
	}
	else if (totemRegister_IsObject(b.Reg))
	{
		totemExecState_AssignNewInt(state, a.Reg, totemRegister_GetGCObject(b.Reg)->Ptr.Object->NumKeys);
	}
	else if (totemRegister_IsString(b.Reg))
	{
		totemExecState_AssignNewInt(state, a.Reg, (totemInt)totemRegister_GetStringLength(b.Reg));
	}
	else
	{
		totemExecState_AssignNewInt(state, a.Reg, 1);
	}
}

TOTEM_DISPATCH_FUNC(LengthOfArray)
{
	TOTEM_DEBUG_ASSERT(totemRegister_IsArray(b.Reg));
	totemExecState_AssignNewInt(state, a.Reg, totemRegister_GetGCObject(b.Reg)->NumRegisters);
}

TOTEM_DISPATCH_FUNC(LengthOfObject)
{
	TOTEM_DEBUG_ASSERT(totemRegister_IsObject(b.Reg));
	totemExecState_AssignNewInt(state, a.Reg, totemRegister_GetGCObject(b.Reg)->Ptr.Object->NumKeys);
}

TOTEM_DISPATCH_FUNC(LengthOfString)
{
	TOTEM_DEBUG_ASSERT(totemRegister_IsString(b.Reg));
	totemExecState_AssignNewInt(state, a.Reg, (totemInt)totemRegister_GetStringLength(b.Reg));
}

TOTEM_DISPATCH_FUNC(CloneOf)
{
	if (totemRegister_IsGarbageCollected(b.Reg))
	{
		totemExecState_CloneGCRegister(state, a.Reg, b.Reg);
	}
	else
	{
		totemExecState_Assign(state, a.Reg, b.Reg);
	}
}

TOTEM_DISPATCH_FUNC(CloneOfGCObject)
{
	totemExecState_CloneGCRegister(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(CloneOfArray)
{
	totemExecState_CloneArrayRegister(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(CloneOfObject)
{
	totemExecState_CloneObjectRegister(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(CloneOfUserdata)
{
	totemExecState_CloneUserdataRegister(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(CloneOfCoroutine)
{
	totemExecState_CloneCoroutineRegister(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(CloneOfInstance)
{
	totemExecState_CloneInstanceRegister(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(ComplexGet)
{
	if (totemRegister_IsString(b.Reg))
	{
		TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(c.Reg), state, totemExecStatus_InvalidKey);
		TOTEM_DISPATCH_BREAK(totemExecState_InternStringChar(state, b.Reg, totemRegister_GetInt(c.Reg), a.Reg), state);
	}
	else if (totemRegister_IsArray(b.Reg))
	{
		totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
		TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(c.Reg), state, totemExecStatus_InvalidKey);
		TOTEM_DISPATCH_BREAK(totemExecState_ArrayGet(state, gc->Data.Registers, gc->NumRegisters, totemRegister_GetInt(c.Reg), a.Reg), state);
	}
	else if (totemRegister_IsObject(b.Reg))
	{
		totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
		TOTEM_DISPATCH_BREAK(totemExecState_ObjectGet(state, gc, c.Reg, a.Reg), state);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}
}

TOTEM_DISPATCH_FUNC(ComplexGetArray)
{
	totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(c.Reg), state, totemExecStatus_InvalidKey);
	TOTEM_DISPATCH_BREAK(totemExecState_ArrayGet(state, gc->Data.Registers, gc->NumRegisters, totemRegister_GetInt(c.Reg), a.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexGetArrayAtIndex)
{
	totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
	totemOperandXUnsigned c1u = c.XUnsigned;
	TOTEM_DISPATCH_BREAK(totemExecState_ArrayGet(state, gc->Data.Registers, gc->NumRegisters, c1u, a.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexGetObject)
{
	totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(c.Reg), state, totemExecStatus_InvalidKey);
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectGet(state, gc, c.Reg, a.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexGetString)
{
	totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(c.Reg), state, totemExecStatus_InvalidKey);
	TOTEM_DISPATCH_BREAK(totemExecState_InternStringChar(state, b.Reg, totemRegister_GetInt(c.Reg), a.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexGetStringAtIndex)
{
	totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
	totemOperandXUnsigned c1u = c.XUnsigned;
	TOTEM_DISPATCH_BREAK(totemExecState_InternStringChar(state, b.Reg, c1u, a.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexSet)
{
	totemGCObject *gc;

	if (totemRegister_IsArray(a.Reg))
	{
		gc = totemRegister_GetGCObject(a.Reg);
		TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(b.Reg), state, totemExecStatus_InvalidKey);
		TOTEM_DISPATCH_BREAK(totemExecState_ArraySet(state, gc->Data.Registers, gc->NumRegisters, totemRegister_GetInt(b.Reg), c.Reg), state);
	}
	else if (totemRegister_IsObject(a.Reg))
	{
		gc = totemRegister_GetGCObject(a.Reg);
		TOTEM_DISPATCH_BREAK(totemExecState_ObjectSet(state, gc, b.Reg, c.Reg), state);
	}
	else
	{
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
	}

	totemExecState_WriteBarrier(state, gc);
}

TOTEM_DISPATCH_FUNC(ComplexSetArray)
{
	totemGCObject *gc = totemRegister_GetGCObject(a.Reg);
	TOTEM_DISPATCH_ASSERT(totemRegister_IsInt(b.Reg), state, totemExecStatus_InvalidKey);
	TOTEM_DISPATCH_BREAK(totemExecState_ArraySet(state, gc->Data.Registers, gc->NumRegisters, totemRegister_GetInt(b.Reg), c.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexSetArrayAtIndex)
{
	totemGCObject *gc = totemRegister_GetGCObject(b.Reg);
	totemOperandXUnsigned c1u = c.XUnsigned;
TOTEM_DISPATCH_BREAK(totemExecState_ArraySet(state, gc->Data.Registers, gc->NumRegisters, totemRegister_GetInt(b.Reg), c.Reg), state);
}

TOTEM_DISPATCH_FUNC(ComplexSetObject)
{
	totemGCObject *gc = totemRegister_GetGCObject(a.Reg);
	TOTEM_DISPATCH_BREAK(totemExecState_ObjectSet(state, gc, b.Reg, c.Reg), state);
}

TOTEM_DISPATCH_FUNC(Is)
{
	totemPrivateDataType bType = totemRegister_GetType(b.Reg);

	if (totemRegister_IsTypeValue(c.Reg))
	{
		totemExecState_AssignNewBoolean(state, a.Reg, totemPrivateDataType_ToPublic(bType) == totemRegister_GetTypeValue(c.Reg));
	}
	else
	{
		totemExecState_AssignNewBoolean(state, a.Reg, totemPrivateDataType_ToPublic(bType) == totemPrivateDataType_ToPublic(totemRegister_GetType(c.Reg)));
	}
}

TOTEM_DISPATCH_FUNC(IsType)
{
	totemPublicDataType type = (totemPublicDataType)c.XUnsigned;
	totemExecState_AssignNewBoolean(state, a.Reg, totemPrivateDataType_ToPublic(totemRegister_GetType(b.Reg)) == type);
}

TOTEM_DISPATCH_FUNC(Cast)
{
	totemExecState_Cast(state, a.Reg, b.Reg, c.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToInt)
{
	totemExecState_CastToInt(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_BoolToInt)
{
	totemExecState_CastBoolToInt(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_FloatToInt)
{
	totemExecState_CastFloatToInt(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_StringToInt)
{
	totemExecState_CastStringToInt(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ArrayToInt)
{
	totemExecState_CastArrayToInt(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ObjectToInt)
{
	totemExecState_CastObjectToInt(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToFloat)
{
	totemExecState_CastToFloat(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_BoolToFloat)
{
	totemExecState_CastBoolToFloat(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_IntToFloat)
{
	totemExecState_CastIntToFloat(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_StringToFloat)
{
	totemExecState_CastStringToFloat(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ArrayToFloat)
{
	totemExecState_CastArrayToFloat(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ObjectToFloat)
{
	totemExecState_CastObjectToFloat(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToString)
{
	totemExecState_CastToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_IntToString)
{
	totemExecState_CastIntToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_TypeToString)
{
	totemExecState_CastTypeToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ArrayToString)
{
	totemExecState_CastArrayToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ObjectToString)
{
	totemExecState_CastObjectToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_FloatToString)
{
	totemExecState_CastFloatToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_CoroutineToString)
{
	totemExecState_CastCoroutineToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_FunctionToString)
{
	totemExecState_CastFunctionToString(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToArray)
{
	totemExecState_CastToArray(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_StringToArray)
{
	totemExecState_CastStringToArray(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ObjectToArray)
{
	totemExecState_CastObjectToArray(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToFunction)
{
	totemExecState_CastToFunction(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_StringToFunction)
{
	totemExecState_CastStringToFunction(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_CoroutineToFunction)
{
	totemExecState_CastCoroutineToFunction(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToCoroutine)
{
	totemExecState_CastToCoroutine(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_FunctionToCoroutine)
{
	totemExecState_CastFunctionToCoroutine(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToBoolean)
{
	totemExecState_CastToBoolean(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToObject)
{
	totemExecState_CastToObject(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ArrayToObject)
{
	totemExecState_CastArrayToObject(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_StringToObject)
{
	totemExecState_CastStringToObject(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(Cast_ToType)
{
	totemExecState_CastToTypeValue(state, a.Reg, b.Reg);
}

TOTEM_DISPATCH_FUNC(PreInvoke)
{
	totemExecState_PreInvoke(state, a.Reg, b.XUnsigned);
}

TOTEM_DISPATCH_FUNC(PreInvoke_Native)
{
	totemExecState_PreInvokeNative(state, a.Reg, b.XUnsigned);
}

TOTEM_DISPATCH_FUNC(PreInvoke_Instance)
{
	totemExecState_PreInvokeInstance(state, a.Reg, b.XUnsigned);
}

TOTEM_DISPATCH_FUNC(PreInvoke_Coroutine)
{
	totemExecState_PreInvokeCoroutine(state, a.Reg, b.XUnsigned);
}

TOTEM_DISPATCH_FUNC(Invoke)
{
	totemExecState_Invoke(state, a.Reg);
}

TOTEM_DISPATCH_FUNC(PreInvoke_2Args)
{
	totemExecState_PreInvoke(state, a.Reg, 2);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_PushFunctionArg(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(PreInvoke_2Args_Native)
{
	totemExecState_PreInvokeNative(state, a.Reg, 2);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_PushFunctionArg(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(PreInvoke_2Args_Instance)
{
	totemExecState_PreInvokeInstance(state, a.Reg, 2);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_PushFunctionArg(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(PreInvoke_2Args_Coroutine)
{
	totemExecState_PreInvokeCoroutine(state, a.Reg, 2);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_PushFunctionArg(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(FunctionArg_Invoke)
{
	totemExecState_PushFunctionArg(state, a.Reg);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FunctionArg_2_Invoke)
{
	totemExecState_PushFunctionArg(state, a.Reg);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_Invoke(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(FunctionArg_3)
{
	totemExecState_PushFunctionArg(state, a.Reg);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_PushFunctionArg(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_NoArgs)
{
	totemExecState_PreInvoke(state, a.Reg, 0);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_NoArgs_Native)
{
	totemExecState_PreInvokeNative(state, a.Reg, 0);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_NoArgs_Instance)
{
	totemExecState_PreInvokeInstance(state, a.Reg, 0);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_NoArgs_Coroutine)
{
	totemExecState_PreInvokeInstance(state, a.Reg, 0);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_1Arg)
{
	totemExecState_PreInvoke(state, a.Reg, 1);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_Invoke(state, c.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_1Arg_Native)
{
	totemExecState_PreInvokeNative(state, a.Reg, 1);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_1Arg_Instance)
{
	totemExecState_PreInvokeInstance(state, a.Reg, 1);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_DISPATCH_FUNC(FullInvoke_1Arg_Coroutine)
{
	totemExecState_PreInvokeInstance(state, a.Reg, 1);
	totemExecState_PushFunctionArg(state, b.Reg);
	totemExecState_Invoke(state, b.Reg);
}

TOTEM_SUPPRESS_LIFT

/*
* DISPATCH FUNCS END
*/

#endif