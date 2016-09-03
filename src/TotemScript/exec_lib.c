//
//  exec_lib.c
//  TotemScript
//
//  Created by Timothy Smale on 19/07/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <string.h>
#include <math.h>

totemExecStatus totemArgV(totemExecState *state)
{
    if (!state->CallStack->NumArguments)
    {
        printf("no arguments argv\n");
        return totemExecStatus_Break(totemExecStatus_Stop);
    }
    
    totemRegister *arg = &state->LocalRegisters[0];
    
    if (!totemRegister_IsInt(arg))
    {
        printf("expected int argv\n");
        return totemExecStatus_Break(totemExecStatus_Stop);
    }
    
    totemInt val = totemRegister_GetInt(arg);
    
    if (!state->ArgV || val >= state->ArgC)
    {
        totemExecState_AssignNull(state, state->CallStack->ReturnRegister);
        return totemExecStatus_Continue;
    }
    else
    {
        totemString str = TOTEM_STRING_VAL(state->ArgV[val]);
        return totemExecState_InternString(state, &str, state->CallStack->ReturnRegister);
    }
}

totemExecStatus totemPrint(totemExecState *state)
{
    for (uint8_t i = 0; i < state->CallStack->NumArguments; i++)
    {
        totemRegister *reg = &state->LocalRegisters[i];
        totemExecState_PrintRegister(state, stdout, reg);
    }
    
    return totemExecStatus_Continue;
}

totemExecStatus totemAssert(totemExecState *state)
{
    if (!state->CallStack->NumArguments)
    {
        printf("no arguments assert\n");
        return totemExecStatus_Break(totemExecStatus_Stop);
    }
    
    totemRegister *reg = &state->LocalRegisters[0];
    if (totemRegister_IsZero(reg))
    {
        printf("assertion failed\n");
        return totemExecStatus_Stop;
    }
    else
    {
        return totemExecStatus_Continue;
    }
}

totemExecStatus totemSqrt(totemExecState *state)
{
    if (!state->CallStack->NumArguments)
    {
        printf("no arguments sqrt\n");
        return totemExecStatus_Break(totemExecStatus_Stop);
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
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
    }
    
    return totemExecStatus_Continue;
}

void totemFileDestructor(totemExecState *state, void *data)
{
    fclose((FILE*)data);
}

totemExecStatus totemFOpen(totemExecState *state)
{
    if (state->CallStack->NumArguments < 2)
    {
        printf("no arguments fopen\n");
        return totemExecStatus_Break(totemExecStatus_Stop);
    }
    
    totemRegister *srcReg = &state->LocalRegisters[0];
    totemRegister *modeReg = &state->LocalRegisters[1];
    
    if (!totemRegister_IsString(srcReg) || !totemRegister_IsString(modeReg))
    {
        printf("not string\n");
        return totemExecStatus_Break(totemExecStatus_UnexpectedDataType);
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
        return totemExecStatus_Break(totemExecStatus_Stop);
    }
    
    totemGCObject *gc = NULL;
    totemExecStatus status = totemExecState_CreateUserdata(state, (void*)f, totemFileDestructor, &gc);
    if (status != totemExecStatus_Continue)
    {
        fclose(f);
        return status;
    }
    
    totemExecState_AssignNewUserdata(state, state->CallStack->ReturnRegister, gc);
    return status;
}

totemExecStatus totemGCCollect(totemExecState *state)
{
    totemExecState_CollectGarbage(state, state->CallStack->NumArguments ? !totemRegister_IsZero(&state->LocalRegisters[0]) : totemBool_False);
    return totemExecStatus_Continue;
}

totemExecStatus totemGCNum(totemExecState *state)
{
    totemExecState_AssignNewInt(state, state->CallStack->ReturnRegister, state->GCNum);
    return totemExecStatus_Continue;
}

totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime)
{
    totemNativeFunctionPrototype funcs[] =
    {
        { totemPrint, TOTEM_STRING_VAL("print") },
        { totemAssert, TOTEM_STRING_VAL("assert") },
        { totemFOpen, TOTEM_STRING_VAL("fopen") },
        { totemGCCollect, TOTEM_STRING_VAL("gc_collect") },
        { totemGCNum, TOTEM_STRING_VAL("gc_num") },
        { totemSqrt, TOTEM_STRING_VAL("sqrt") },
        { totemArgV, TOTEM_STRING_VAL("argv") }
    };
    
    return totemRuntime_LinkNativeFunctions(runtime, funcs, TOTEM_ARRAY_SIZE(funcs));
}