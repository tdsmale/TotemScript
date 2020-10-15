//
//  eval.c
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>
#include <math.h>

totemEvalStatus totemEvalStatus_Break(totemEvalStatus status)
{
    return status;
}

totemEvalStatus totemBuildPrototype_Break(totemBuildPrototype *build, totemEvalStatus status, totemBufferPositionInfo *errorAt)
{
    build->ErrorAt = errorAt;
    return totemEvalStatus_Break(status);
}

void totemBuildPrototype_Init(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Init(&build->GlobalRegisters, totemOperandType_GlobalRegister);
    totemHashMap_Init(&build->FunctionLookup);
	totemScriptFunctionPrototypeBuffer_Init(&build->Functions);
    totemInstructionBuffer_Init(&build->Instructions);
    totemStringBuffer_Init(&build->NativeFunctionNames);
	totemOperandPrototypeBuffer_Init(&build->FunctionArguments);
	totemOperandPrototypeBuffer_Init(&build->RecycledRegisters);
	totemOperandXUnsignedBuffer_Init(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentAnonFunc = 0;
}

void totemBuildPrototype_Reset(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
	totemHashMap_Reset(&build->FunctionLookup);
	totemScriptFunctionPrototypeBuffer_Reset(&build->Functions);
	totemInstructionBuffer_Reset(&build->Instructions);
	totemStringBuffer_Reset(&build->NativeFunctionNames);
	totemOperandPrototypeBuffer_Reset(&build->FunctionArguments);
	totemOperandPrototypeBuffer_Reset(&build->RecycledRegisters);
	totemOperandXUnsignedBuffer_Reset(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentAnonFunc = 0;
}

void totemBuildPrototype_Cleanup(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Cleanup(&build->GlobalRegisters);
	totemHashMap_Cleanup(&build->FunctionLookup);
	totemScriptFunctionPrototypeBuffer_Cleanup(&build->Functions);
	totemInstructionBuffer_Cleanup(&build->Instructions);
	totemStringBuffer_Cleanup(&build->NativeFunctionNames);
	totemOperandPrototypeBuffer_Cleanup(&build->FunctionArguments);
	totemOperandPrototypeBuffer_Cleanup(&build->RecycledRegisters);
	totemOperandXUnsignedBuffer_Cleanup(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentAnonFunc = 0;
}

totemEvalStatus totemBuildPrototype_EnterLocalScope(totemBuildPrototype *build)
{
    totemRegisterListPrototype *list = totemBuildPrototype_GetLocalScope(build);
    return totemRegisterListPrototype_EnterScope(list);
}

totemEvalStatus totemBuildPrototype_ExitLocalScope(totemBuildPrototype *build)
{
    totemRegisterListPrototype *list = totemBuildPrototype_GetLocalScope(build);
    if (list->Scope)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateGlobalCache(build, list, list->Scope, totemBool_False));
        return totemRegisterListPrototype_ExitScope(list);
    }
    else
    {
        return totemEvalStatus_Success;
    }
}

totemEvalStatus totemBuildPrototype_EvalFunctionCall(totemBuildPrototype *build, totemExpressionPrototype *parametersStart, totemOperandPrototype *dst, totemOperandPrototype *src)
{
    // we need to eval the func-args before we call the function
    // 1. count num args
	totemOperandXUnsigned numArgs = 0;
    for (totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
		if (numArgs++ >= TOTEM_MAX_LOCAL_REGISTERS)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_TooManyFunctionArguments, &parameter->Position);
        }
    }

    // 2. alloc mem
	totemOperandPrototypeBuffer_Reset(&build->FunctionArguments);
	totemOperandPrototype *funcArgs = totemOperandPrototypeBuffer_Secure(&build->FunctionArguments, numArgs);
    if (!funcArgs)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    // 3. eval args
    totemOperandXUnsigned currentArg = 0;
    for (totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandPrototype *operand = &funcArgs[currentArg];
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, NULL, operand));
        currentArg++;
    }
    
    // start function call
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);

	currentArg = 0;
	totemOperandXUnsigned numArgsLeft = numArgs;
	totemBool finalInvoke = totemBool_True;
    
	if (numArgsLeft == 2)
	{
		totemOperandPrototype *operand = totemOperandPrototypeBuffer_At(&build->FunctionArguments, 0);
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, src, operand, operand + 1, totemOperationType_PreInvoke_2Args));
		numArgsLeft = 0;
	}
	else if (numArgsLeft == 1)
	{
		totemOperandPrototype *operand = totemOperandPrototypeBuffer_At(&build->FunctionArguments, 0);
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, src, operand, dst, totemOperationType_FullInvoke_1Arg));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_True));
		numArgsLeft = 0;
		finalInvoke = totemBool_False;
	}
	else if (numArgsLeft == 0)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbInstruction(build, src, dst, totemOperationType_FullInvoke_NoArgs));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_True));
		finalInvoke = totemBool_False;
	}
	else
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxuInstruction(build, src, numArgs, totemOperationType_PreInvoke));
	}
    
    // function arguments
	while(numArgsLeft)
	{
		totemOperandPrototype *operand = totemOperandPrototypeBuffer_At(&build->FunctionArguments, currentArg);

		if (numArgsLeft > 3)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, operand, operand + 1, operand + 2, totemOperationType_FunctionArg_3));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 1));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 2));
			currentArg += 3;
			numArgsLeft -= 3;
		}
		else if (numArgsLeft == 3)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, operand, operand + 1, operand + 2, totemOperationType_FunctionArg_3));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 1));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 2));
			currentArg += 3;
			numArgsLeft -= 3;
		}
		else if (numArgsLeft == 2)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, operand, operand + 1, dst, totemOperationType_FunctionArg_2_Invoke));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_True));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 1));
			currentArg += 2;
			numArgsLeft -= 2;
			finalInvoke = totemBool_False;
		}
		else
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbInstruction(build, operand, dst, totemOperationType_FunctionArg_Invoke));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_True));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			currentArg++;
			numArgsLeft--;
			finalInvoke = totemBool_False;
		}
	}
    
    // finish function call
	if (finalInvoke)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAInstruction(build, dst, totemOperationType_Invoke));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_True));
	}
    
    return totemEvalStatus_Success;
}

totemRegisterListPrototype *totemBuildPrototype_GetRegisterList(totemBuildPrototype *build, totemOperandType scope)
{
    if(scope == totemOperandType_GlobalRegister)
    {
        return &build->GlobalRegisters;
    }
    
    return totemBuildPrototype_GetLocalScope(build);
}

totemRegisterListPrototype *totemBuildPrototype_GetLocalScope(totemBuildPrototype *build)
{
    if(build->LocalRegisters)
    {
        return build->LocalRegisters;
    }
    
    return &build->GlobalRegisters;
}

totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandPrototype *opOut)
{
    totemEvalStatus status = totemEvalStatus_Success;
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    switch(preferredScope)
    {
        case totemOperandType_LocalRegister:
            status = totemRegisterListPrototype_AddRegister(localScope, opOut);
            break;
            
        case totemOperandType_GlobalRegister:
        {
            status = totemRegisterListPrototype_AddRegister(&build->GlobalRegisters, opOut);
            if(status == totemEvalStatus_Success)
            {
                status = totemBuildPrototype_GlobalCacheCheck(build, opOut, NULL);
            }
            
            break;
        }
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_RecycleRegister(totemBuildPrototype *build, totemOperandPrototype *op)
{
    totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, op->RegisterScopeType);
    
    totemRegisterPrototypeFlag flags;
    if(totemRegisterListPrototype_GetRegisterFlags(scope, op->RegisterIndex, &flags))
    {
        // free if temp
		if (TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsUsed | totemRegisterPrototypeFlag_IsTemporary))
        {
			TOTEM_UNSETBITS(flags, totemRegisterPrototypeFlag_IsUsed);
			totemRegisterListPrototype_SetRegisterFlags(scope, op->RegisterIndex, flags);

            if (!totemOperandPrototypeBuffer_Insert(&build->RecycledRegisters, op, 1))
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_FlushRecycledRegisters(totemBuildPrototype *build)
{
	size_t max = totemOperandPrototypeBuffer_Num(&build->RecycledRegisters);
	totemOperandPrototype *ops = totemOperandPrototypeBuffer_Bottom(&build->RecycledRegisters);
    
    for (size_t i = 0; i < max; i++)
    {
        totemOperandPrototype *op = &ops[i];
        totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, op->RegisterScopeType);
        
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(scope, op));
    }
    
	totemOperandPrototypeBuffer_Reset(&build->RecycledRegisters);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalBoolean(totemBuildPrototype *build, totemBool val, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddBoolean(&build->GlobalRegisters, val, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalNull(totemBuildPrototype *build, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNull(&build->GlobalRegisters, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalInt(totemBuildPrototype *build, totemInt number, totemOperandPrototype *operand, totemOperandPrototype *hint)
{
    char buffer[256];
	int result = totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "%"TOTEM_INT_PRINTF, number);
    
    if (result < 0 || result >= TOTEM_ARRAY_SIZE(buffer))
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    totemString string;
    string.Length = result;
    string.Value = buffer;
    
    return totemBuildPrototype_EvalNumber(build, &string, operand, hint);
}

totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemString *number, totemOperandPrototype *operand, totemOperandPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNumberConstant(&build->GlobalRegisters, number, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemString *buffer, totemOperandPrototype *operand, totemOperandPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddStringConstant(&build->GlobalRegisters, buffer, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalIdentifier(totemBuildPrototype *build, totemString *name, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    if (totemRegisterListPrototype_GetIdentifier(localScope, name, op, totemBool_False))
    {
        return totemEvalStatus_Success;
    }
    
    if (totemRegisterListPrototype_GetIdentifier(&build->GlobalRegisters, name, op, totemBool_False))
    {
        return totemEvalStatus_Success;
    }
    
    // add native function name
    if(totemStringBuffer_Num(&build->NativeFunctionNames) >= TOTEM_MAX_NATIVEFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyNativeFunctions);
    }
    
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Native;
	ptr.Address = (totemOperandXUnsigned)totemStringBuffer_Num(&build->NativeFunctionNames);
    
	if (totemStringBuffer_Insert(&build->NativeFunctionNames, name, 1) == NULL)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    return totemBuildPrototype_EvalFunctionPointer(build, name, &ptr, op, hint);
}

totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemString *val, totemFunctionPointerPrototype *value, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFunctionPointer(&build->GlobalRegisters, val, value, op);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Instance;
    
    if (build->CurrentAnonFunc < totemOperandXUnsignedBuffer_Num(&build->AnonFunctions))
    {
		totemOperandXUnsigned *addr = totemOperandXUnsignedBuffer_At(&build->AnonFunctions, build->CurrentAnonFunc);
        ptr.Address = *addr;
    }
    else
    {
        // increment script function count
		if (totemScriptFunctionPrototypeBuffer_Num(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_TooManyScriptFunctions, &func->Position);
        }
        
		totemOperandXUnsigned funcIndex = (totemOperandXUnsigned)totemScriptFunctionPrototypeBuffer_Num(&build->Functions);
        totemScriptFunctionPrototype *functionPrototype;
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
        
        ptr.Address = funcIndex;
        
		if (!totemOperandXUnsignedBuffer_Insert(&build->AnonFunctions, &funcIndex, 1))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    // enqueue function declaration to be evaluated later
    func->Next = NULL;
    if (build->AnonymousFunctionHead)
    {
        build->AnonymousFunctionTail->Next = func;
    }
    else
    {
        build->AnonymousFunctionHead = func;
    }
    
    build->AnonymousFunctionTail = func;
    
    build->CurrentAnonFunc++;
    return totemBuildPrototype_EvalFunctionPointer(build, NULL, &ptr, op, hint);
}

totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandPrototype *operand, totemOperandPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddType(&build->GlobalRegisters, type, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype)
{
    totemBuildPrototype_Reset(build);
    
    // global function is always at index 0
    totemScriptFunctionPrototype *globalFunction;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &globalFunction));
    
    build->LocalRegisters = &build->GlobalRegisters;
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->Flags = totemBuildPrototypeFlag_None;
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    
    // eval script function names first, and create pointers for each
	// this makes them the most accessible global vars, least likely to need MoveToLocal instructions
    for (totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch (block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                // named function cannot already exist
				if (totemHashMap_Find(&build->FunctionLookup, block->Ptr.FuncDec->Identifier->Value, block->Ptr.FuncDec->Identifier->Length) != NULL)
                {
					return totemBuildPrototype_Break(build, totemEvalStatus_ScriptFunctionAlreadyDefined, &block->Ptr.FuncDec->Position);
                }
                
                // secure named function
				size_t functionIndex = totemScriptFunctionPrototypeBuffer_Num(&build->Functions);
                totemScriptFunctionPrototype *functionPrototype = NULL;
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
				functionPrototype->Name = *block->Ptr.FuncDec->Identifier;
                
				if (!totemHashMap_Insert(&build->FunctionLookup, block->Ptr.FuncDec->Identifier->Value, block->Ptr.FuncDec->Identifier->Length, functionIndex))
                {
					return totemBuildPrototype_Break(build, totemEvalStatus_OutOfMemory, &block->Ptr.FuncDec->Position);
                }
                
                // setup the function pointer
                totemOperandPrototype dummy;
                totemFunctionPointerPrototype ptr;
                ptr.Type = totemFunctionType_Instance;
                ptr.Address = (totemOperandXUnsigned)functionIndex;
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionPointer(build, block->Ptr.FuncDec->Identifier, &ptr, &dummy, NULL));
                break;
            }
                
            default:
                break;
        }
    }
    
    // now eval global values
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
				TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(block->Ptr.Statement, build));
                break;
                
            default:
                break;
        }
    }
    
    // eval constant values from functions into global scope
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
				for (totemStatementPrototype *stmt = block->Ptr.FuncDec->StatementsStart; stmt != NULL; stmt = stmt->Next)
                {
                    TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
                }
                
                break;
            }
                
            default:
                break;
        }
    }
    
    // eval anonymous function values
    while (build->AnonymousFunctionHead)
    {
        totemFunctionDeclarationPrototype *func = build->AnonymousFunctionHead;
        build->AnonymousFunctionHead = build->AnonymousFunctionHead->Next;
        
        for (totemStatementPrototype *stmt = func->StatementsStart; stmt != NULL; stmt = stmt->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
        }
    }
    
    build->CurrentAnonFunc = 0;
    
    // eval "global" function instructions first
    totemRegisterListPrototype localRegisters;
    totemRegisterListPrototype_Init(&localRegisters, totemOperandType_LocalRegister);
    build->LocalRegisters = &localRegisters;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->Flags = totemBuildPrototypeFlag_EvalGlobalCaches;
    globalFunction->InstructionsStart = 0;
    build->LocalVariableScope = &build->GlobalRegisters;
    
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
            {
				TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(block->Ptr.Statement, build));
                break;
            }
                
            default:
                break;
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalImplicitReturn(build));
    globalFunction->RegistersNeeded = (uint8_t)totemRegisterPrototypeBuffer_Num(&localRegisters.Registers);
	globalFunction->NumInstructions = totemInstructionBuffer_Num(&build->Instructions);
    
    // now eval all other function instructions
    totemOperandXUnsigned funcIndex = 1;
    build->LocalVariableScope = build->LocalRegisters;
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                totemScriptFunctionPrototype *functionPrototype = totemScriptFunctionPrototypeBuffer_At(&build->Functions, funcIndex);
				TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(block->Ptr.FuncDec, build, functionPrototype));
                
                funcIndex++;
                break;
            }
                
            default:
                break;
        }
    }
    
    // eval anonymous functions
    while (build->AnonymousFunctionHead)
    {
        totemFunctionDeclarationPrototype *func = build->AnonymousFunctionHead;
        build->AnonymousFunctionHead = build->AnonymousFunctionHead->Next;
        
		totemScriptFunctionPrototype *functionPrototype = totemScriptFunctionPrototypeBuffer_At(&build->Functions, funcIndex);
        TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(func, build, functionPrototype));
        
        funcIndex++;
    }
    
    /*
     FILE *file = fopen("ins.txt", "w");
     totemInstruction_PrintList(stdout, totemInstructionBuffer_Bottom(&build->Instructions), totemInstructionBuffer_Num(&build->Instructions));
     fclose(file);
     */
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut)
{
	if (totemScriptFunctionPrototypeBuffer_Num(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
    }
    
	*functionOut = totemScriptFunctionPrototypeBuffer_Secure(&build->Functions, 1);
    if(*functionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *funcPrototype)
{
    funcPrototype->InstructionsStart = totemInstructionBuffer_Num(&build->Instructions);
    totemRegisterListPrototype_Reset(build->LocalRegisters);
    totemOperandPrototypeBuffer_Reset(&build->RecycledRegisters);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    
    // parameters
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemVariablePrototype_Eval(parameter, build, &dummy));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    totemEvalStatus status = totemBuildPrototype_EvalImplicitReturn(build);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
    funcPrototype->RegistersNeeded = (uint8_t)totemRegisterPrototypeBuffer_Num(&build->LocalRegisters->Registers);
	funcPrototype->NumInstructions = totemInstructionBuffer_Num(&build->Instructions) - funcPrototype->InstructionsStart;
    
    return status;
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    totemOperandPrototype result;
	TOTEM_ZERO(&result);
    
    switch(statement->Type)
    {
        case totemStatementType_DoWhileLoop:
			TOTEM_EVAL_CHECKRETURN(totemDoWhileLoopPrototype_Eval(statement->Ptr.DoWhileLoop, build));
            break;
            
        case totemStatementType_ForLoop:
			TOTEM_EVAL_CHECKRETURN(totemForLoopPrototype_Eval(statement->Ptr.ForLoop, build));
            break;
            
        case totemStatementType_IfBlock:
			TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_Eval(statement->Ptr.IfBlock, build));
            break;
            
        case totemStatementType_WhileLoop:
			TOTEM_EVAL_CHECKRETURN(totemWhileLoopPrototype_Eval(statement->Ptr.WhileLoop, build));
            break;
            
        case totemStatementType_Simple:
			TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Ptr.Simple, build, NULL, &result));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
            break;
            
        case totemStatementType_Return:
			TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Ptr.Return, build, NULL, &result));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, &result));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
            break;

		case totemStatementType_Yield:
		{
			TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Ptr.Return, build, NULL, &result));
			totemScriptFunctionPrototype *func = totemScriptFunctionPrototypeBuffer_Top(&build->Functions);
			TOTEM_DEBUG_ASSERT(func);
			size_t offset = totemInstructionBuffer_Num(&build->Instructions) - func->InstructionsStart;
			TOTEM_DEBUG_ASSERT(offset < TOTEM_B2U_MAX);
			totemOperandXUnsigned xu = (totemOperandXUnsigned)offset;
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxuInstruction(build, &result, xu, totemOperationType_Yield));
			break;
		}
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_FlushRecycledRegisters(build));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalArrayAccessEnd(totemBuildPrototype *build, totemOperandPrototype *lValue, totemOperandPrototype *lValueSrc, totemOperandPrototype *arrIndex)
{
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, lValueSrc, arrIndex, lValue, totemOperationType_ComplexSet));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, arrIndex));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, lValueSrc));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalMove(totemBuildPrototype *build, totemOperandPrototype *lValue, totemOperandPrototype *rValue)
{
	// determine which move instruction to encode based on rValue

	totemPublicDataType rValueType = totemPublicDataType_Max;
	totemRegisterPrototypeValue rValueVal;
	TOTEM_ZERO(&rValueVal);

	totemRegisterListPrototype *rValueScope = totemBuildPrototype_GetRegisterList(build, rValue->RegisterScopeType);
	totemRegisterListPrototype_GetRegisterType(rValueScope, rValue->RegisterIndex, &rValueType);
	totemRegisterListPrototype_GetRegisterValue(rValueScope, rValue->RegisterIndex, &rValueVal);

	switch (rValueType)
	{
		case totemPublicDataType_Null:
			return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveNull);

		case totemPublicDataType_Boolean:
			if (rValueVal.Boolean)
			{
				return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTrue);
			}
			else
			{
				return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveFalse);
			}

		case totemPublicDataType_Type:
			switch (rValueVal.Type)
			{
				case totemPublicDataType_Null:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeNull);

				case totemPublicDataType_Int:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeInt);

				case totemPublicDataType_Float:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeFloat);

				case totemPublicDataType_String:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeString);

				case totemPublicDataType_Array:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeArray);

				case totemPublicDataType_Type:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeType);

				case totemPublicDataType_Function:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeFunction);

				case totemPublicDataType_Coroutine:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeCoroutine);

				case totemPublicDataType_Object:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeObject);

				case totemPublicDataType_Userdata:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeUserdata);

				case totemPublicDataType_Boolean:
					return totemBuildPrototype_EvalAInstruction(build, lValue, totemOperationType_MoveTypeBoolean);
				
				default:
					break;
			}
			break;

		case totemPublicDataType_Int:
			if (rValueVal.Int < TOTEM_B2_MAX && rValueVal.Int > TOTEM_B2_MIN)
			{
				return totemBuildPrototype_EvalAbxInstruction(build, lValue, rValueVal.Int, totemOperationType_MoveInt);
			}
			break;

		default:
			break;
	}

	// degrade to generic move if we can't determine at compile time
	return totemBuildPrototype_EvalAbInstruction(build, lValue, rValue, totemOperationType_Move);
}

totemEvalStatus totemBuildPrototype_EvalCast(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperandPrototype *rgt)
{
	// determine which cast op to encode based on type

	totemRegisterListPrototype *rValueScope = totemBuildPrototype_GetRegisterList(build, rgt->RegisterScopeType);
	totemPublicDataType type = totemPublicDataType_Max;
	totemRegisterListPrototype_GetRegisterType(rValueScope, rgt->RegisterIndex, &type);
	if (type == totemPublicDataType_Type)
	{
		totemRegisterPrototypeValue rValueVal;
		TOTEM_ZERO(&rValueVal);
		totemRegisterListPrototype_GetRegisterValue(rValueScope, rgt->RegisterIndex, &rValueVal);
		type = rValueVal.Type;
	}

	switch (type)
	{
	case totemPublicDataType_Null:
	case totemPublicDataType_Userdata:
	{
		totemOperandPrototype nullReg;
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNull(build, &nullReg, NULL));
		return totemBuildPrototype_EvalMove(build, dst, &nullReg);
	}

	case totemPublicDataType_Int:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToInt);

	case totemPublicDataType_Float:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToFloat);

	case totemPublicDataType_String:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToString);

	case totemPublicDataType_Array:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToArray);

	case totemPublicDataType_Type:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToType);

	case totemPublicDataType_Function:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToFunction);

	case totemPublicDataType_Coroutine:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToCoroutine);

	case totemPublicDataType_Object:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToObject);

	case totemPublicDataType_Boolean:
		return totemBuildPrototype_EvalAbInstruction(build, dst, lft, totemOperationType_Cast_ToBoolean);

	default:
		return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, totemOperationType_Cast);
	}
}

totemEvalStatus totemBuildPrototype_EvalBinaryArith(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperandPrototype *rgt, totemOperationType base)
{
	// determine which binary arith op to encode based on lValue and rValue

	totemOperationType intInt = base + 1;
	totemOperationType intIntConst = base + 2;
	totemOperationType intConstIntConst = base + 3;
	totemOperationType floatFloat = base + 4;
	totemOperationType arrayArray = base + 5;
	totemOperationType stringString = base + 6;
	totemBool allowComplexTypes = base == totemOperationType_Add;

	totemRegisterListPrototype *lftList = totemBuildPrototype_GetRegisterList(build, lft->RegisterScopeType);
	totemRegisterListPrototype *rgtList = totemBuildPrototype_GetRegisterList(build, rgt->RegisterScopeType);

	totemPublicDataType lftType = totemPublicDataType_Max, rgtType = totemPublicDataType_Max;
	totemRegisterListPrototype_GetRegisterType(lftList, lft->RegisterIndex, &lftType);
	totemRegisterListPrototype_GetRegisterType(rgtList, rgt->RegisterIndex, &rgtType);

	totemRegisterPrototypeFlag lftFlags = totemRegisterPrototypeFlag_None, rgtFlags = totemRegisterPrototypeFlag_None;
	totemRegisterListPrototype_GetRegisterFlags(lftList, lft->RegisterIndex, &lftFlags);
	totemRegisterListPrototype_GetRegisterFlags(rgtList, rgt->RegisterIndex, &rgtFlags);

	totemOperandPrototype intType, floatType, arrayType, stringType;
	TOTEM_ZERO(&intType);
	TOTEM_ZERO(&floatType);
	TOTEM_ZERO(&arrayType);
	TOTEM_ZERO(&stringType);
	totemBuildPrototype_EvalType(build, totemPublicDataType_Int, &intType, NULL);
	totemBuildPrototype_EvalType(build, totemPublicDataType_Float, &floatType, NULL);
	totemBuildPrototype_EvalType(build, totemPublicDataType_Array, &arrayType, NULL);
	totemBuildPrototype_EvalType(build, totemPublicDataType_String, &stringType, NULL);

	if (lftType == totemPublicDataType_Int || rgtType == totemPublicDataType_Int)
	{
		if (lftType != totemPublicDataType_Int)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, lft, lft, &intType));
		}
		else if (rgtType != totemPublicDataType_Int)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, rgt, rgt, &intType));
		}

		totemRegisterPrototypeValue lftValue;
		totemRegisterPrototypeValue rgtValue;
		TOTEM_ZERO(&lftValue);
		TOTEM_ZERO(&rgtValue);

		totemRegisterListPrototype_GetRegisterValue(lftList, lft->RegisterIndex, &lftValue);
		totemRegisterListPrototype_GetRegisterValue(rgtList, rgt->RegisterIndex, &rgtValue);

		totemOperandXSigned lValueVal = lftValue.Int;
		totemOperandXSigned rValueVal = rgtValue.Int;

		totemBool lIsConst = TOTEM_HASBITS(lftFlags, totemRegisterPrototypeFlag_IsValue) && lValueVal <= TOTEM_C1_MAX && lValueVal >= TOTEM_C1_MIN;
		totemBool rIsConst = TOTEM_HASBITS(rgtFlags, totemRegisterPrototypeFlag_IsValue) && rValueVal <= TOTEM_C1_MAX && rValueVal >= TOTEM_C1_MIN;

		if (lIsConst && rIsConst)
		{
			return totemBuildPrototype_EvalAbxxcxxInstruction(build, dst, lValueVal, rValueVal, intConstIntConst);
		}
		else if (lIsConst)
		{
			return totemBuildPrototype_EvalAbcxxInstruction(build, dst, rgt, lValueVal, intIntConst);
		}
		else if (rIsConst)
		{
			return totemBuildPrototype_EvalAbcxxInstruction(build, dst, lft, rValueVal, intIntConst);
		}
		else
		{
			return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, intInt);
		}
	}
	else if (lftType == totemPublicDataType_Float || rgtType == totemPublicDataType_Float)
	{
		if (lftType != totemPublicDataType_Float)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, lft, lft, &floatType));
		}
		else if (rgtType != totemPublicDataType_Float)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, rgt, rgt, &floatType));
		}
		else
		{
			return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, floatFloat);
		}
	}
	else if (allowComplexTypes)
	{
		// these are non-commutative, so only attempt to cast the rValue (if neccessary)

		if (lftType == totemPublicDataType_Array)
		{
			if (rgtType != totemPublicDataType_Array)
			{
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, rgt, rgt, &arrayType));
			}

			return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, arrayArray);
		}
		else if (lftType == totemPublicDataType_String)
		{
			if (rgtType != totemPublicDataType_String)
			{
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, rgt, rgt, &stringType));
			}

			return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, stringString);
		}
	}

	// degrade to base, we don't know enough at compile-time to optimise this
	return totemBuildPrototype_EvalAbcInstruction(build, dst, lft, rgt, base);
}

totemEvalStatus totemBuildPrototype_EvalLengthOf(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *src)
{
	totemPublicDataType type = totemPublicDataType_Max;
	totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, src->RegisterScopeType);
	if (totemRegisterListPrototype_GetRegisterType(scope, src->RegisterIndex, &type) && type != totemPublicDataType_Max)
	{
		switch (type)
		{
			case totemPublicDataType_Array:
				return totemBuildPrototype_EvalAbInstruction(build, dst, src, totemOperationType_LengthOfArray);
				break;

			case totemPublicDataType_Object:
				return totemBuildPrototype_EvalAbInstruction(build, dst, src, totemOperationType_LengthOfObject);
				break;

			case totemPublicDataType_String:
				return totemBuildPrototype_EvalAbInstruction(build, dst, src, totemOperationType_LengthOfString);

			default:
			{
				totemOperandPrototype one;
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, 1, &one, dst));
				if (memcmp(dst, &one, sizeof(one)) != 0)
				{
					return totemBuildPrototype_EvalMove(build, dst, &one);
				}

				return totemEvalStatus_Success;
			}
		}
	}
	else
	{
		return totemBuildPrototype_EvalAbInstruction(build, dst, src, totemOperationType_LengthOf);
	}
}

totemEvalStatus totemBuildPrototype_EvalCloneOf(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *src)
{
	totemPublicDataType type = totemPublicDataType_Max;
	totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, src->RegisterScopeType);
	if (totemRegisterListPrototype_GetRegisterType(scope, src->RegisterIndex, &type) && type != totemPublicDataType_Max)
	{
		if (totemPublicDataType_IsGarbageCollected(type))
		{
			return totemBuildPrototype_EvalAbInstruction(build, dst, src, totemOperationType_CloneOfGCObject);
		}
		else
		{
			return totemBuildPrototype_EvalMove(build, dst, src);
		}
	}
	else
	{
		return totemBuildPrototype_EvalAbInstruction(build, dst, src, totemOperationType_CloneOf);
	}
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandPrototype *lValueHint, totemOperandPrototype *result)
{
    totemOperandPrototype lValueSrc;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValue.Argument, build, lValueHint, &lValueSrc));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValue.Expression, build, lValueHint, &lValueSrc));
            break;
    }
    
    totemBool mutatedLValueRegisterUnary = totemBool_False;
    totemBool mutatedLValueRegisterBinary = totemBool_False;
	totemBool lValueRegisterIsArrayMember = totemBool_False;
	totemBool lValueRegisterIsInvocation = totemBool_False;
	size_t numlValueAccesses = 0;
	totemRegisterPrototypeFlag lValueSrcFlags = totemRegisterPrototypeFlag_None;
    totemRegisterListPrototype *lValueSrcScope = totemBuildPrototype_GetRegisterList(build, lValueSrc.RegisterScopeType);
    totemRegisterListPrototype_GetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, &lValueSrcFlags);
    
    // check the operators used against this lvalue to see if it gets mutated
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        if(op->Type == totemPreUnaryOperatorType_Dec || op->Type == totemPreUnaryOperatorType_Inc)
        {
            if (TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsConst))
            {
                return totemBuildPrototype_Break(build, totemEvalStatus_AssignmentLValueNotMutable, &expression->Position);
            }
            
            mutatedLValueRegisterUnary = totemBool_True;
            break;
        }
    }
    
    for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        if(op->Type == totemPostUnaryOperatorType_Dec || op->Type == totemPostUnaryOperatorType_Inc)
        {
            mutatedLValueRegisterUnary = totemBool_True;
        }
        
        if(op->Type == totemPostUnaryOperatorType_ArrayAccess)
        {
            numlValueAccesses++;
            lValueRegisterIsArrayMember = totemBool_True;
        }
        
        if (op->Type == totemPostUnaryOperatorType_Invocation)
        {
            numlValueAccesses++;
            lValueRegisterIsInvocation = totemBool_True;
        }
    }
    
    switch(expression->BinaryOperator)
    {
        case totemBinaryOperatorType_Assign:
        case totemBinaryOperatorType_DivideAssign:
        case totemBinaryOperatorType_MinusAssign:
        case totemBinaryOperatorType_MultiplyAssign:
        case totemBinaryOperatorType_PlusAssign:
        case totemBinaryOperatorType_LeftShift:
		case totemBinaryOperatorType_RightShift:
            mutatedLValueRegisterBinary = totemBool_True;
            break;
            
        default:
            break;
    }
    
    if(mutatedLValueRegisterBinary | mutatedLValueRegisterUnary)
    {
        // if already assigned, and is const, throw an error
        if (TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsAssigned | totemRegisterPrototypeFlag_IsConst) && !lValueRegisterIsInvocation)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_AssignmentLValueNotMutable, &expression->Position);
        }
        
        // assignment expressions can only happen on mutable lValues (variables)
        if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsValue) && !lValueRegisterIsArrayMember)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_AssignmentLValueNotMutable, &expression->Position);
        }
    }
    
    // when lValue is an array member or invocation, we must retrieve it first and place it in a register before any other op
    totemOperandPrototype lValue = lValueSrc;
    totemOperandPrototype arrIndex;
    totemRegisterPrototypeFlag lValueFlags = lValueSrcFlags;
	totemRegisterListPrototype *lValueScope = lValueSrcScope;
    
    if(lValueRegisterIsArrayMember || lValueRegisterIsInvocation)
    {
        size_t accessesPerformed = 0;
        
        for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
        {
            switch (op->Type)
            {
                case totemPostUnaryOperatorType_ArrayAccess:
                    accessesPerformed++;
                    
                    // we don't need to hang onto all the array srcs, just the last one
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValueSrc));
                    
                    // lvalue is now the src
					lValueSrc = lValue;
                    lValueSrcFlags = lValueFlags;
                    lValueSrcScope = lValueScope;
                    
                    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->Parameters, build, NULL, &arrIndex));
                    
                    totemBool performAccess = totemBool_True;
                    
                    if (numlValueAccesses == accessesPerformed)
                    {
                        // if we're moving to this location, we can skip this array_get - we'll be grabbing the new value and performing a direct array_set further below
                        if (expression->BinaryOperator == totemBinaryOperatorType_Assign)
                        {
                            performAccess = totemBool_False;
                        }
                        
                        // if caller has given us a hint, we need to use it for the last array access
                        if (lValueHint)
                        {
							lValue = *lValueHint;
                        }
                        else
                        {
                            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                        }
                    }
                    else
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                    }
                    
                    if (performAccess)
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValueSrc, &arrIndex, totemOperationType_ComplexGet));
                    }
                    
                    lValueRegisterIsArrayMember = totemBool_True;
                    lValueRegisterIsInvocation = totemBool_False;
                    lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
                    totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
                    
                    break;
                    
                case totemPostUnaryOperatorType_Invocation:
                    accessesPerformed++;
                    
                    if (TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsValue))
                    {
                        totemPublicDataType dataType = 0;
                        if (totemRegisterListPrototype_GetRegisterType(lValueScope, lValue.RegisterIndex, &dataType))
                        {
                            if (dataType != totemPublicDataType_Function)
                            {
                                return totemBuildPrototype_Break(build, totemEvalStatus_InvalidDataType, &expression->Position);
                            }
                        }
                    }
                    
                    // retain pointer address
					totemOperandPrototype pointer = lValue;
                    
                    // replace lvalue, we're done with it
                    if (lValueRegisterIsArrayMember && mutatedLValueRegisterUnary)
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
                    }
                    
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
                    
                    // if caller has given us a hint, we need to use it for the last array access
                    if (numlValueAccesses == accessesPerformed && lValueHint)
                    {
						lValue = *lValueHint;
                    }
                    else
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                    }
                    
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionCall(build, op->Parameters, &lValue, &pointer));
                    
                    lValueRegisterIsArrayMember = totemBool_False;
                    lValueRegisterIsInvocation = totemBool_True;
                    lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
                    totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
                    break;
                    
                default:
                    break;
            }
        }
    }

	figureOutHowComplexTypesRelateToTheseBullshitUnaryOps();
    
    totemOperandPrototype preUnaryRegister;
    
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
			case totemPreUnaryOperatorType_LengthOf:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalLengthOf(build, &lValue, &lValue));
				lValueRegisterIsArrayMember = totemBool_False;
				lValueRegisterIsInvocation = totemBool_True;
				lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
				totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
				break;

			case totemPreUnaryOperatorType_CloneOf:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCloneOf(build, &lValue, &lValue));
				lValueRegisterIsArrayMember = totemBool_False;
				lValueRegisterIsInvocation = totemBool_True;
				lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
				totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
				break;

            case totemPreUnaryOperatorType_Dec:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, 1, &preUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPreUnaryOperatorType_Inc:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, 1, &preUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_LogicalNegate));
				notArrayMember();
                break;
                
            case totemPreUnaryOperatorType_Negate:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalUnaryArith(build, &lValue, &lValue, totemOperationType_Negate));
				notArrayMember();
                break;
                
            case totemPreUnaryOperatorType_None:
                break;
        }
    }
    
	totemOperandPrototype postUnaryRegister;
    
    for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
            case totemPostUnaryOperatorType_Dec:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, 1, &postUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPostUnaryOperatorType_Inc:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, 1, &postUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
            case totemPostUnaryOperatorType_Invocation:
                // already done above
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    totemOperandPrototype rValue;
	TOTEM_ZERO(&rValue);
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemBool recycleRValue = totemBool_False;
        totemBool recycleLValue = totemBool_False;
        
        if(expression->BinaryOperator == totemBinaryOperatorType_Assign)
        {
            if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsTemporary))
            {
                // result of rValue expression can now be stored directly in lValue
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &lValue));
            }
            else
            {
                // otherwise we need the move op
                totemOperandPrototype *lValueToUse = NULL;
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsVariable))
                {
                    lValueToUse = &lValue;
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, lValueToUse, &rValue));
                
                // if expression wasn't eval'd directly to lValue, perform move
                if(lValueToUse == NULL || memcmp(lValueToUse, &rValue, sizeof(totemOperandPrototype)) != 0)
                {
                    recycleRValue = totemBool_True;
                    
                    totemRegisterPrototypeFlag rValueFlags;
                    totemRegisterListPrototype *rValueScope = totemBuildPrototype_GetRegisterList(build, rValue.RegisterScopeType);
                    totemRegisterListPrototype_GetRegisterFlags(rValueScope, rValue.RegisterIndex, &rValueFlags);
                    
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalMove(build, &lValue, &rValue));
                }
                
				*result = lValue;
            }
        }
        else
        {
            recycleRValue = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &rValue));
            
            switch(expression->BinaryOperator)
            {
                case totemBinaryOperatorType_Plus:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_Add));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_PlusAssign:
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &rValue, totemOperationType_Add));
					*result = lValue;
                    break;
                    
                case totemBinaryOperatorType_Minus:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_Subtract));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MinusAssign:
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &rValue, totemOperationType_Subtract));
					*result = lValue;
                    break;
                    
                case totemBinaryOperatorType_Multiply:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_Multiply));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MultiplyAssign:
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &rValue, totemOperationType_Multiply));
					*result = lValue;
                    break;
                    
                case totemBinaryOperatorType_Divide:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_Divide));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_DivideAssign:
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &rValue, totemOperationType_Divide));
					*result = lValue;
                    break;
                    
                case totemBinaryOperatorType_Assign:
                    // already dealt with
                    break;
                    
                case totemBinaryOperatorType_Equals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Equals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_NotEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_NotEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MoreThan:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_MoreThan));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LessThan:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_LessThan));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MoreThanEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_MoreThanEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LessThanEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_LessThanEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LogicalAnd:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_LogicalAnd));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LogicalOr:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_LogicalOr));
                    recycleLValue = totemBool_True;
                    break;
                    
				case totemBinaryOperatorType_IsType:
				{
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));

					totemRegisterListPrototype *rValueScope = totemBuildPrototype_GetRegisterList(build, rValue.RegisterScopeType);
					if (TOTEM_HASBITS(rValueFlags, totemRegisterPrototypeFlag_HasType))
					{
						totemPublicDataType rValueType = totemPublicDataType_Max;
						totemRegisterListPrototype_GetRegisterType(rValueScope, rValue.RegisterIndex, &rValueType);

						if (rValueType == totemPublicDataType_Type && TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_HasType))
						{
							// determine at compile-time whether types match

							totemPublicDataType lValueType = totemPublicDataType_Max;
							totemRegisterListPrototype_GetRegisterType(lValueScope, lValue.RegisterIndex, &lValueType);

							TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBoolean(build, lValueType == rValueType, result, NULL));
						}
						else
						{
							totemRegisterPrototypeValue rValueVal;
							TOTEM_ZERO(&rValueVal);
							totemRegisterListPrototype_GetRegisterValue(rValueScope, rValue.RegisterIndex, &rValueVal);

							TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbc1uInstruction(build, result, &lValue, (totemOperandXUnsigned)rValueVal.Type, totemOperationType_IsType));
						}
					}
					else
					{
						TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Is));
					}
					recycleLValue = totemBool_True;
					break;
				}
                    
				case totemBinaryOperatorType_AsType:
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalCast(build, result, &lValue, &rValue));
					recycleLValue = totemBool_True;
					break;
                    
                case totemBinaryOperatorType_None:
                    break;
            }
        }
        
        if(recycleRValue)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &rValue));
        }
        
        if(recycleLValue)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
        }
    }
    else
    {
        // no binary operation - result is lValue
		*result = lValue;
    }
    
    if (mutatedLValueRegisterBinary | mutatedLValueRegisterUnary)
    {
        // this lvalue is now assigned
        totemRegisterListPrototype_SetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
    }
    
    // finish the array-access if the value was mutated
    if (lValueRegisterIsArrayMember && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary))
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_EvalHint(totemBuildPrototype *build, totemOperandPrototype *value, totemOperandPrototype *hint)
{
    if (hint)
    {
		*value = *hint;
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, value));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemNewObjectPrototype_Eval(totemNewObjectPrototype *obj, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value)
{
	TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, value, hint));

	if (obj->Num <= TOTEM_B2U_MAX)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxuInstruction(build, value, (totemOperandXUnsigned)obj->Num, totemOperationType_NewObjectWithLength));
	}
	else
	{
		totemOperandPrototype size;
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, obj->Num, &size, NULL));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbInstruction(build, value, &size, totemOperationType_NewObject));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &size));
	}
    
    while (obj->Keys)
    {
        totemOperandPrototype key, val;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(obj->Keys, build, NULL, &key));
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(obj->Values, build, NULL, &val));
        
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &key, &val, totemOperationType_ComplexSet));
        
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &key));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &val));
        
        obj->Keys = obj->Keys->Next;
        obj->Values = obj->Values->Next;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandPrototype *hint,  totemOperandPrototype *value)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
			return totemVariablePrototype_Eval(argument->Value.Variable, build, value);
            
        case totemArgumentType_String:
			return totemBuildPrototype_EvalString(build, argument->Value.String, value, hint);
            
        case totemArgumentType_Null:
            return totemBuildPrototype_EvalNull(build, value, hint);
            
        case totemArgumentType_Boolean:
			return totemBuildPrototype_EvalBoolean(build, argument->Value.Boolean, value, hint);
            
        case totemArgumentType_Number:
			return totemBuildPrototype_EvalNumber(build, argument->Value.Number, value, hint);
            
        case totemArgumentType_Type:
			return totemBuildPrototype_EvalType(build, argument->Value.DataType, value, hint);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, argument->Value.FunctionDeclaration, value, hint);
            
        case totemArgumentType_Identifier:
			return totemBuildPrototype_EvalIdentifier(build, argument->Value.Identifier, value, hint);
            
        case totemArgumentType_NewArray:
			return totemNewArrayPrototype_Eval(argument->Value.NewArray, build, hint, value);
            
        case totemArgumentType_NewObject:
			return totemNewObjectPrototype_Eval(argument->Value.NewObject, build, hint, value);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value)
{
    totemOperandPrototype arraySize;
	totemBool isInitList = newArray->Num > 1;
	totemBool isConstantLength = newArray->Num < TOTEM_B2U_MAX && newArray->Num >= 0;

	TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, value, hint));

	if (!isInitList)
	{
		TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(newArray->Accessor, build, NULL, &arraySize));
	}
	else if (!isConstantLength)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, newArray->Num, &arraySize, NULL));
	}

	if (isConstantLength)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxuInstruction(build, value, (totemOperandXUnsigned)newArray->Num, totemOperationType_NewArrayWithLength));
	}
	else
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbInstruction(build, value, &arraySize, totemOperationType_NewArray));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &arraySize));
	}
    
	if (isInitList)
    {
        totemInt num = 0;
        totemOperandPrototype index, member;
        for (totemExpressionPrototype *exp = newArray->Accessor; exp; exp = exp->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, num, &index, NULL));
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(exp, build, NULL, &member));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &index, &member, totemOperationType_ComplexSet));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &index));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &member));
            num++;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandPrototype *index)
{
    totemRegisterListPrototype *localScope = build->LocalVariableScope;
    totemBool varFound = totemRegisterListPrototype_GetVariable(localScope, &variable->Identifier, index, totemBool_True);
    
    if (varFound)
    {
        return totemBuildPrototype_Break(build, totemEvalStatus_IdentifierAlreadyDefined, &variable->Position);
    }
    
    // create locally
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(localScope, &variable->Identifier, index));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_GlobalCacheCheck(build, index, NULL));
    
    totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, index->RegisterScopeType);
    
    if(TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsConst))
    {
        totemRegisterListPrototype_SetRegisterFlags(scope, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build)
{
    totemInstruction *lastInstruction = totemInstructionBuffer_Top(&build->Instructions);
    if(lastInstruction == NULL || TOTEM_INSTRUCTION_GET_OP(*lastInstruction) != totemOperationType_Return)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, NULL));
		lastInstruction = totemInstructionBuffer_Top(&build->Instructions);
    }
    
    totemOperandXUnsigned flags = TOTEM_INSTRUCTION_GET_B2U(*lastInstruction);
    totemInstruction_SetBxUnsigned(lastInstruction, flags | totemReturnFlag_Last);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandPrototype *src)
{
    // free moved global registers if any still exist in local scope
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    
    totemOperandXUnsigned flags = totemReturnFlag_Register;
    totemOperandPrototype def;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNull(build, &def, NULL));
    
    // implicit return
    if (src == NULL)
    {
        src = &def;
        flags = totemReturnFlag_None;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxuInstruction(build, src, flags, totemOperationType_Return));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InvalidDataType);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_ScriptFunctionAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_FunctionNotDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_Success);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyRegisters);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_IdentifierAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_IdentifierNotDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_AssignmentLValueCannotBeConst);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_AssignmentLValueNotMutable);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableAlreadyAssigned);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyNativeFunctions);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyScriptFunctions);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyFunctionArguments);
    }
    
    return "UNKNOWN";
}
