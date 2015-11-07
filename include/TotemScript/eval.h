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
        totemEvalStatus_InvalidArgument,
        totemEvalStatus_FunctionNotDefined,
        totemEvalStatus_TooManyRegisters
    }
    totemEvalStatus;
    
    typedef struct
    {
        totemRegisterScopeType RegisterScopeType;
        totemRegisterIndex RegisterIndex;
    }
    totemOperandRegisterPrototype;
    
    void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list);
    void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype);
    totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemNumber *number, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *buffer, totemOperandRegisterPrototype *operand);
    totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand);

    /**
     * Convert parse tree into instructions, link globals * functions
     */
    void totemBuildPrototype_Init(totemBuildPrototype *build, totemRuntime *runtime);
    totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype);
    totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemString *name, totemFunction **functionOut);
    totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemRegisterListPrototype *globals);
    totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);

    totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemSwitchBlockPrototype_Eval(totemSwitchBlockPrototype *switchBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);
    
    totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals);

    totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value);

    totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value);
    
    totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *index, totemRegisterScopeType type);
    
    totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemBuildPrototype *build, totemOperandRegisterPrototype *index);

    totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperation operationType);
    totemEvalStatus totemBuildPrototype_EvalAbxInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandX bx, totemOperation operationType);
    totemEvalStatus totemBuildPrototype_EvalAxxInstruction(totemBuildPrototype *build, totemOperandX ax, totemOperation operationType);
    totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build);
    
#define TOTEM_EVAL_CHECKRETURN(exp) { totemEvalStatus status = exp; if(status != totemEvalStatus_Success) return status; }

#ifdef __cplusplus
}
#endif

#endif
