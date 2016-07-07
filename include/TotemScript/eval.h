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
        totemEvalStatus_TooManyScriptFunctions,
        totemEvalStatus_TooManyFunctionArguments,
        totemEvalStatus_InvalidDataType
    }
    totemEvalStatus;
    
    const char *totemEvalStatus_Describe(totemEvalStatus status);
    totemEvalStatus totemEvalStatus_Break(totemEvalStatus status);
    
    typedef enum
    {
        totemEvalVariableFlag_None = 0,
        totemEvalVariableFlag_LocalOnly = 1,
        totemEvalVariableFlag_MustBeDefined = 1 << 1
    }
    totemEvalVariableFlag;
    
    typedef enum
    {
        totemEvalExpressionFlag_None = 0,
        totemEvalExpressionFlag_Shift = 1
    }
    totemEvalExpressionFlag;
    
    typedef enum
    {
        totemRegisterPrototypeFlag_None = 0,
        totemRegisterPrototypeFlag_IsConst = 1,
        totemRegisterPrototypeFlag_IsAssigned = 1 << 1,
        totemRegisterPrototypeFlag_IsVariable = 1 << 2,
        totemRegisterPrototypeFlag_IsValue = 1 << 3,
        totemRegisterPrototypeFlag_IsTemporary = 1 << 4,
        totemRegisterPrototypeFlag_IsUsed = 1 << 5,
        totemRegisterPrototypeFlag_IsGlobalAssoc = 1 << 6
    }
    totemRegisterPrototypeFlag;
    
    typedef enum
    {
        totemBuildPrototypeFlag_None = 0,
        totemBuildPrototypeFlag_EvalVariables = 1,
        totemBuildPrototypeFlag_EvalGlobalAssocs = 1 << 1,
        totemBuildPrototypeFlag_EnforceVariableDefinitions = 1 << 2
    }
    totemBuildPrototypeFlag;
    
    typedef struct
    {
        totemOperandXUnsigned Address;
        totemFunctionType Type;
    }
    totemFunctionPointerPrototype;
    
    typedef struct
    {
        union
        {
            totemFloat Float;
            totemInt Int;
            totemString *String;
            totemFunctionPointerPrototype FunctionPointer;
            totemPublicDataType TypeValue;
        };
        //totemRegisterValue Value;
        size_t RefCount;
        totemOperandXUnsigned GlobalAssoc;
        totemPublicDataType DataType;
        totemRegisterPrototypeFlag Flags;
    }
    totemRegisterPrototype;
    
    typedef struct
    {
        totemOperandXUnsigned RegisterIndex;
        totemOperandType RegisterScopeType;
    }
    totemOperandRegisterPrototype;
    
    typedef struct
    {
        totemOperandXUnsigned DataTypes[totemPublicDataType_Max];
        totemBool HasDataType[totemPublicDataType_Max];
        totemMemoryBuffer Registers;
        totemMemoryBuffer RegisterFreeList;
        totemHashMap Variables;
        totemHashMap Strings;
        totemHashMap Numbers;
        totemHashMap MoveToLocalVars;
        totemHashMap FunctionPointers;
        totemOperandType Scope;
    }
    totemRegisterListPrototype;
    
    typedef struct
    {
        totemString Name;
        size_t InstructionsStart;
        uint8_t RegistersNeeded;
    }
    totemScriptFunctionPrototype;
    
    void totemRegisterListPrototype_Init(totemRegisterListPrototype *list, totemOperandType scope);
    void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list);
    void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand);
    totemBool totemRegisterListPrototype_IncRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *count);
    totemBool totemRegisterListPrototype_DecRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *count);
    totemBool totemRegisterListPrototype_GetRegisterRefCount(totemRegisterListPrototype *list, totemOperandXUnsigned index, size_t *count);
    totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand);
    totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag *flags);
    totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType *type);
    totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType type);
    totemBool totemRegisterListPrototype_SetRegisterGlobalAssoc(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned assoc);
    totemBool totemRegisterListPrototype_GetRegisterGlobalAssoc(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned *assoc);
    
    totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype);
    totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *buffer, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemFunctionPointerPrototype *value, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddType(totemRegisterListPrototype *list, totemPublicDataType type, totemOperandRegisterPrototype *op);
    
    totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand);
    
    typedef struct
    {
        totemRegisterListPrototype GlobalRegisters;
        totemRegisterListPrototype *LocalRegisters;
        totemHashMap FunctionLookup;
        totemHashMap NativeFunctionNamesLookup;
        totemHashMap AnonymousFunctions;
        totemMemoryBuffer Functions;
        totemMemoryBuffer Instructions;
        totemMemoryBuffer NativeFunctionNames;
        totemMemoryBuffer FunctionArguments;
        void *ErrorContext;
        totemFunctionDeclarationPrototype *AnonymousFunctionHead;
        totemFunctionDeclarationPrototype *AnonymousFunctionTail;
        totemBuildPrototypeFlag Flags;
    }
    totemBuildPrototype;
    
    void totemBuildPrototype_Init(totemBuildPrototype *build);
    void totemBuildPrototype_Reset(totemBuildPrototype *build);
    void totemBuildPrototype_Cleanup(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandRegisterPrototype *opOut);
    totemEvalStatus totemBuildPrototype_RecycleRegister(totemBuildPrototype *build, totemOperandRegisterPrototype *op);
    totemRegisterListPrototype *totemBuildPrototype_GetLocalScope(totemBuildPrototype *build);
    totemRegisterListPrototype *totemBuildPrototype_GetRegisterList(totemBuildPrototype *build, totemOperandType scope);
    totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype);
    totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut);
    
    totemEvalStatus totemStatementPrototype_EvalValues(totemStatementPrototype *statement, totemBuildPrototype *build);
    totemEvalStatus totemWhileLoopPrototype_EvalValues(totemWhileLoopPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemIfBlockPrototype_EvalValues(totemIfBlockPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemForLoopPrototype_EvalValues(totemForLoopPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemDoWhileLoopPrototype_EvalValues(totemDoWhileLoopPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemExpressionPrototype_EvalValues(totemExpressionPrototype *expression, totemBuildPrototype *build, totemEvalVariableFlag varFlags);
    totemEvalStatus totemArgumentPrototype_EvalValues(totemArgumentPrototype *arg, totemBuildPrototype *build, totemEvalVariableFlag varFlags);
    totemEvalStatus totemFunctionCallPrototype_EvalValues(totemFunctionCallPrototype *call, totemBuildPrototype *build);
    totemEvalStatus totemVariablePrototype_EvalValues(totemVariablePrototype *call, totemBuildPrototype *build, totemEvalVariableFlag varFlags);
    
    totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemString *number, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemString *buffer, totemOperandRegisterPrototype *operand);
    totemEvalStatus totemBuildPrototype_EvalFunctionName(totemBuildPrototype *build, totemString *name, totemFunctionPointerPrototype *func);
    totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemFunctionPointerPrototype *value, totemOperandRegisterPrototype *op);
    totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandRegisterPrototype *op);
    totemEvalStatus totemBuildPrototype_EvalNamedFunctionPointer(totemBuildPrototype *build, totemString *name, totemOperandRegisterPrototype *op);
    totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandRegisterPrototype *operand);
    
    totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *prototype);
    totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build);
    totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build);
    totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build);
    totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build);
    totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build);
    totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandRegisterPrototype *lValueHint, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags, totemEvalExpressionFlag exprFlags);
    totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags);
    totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *value);
    totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandRegisterPrototype *index, totemEvalVariableFlag flags);
    totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *index);
    
    totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperationType operationType);
    totemEvalStatus totemBuildPrototype_EvalAbxInstructionSigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXSigned bx, totemOperationType operationType);
    totemEvalStatus totemBuildPrototype_EvalAbxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXUnsigned bx, totemOperationType operationType);
    totemEvalStatus totemBuildPrototype_EvalAxxInstructionSigned(totemBuildPrototype *build, totemOperandXSigned ax, totemOperationType operationType);
    
    totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *dest);
    
    totemEvalStatus totemInstruction_SetRegisterA(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope);
    totemEvalStatus totemInstruction_SetRegisterB(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope);
    totemEvalStatus totemInstruction_SetRegisterC(totemInstruction *instruction, totemLocalRegisterIndex index, totemOperandType scope);
    totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op);
    totemEvalStatus totemInstruction_SetBxSigned(totemInstruction *instruction, totemOperandXSigned bx);
    totemEvalStatus totemInstruction_SetBxUnsigned(totemInstruction *instruction, totemOperandXUnsigned bx);
    totemEvalStatus totemInstruction_SetAxSigned(totemInstruction *instruction, totemOperandXSigned ax);
    totemEvalStatus totemInstruction_SetAxUnsigned(totemInstruction *instruction, totemOperandXUnsigned ax);
    
#define TOTEM_EVAL_CHECKRETURN(exp) { totemEvalStatus _status = exp; if(_status != totemEvalStatus_Success) return _status; }
    
#ifdef __cplusplus
}
#endif

#endif
