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
    
    if (arg->DataType != totemPrivateDataType_Int)
    {
        printf("expected int argv\n");
        return totemExecStatus_Break(totemExecStatus_Stop);
    }
    
    if (!state->ArgV || arg->Value.Int >= state->ArgC)
    {
        totemExecState_AssignNull(state, state->CallStack->ReturnRegister);
        return totemExecStatus_Continue;
    }
    else
    {
        totemString str = TOTEM_STRING_VAL(state->ArgV[arg->Value.Int]);
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
    if (reg->Value.Data)
    {
        return totemExecStatus_Continue;
    }
    else
    {
        printf("assertion failed\n");
        return totemExecStatus_Stop;
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
    switch (reg->DataType)
    {
        case totemPrivateDataType_Int:
            totemExecState_AssignNewFloat(state, state->CallStack->ReturnRegister, sqrt((totemFloat)reg->Value.Int));
            break;
            
        case totemPrivateDataType_Float:
            totemExecState_AssignNewFloat(state, state->CallStack->ReturnRegister, sqrt(reg->Value.Float));
            break;
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
    
    const char *src = totemRegister_GetStringValue(srcReg);
    const char *mode = totemRegister_GetStringValue(modeReg);
    
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
    totemExecState_CollectGarbage(state, state->CallStack->NumArguments ? state->LocalRegisters[0].Value.Data != 0 : totemBool_False);
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