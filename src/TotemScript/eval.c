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
#include <TotemScript/log.h>
#include <string.h>
#include <math.h>

totemEvalStatus totemEvalStatus_Break(totemEvalStatus status)
{
    return status;
}

totemEvalStatus totemBuildPrototype_Break(totemBuildPrototype *build, totemEvalStatus status, totemTokenPositionInfo *errorAt)
{
    build->ErrorAt = errorAt;
    return totemEvalStatus_Break(status);
}

void totemBuildPrototype_Init(totemBuildPrototype *build, totemParseTree *tree)
{
    totemRegisterListPrototype_Init(&build->GlobalRegisters, totemOperandType_GlobalRegister);
    totemEvalStrHashMap_Init(&build->FunctionLookup);
	totemScriptFunctionPrototypeDynBuffer_Init(&build->Functions);
    totemInstructionDynBuffer_Init(&build->Instructions);
    totemParseStringDynBuffer_Init(&build->NativeFunctionNames);
	totemOperandPrototypeDynBuffer_Init(&build->FunctionArguments);
	totemOperandPrototypeDynBuffer_Init(&build->RecycledRegisters);
	totemOperandXuDynBuffer_Init(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentFunction = NULL;
    build->CurrentAnonFunc = 0;
    build->ParseTree = tree;
}

void totemBuildPrototype_Reset(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
	totemEvalStrHashMap_Reset(&build->FunctionLookup);
	totemScriptFunctionPrototypeDynBuffer_Reset(&build->Functions);
	totemInstructionDynBuffer_Reset(&build->Instructions);
	totemParseStringDynBuffer_Reset(&build->NativeFunctionNames);
	totemOperandPrototypeDynBuffer_Reset(&build->FunctionArguments);
	totemOperandPrototypeDynBuffer_Reset(&build->RecycledRegisters);
	totemOperandXuDynBuffer_Reset(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentFunction = NULL;
    build->CurrentAnonFunc = 0;
}

void totemBuildPrototype_Cleanup(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Cleanup(&build->GlobalRegisters);
	totemEvalStrHashMap_Cleanup(&build->FunctionLookup);
	totemScriptFunctionPrototypeDynBuffer_Cleanup(&build->Functions);
	totemInstructionDynBuffer_Cleanup(&build->Instructions);
	totemParseStringDynBuffer_Cleanup(&build->NativeFunctionNames);
	totemOperandPrototypeDynBuffer_Cleanup(&build->FunctionArguments);
	totemOperandPrototypeDynBuffer_Cleanup(&build->RecycledRegisters);
	totemOperandXuDynBuffer_Cleanup(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentFunction = NULL;
    build->CurrentAnonFunc = 0;
}

totemEvalStatus totemBuildPrototype_EnterLocalScope(totemBuildPrototype *build)
{
    totemRegisterListPrototype *list = totemBuildPrototype_GetLocalRegisters(build);
    return totemRegisterListPrototype_EnterScope(list);
}

totemEvalStatus totemBuildPrototype_ExitLocalScope(totemBuildPrototype *build)
{
    totemRegisterListPrototype *list = totemBuildPrototype_GetLocalRegisters(build);
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
	totemOperandXu numArgs = 0;
    for (totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
		if (numArgs++ >= TOTEM_MAX_LOCAL_REGISTERS)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_TooManyFunctionArguments, &parameter->Position);
        }
    }

    // 2. alloc mem
    // we might end up needing to eval another function call
    // so allocate space for everything we need up front before that might happen
    // and just pop it all at the end
	size_t originalFuncArgBufferLen = totemOperandPrototypeDynBuffer_Length(&build->FunctionArguments);
    if (!totemOperandPrototypeDynBuffer_Secure(&build->FunctionArguments, numArgs))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    // 3. eval args
    totemOperandXu currentArg = 0;
    for (totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandPrototype operand;
        TOTEM_ZERO(operand);
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, NULL, &operand));
        totemOperandPrototype *funcArg = totemOperandPrototypeDynBuffer_At(&build->FunctionArguments, originalFuncArgBufferLen + currentArg);
        currentArg++;
        memcpy(funcArg, &operand, sizeof(*funcArg));
    }
    
    // start function call
    totemRegisterListPrototype *localList = totemBuildPrototype_GetLocalRegisters(build);

	currentArg = 0;
	totemOperandXu numArgsLeft = numArgs;
	totemBool finalInvoke = totemBool_True;
    
	if (numArgsLeft == 2)
	{
		totemOperandPrototype *operand = totemOperandPrototypeDynBuffer_At(&build->FunctionArguments, originalFuncArgBufferLen);
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_PreInvoke_2Args(build, src, operand, operand + 1));
		numArgsLeft = 0;
	}
	else if (numArgsLeft == 1)
	{
		totemOperandPrototype *operand = totemOperandPrototypeDynBuffer_At(&build->FunctionArguments, originalFuncArgBufferLen);
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_False));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_FullInvoke_1Arg(build, dst, src, operand));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_True));
		numArgsLeft = 0;
		finalInvoke = totemBool_False;
	}
	else if (numArgsLeft == 0)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_False));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_FullInvoke_NoArgs(build, dst, src));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_True));
		finalInvoke = totemBool_False;
	}
	else
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_PreInvoke(build, src, numArgs));
	}
    
    // function arguments
	while(numArgsLeft)
	{
		totemOperandPrototype *operand = totemOperandPrototypeDynBuffer_At(&build->FunctionArguments, currentArg + originalFuncArgBufferLen);

		if (numArgsLeft >= 3)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_FunctionArg_3(build, operand, operand + 1, operand + 2));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 1));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 2));
			currentArg += 3;
			numArgsLeft -= 3;
		}
		else if (numArgsLeft == 2)
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_False));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_FunctionArg_2_Invoke(build, operand, operand + 1, dst));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_True));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand + 1));
			currentArg += 2;
			numArgsLeft -= 2;
			finalInvoke = totemBool_False;
		}
		else
		{
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_False));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_FunctionArg_Invoke(build, operand, dst));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_True));
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
			currentArg++;
			numArgsLeft--;
			finalInvoke = totemBool_False;
		}
	}
    
    // finish function call
	if (finalInvoke)
	{
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_False));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Invoke(build, dst));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localList, totemBool_True));
	}
    
    // pop space we allocated
    totemOperandPrototypeDynBuffer_Pop(&build->FunctionArguments, numArgs);
    
    return totemEvalStatus_Success;
}

totemRegisterListPrototype *totemBuildPrototype_GetRegisterList(totemBuildPrototype *build, totemOperandType scope)
{
    if(scope == totemOperandType_GlobalRegister)
    {
        return &build->GlobalRegisters;
    }
    
    return totemBuildPrototype_GetLocalRegisters(build);
}

totemRegisterListPrototype *totemBuildPrototype_GetLocalRegisters(totemBuildPrototype *build)
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
    
    switch(preferredScope)
    {
        case totemOperandType_LocalRegister:
        {
            totemRegisterListPrototype *localList = totemBuildPrototype_GetLocalRegisters(build);
            status = totemRegisterListPrototype_AddRegister(localList, opOut);
            break;
        }
            
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
    totemRegisterListPrototype* localReg = totemBuildPrototype_GetLocalRegisters(build);
    
    totemRegisterPrototypeFlag flags;
    if(totemRegisterListPrototype_GetRegisterFlags(localReg, op->RegisterIndex, &flags))
    {
        // free if temp
		if (TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsUsed | totemRegisterPrototypeFlag_IsTemporary))
        {
			TOTEM_UNSETBITS(flags, totemRegisterPrototypeFlag_IsUsed);
			totemRegisterListPrototype_SetRegisterFlags(localReg, op->RegisterIndex, flags);

            if (!totemOperandPrototypeDynBuffer_Push(&build->RecycledRegisters, op, 1))
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_FlushRecycledRegisters(totemBuildPrototype *build)
{
    size_t max = totemOperandPrototypeDynBuffer_Length(&build->RecycledRegisters);
	totemOperandPrototype *ops = totemOperandPrototypeDynBuffer_Bottom(&build->RecycledRegisters);
    totemRegisterListPrototype* localReg = totemBuildPrototype_GetLocalRegisters(build);
    
    for (size_t i = 0; i < max; i++)
    {
        totemOperandPrototype *op = &ops[i];
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(localReg, op));
    }
    
	totemOperandPrototypeDynBuffer_Reset(&build->RecycledRegisters);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalIdentifier(totemBuildPrototype *build, totemParseString *name, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    TOTEM_LOG_BEGIN(Eval, Identifier, Debug, logger);
        totemLog_Write(logger, "Evaluating identifier: ");
        totemParseString_Print(name, logger);
    TOTEM_LOG_END();

    totemRegisterListPrototype *localReg = totemBuildPrototype_GetLocalRegisters(build);
    if (totemRegisterListPrototype_GetIdentifier(localReg, name, op, totemBool_False))
    {
        return totemEvalStatus_Success;
    }
    
    
    
    if (totemRegisterListPrototype_GetIdentifier(&build->GlobalRegisters, name, op, totemBool_False))
    {
        return totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    // from here on out, assume it is a native function name
    
    // add native function name
    size_t numNativeFuncs = totemParseStringDynBuffer_Length(&build->NativeFunctionNames);
    if(numNativeFuncs >= TOTEM_MAX_NATIVEFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyNativeFunctions);
    }
    
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Native;
    ptr.Address = (totemOperandXu)numNativeFuncs;
    
	if (totemParseStringDynBuffer_Push(&build->NativeFunctionNames, name, 1) == NULL)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    return totemBuildPrototype_EvalFunctionPointer(build, name, &ptr, op, hint);
}

totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandPrototype *op, totemOperandPrototype *hint)
{
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Instance;
    
    size_t numAnonFunc = totemOperandXuDynBuffer_Length(&build->AnonFunctions);
    if (build->CurrentAnonFunc < numAnonFunc)
    {
		totemOperandXu *addr = totemOperandXuDynBuffer_At(&build->AnonFunctions, build->CurrentAnonFunc);
        ptr.Address = *addr;
    }
    else
    {
        // increment script function count
        size_t numScriptFunc = totemScriptFunctionPrototypeDynBuffer_Length(&build->Functions);
		if (numScriptFunc + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_TooManyScriptFunctions, &func->Position);
        }
        
		totemOperandXu funcIndex = (totemOperandXu)numScriptFunc;
        totemScriptFunctionPrototype *functionPrototype;
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
        
        ptr.Address = funcIndex;
        
		if (!totemOperandXuDynBuffer_Push(&build->AnonFunctions, &funcIndex, 1))
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
    for (totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch (block->Type)
        {
            case totemBlockType_Function:
            {
                // named function cannot already exist
                totemEvalStrHashMapKey key;
                key.Key = &block->Ptr.Function->Identifier;
                if(totemEvalStrHashMap_Exists(&build->FunctionLookup, &key))
                {
					return totemBuildPrototype_Break(build, totemEvalStatus_ScriptFunctionAlreadyDefined, &block->Ptr.Function->Dec.Position);
                }
                
                // secure named function
                totemOperandXu functionIndex = (totemOperandXu)totemScriptFunctionPrototypeDynBuffer_Length(&build->Functions);
                totemScriptFunctionPrototype *functionPrototype = NULL;
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
				functionPrototype->Name = &block->Ptr.Function->Identifier;

                key.Value = functionIndex;
				if (!totemEvalStrHashMap_InsertNoSearch(&build->FunctionLookup, &key))
                {
					return totemBuildPrototype_Break(build, totemEvalStatus_OutOfMemory, &block->Ptr.Function->Dec.Position);
                }
                
                // setup the function pointer
                totemOperandPrototype dummy;
                totemFunctionPointerPrototype ptr;
                ptr.Type = totemFunctionType_Instance;
                ptr.Address = functionIndex;
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionPointer(build, &block->Ptr.Function->Identifier, &ptr, &dummy, NULL));
                break;
            }
                
            default:
                break;
        }
    }
    
    // eval global values
    for(totemParseGlobalValue *global = prototype->Globals; global; global = global->Next)
    {
        totemOperandPrototype dummy;
        TOTEM_ZERO(dummy);
        switch(global->Arg->Type)
        {
            case totemArgumentType_Variable:
            case totemArgumentType_NewArray:
            case totemArgumentType_NewObject:
                return totemEvalStatus_Break(totemEvalStatus_BadGlobal);
                
            default:
                break;
        }
        
        TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(global->Arg, build, NULL, &dummy));
    }
    
    // todo - if a function pointer is never allocated & the function is not exported, do not eval it
    
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
    build->CurrentFunction = globalFunction;
    
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
    size_t numIns = totemInstructionDynBuffer_Length(&build->Instructions);
    size_t numLocalReg = totemRegisterPrototypeDynBuffer_Length(&localRegisters.Registers);
    globalFunction->RegistersNeeded = (uint8_t)numLocalReg;
    globalFunction->NumInstructions = numIns;
    
    // now eval all other function instructions
    totemOperandXu funcIndex = 1;
    build->LocalVariableScope = build->LocalRegisters;
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Function:
            {
                totemScriptFunctionPrototype *functionPrototype = totemScriptFunctionPrototypeDynBuffer_At(&build->Functions, funcIndex);
				TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(&block->Ptr.Function->Dec, build, functionPrototype));
                
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
        
		totemScriptFunctionPrototype *functionPrototype = totemScriptFunctionPrototypeDynBuffer_At(&build->Functions, funcIndex);
        TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(func, build, functionPrototype));
        
        funcIndex++;
    }
    
    // todo - remove unneeded global values
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut)
{
    size_t numFuncs = totemScriptFunctionPrototypeDynBuffer_Length(&build->Functions);
	if (numFuncs + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
    }
    
	*functionOut = totemScriptFunctionPrototypeDynBuffer_Secure(&build->Functions, 1);
    if(*functionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *funcPrototype)
{
    funcPrototype->InstructionsStart = totemInstructionDynBuffer_Length(&build->Instructions);
    totemRegisterListPrototype_Reset(build->LocalRegisters);
    totemOperandPrototypeDynBuffer_Reset(&build->RecycledRegisters);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    
    totemScriptFunctionPrototype *oldFunc = build->CurrentFunction;
    build->CurrentFunction = funcPrototype;
    
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
    size_t numLocalReg = totemRegisterPrototypeDynBuffer_Length(&build->LocalRegisters->Registers);
    size_t numInstructions = totemInstructionDynBuffer_Length(&build->Instructions);
    
    funcPrototype->RegistersNeeded = (uint8_t)numLocalReg;
	funcPrototype->NumInstructions = numInstructions - funcPrototype->InstructionsStart;
    
    build->CurrentFunction = oldFunc;
    
    return status;
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    totemOperandPrototype result;
	TOTEM_ZERO(result);
    
    switch(statement->Type)
    {
        case totemStatementType_Loop:
            TOTEM_EVAL_CHECKRETURN(totemLoopPrototype_Eval(statement->Ptr.Set, build));
            break;

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
            totemScriptFunctionPrototype *func = build->CurrentFunction;//totemScriptFunctionPrototypeDynBuffer_Top(&build->Functions);
            TOTEM_ASSERT(func != NULL);
            size_t numIns = totemInstructionDynBuffer_Length(&build->Instructions);
			size_t offset = numIns - func->InstructionsStart + 1; // start with instruction after this yield
            
            if(offset > TOTEM_B2U_MAX)
            {
                return totemEvalStatus_Break(totemEvalStatus_TooManyInstructions);
            }
			
			totemOperandXu xu = (totemOperandXu)offset;
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Yield(build, &result, xu));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
			break;
		}

        case totemStatementType_Scope:
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
            for (totemStatementPrototype* stmt = statement->Ptr.Set; stmt; stmt = stmt->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(stmt, build));
            }
            
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
            break;
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_FlushRecycledRegisters(build));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalArrayAccessEnd(totemBuildPrototype *build, totemOperandPrototype *lValue, totemOperandPrototype *lValueSrc, totemOperandPrototype *arrIndex)
{
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_ComplexSet(build, lValueSrc, arrIndex, lValue));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, arrIndex));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, lValueSrc));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalMove(totemBuildPrototype *build, totemOperandPrototype *lValue, totemOperandPrototype *rValue)
{
	// determine which move instruction to encode based on rValue

	totemPublicDataType rValueType = totemPublicDataType_Max;
	totemRegisterPrototypeValue rValueVal;
	TOTEM_ZERO(rValueVal);

    totemRegisterListPrototype* rValueList = totemBuildPrototype_GetLocalRegisters(build);
	totemRegisterListPrototype_GetRegisterType(rValueList, rValue->RegisterIndex, &rValueType);
	totemRegisterListPrototype_GetRegisterValue(rValueList, rValue->RegisterIndex, &rValueVal);

	switch (rValueType)
	{
		case totemPublicDataType_Null:
			return totemBuildPrototype_AddInstruction_Move_Null(build, lValue);

		case totemPublicDataType_Boolean:
            return totemBuildPrototype_AddInstruction_Move_Bool(build, lValue, rValueVal.Bool);
			
		case totemPublicDataType_Type:
            return totemBuildPrototype_AddInstruction_Move_Type(build, lValue, (totemOperandXu)rValueVal.Type);
			
		case totemPublicDataType_Int:
			if (rValueVal.Int < TOTEM_B2U_MAX && rValueVal.Int > TOTEM_B2U_MIN)
			{
				return totemBuildPrototype_AddInstruction_Move_Int(build, lValue, (totemOperandXu)rValueVal.Int);
			}
			break;

		default:
			break;
	}

	// degrade to generic move if we can't determine at compile time
	return totemBuildPrototype_AddInstruction_Move(build, lValue, rValue);
}

totemEvalStatus totemBuildPrototype_EvalCast(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *lft, totemOperandPrototype *rgt)
{
    // todo - this needs to be a table
    
	// determine which cast op to encode based on type
	totemRegisterListPrototype *rValueList = totemBuildPrototype_GetLocalRegisters(build);
	totemPublicDataType type = totemPublicDataType_Max;
	totemRegisterListPrototype_GetRegisterType(rValueList, rgt->RegisterIndex, &type);
	if (type == totemPublicDataType_Type)
	{
		totemRegisterPrototypeValue rValueVal;
		TOTEM_ZERO(rValueVal);
        if (totemRegisterListPrototype_GetRegisterValue(rValueList, rgt->RegisterIndex, &rValueVal))
        {
            type = rValueVal.Type;
        }
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
		return totemBuildPrototype_AddInstruction_Cast_ToInt(build, dst, lft);

	case totemPublicDataType_Float:
		return totemBuildPrototype_AddInstruction_Cast_ToFloat(build, dst, lft);

	case totemPublicDataType_String:
		return totemBuildPrototype_AddInstruction_Cast_ToString(build, dst, lft);

	case totemPublicDataType_Array:
		return totemBuildPrototype_AddInstruction_Cast_ToArray(build, dst, lft);

	case totemPublicDataType_Type:
		return totemBuildPrototype_AddInstruction_Cast_ToType(build, dst, lft);

	case totemPublicDataType_Function:
		return totemBuildPrototype_AddInstruction_Cast_ToFunction(build, dst, lft);

	case totemPublicDataType_Coroutine:
		return totemBuildPrototype_AddInstruction_Cast_ToCoroutine(build, dst, lft);

	case totemPublicDataType_Object:
		return totemBuildPrototype_AddInstruction_Cast_ToObject(build, dst, lft);

	case totemPublicDataType_Boolean:
		return totemBuildPrototype_AddInstruction_Cast_ToBool(build, dst, lft);

	default:
		return totemBuildPrototype_AddInstruction_Cast(build, dst, lft, rgt);
	}
}

totemEvalStatus totemBuildPrototype_EvalLengthOf(totemBuildPrototype *build, totemOperandPrototype *dst, totemOperandPrototype *src)
{
	totemPublicDataType type = totemPublicDataType_Max;
	totemRegisterListPrototype *localReg = totemBuildPrototype_GetLocalRegisters(build);
	if (totemRegisterListPrototype_GetRegisterType(localReg, src->RegisterIndex, &type) && type != totemPublicDataType_Max)
	{
		switch (type)
		{
			case totemPublicDataType_Array:
				return totemBuildPrototype_AddInstruction_LengthOf_Array(build, dst, src);
				break;

			case totemPublicDataType_Object:
				return totemBuildPrototype_AddInstruction_LengthOf_Object(build, dst, src);
				break;

			case totemPublicDataType_String:
				return totemBuildPrototype_AddInstruction_LengthOf_String(build, dst, src);

			default:
			{
				totemOperandPrototype one;
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, 1, &one, dst));
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
		return totemBuildPrototype_AddInstruction_LengthOf(build, dst, src);
	}
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandPrototype *lValueHint, totemOperandPrototype *result)
{
    TOTEM_LOG_BEGIN(Eval, Expression, Debug, logger);
        totemLog_Write(logger, "Evaluating expression: ");
        totemTokenPositionInfo_Print(&expression->Position, build->ParseTree, logger);
    TOTEM_LOG_END();
    
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
    totemRegisterListPrototype* lValueSrcList = totemBuildPrototype_GetLocalRegisters(build);
    totemRegisterListPrototype_GetRegisterFlags(lValueSrcList, lValueSrc.RegisterIndex, &lValueSrcFlags);
    
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
	totemRegisterListPrototype *lValueList = lValueSrcList;
    
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
                    lValueSrcList = lValueList;
                    
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
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_ComplexGet(build, &lValue, &lValueSrc, &arrIndex));
                    }
                    
                    lValueRegisterIsArrayMember = totemBool_True;
                    lValueRegisterIsInvocation = totemBool_False;
                    totemRegisterListPrototype_GetRegisterFlags(lValueList, lValue.RegisterIndex, &lValueFlags);
                    
                    break;
                    
                case totemPostUnaryOperatorType_Invocation:
                    accessesPerformed++;
                    
                    if (TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsValue))
                    {
                        totemPublicDataType dataType = 0;
                        if (totemRegisterListPrototype_GetRegisterType(lValueList, lValue.RegisterIndex, &dataType))
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
                    totemRegisterListPrototype_GetRegisterFlags(lValueList, lValue.RegisterIndex, &lValueFlags);
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    totemOperandPrototype preUnaryRegister;
    
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
			case totemPreUnaryOperatorType_LengthOf:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalLengthOf(build, &lValue, &lValue));
				lValueRegisterIsArrayMember = totemBool_False;
				lValueRegisterIsInvocation = totemBool_True;
				totemRegisterListPrototype_GetRegisterFlags(lValueList, lValue.RegisterIndex, &lValueFlags);
				break;

            case totemPreUnaryOperatorType_Dec:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, 1, &preUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPreUnaryOperatorType_Inc:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, 1, &preUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_LogicalNegate(build, &lValue, &lValue));
				//TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalUnaryArith(build, &lValue, &lValue, totemOperationType_LogicalNegate));
                break;

            case totemPreUnaryOperatorType_BinaryNot:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_BinaryNot(build, &lValue, &lValue));
                //TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalUnaryArith(build, &lValue, &lValue, totemOperationType_BinaryNot));
                break;
                
            case totemPreUnaryOperatorType_Negate:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Negate(build, &lValue, &lValue));
				//TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalUnaryArith(build, &lValue, &lValue, totemOperationType_Negate));
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
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, 1, &postUnaryRegister, NULL));
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPostUnaryOperatorType_Inc:
				TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, 1, &postUnaryRegister, NULL));
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
	TOTEM_ZERO(rValue);
    
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
                totemOperandPrototype *lValueToUse = NULL;
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsVariable))
                {
                    // if this is a variable, eval directly to it
                    lValueToUse = &lValue;
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, lValueToUse, &rValue));
                
                // if expression wasn't eval'd directly to lValue, perform move
                if(lValueToUse == NULL || memcmp(lValueToUse, &rValue, sizeof(totemOperandPrototype)) != 0)
                {
                    recycleRValue = totemBool_True;
                    
                    totemRegisterPrototypeFlag rValueFlags;
                    totemRegisterListPrototype* rValueList = lValueList;
                    totemRegisterListPrototype_GetRegisterFlags(rValueList, rValue.RegisterIndex, &rValueFlags);
                    
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
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Equals(build, result, &lValue, &rValue));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_NotEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_NotEquals(build, result, &lValue, &rValue));
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

                case totemBinaryOperatorType_BinaryLeftShift:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_BinaryLeftShift));
                    recycleLValue = totemBool_True;
                    break;

                case totemBinaryOperatorType_BinaryRightShift:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemOperationType_BinaryRightShift));
                    recycleLValue = totemBool_True;
                    break;

                case totemBinaryOperatorType_BinaryAnd:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemBinaryOperatorType_BinaryAnd));
                    recycleLValue = totemBool_True;
                    break;

                case totemBinaryOperatorType_BinaryOr:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemBinaryOperatorType_BinaryOr));
                    recycleLValue = totemBool_True;
                    break;

                case totemBinaryOperatorType_BinaryXor:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBinaryArith(build, result, &lValue, &rValue, totemBinaryOperatorType_BinaryXor));
                    recycleLValue = totemBool_True;
                    break;
                    
				case totemBinaryOperatorType_IsType:
				{
					TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));

                    totemRegisterListPrototype* rValueList = lValueList;
                    totemPublicDataType rValueType = 0, lValueType = 0;

                    if(totemRegisterListPrototype_GetRegisterType(rValueList, rValue.RegisterIndex, &rValueType)
                        && rValueType == totemPublicDataType_Type)
					{
                        if (totemRegisterListPrototype_GetRegisterType(lValueList, lValue.RegisterIndex, &lValueType)
                            && lValueType != totemPublicDataType_Max)
						{
							// determine at compile-time whether types match
							TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalBool(build, lValueType == rValueType, result, NULL));
						}
						else
						{
							totemRegisterPrototypeValue rValueVal;
							TOTEM_ZERO(rValueVal);
							totemRegisterListPrototype_GetRegisterValue(rValueList, rValue.RegisterIndex, &rValueVal);

							TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_IsType(build, result, &lValue, (totemOperandXu)rValueVal.Type));
						}
					}
					else
					{
						TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Is(build, result, &lValue, &rValue));
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
        totemRegisterListPrototype_SetRegisterFlags(lValueSrcList, lValueSrc.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
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
        TOTEM_ASSERT(totemOperationType_GetInstructionType(totemOperationType_NewObject_WithIntConst) == totemInstructionType_Ab2u);
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_NewObject_WithIntConst(build, value, (totemOperandXu)obj->Num));
	}
	else
	{
		totemOperandPrototype size;
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, obj->Num, &size, NULL));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_NewObject(build, value, &size));
		TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &size));
	}
    
    while (obj->Keys)
    {
        totemOperandPrototype key, val;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(obj->Keys, build, NULL, &key));
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(obj->Values, build, NULL, &val));
        
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_ComplexSet(build, value, &key, &val));
        
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
			return totemBuildPrototype_EvalString(build, &argument->Value.String, value, hint);
            
        case totemArgumentType_Null:
            return totemBuildPrototype_EvalNull(build, value, hint);
            
        case totemArgumentType_Bool:
			return totemBuildPrototype_EvalBool(build, argument->Value.Bool, value, hint);
            
        case totemArgumentType_Number:
			return totemBuildPrototype_EvalNumber(build, argument->Value.Number, value, hint);
            
        case totemArgumentType_Type:
			return totemBuildPrototype_EvalType(build, argument->Value.DataType, value, hint);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, argument->Value.Func, value, hint);
            
        case totemArgumentType_CoroutineDeclaration:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAnonymousFunction(build, argument->Value.Func, value, hint));
            return totemBuildPrototype_AddInstruction_Cast_Function_ToCoroutine(build, value, value);

        case totemArgumentType_Identifier:
			return totemBuildPrototype_EvalIdentifier(build, &argument->Value.Identifier, value, hint);
            
        case totemArgumentType_NewArray:
			return totemNewArrayPrototype_Eval(argument->Value.NewArray, build, hint, value);
            
        case totemArgumentType_NewObject:
			return totemNewObjectPrototype_Eval(argument->Value.NewObject, build, hint, value);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandPrototype *hint, totemOperandPrototype *value)
{
    TOTEM_ASSERT(totemOperationType_GetInstructionType(totemOperationType_NewArray_WithIntConst) == totemInstructionType_Ab2u);
    const totemOperand2u MaxArrayLen = TOTEM_B2U_MAX;
    totemOperandPrototype arraySizeReg;
    totemBool useConstSize = totemBool_True;
    size_t constSize = newArray->NumElements;
    
	TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, value, hint));
    
    if (newArray->NumExpression)
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(newArray->NumExpression, build, NULL, &arraySizeReg));
        useConstSize = totemBool_False;
        
        totemRegisterPrototypeValue regValue;
        totemPublicDataType regType = totemPublicDataType_Max;
        totemRegisterListPrototype* scope = totemBuildPrototype_GetLocalRegisters(build);

        if (totemRegisterListPrototype_GetRegisterValueAndType(scope, arraySizeReg.RegisterIndex, &regValue, &regType))
        {
            if (regType == totemPublicDataType_Int || regType == totemPublicDataType_Float)
            {
                totemInt val = regValue.Int;

                if (regType == totemPublicDataType_Float)
                {
                    val = (totemInt)regValue.Float;
                }
                
                if(val < newArray->NumElements)
                {
                    return totemEvalStatus_Break(totemEvalStatus_BadNumArrayElements);
                }
                
                useConstSize = totemBool_True;
                constSize = val;
            }
        }
    }
    
    if(useConstSize)
    {
        if(constSize <= MaxArrayLen)
        {
            totemOperand2u length = (totemOperand2u)constSize;
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_NewArray_WithIntConst(build, value, length));
        }
        else
        {
            return totemEvalStatus_Break(totemEvalStatus_BadArrayLength);
        }
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_NewArray(build, value, &arraySizeReg));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &arraySizeReg));
    }
    
    totemInt num = 0;
    totemOperandPrototype index, member;
    for (totemExpressionPrototype *exp = newArray->Accessor; exp; exp = exp->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalIntConstant(build, num, &index, NULL));
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(exp, build, NULL, &member));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_ComplexSet(build, value, &index, &member));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &index));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &member));
        num++;
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
    if (localScope->ScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_GlobalCacheCheck(build, index, NULL));
    }

    totemRegisterListPrototype* scope = totemBuildPrototype_GetLocalRegisters(build);
    
    if(TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsConst))
    {
        totemRegisterListPrototype_SetRegisterFlags(scope, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build)
{
    totemInstruction *lastInstruction = totemInstructionDynBuffer_Top(&build->Instructions);
    if(lastInstruction == NULL || TOTEM_INSTRUCTION_GET_OP(*lastInstruction) != totemOperationType_Return)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, NULL));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandPrototype *src)
{
    // free moved global registers if any still exist in local scope
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalRegisters(build);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    
    totemOperandPrototype def;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNull(build, &def, NULL));
    
    // implicit return
    if (src == NULL)
    {
        src = &def;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddInstruction_Return(build, src));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemEvalStatus_UnknownDataType);
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
            TOTEM_STRINGIFY_CASE(totemEvalStatus_BadInt);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_BadGlobal);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_DivideByZero);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_BadFold);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_BadArrayLength);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_BadNumArrayElements);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyInstructions);
    }
    
    return "UNKNOWN";
}
