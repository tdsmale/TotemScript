//
//  eval_value.c
//  TotemScript
//
//  Created by Timothy Smale on 31/10/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>

totemEvalStatus totemBuildPrototype_EvalBool(totemBuildPrototype* build, totemBool val, totemOperandPrototype* op, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddBool(&build->GlobalRegisters, val, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalNull(totemBuildPrototype* build, totemOperandPrototype* op, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNull(&build->GlobalRegisters, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemNumberPrototype *number, totemOperandPrototype *operand, totemOperandPrototype *hint)
{
    if(number->IsFloat)
    {
        return totemBuildPrototype_EvalFloatString(build, &number->Value, operand, hint);
    }
    else
    {
        return totemBuildPrototype_EvalIntString(build, &number->Value, operand, hint);
    }
}

totemEvalStatus totemBuildPrototype_EvalIntString(totemBuildPrototype* build, totemParseString* number, totemOperandPrototype* operand, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddIntString(&build->GlobalRegisters, number, operand);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalIntConstant(totemBuildPrototype* build, totemInt number, totemOperandPrototype* operand, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddIntConstant(&build->GlobalRegisters, number, operand);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalFloatString(totemBuildPrototype* build, totemParseString* number, totemOperandPrototype* operand, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFloatString(&build->GlobalRegisters, number, operand);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalFloatConstant(totemBuildPrototype* build, totemFloat number, totemOperandPrototype* operand, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFloatConstant(&build->GlobalRegisters, number, operand);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype* build, totemParseString* buffer, totemOperandPrototype* operand, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddStringConstant(&build->GlobalRegisters, buffer, operand);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype* build, totemParseString* val, totemFunctionPointerPrototype* value, totemOperandPrototype* op, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFunctionPointer(&build->GlobalRegisters, val, value, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }

    return status;
}

totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype* build, totemPublicDataType type, totemOperandPrototype* operand, totemOperandPrototype* hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddType(&build->GlobalRegisters, type, operand);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }

    return status;
}
