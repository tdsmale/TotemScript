//
//  eval_arith.c
//  TotemScript
//
//  Created by Timothy Smale on 31/10/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/exec.h>
#include <TotemScript/opcodes.h>
#include <TotemScript/dispatch.h>

// todo - make more data-driven in opcodes.h / instructions.h 
// e.g. opcodes & instruction types should be able to encode parameter types etc.

typedef struct totemEvalArithOperandPrototype
{
    totemPublicDataType Type;
    totemRegisterPrototypeValue Val;
    totemRegisterPrototypeFlag Flags;
}
totemEvalArithOperandPrototype;

static void totemEvalArithOperandPrototype_FromOperandPrototype(totemRegisterListPrototype *scope, totemOperandPrototype *from, totemEvalArithOperandPrototype *to)
{
    totemRegisterListPrototype_GetRegisterType(scope, from->RegisterIndex, &to->Type);
    totemRegisterListPrototype_GetRegisterFlags(scope, from->RegisterIndex, &to->Flags);
    totemRegisterListPrototype_GetRegisterValue(scope, from->RegisterIndex, &to->Val);
}

static totemBool totemOperandPrototype_ToRegister(totemEvalArithOperandPrototype *in, totemRegister* out)
{
    if(TOTEM_HASBITS(in->Flags, totemRegisterPrototypeFlag_IsVariable))
    {
        return totemBool_False;
    }
    
    TOTEM_ZERO(*out);
    
    switch (in->Type)
    {
        case totemPublicDataType_Int:
            totemRegister_SetInt(out, in->Val.Int);
            break;

        case totemPublicDataType_Float:
            totemRegister_SetFloat(out, in->Val.Float);
            break;

        default:
            return totemBool_False;
    }
    
    return totemBool_True;
}

static totemBool totemOperandPrototype_ToC1u(totemEvalArithOperandPrototype *in, totemOperand1u *out)
{
    if(TOTEM_HASBITS(in->Flags, totemRegisterPrototypeFlag_IsVariable))
    {
        return totemBool_False;
    }
    
    TOTEM_ZERO(*out);
    
    switch (in->Type)
    {
        case totemPublicDataType_Int:
            if(in->Val.Int > TOTEM_C1U_MAX || in->Val.Int < TOTEM_C1U_MIN)
            {
                return totemBool_False;
            }
            *out = in->Val.Int;
            break;

        default:
            return totemBool_False;
    }
    
    return totemBool_True;
}

static totemEvalStatus totemRegister_ToOperandPrototype(totemBuildPrototype *build, totemRegister* in, totemOperandPrototype* out)
{
    totemFloat numVal = 0;
    switch(totemRegister_GetType(in))
    {
        case totemPublicDataType_Float:
            numVal = totemRegister_GetFloat(in);
            break;
            
        case totemPublicDataType_Int:
            numVal = (totemFloat)totemRegister_GetInt(in);
            break;
            
        default:
            return totemEvalStatus_Break(totemEvalStatus_BadFold);
    }
    
    return totemBuildPrototype_EvalFloatConstant(build, numVal, out, NULL);
}

static totemEvalStatus DispatchArith(totemBuildPrototype *build, totemOperationType op, totemOperandPrototype* dst, totemEvalArithOperandPrototype* lft, totemEvalArithOperandPrototype* rgt)
{
    totemRegister dstR, lftR, rgtR;
    totemOperand1u c1u = 0;
    TOTEM_ZERO(dstR);
    TOTEM_ZERO(lftR);
    TOTEM_ZERO(rgtR);
    
#define PREPARE_PARAMS_Abc \
    if(!totemOperandPrototype_ToRegister(lft, &lftR) || !totemOperandPrototype_ToRegister(rgt, &rgtR)) \
    { \
        return totemEvalStatus_Break(totemEvalStatus_BadFold); \
    }

#define PREPARE_PARAMS_Abc1u \
    if(!totemOperandPrototype_ToRegister(lft, &lftR) || !totemOperandPrototype_ToC1u(rgt, &c1u)) \
    { \
        return totemEvalStatus_Break(totemEvalStatus_BadFold); \
    }
    
#define FOLD_Abc (&dstR, &lftR, &rgtR)
#define FOLD_Abc1u (&dstR, &lftR, c1u)
    
    switch (op)
    {
#define FOLD_OP_DEFAULT(opType, insType, dst, lft, rht) return totemEvalStatus_Break(totemEvalStatus_BadFold);
#define FOLD_OP_Pure(opType, insType, dst, lft, rht) \
    PREPARE_PARAMS_ ## insType; \
    TOTEM_DISPATCH_NAME(opType) FOLD_ ## insType;

#define FOLD_OP_PureControl FOLD_OP_Pure
#define FOLD_OP_Return FOLD_OP_DEFAULT
#define FOLD_OP_None FOLD_OP_DEFAULT

#define FOLD_OP(opType, type, behaviour) \
        case totemOperationType_ ## opType: \
            FOLD_OP_ ## behaviour(opType, type, dst, lft, rgt); \
            break;

        TOTEM_EMIT_OPCODES_BINARY_ARITH(FOLD_OP)
    }

    return totemRegister_ToOperandPrototype(build, &dstR, dst);
}

/*
totemEvalStatus totemBuildPrototype_EvalUnaryArith(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperationType base)
{
    // todo - unary folding
    return totemEvalStatus_Success;
}*/

totemEvalStatus totemBuildPrototype_EvalBinaryArith(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperandPrototype *rgt, totemOperationType base)
{
    // determine which binary arith op to encode based on lValue and rValue
    totemOperationType op = base;

    totemRegisterListPrototype *regList = totemBuildPrototype_GetLocalRegisters(build);
    
    totemEvalArithOperandPrototype lftOp, rgtOp;
    TOTEM_ZERO(lftOp);
    TOTEM_ZERO(rgtOp);
    totemEvalArithOperandPrototype_FromOperandPrototype(regList, lft, &lftOp);
    totemEvalArithOperandPrototype_FromOperandPrototype(regList, rgt, &rgtOp);
    
    totemBool lIsConst = TOTEM_HASBITS(lftOp.Flags, totemRegisterPrototypeFlag_IsValue);
    totemBool rIsConst = TOTEM_HASBITS(rgtOp.Flags, totemRegisterPrototypeFlag_IsValue);
    //totemBool isComplex = base == totemOperationType_Add; // todo - fold complex types
    totemBool isDivide = base == totemOperationType_Divide; //todo - fold division ops
    
    /*
     // todo - specialised opcodes for types
    if (lftOp.Type == totemPublicDataType_Int && rgtOp.Type == totemPublicDataType_Int)
    {
        totemInt lValueVal = lftOp.Val.Int;
        totemInt rValueVal = rgtOp.Val.Int;
        
        if (rIsConst && rValueVal <= TOTEM_C1U_MAX && rValueVal >= TOTEM_C1U_MIN)
        {
            op = IntIntConst;
        }
        else if (lIsConst && lValueVal <= TOTEM_C1U_MAX && lValueVal >= TOTEM_C1U_MIN)
        {
            // these are commutative, so we can swap the operands around
            totemOperandPrototype *swap = lft;
            lft = rgt;
            rgt = swap;
            op = IntIntConst;
        }
        else
        {
            op = IntInt;
        }
    }
    else if (lftOp.Type == totemPublicDataType_Float && rgtOp.Type == totemPublicDataType_Float)
    {
        op = FloatFloat;
    }*/
    
    /*
    // todo - complex value folding
    else if (isComplex)
    {
        // these are not commutative, so we cannot swap operands around
        
        if (lftType == totemPublicDataType_Array && rgtType == totemPublicDataType_Array)
        {
            op = ArrayArray;
        }
        else if (lftType == totemPublicDataType_String && rgtType == totemPublicDataType_String)
        {
            op = StringString;
        }
    }*/
    
    /*
    TOTEM_LOG_V(Eval, Arith, Debug, "TryFold %s %s %i %i %i %i %s",
                     totemPublicDataType_Describe(lftOp.Type),
                     totemPublicDataType_Describe(rgtOp.Type),
                     isDivide,
                     lIsConst,
                     rIsConst,
                     totemOperationType_IsPure(op),
                     totemOperationType_Describe(op)
                     );*/
    
    if((lftOp.Type == totemPublicDataType_Float || lftOp.Type == totemPublicDataType_Int) && (rgtOp.Type == totemPublicDataType_Float || rgtOp.Type == totemPublicDataType_Int) && !isDivide && lIsConst && rIsConst && totemOperationType_IsPure(op))
    {
        // fold the value at compile-time, no need to write instruction
        return DispatchArith(build, op, dst, &lftOp, &rgtOp);
    }
 
    // cannot fold, defer to runtime
    // todo - make a better version of this "emit generic instruction" procedure
#define EMIT_Abc return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, op);
#define EMIT_Abc1u \
    { \
        totemOperand1u c1u = 0; \
        if(!totemOperandPrototype_ToC1u(&rgtOp, &c1u)) \
        { \
            return totemEvalStatus_Break(totemEvalStatus_BadFold); \
        } \
        return totemBuildPrototype_EvalAbc1uInstruction(build, dst, lft, c1u, op); \
    }
    
    switch (op)
    {
#define EMIT_OP(opType, type, behaviour) \
        case totemOperationType_ ## opType: \
            EMIT_ ## type; \
            break;

        TOTEM_EMIT_OPCODES_BINARY_ARITH(EMIT_OP)
        default: return totemEvalStatus_Break(totemEvalStatus_BadFold);
    }
}
