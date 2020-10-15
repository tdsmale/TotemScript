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
#include <TotemScript/memory.h>

#ifdef __cplusplus
extern "C" {
#endif
    
	typedef enum totemEvalStatus
    {
        totemEvalStatus_Success,
        totemEvalStatus_OutOfMemory,
        totemEvalStatus_FunctionNotDefined,
        totemEvalStatus_ScriptFunctionAlreadyDefined,
        totemEvalStatus_TooManyRegisters,
        totemEvalStatus_InstructionOverflow,
        totemEvalStatus_VariableAlreadyAssigned,
        totemEvalStatus_IdentifierNotDefined,
        totemEvalStatus_IdentifierAlreadyDefined,
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
    
	typedef enum totemRegisterPrototypeFlag
    {
        totemRegisterPrototypeFlag_None = 0,
        totemRegisterPrototypeFlag_IsConst = 1,
        totemRegisterPrototypeFlag_IsAssigned = 1 << 1,
        totemRegisterPrototypeFlag_IsVariable = 1 << 2,
        totemRegisterPrototypeFlag_IsValue = 1 << 3,
        totemRegisterPrototypeFlag_IsTemporary = 1 << 4,
        totemRegisterPrototypeFlag_IsUsed = 1 << 5,
        totemRegisterPrototypeFlag_IsGlobalCache = 1 << 6
    }
    totemRegisterPrototypeFlag;
    
	typedef enum totemBuildPrototypeFlag
    {
        totemBuildPrototypeFlag_None = 0,
        totemBuildPrototypeFlag_EvalGlobalCaches = 1
    }
    totemBuildPrototypeFlag;
    
	typedef struct totemFunctionPointerPrototype
    {
        totemOperandXUnsigned Address;
        totemFunctionType Type;
    }
    totemFunctionPointerPrototype;
    
	typedef union totemRegisterPrototypeValue
	{
		totemFloat Float;
		totemInt Int;
		totemString *String;
		totemFunctionPointerPrototype FunctionPointer;
		totemPublicDataType Type;
		totemBool Boolean;
	}
	totemRegisterPrototypeValue;

	typedef struct totemRegisterPrototype
    {
		totemRegisterPrototypeValue Value;
		totemPublicDataType DataType;
        totemOperandXUnsigned GlobalCache;
        totemRegisterPrototypeFlag Flags;
    }
    totemRegisterPrototype;
    
	TOTEM_CACHEBUFFER_DEFINITION(totemRegisterPrototype);

	typedef struct totemOperandPrototype
    {
        totemOperandXUnsigned RegisterIndex;
        totemOperandType RegisterScopeType;
    }
    totemOperandPrototype;

	TOTEM_CACHEBUFFER_DEFINITION(totemOperandPrototype);
    
    typedef struct totemRegisterListPrototypeScope
    {
        totemHashMap Identifiers;
        totemHashMap MoveToLocalVars;
        struct totemRegisterListPrototypeScope *Prev;
    }
    totemRegisterListPrototypeScope;
    
	typedef struct totemRegisterListPrototype
    {
        totemOperandXUnsigned DataTypes[totemPublicDataType_Max];
        totemOperandXUnsigned Boolean[2];
        totemOperandXUnsigned Null;
        totemBool HasDataType[totemPublicDataType_Max];
        totemBool HasBoolean[2];
        totemBool HasNull;
        totemRegisterPrototypeBuffer Registers;
        totemOperandXUnsignedBuffer RegisterFreeList;
        totemHashMap Strings;
        totemHashMap Numbers;
        totemRegisterListPrototypeScope *Scope;
        totemOperandType ScopeType;
    }
    totemRegisterListPrototype;
    
    void totemRegisterListPrototype_Init(totemRegisterListPrototype *list, totemOperandType scope);
    void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list);
    void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand);
    totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeFlag *flags);
    totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType *type);
    totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemPublicDataType type);
    totemBool totemRegisterListPrototype_SetRegisterGlobalCache(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned assoc);
    totemBool totemRegisterListPrototype_GetRegisterGlobalCache(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemOperandXUnsigned *assoc);
	totemBool totemRegisterListPrototype_GetRegisterValue(totemRegisterListPrototype *list, totemOperandXUnsigned index, totemRegisterPrototypeValue *valueOut);

    totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandPrototype *prototype);
    totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *buffer, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemString *name, totemFunctionPointerPrototype *value, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddType(totemRegisterListPrototype *list, totemPublicDataType type, totemOperandPrototype *op);
    totemEvalStatus totemRegisterListPrototype_AddBoolean(totemRegisterListPrototype *list, totemBool val, totemOperandPrototype *op);
    totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandPrototype *op);
    
    totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandPrototype *operand, totemBool currentOnly);
    totemBool totemRegisterListPrototype_GetIdentifier(totemRegisterListPrototype *list, totemString *name, totemOperandPrototype *operand, totemBool currentOnly);
    
    totemEvalStatus totemRegisterListPrototype_EnterScope(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_ExitScope(totemRegisterListPrototype *list);
    
    void totemRegisterListPrototypeScope_Init(totemRegisterListPrototypeScope *scope);
    void totemRegisterListPrototypeScope_Reset(totemRegisterListPrototypeScope *scope);
    void totemRegisterListPrototypeScope_Cleanup(totemRegisterListPrototypeScope *scope);
	totemEvalStatus totemRegisterListPrototypeScope_FreeGlobalCache(totemRegisterListPrototypeScope *scope, totemRegisterListPrototype *list);
    
	typedef struct totemScriptFunctionPrototype
    {
        totemString Name;
        size_t InstructionsStart;
		size_t NumInstructions;
        totemInt RegistersNeeded;
    }
    totemScriptFunctionPrototype;

	TOTEM_CACHEBUFFER_DEFINITION(totemScriptFunctionPrototype);
    
	typedef struct totemBuildPrototype
    {
        totemRegisterListPrototype GlobalRegisters;
        totemHashMap FunctionLookup;
		totemScriptFunctionPrototypeBuffer Functions;
        totemInstructionBuffer Instructions;
        totemStringBuffer NativeFunctionNames;
		totemOperandPrototypeBuffer FunctionArguments;
		totemOperandPrototypeBuffer RecycledRegisters;
        totemOperandXUnsignedBuffer AnonFunctions;
        size_t CurrentAnonFunc;
        totemBufferPositionInfo *ErrorAt;
        totemFunctionDeclarationPrototype *AnonymousFunctionHead;
        totemFunctionDeclarationPrototype *AnonymousFunctionTail;
        totemRegisterListPrototype *LocalRegisters;
        totemRegisterListPrototype *LocalVariableScope;
        totemBuildPrototypeFlag Flags;
    }
    totemBuildPrototype;
    
    void totemBuildPrototype_Init(totemBuildPrototype *build);
    void totemBuildPrototype_Reset(totemBuildPrototype *build);
    void totemBuildPrototype_Cleanup(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandPrototype *opOut);
    totemEvalStatus totemBuildPrototype_RecycleRegister(totemBuildPrototype *build, totemOperandPrototype *op);
    totemRegisterListPrototype *totemBuildPrototype_GetLocalScope(totemBuildPrototype *build);
    totemRegisterListPrototype *totemBuildPrototype_GetRegisterList(totemBuildPrototype *build, totemOperandType scope);
    totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype);
    totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut);
    
    totemEvalStatus totemStatementPrototype_EvalValues(totemStatementPrototype *statement, totemBuildPrototype *build);
    totemEvalStatus totemWhileLoopPrototype_EvalValues(totemWhileLoopPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemIfBlockPrototype_EvalValues(totemIfBlockPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemForLoopPrototype_EvalValues(totemForLoopPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemDoWhileLoopPrototype_EvalValues(totemDoWhileLoopPrototype *loop, totemBuildPrototype *build);
    totemEvalStatus totemExpressionPrototype_EvalValues(totemExpressionPrototype *expression, totemBuildPrototype *build);
    totemEvalStatus totemArgumentPrototype_EvalValues(totemArgumentPrototype *arg, totemBuildPrototype *build);
    totemEvalStatus totemVariablePrototype_EvalValues(totemVariablePrototype *call, totemBuildPrototype *build);

	totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandPrototype *lValueHint, totemOperandPrototype *result);
	totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build);
	totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value);
	totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value);
	totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build);
	totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build);
	totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build);
	totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build);
	totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandPrototype *index);
	totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *funcPrototype);
    
    totemEvalStatus totemBuildPrototype_EnterLocalScope(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_ExitLocalScope(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_RotateGlobalCache(totemBuildPrototype *build, totemRegisterListPrototype *list, totemRegisterListPrototypeScope *scope, totemBool toLocal);
    totemEvalStatus totemBuildPrototype_RotateAllGlobalCaches(totemBuildPrototype *build, totemRegisterListPrototype *list, totemBool toLocal);
    totemEvalStatus totemBuildPrototype_GlobalCacheCheck(totemBuildPrototype *build, totemOperandPrototype *opOut, totemOperandPrototype *hint);
    
    totemEvalStatus totemBuildPrototype_EvalBoolean(totemBuildPrototype *build, totemBool val, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalInt(totemBuildPrototype *build, totemInt number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemString *number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemString *buffer, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalFunctionName(totemBuildPrototype *build, totemString *name, totemFunctionPointerPrototype *func);
    totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemString *name, totemFunctionPointerPrototype *value, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalNull(totemBuildPrototype *build, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalIdentifier(totemBuildPrototype *build, totemString *name, totemOperandPrototype *op, totemOperandPrototype *hint);
	totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandPrototype *src);
	totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build);
    
    totemEvalStatus totemInstruction_SetA(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope);
    totemEvalStatus totemInstruction_SetB(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope);
    totemEvalStatus totemInstruction_SetC(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope);
    totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op);
	totemEvalStatus totemInstruction_SetB1(totemInstruction *instruction, totemOperandXSigned bx);
    totemEvalStatus totemInstruction_SetB2(totemInstruction *instruction, totemOperandXSigned bx);
    totemEvalStatus totemInstruction_SetB2u(totemInstruction *instruction, totemOperandXUnsigned bx);
    totemEvalStatus totemInstruction_SetA3(totemInstruction *instruction, totemOperandXSigned ax);
    totemEvalStatus totemInstruction_SetA3u(totemInstruction *instruction, totemOperandXUnsigned ax);
	totemEvalStatus totemInstruction_SetC1(totemInstruction *instruction, totemOperandXSigned cx);
	totemEvalStatus totemInstruction_SetC1u(totemInstruction *instruction, totemOperandXUnsigned cx);

#define EMIT_PARAMS_A		totemInstruction *instruction, totemOperandPrototype *a
#define EMIT_PARAMS_A3		totemInstruction *instruction, totemOperandXSigned a3
#define EMIT_PARAMS_A3u		totemInstruction *instruction, totemOperandXUnsigned a3u
#define EMIT_PARAMS_Ab		totemInstruction *instruction, totemOperandPrototype *a, totemOperandPrototype *b
#define EMIT_PARAMS_Ab2		totemInstruction *instruction, totemOperandPrototype *a, totemOperandXSigned b2
#define EMIT_PARAMS_Ab2u	totemInstruction *instruction, totemOperandPrototype *a, totemOperandXUnsigned b2u
#define EMIT_PARAMS_Ab2ug	totemInstruction *instruction, totemOperandPrototype *a, totemOperandXUnsigned b2u
#define EMIT_PARAMS_Abc		totemInstruction *instruction, totemOperandPrototype *a, totemOperandPrototype *b, totemOperandPrototype *c
#define EMIT_PARAMS_Abc1	totemInstruction *instruction, totemOperandPrototype *a, totemOperandPrototype *b, totemOperandXSigned c1
#define EMIT_PARAMS_Abc1u	totemInstruction *instruction, totemOperandPrototype *a, totemOperandPrototype *b, totemOperandXUnsigned c1u
#define EMIT_PARAMS_Ab1c1	totemInstruction *instruction, totemOperandPrototype *a, totemOperandXSigned b1, totemOperandXSigned c1

	totemEvalStatus totemInstruction_AsA ( EMIT_PARAMS_A, totemOperationType op );
	totemEvalStatus totemInstruction_AsA3(EMIT_PARAMS_A3, totemOperationType op);
	totemEvalStatus totemInstruction_AsA3u(EMIT_PARAMS_A3u, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb(EMIT_PARAMS_Ab, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb2(EMIT_PARAMS_Ab2, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb2u(EMIT_PARAMS_Ab2u, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb2ug(EMIT_PARAMS_Ab2ug, totemOperationType op);
	totemEvalStatus totemInstruction_AsAbc(EMIT_PARAMS_Abc, totemOperationType op);
	totemEvalStatus totemInstruction_AsAbc1(EMIT_PARAMS_Abc1, totemOperationType op);
	totemEvalStatus totemInstruction_AsAbc1u(EMIT_PARAMS_Abc1u, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb1c1(EMIT_PARAMS_Ab1c1, totemOperationType op);

#define EMIT(op, type, behaviour) \
	totemEvalStatus totemInstruction_As ## op ( EMIT_PARAMS_ ## type ); \
	totemEvalStatus totemBuildPrototype_EvalInstruction_As ## op ( totemBuildPrototype *prototype, EMIT_PARAMS_ ## type ); 
	TOTEM_EMIT_OPCODES(EMIT)
#undef EMIT

#define TOTEM_EVAL_CHECKRETURN(x) { totemEvalStatus _status = (x); if(_status != totemEvalStatus_Success) return totemEvalStatus_Break(_status); }
    
#ifdef __cplusplus
}
#endif

#endif
