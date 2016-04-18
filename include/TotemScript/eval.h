//
//  eval.h
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_EVAL_H
#define TOTEMSCRIPT_EVAL_H

#include <TotemScript/base.h>
#include <TotemScript/parse.h>
#include <TotemScript/exec.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        totemEvalStatus_Success,
        totemEvalStatus_OutOfMemory,
        totemEvalStatus_FunctionNotDefined,
        totemEvalStatus_NativeFunctionAlreadyDefined,
        totemEvalStatus_ScriptFunctionAlreadyDefined,
        totemEvalStatus_TooManyRegisters,
        totemEvalStatus_InstructionOverflow,
        totemEvalStatus_VariableAlreadyAssigned,
        totemEvalStatus_VariableNotDefined,
        totemEvalStatus_VariableAlreadyDefined,
        totemEvalStatus_AssignmentLValueNotMutable,
        totemEvalStatus_AssignmentLValueCannotBeConst,
        totemEvalStatus_TooManyNativeFunctions,
        totemEvalStatus_TooManyScriptFunctions
    }
    totemEvalStatus;
    
    const char *totemEvalStatus_Describe(totemEvalStatus status);
    
    typedef enum
    {
        totemEvalVariableFlag_None = 0,
        totemEvalVariableFlag_LocalOnly = 1,
        totemEvalVariableFlag_MustBeDefined = 1 << 1
    }
    totemEvalVariableFlag;
    
    typedef enum
    {
        totemRegisterPrototypeFlag_None = 0,
        totemRegisterPrototypeFlag_IsConst = 1,
        totemRegisterPrototypeFlag_IsAssigned = 1 << 1,
        totemRegisterPrototypeFlag_IsVariable = 1 << 2,
        totemRegisterPrototypeFlag_IsValue = 1 << 3,
        totemRegisterPrototypeFlag_IsTemporary = 1 << 4,
        totemRegisterPrototypeFlag_IsUsed = 1 << 5
    }
    totemRegisterPrototypeFlag;
    
    typedef struct
    {
        totemRegisterValue Value;
        totemDataType DataType;
        totemRegisterPrototypeFlag Flags;
    }
    totemRegisterPrototype;
    
    typedef struct
    {
        totemRegisterScopeType RegisterScopeType;
        totemRegisterIndex RegisterIndex;
    }
    totemOperandRegisterPrototype;
    
    void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list, totemRegisterScopeType scope);
    void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand);
    totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag *flags);
    totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemRegisterIndex index, totemDataType *type);
    totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemRegisterIndex index, totemDataType type);
    totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype);
    totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *buffer, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand);
    totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand);

    /**
     * Convert parse tree into instructions, link globals * functions
     */
    void totemBuildPrototype_Init(totemBuildPrototype *build);
    void totemBuildPrototype_Reset(totemBuildPrototype *build);
    void totemBuildPrototype_Cleanup(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype);
    totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemFunction **functionOut);
    totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemRegisterListPrototype *globals);
    totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);

    totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);

    totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *lValueHint, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags);

    totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags);
    
    totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *value);
    
    totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *index, totemEvalVariableFlag flags);
    
    totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *index);

    totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperationType operationType);
    totemEvalStatus totemBuildPrototype_EvalAbxInstructionSigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXSigned bx, totemOperationType operationType);
    totemEvalStatus totemBuildPrototype_EvalAbxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXUnsigned bx, totemOperationType operationType);

    totemEvalStatus totemBuildPrototype_EvalAxxInstructionSigned(totemBuildPrototype *build, totemOperandXSigned ax, totemOperationType operationType);
    totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *dest);
    
    totemEvalStatus totemInstruction_SetRegisterA(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope);
    totemEvalStatus totemInstruction_SetRegisterB(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope);
    totemEvalStatus totemInstruction_SetRegisterC(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope);
    totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op);
    totemEvalStatus totemInstruction_SetBxSigned(totemInstruction *instruction, totemOperandXSigned bx);
    totemEvalStatus totemInstruction_SetBxUnsigned(totemInstruction *instruction, totemOperandXUnsigned bx);
    totemEvalStatus totemInstruction_SetAxSigned(totemInstruction *instruction, totemOperandXSigned ax);
    totemEvalStatus totemInstruction_SetAxunsigned(totemInstruction *instruction, totemOperandXUnsigned ax);

#ifdef __cplusplus
}
#endif

#endif
