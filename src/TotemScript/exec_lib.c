//
//  exec_lib.c
//  TotemScript
//
//  Created by Timothy Smale on 19/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <TotemScript/dispatch.h>
#include <string.h>
#include <math.h>

void totemArgV(totemExecState *state)
{
	if (!state->CallStack->NumRegisters)
    {
        printf("no arguments argv\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemRegister *arg = &state->LocalRegisters[0];
    
    if (!totemRegister_IsInt(arg))
    {
        printf("expected int argv\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemInt val = totemRegister_GetInt(arg);
    
    if (!state->ArgV || val >= state->ArgC)
    {
        totemExecState_AssignNull(state, state->CallStack->ReturnRegister);
    }
    else
    {
		totemString str;
		totemString_FromLiteral(&str, state->ArgV[val]);
        TOTEM_DISPATCH_BREAK(totemExecState_InternString(state, &str, state->CallStack->ReturnRegister), state);
    }
}

void totemPrint(totemExecState *state)
{
	for (uint32_t i = 0; i < state->CallStack->NumRegisters; i++)
    {
        totemRegister *reg = &state->LocalRegisters[i];
        totemExecState_PrintRegister(state, stdout, reg);
    }
}

void totemAssert(totemExecState *state)
{
    if (!state->CallStack->NumRegisters)
    {
        printf("no arguments assert\n");
        TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemRegister *reg = &state->LocalRegisters[0];
    if (totemRegister_IsZero(reg))
    {
        printf("assertion failed\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
}

void totemSqrt(totemExecState *state)
{
	if (!state->CallStack->NumRegisters)
    {
        printf("no arguments sqrt\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemRegister *reg = &state->LocalRegisters[0];
    
    if (totemRegister_IsFloat(reg))
    {
        totemExecState_AssignNewFloat(state, state->CallStack->ReturnRegister, sqrt(totemRegister_GetFloat(reg)));
    }
    else if (totemRegister_IsInt(reg))
    {
        totemExecState_AssignNewFloat(state, state->CallStack->ReturnRegister, sqrt((totemFloat)totemRegister_GetInt(reg)));
    }
    else
    {
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
    }
}

void totemFileDestructor(totemExecState *state, void *data)
{
    fclose((FILE*)data);
}

void totemFOpen(totemExecState *state)
{
	if (state->CallStack->NumRegisters < 2)
    {
        printf("no arguments fopen\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemRegister *srcReg = &state->LocalRegisters[0];
    totemRegister *modeReg = &state->LocalRegisters[1];
    
    if (!totemRegister_IsString(srcReg) || !totemRegister_IsString(modeReg))
    {
        printf("not string\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_UnexpectedDataType);
    }
    
    totemRuntimeStringValue srcVal;
    totemRuntimeStringValue modeVal;
    totemRegister_GetStringValue(srcReg, &srcVal);
    totemRegister_GetStringValue(modeReg, &modeVal);
    
    const char *src = srcVal.Value;
    const char *mode = modeVal.Value;
    
    FILE *f = totem_fopen(src, mode);
    if (!f)
    {
        printf("could not open file\n");
		TOTEM_DISPATCH_ERROR(state, totemExecStatus_Stop);
    }
    
    totemGCObject *gc = NULL;
    totemExecStatus status = totemExecState_CreateUserdata(state, (void*)f, totemFileDestructor, &gc);
    if (status != totemExecStatus_Continue)
    {
        fclose(f);
		TOTEM_DISPATCH_ERROR(state, status);
    }
    
    totemExecState_AssignNewUserdata(state, state->CallStack->ReturnRegister, gc);
}

void totemGCCollect(totemExecState *state)
{
	totemExecState_CollectGarbage(state, state->CallStack->NumRegisters ? !totemRegister_IsZero(&state->LocalRegisters[0]) : totemBool_False);
}

void totemGCNum(totemExecState *state)
{
    totemExecState_AssignNewInt(state, state->CallStack->ReturnRegister, (totemInt)state->GCNum);
}

totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime)
{
    totemNativeFunctionPrototype funcs[] =
    {
		{ totemPrint, { "print", strlen("print") } },
		{ totemAssert, { "assert", strlen("assert") } },
		{ totemFOpen, { "fopen", strlen("fopen") } },
		{ totemGCCollect, { "gc_collect", strlen("gc_collect") } },
		{ totemGCNum, { "gc_num", strlen("gc_num") } },
		{ totemSqrt, { "sqrt", strlen("sqrt") } },
		{ totemArgV, { "argv", strlen("argv") } }
    };
    
    return totemRuntime_LinkNativeFunctions(runtime, funcs, TOTEM_ARRAY_SIZE(funcs));
}