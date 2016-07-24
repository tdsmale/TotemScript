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
    if (state->ArgV)
    {
        totemExecState_AssignNewArray(state, state->CallStack->ReturnRegister, state->ArgV);
    }
    else
    {
        totemExecState_AssignNull(state, state->CallStack->ReturnRegister);
    }
    
    return totemExecStatus_Continue;
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
        return totemExecStatus_Stop;
    }
    
    totemRegister *reg = &state->LocalRegisters[0];
    return reg->Value.Data ? totemExecStatus_Continue : totemExecStatus_Stop;
}

totemExecStatus totemSqrt(totemExecState *state)
{
    if (!state->CallStack->NumArguments)
    {
        return totemExecStatus_Stop;
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

void totemFileDestructor(totemExecState *state, totemUserdata *data)
{
    FILE *f = (FILE*)data->Data;
    fclose(f);
}

totemExecStatus totemFOpen(totemExecState *state)
{
    if (state->CallStack->NumArguments < 2)
    {
        return totemExecStatus_Stop;
    }
    
    totemRegister *srcReg = &state->LocalRegisters[0];
    totemRegister *modeReg = &state->LocalRegisters[1];
    
    const char *src = totemRegister_GetStringValue(srcReg);
    const char *mode = totemRegister_GetStringValue(modeReg);
    
    FILE *f = totem_fopen(src, mode);
    if (!f)
    {
        return totemExecStatus_Stop;
    }
    
    totemGCObject *gc = NULL;
    totemExecStatus status = totemExecState_CreateUserdata(state, (uint64_t)f, totemFileDestructor, &gc);
    if (status != totemExecStatus_Continue)
    {
        fclose(f);
        return status;
    }
    
    totemExecState_AssignNewUserdata(state, state->CallStack->ReturnRegister, gc);
    return status;
}

totemExecStatus totemGCTest(totemExecState *state)
{
    totemExecState_CollectGarbage(state, totemBool_True);
    return totemExecStatus_Continue;
}

totemLinkStatus totemRuntime_LinkStdLib(totemRuntime *runtime)
{
    totemNativeFunctionPrototype funcs[] =
    {
        { totemPrint, TOTEM_STRING_VAL("print") },
        { totemAssert, TOTEM_STRING_VAL("assert") },
        { totemFOpen, TOTEM_STRING_VAL("fopen") },
        { totemGCTest, TOTEM_STRING_VAL("collectGC") },
        { totemSqrt, TOTEM_STRING_VAL("sqrt") },
        { totemArgV, TOTEM_STRING_VAL("argv") }
    };
    
    return totemRuntime_LinkNativeFunctions(runtime, funcs, TOTEM_ARRAYSIZE(funcs));
}