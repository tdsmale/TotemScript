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
        totemEvalStatus_InvalidDataType,
        totemEvalStatus_UnknownDataType,
        totemEvalStatus_BadInt,
        totemEvalStatus_BadGlobal,
        totemEvalStatus_DivideByZero,
        totemEvalStatus_BadFold,
        totemEvalStatus_BadArrayLength,
        totemEvalStatus_BadNumArrayElements,
        totemEvalStatus_TooManyInstructions
    }
    totemEvalStatus;
    
    const char *totemEvalStatus_Describe(totemEvalStatus status);
    totemEvalStatus totemEvalStatus_Break(totemEvalStatus status);
    
	typedef enum totemRegisterPrototypeFlag
    {
        totemRegisterPrototypeFlag_None = 0,
        totemRegisterPrototypeFlag_IsConst = 1,             // let x = ??
        totemRegisterPrototypeFlag_IsAssigned = 1 << 1,     // mutated as lValue once
        totemRegisterPrototypeFlag_IsVariable = 1 << 2,     // is a named var i.e. let/var
        totemRegisterPrototypeFlag_IsValue = 1 << 3,        // is a compile-time value
        totemRegisterPrototypeFlag_IsTemporary = 1 << 4,    // a temporary/intermediary allocation
        totemRegisterPrototypeFlag_IsUsed = 1 << 5,         // has been allocated
    }
    totemRegisterPrototypeFlag;
    
    typedef enum totemOperandPrototypeFlag
    {
        totemOperandPrototypeFlag_None = 0,
        totemOperandPrototypeFlag_Allocated = 1,
        totemOperandPrototypeFlag_IsVariable = 1 << 1,
    }
    totemOperandPrototypeFlag;

	typedef enum totemBuildPrototypeFlag
    {
        totemBuildPrototypeFlag_None = 0,
        totemBuildPrototypeFlag_EvalGlobalCaches = 1
    }
    totemBuildPrototypeFlag;

    TOTEM_DYNBUFFER_DECLARATION(totemParseString);

    typedef struct totemEvalStrHashMapKey
    {
        totemParseString *Key;
        totemOperandXu Value;
    }
    totemEvalStrHashMapKey;

    typedef struct totemEvalStrHashMapEntry
    {
        totemHashMapEntry Entry;
        totemEvalStrHashMapKey Data;
    }
    totemEvalStrHashMapEntry;

    TOTEM_HASHMAP_DECLARATION(totemEvalStrHashMap);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemEvalStrHashMap, totemEvalStrHashMapKey);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemEvalStrHashMap, totemEvalStrHashMapKey);

    typedef struct totemEvalFloatHashMapKey
    {
        totemFloat Key;
        totemOperandXu Value;
    }
    totemEvalFloatHashMapKey;

    typedef struct totemEvalFloatHashMapEntry
    {
        totemHashMapEntry Entry;
        totemEvalFloatHashMapKey Data;
    }
    totemEvalFloatHashMapEntry;

    TOTEM_HASHMAP_DECLARATION(totemEvalFloatHashMap);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemEvalFloatHashMap, totemEvalFloatHashMapKey);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemEvalFloatHashMap, totemEvalFloatHashMapKey);

    typedef struct totemEvalIntHashMapKey
    {
        totemInt Key;
        totemOperandXu Value;
    }
    totemEvalIntHashMapKey;

    typedef struct totemEvalIntHashMapEntry
    {
        totemHashMapEntry Entry;
        totemEvalIntHashMapKey Data;
    }
    totemEvalIntHashMapEntry;

    TOTEM_HASHMAP_DECLARATION(totemEvalIntHashMap);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemEvalIntHashMap, totemEvalIntHashMapKey);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemEvalIntHashMap, totemEvalIntHashMapKey);

    typedef struct totemEvalOperandHashMapKey
    {
        totemOperandXu Key;
        totemOperandXu Value;
    }
    totemEvalOperandHashMapKey;

    typedef struct totemEvalOperandHashMapEntry
    {
        totemHashMapEntry Entry;
        totemEvalOperandHashMapKey Data;
    }
    totemEvalOperandHashMapEntry;

    TOTEM_HASHMAP_DECLARATION(totemEvalOperandHashMap);
    TOTEM_HASHMAP_SEARCH_DECLARATION(totemEvalOperandHashMap, totemEvalOperandHashMapKey);
    TOTEM_HASHMAP_INSERT_DECLARATION(totemEvalOperandHashMap, totemEvalOperandHashMapKey);
    
	typedef struct totemFunctionPointerPrototype
    {
        totemOperandXu Address;
        totemFunctionType Type;
    }
    totemFunctionPointerPrototype;
    
	typedef union totemRegisterPrototypeValue
	{
		totemFloat Float;
		totemInt Int;
		totemParseString *String;
		totemFunctionPointerPrototype FunctionPointer;
		totemPublicDataType Type;
		totemBool Bool;
	}
	totemRegisterPrototypeValue;

	typedef struct totemRegisterPrototype
    {
		totemRegisterPrototypeValue Value;
		totemPublicDataType DataType;
        totemRegisterPrototypeFlag Flags;
    }
    totemRegisterPrototype;
    
	TOTEM_DYNBUFFER_DECLARATION(totemRegisterPrototype);

	typedef struct totemOperandPrototype
    {
        totemOperandXu RegisterIndex;
        totemOperandPrototypeFlag Flags;
    }
    totemOperandPrototype;

    TOTEM_DYNBUFFER_DECLARATION(totemOperandPrototype);
    
    typedef struct totemRegisterListPrototypeScope
    {
        totemEvalStrHashMap Identifiers;
        totemEvalOperandHashMap MoveToLocalVars;
        struct totemRegisterListPrototypeScope *Prev;
    }
    totemRegisterListPrototypeScope;
    
	typedef struct totemRegisterListPrototype
    {
        totemOperandXu DataTypes[totemPublicDataType_Max];
        totemOperandXu Bool[2];
        totemOperandXu Null;
        totemBool HasDataType[totemPublicDataType_Max];
        totemBool HasBool[2];
        totemBool HasNull;
        totemRegisterPrototypeDynBuffer Registers;
        totemOperandXuDynBuffer RegisterFreeList;
        totemEvalStrHashMap Strings;
        totemEvalFloatHashMap Floats;
        totemEvalIntHashMap Ints;
        totemRegisterListPrototypeScope *Scope;
        totemOperandType ScopeType;
    }
    totemRegisterListPrototype;

    void totemRegisterListPrototype_Init(totemRegisterListPrototype *list, totemOperandType scope);
    void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list);
    void totemRegisterListPrototype_Cleanup(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_FreeRegister(totemRegisterListPrototype *list, totemOperandPrototype *operand);
    totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_UnsetRegisterFlags(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeFlag flags);
    totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeFlag *flags);
    totemBool totemRegisterListPrototype_GetRegisterType(totemRegisterListPrototype *list, totemOperandXu index, totemPublicDataType *type);
    totemBool totemRegisterListPrototype_SetRegisterType(totemRegisterListPrototype *list, totemOperandXu index, totemPublicDataType type);
	totemBool totemRegisterListPrototype_GetRegisterValue(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeValue *valueOut);
    totemBool totemRegisterListPrototype_SetRegisterValue(totemRegisterListPrototype *list, totemOperandXu index, totemRegisterPrototypeValue *valueOut);
    totemBool totemRegisterListPrototype_GetRegisterValueAndType(totemRegisterListPrototype* list, totemOperandXu index, totemRegisterPrototypeValue* valueOut, totemPublicDataType* typeOut);
    
    totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemParseString *name, totemOperandPrototype *prototype);
    totemEvalStatus totemRegisterListPrototype_AddFloatString(totemRegisterListPrototype *list, totemParseString *number, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddFloatConstant(totemRegisterListPrototype *list, totemFloat number, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddIntString(totemRegisterListPrototype *list, totemParseString *number, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddIntConstant(totemRegisterListPrototype *list, totemInt number, totemOperandPrototype *operand);
totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemParseString *buffer, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddFunctionPointer(totemRegisterListPrototype *list, totemParseString *name, totemFunctionPointerPrototype *value, totemOperandPrototype *operand);
    totemEvalStatus totemRegisterListPrototype_AddType(totemRegisterListPrototype *list, totemPublicDataType type, totemOperandPrototype *op);
    totemEvalStatus totemRegisterListPrototype_AddBool(totemRegisterListPrototype *list, totemBool val, totemOperandPrototype *op);
    totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandPrototype *op);
    
    totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemParseString *name, totemOperandPrototype *operand, totemBool currentOnly);
    totemBool totemRegisterListPrototype_GetIdentifier(totemRegisterListPrototype *list, totemParseString *name, totemOperandPrototype *operand, totemBool currentOnly);
    
    totemEvalStatus totemRegisterListPrototype_EnterScope(totemRegisterListPrototype *list);
    totemEvalStatus totemRegisterListPrototype_ExitScope(totemRegisterListPrototype *list);
    
    void totemRegisterListPrototypeScope_Init(totemRegisterListPrototypeScope *scope);
    void totemRegisterListPrototypeScope_Reset(totemRegisterListPrototypeScope *scope);
    void totemRegisterListPrototypeScope_Cleanup(totemRegisterListPrototypeScope *scope);
	totemEvalStatus totemRegisterListPrototypeScope_FreeGlobalCache(totemRegisterListPrototypeScope *scope, totemRegisterListPrototype *list);
    
	typedef struct totemScriptFunctionPrototype
    {
        totemParseString *Name;
        size_t InstructionsStart;
		size_t NumInstructions;
        totemOperandXu RegistersNeeded;
    }
    totemScriptFunctionPrototype;

    TOTEM_DYNBUFFER_DECLARATION(totemScriptFunctionPrototype);
    
	typedef struct totemBuildPrototype
    {
        totemRegisterListPrototype GlobalRegisters;
        totemEvalStrHashMap FunctionLookup;
		totemScriptFunctionPrototypeDynBuffer Functions;
        totemInstructionDynBuffer Instructions;
        totemParseStringDynBuffer NativeFunctionNames;
		totemOperandPrototypeDynBuffer FunctionArguments;
		totemOperandPrototypeDynBuffer RecycledRegisters;
        totemOperandXuDynBuffer AnonFunctions;
        size_t CurrentAnonFunc;
        totemTokenPositionInfo *ErrorAt;
        totemFunctionDeclarationPrototype *AnonymousFunctionHead;
        totemFunctionDeclarationPrototype *AnonymousFunctionTail;
        totemRegisterListPrototype *LocalRegisters;
        totemRegisterListPrototype *LocalVariableScope;
        totemScriptFunctionPrototype *CurrentFunction;
        totemParseTree *ParseTree;
        totemBuildPrototypeFlag Flags;
    }
    totemBuildPrototype;
    
    void totemBuildPrototype_Init(totemBuildPrototype *build, totemParseTree *tree);
    void totemBuildPrototype_Reset(totemBuildPrototype *build);
    void totemBuildPrototype_Cleanup(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandPrototype *opOut);
    totemEvalStatus totemBuildPrototype_RecycleRegister(totemBuildPrototype *build, totemOperandPrototype *op);
    totemRegisterListPrototype *totemBuildPrototype_GetLocalRegisters(totemBuildPrototype *build);
    totemRegisterListPrototype *totemBuildPrototype_GetRegisterList(totemBuildPrototype *build, totemOperandType scope);
    totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype);
    totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut);
    
	totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandPrototype *lValueHint, totemOperandPrototype *result);
	totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build);
	totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value);
	totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value);
	totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build);
	totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build);
    totemEvalStatus totemLoopPrototype_Eval(totemStatementPrototype* loop, totemBuildPrototype* build);
	totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build);
	totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build);
	totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandPrototype *index);
	totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *funcPrototype);
    
    totemEvalStatus totemBuildPrototype_EnterLocalScope(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_ExitLocalScope(totemBuildPrototype *build);
    totemEvalStatus totemBuildPrototype_RotateGlobalCache(totemBuildPrototype *build, totemRegisterListPrototype *list, totemRegisterListPrototypeScope *scope, totemBool toLocal);
    totemEvalStatus totemBuildPrototype_RotateAllGlobalCaches(totemBuildPrototype *build, totemRegisterListPrototype *list, totemBool toLocal);
    totemEvalStatus totemBuildPrototype_GlobalCacheCheck(totemBuildPrototype *build, totemOperandPrototype *opOut, totemOperandPrototype *hint);
    
    totemEvalStatus totemBuildPrototype_EvalBool(totemBuildPrototype *build, totemBool val, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemNumberPrototype *number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalIntString(totemBuildPrototype *build, totemParseString *number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalIntConstant(totemBuildPrototype *build, totemInt number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalFloatString(totemBuildPrototype *build, totemParseString *number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalFloatConstant(totemBuildPrototype *build, totemFloat number, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemParseString *buffer, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemParseString *name, totemFunctionPointerPrototype *value, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandPrototype *operand, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalNull(totemBuildPrototype *build, totemOperandPrototype *op, totemOperandPrototype *hint);
    totemEvalStatus totemBuildPrototype_EvalIdentifier(totemBuildPrototype *build, totemParseString *name, totemOperandPrototype *op, totemOperandPrototype *hint);
	totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandPrototype *src);
	totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build);
    
    totemEvalStatus totemBuildPrototype_EvalBinaryArith(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperandPrototype *rgt, totemOperationType base);
    totemEvalStatus totemBuildPrototype_EvalUnaryArith(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperationType base);

    totemEvalStatus totemInstruction_SetA(totemInstruction *instruction, totemOperandPrototype* index);
    totemEvalStatus totemInstruction_SetB(totemInstruction *instruction, totemOperandPrototype* index);
    totemEvalStatus totemInstruction_SetC(totemInstruction *instruction, totemOperandPrototype* index);
    totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op);
	totemEvalStatus totemInstruction_SetB1(totemInstruction *instruction, totemOperandX bx);
    totemEvalStatus totemInstruction_SetB2(totemInstruction *instruction, totemOperandX bx);
    totemEvalStatus totemInstruction_SetB2u(totemInstruction *instruction, totemOperandXu bx);
    totemEvalStatus totemInstruction_SetA3(totemInstruction *instruction, totemOperandX ax);
    totemEvalStatus totemInstruction_SetA3u(totemInstruction *instruction, totemOperandXu ax);
	totemEvalStatus totemInstruction_SetC1(totemInstruction *instruction, totemOperandX cx);
	totemEvalStatus totemInstruction_SetC1u(totemInstruction *instruction, totemOperandXu cx);

#define EMIT_PARAMS_A		totemOperandPrototype* a
#define EMIT_PARAMS_A3		totemOperandX a3
#define EMIT_PARAMS_A3u		totemOperandXu a3u
#define EMIT_PARAMS_Ab		totemOperandPrototype* a, totemOperandPrototype* b
#define EMIT_PARAMS_Ab2		totemOperandPrototype* a, totemOperandX b2
#define EMIT_PARAMS_Ab2u	totemOperandPrototype* a, totemOperandXu b2u
#define EMIT_PARAMS_Ab2ug	totemOperandPrototype* a, totemOperandXu b2ug
#define EMIT_PARAMS_Abc		totemOperandPrototype* a, totemOperandPrototype* b, totemOperandPrototype* c
#define EMIT_PARAMS_Abc1	totemOperandPrototype* a, totemOperandPrototype* b, totemOperandX c1
#define EMIT_PARAMS_Abc1u	totemOperandPrototype* a, totemOperandPrototype* b, totemOperandXu c1u
#define EMIT_PARAMS_Ab1c1	totemOperandPrototype* a, totemOperandX b1, totemOperandX c1

	totemEvalStatus totemInstruction_AsA(totemInstruction *ins, EMIT_PARAMS_A, totemOperationType op);
	totemEvalStatus totemInstruction_AsA3(totemInstruction* ins, EMIT_PARAMS_A3, totemOperationType op);
	totemEvalStatus totemInstruction_AsA3u(totemInstruction* ins, EMIT_PARAMS_A3u, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb(totemInstruction* ins, EMIT_PARAMS_Ab, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb2(totemInstruction* ins, EMIT_PARAMS_Ab2, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb2u(totemInstruction* ins, EMIT_PARAMS_Ab2u, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb2ug(totemInstruction* ins, EMIT_PARAMS_Ab2ug, totemOperationType op);
	totemEvalStatus totemInstruction_AsAbc(totemInstruction* ins, EMIT_PARAMS_Abc, totemOperationType op);
	totemEvalStatus totemInstruction_AsAbc1(totemInstruction* ins, EMIT_PARAMS_Abc1, totemOperationType op);
	totemEvalStatus totemInstruction_AsAbc1u(totemInstruction* ins, EMIT_PARAMS_Abc1u, totemOperationType op);
	totemEvalStatus totemInstruction_AsAb1c1(totemInstruction* ins, EMIT_PARAMS_Ab1c1, totemOperationType op);

#define EMIT(op, type, behaviour) \
	totemEvalStatus totemInstruction_As_ ## op ( totemInstruction *ins, EMIT_PARAMS_ ## type ); \
	totemEvalStatus totemBuildPrototype_AddInstruction_ ## op(totemBuildPrototype *prototype, EMIT_PARAMS_ ## type);
	TOTEM_EMIT_OPCODES(EMIT)
#undef EMIT

#define EMIT(ins) \
    totemEvalStatus totemBuildPrototype_Eval ## ins ## Instruction ( totemBuildPrototype *prototype, EMIT_PARAMS_ ## ins , totemOperationType op );
    TOTEM_EMIT_INSTRUCTION_TYPES(EMIT)
#undef EMIT

#define TOTEM_EVAL_CHECKRETURN(x) { totemEvalStatus _status = (x); if(_status != totemEvalStatus_Success) return totemEvalStatus_Break(_status); }
    
#ifdef __cplusplus
}
#endif

#endif
