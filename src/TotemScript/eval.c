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
    totemMemoryBuffer_Init(&build->Functions, sizeof(totemScriptFunctionPrototype));
    totemMemoryBuffer_Init(&build->Instructions, sizeof(totemInstruction));
    totemMemoryBuffer_Init(&build->NativeFunctionNames, sizeof(totemString));
    totemMemoryBuffer_Init(&build->FunctionArguments, sizeof(totemOperandRegisterPrototype));
    totemMemoryBuffer_Init(&build->RecycledRegisters, sizeof(totemOperandRegisterPrototype));
    totemMemoryBuffer_Init(&build->AnonFunctions, sizeof(totemOperandXUnsigned));
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentAnonFunc = 0;
}

void totemBuildPrototype_Reset(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
    totemHashMap_Reset(&build->FunctionLookup);
    totemMemoryBuffer_Reset(&build->Functions);
    totemMemoryBuffer_Reset(&build->Instructions);
    totemMemoryBuffer_Reset(&build->NativeFunctionNames);
    totemMemoryBuffer_Reset(&build->FunctionArguments);
    totemMemoryBuffer_Reset(&build->RecycledRegisters);
    totemMemoryBuffer_Reset(&build->AnonFunctions);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
    build->CurrentAnonFunc = 0;
}

void totemBuildPrototype_Cleanup(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Cleanup(&build->GlobalRegisters);
    totemHashMap_Cleanup(&build->FunctionLookup);
    totemMemoryBuffer_Cleanup(&build->Functions);
    totemMemoryBuffer_Cleanup(&build->Instructions);
    totemMemoryBuffer_Cleanup(&build->NativeFunctionNames);
    totemMemoryBuffer_Cleanup(&build->FunctionArguments);
    totemMemoryBuffer_Cleanup(&build->RecycledRegisters);
    totemMemoryBuffer_Cleanup(&build->AnonFunctions);
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

totemEvalStatus totemBuildPrototype_EvalFunctionCall(totemBuildPrototype *build, totemExpressionPrototype *parametersStart, totemOperandRegisterPrototype *dst, totemOperandRegisterPrototype *src)
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
    totemMemoryBuffer_Reset(&build->FunctionArguments);
    totemOperandRegisterPrototype *funcArgs = totemMemoryBuffer_Secure(&build->FunctionArguments, numArgs);
    if (!funcArgs)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    // 3. eval args
    totemOperandXUnsigned currentArg = 0;
    for (totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype *operand = &funcArgs[currentArg];
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, NULL, operand, totemEvalExpressionFlag_None));
        currentArg++;
    }
    
    // start function call
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, src, numArgs, totemOperationType_PreInvoke));
    
    // function arguments
    for (size_t i = 0; i < numArgs; i++)
    {
        totemOperandRegisterPrototype *operand = totemMemoryBuffer_Get(&build->FunctionArguments, i);
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, operand, numArgs, totemOperationType_FunctionArg));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
    }
    
    // finish function call
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, dst, 0, totemOperationType_Invoke));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_True));
    
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

totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandRegisterPrototype *opOut)
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

totemEvalStatus totemBuildPrototype_RecycleRegister(totemBuildPrototype *build, totemOperandRegisterPrototype *op)
{
    totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, op->RegisterScopeType);
    
    totemRegisterPrototypeFlag flags;
    if(totemRegisterListPrototype_GetRegisterFlags(scope, op->RegisterIndex, &flags) && TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsUsed))
    {
        size_t refCount = 0;
        if(totemRegisterListPrototype_DecRegisterRefCount(scope, op->RegisterIndex, &refCount) && refCount == 0)
        {
            // free if temp
            if (TOTEM_HASANYBITS(flags, totemRegisterPrototypeFlag_IsTemporary))
            {
                if (!totemMemoryBuffer_Insert(&build->RecycledRegisters, op, 1))
                {
                    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
                }
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_FlushRecycledRegisters(totemBuildPrototype *build)
{
    size_t max = totemMemoryBuffer_GetNumObjects(&build->RecycledRegisters);
    totemOperandRegisterPrototype *ops = totemMemoryBuffer_Bottom(&build->RecycledRegisters);
    
    for (size_t i = 0; i < max; i++)
    {
        totemOperandRegisterPrototype *op = &ops[i];
        totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, op->RegisterScopeType);
        
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(scope, op));
    }
    
    totemMemoryBuffer_Reset(&build->RecycledRegisters);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalBoolean(totemBuildPrototype *build, totemBool val, totemOperandRegisterPrototype *op, totemOperandRegisterPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddBoolean(&build->GlobalRegisters, val, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalNull(totemBuildPrototype *build, totemOperandRegisterPrototype *op, totemOperandRegisterPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNull(&build->GlobalRegisters, op);
    if (status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalInt(totemBuildPrototype *build, totemInt number, totemOperandRegisterPrototype *operand, totemOperandRegisterPrototype *op)
{
    char buffer[256];
    int result = totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "%llu", number);
    
    if (result < 0 || result >= TOTEM_ARRAY_SIZE(buffer))
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    totemString string;
    string.Length = result;
    string.Value = buffer;
    
    return totemBuildPrototype_EvalNumber(build, &string, operand, op);
}

totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemString *number, totemOperandRegisterPrototype *operand, totemOperandRegisterPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNumberConstant(&build->GlobalRegisters, number, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemString *buffer, totemOperandRegisterPrototype *operand, totemOperandRegisterPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddStringConstant(&build->GlobalRegisters, buffer, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, operand, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalIdentifier(totemBuildPrototype *build, totemString *name, totemOperandRegisterPrototype *op, totemOperandRegisterPrototype *hint)
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
    if(totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames) >= TOTEM_MAX_NATIVEFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyNativeFunctions);
    }
    
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Native;
    ptr.Address = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames);
    
    if(totemMemoryBuffer_Insert(&build->NativeFunctionNames, name, 1) == NULL)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    return totemBuildPrototype_EvalFunctionPointer(build, name, &ptr, op, hint);
}

totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemString *val, totemFunctionPointerPrototype *value, totemOperandRegisterPrototype *op, totemOperandRegisterPrototype *hint)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFunctionPointer(&build->GlobalRegisters, val, value, op);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalCacheCheck(build, op, hint);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandRegisterPrototype *op, totemOperandRegisterPrototype *hint)
{
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Script;
    
    if (build->CurrentAnonFunc < totemMemoryBuffer_GetNumObjects(&build->AnonFunctions))
    {
        totemOperandXUnsigned *addr = totemMemoryBuffer_Get(&build->AnonFunctions, build->CurrentAnonFunc);
        ptr.Address = *addr;
    }
    else
    {
        // increment script function count
        if (totemMemoryBuffer_GetNumObjects(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
        {
            return totemBuildPrototype_Break(build, totemEvalStatus_TooManyScriptFunctions, &func->Position);
        }
        
        totemOperandXUnsigned funcIndex = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->Functions);
        totemScriptFunctionPrototype *functionPrototype;
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
        
        ptr.Address = funcIndex;
        
        if (!totemMemoryBuffer_Insert(&build->AnonFunctions, &funcIndex, 1))
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

totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandRegisterPrototype *operand, totemOperandRegisterPrototype *hint)
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
    for (totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch (block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                // named function cannot already exist
                if (totemHashMap_Find(&build->FunctionLookup, block->FuncDec->Identifier->Value, block->FuncDec->Identifier->Length) != NULL)
                {
                    return totemBuildPrototype_Break(build, totemEvalStatus_ScriptFunctionAlreadyDefined, &block->FuncDec->Position);
                }
                
                // secure named function
                size_t functionIndex = totemMemoryBuffer_GetNumObjects(&build->Functions);
                totemScriptFunctionPrototype *functionPrototype = NULL;
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
                memcpy(&functionPrototype->Name, block->FuncDec->Identifier, sizeof(totemString));
                
                if (!totemHashMap_Insert(&build->FunctionLookup, block->FuncDec->Identifier->Value, block->FuncDec->Identifier->Length, functionIndex))
                {
                    return totemBuildPrototype_Break(build, totemEvalStatus_OutOfMemory, &block->FuncDec->Position);
                }
                
                // setup the function pointer
                totemOperandRegisterPrototype dummy;
                totemFunctionPointerPrototype ptr;
                ptr.Type = totemFunctionType_Script;
                ptr.Address = (totemOperandXUnsigned)functionIndex;
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionPointer(build, block->FuncDec->Identifier, &ptr, &dummy, NULL));
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
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(block->Statement, build));
                break;
                
            default:
                break;
        }
    }
    
    // eval global values from functions into global scope
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                for (totemStatementPrototype *stmt = block->FuncDec->StatementsStart; stmt != NULL; stmt = stmt->Next)
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
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(block->Statement, build));
                break;
            }
                
            default:
                break;
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalImplicitReturn(build));
    globalFunction->RegistersNeeded = (uint8_t)totemMemoryBuffer_GetNumObjects(&localRegisters.Registers);
    
    // now eval all other function instructions
    totemOperandXUnsigned funcIndex = 1;
    build->LocalVariableScope = build->LocalRegisters;
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
            {
                totemScriptFunctionPrototype *functionPrototype = totemMemoryBuffer_Get(&build->Functions, funcIndex);
                TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(block->FuncDec, build, functionPrototype));
                
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
        
        totemScriptFunctionPrototype *functionPrototype = totemMemoryBuffer_Get(&build->Functions, funcIndex);
        TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(func, build, functionPrototype));
        
        funcIndex++;
    }
    
    /*
     FILE *file = fopen("ins.txt", "w");
     totemInstruction_PrintList(stdout, totemMemoryBuffer_Bottom(&build->Instructions), totemMemoryBuffer_GetNumObjects(&build->Instructions));
     fclose(file);
     */
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemScriptFunctionPrototype **functionOut)
{
    if(totemMemoryBuffer_GetNumObjects(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
    }
    
    *functionOut = totemMemoryBuffer_Secure(&build->Functions, 1);
    if(*functionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *funcPrototype)
{
    funcPrototype->InstructionsStart = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    totemRegisterListPrototype_Reset(build->LocalRegisters);
    totemMemoryBuffer_Reset(&build->RecycledRegisters);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EnterLocalScope(build));
    
    // parameters
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemVariablePrototype_Eval(parameter, build, &dummy));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    totemEvalStatus status = totemBuildPrototype_EvalImplicitReturn(build);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_ExitLocalScope(build));
    funcPrototype->RegistersNeeded = (uint8_t)totemMemoryBuffer_GetNumObjects(&build->LocalRegisters->Registers);
    
    return status;
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    
    
    totemOperandRegisterPrototype result;
    memset(&result, 0, sizeof(totemOperandRegisterPrototype));
    
    
    
    switch(statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemDoWhileLoopPrototype_Eval(statement->DoWhileLoop, build));
            break;
            
        case totemStatementType_ForLoop:
            TOTEM_EVAL_CHECKRETURN(totemForLoopPrototype_Eval(statement->ForLoop, build));
            break;
            
        case totemStatementType_IfBlock:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_Eval(statement->IfBlock, build));
            break;
            
        case totemStatementType_WhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemWhileLoopPrototype_Eval(statement->WhileLoop, build));
            break;
            
        case totemStatementType_Simple:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Simple, build, NULL, &result, totemEvalExpressionFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
            break;
            
        case totemStatementType_Return:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Return, build, NULL, &result, totemEvalExpressionFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, &result));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
            break;
    }
    
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_FlushRecycledRegisters(build));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalArrayAccessEnd(totemBuildPrototype *build, totemOperandRegisterPrototype *lValue, totemOperandRegisterPrototype *lValueSrc, totemOperandRegisterPrototype *arrIndex)
{
    
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, lValueSrc, arrIndex, lValue, totemOperationType_ComplexSet));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, arrIndex));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, lValueSrc));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandRegisterPrototype *lValueHint, totemOperandRegisterPrototype *result, totemEvalExpressionFlag exprFlags)
{
    totemOperandRegisterPrototype lValueSrc;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValueArgument, build, lValueHint, &lValueSrc));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValueExpression, build, lValueHint, &lValueSrc, totemEvalExpressionFlag_None));
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
        case totemBinaryOperatorType_Shift:
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
    totemOperandRegisterPrototype lValue;
    totemOperandRegisterPrototype arrIndex;
    memcpy(&lValue, &lValueSrc, sizeof(totemOperandRegisterPrototype));
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
                    memcpy(&lValueSrc, &lValue, sizeof(totemOperandRegisterPrototype));
                    lValueSrcFlags = lValueFlags;
                    lValueSrcScope = lValueScope;
                    
                    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->ArrayAccess, build, NULL, &arrIndex, totemEvalExpressionFlag_None));
                    
                    totemBool performAccess = totemBool_True;
                    totemBool shouldShift = totemBool_False;
                    
                    if (expression->BinaryOperator == totemBinaryOperatorType_Assign || expression->BinaryOperator == totemBinaryOperatorType_Shift)
                    {
                        shouldShift = numlValueAccesses == accessesPerformed - 1;
                    }
                    else
                    {
                        shouldShift = numlValueAccesses == accessesPerformed;
                    }
                    
                    if (numlValueAccesses == accessesPerformed)
                    {
                        // if we're moving to this location, we can skip this array_get - we'll be grabbing the new value and performing a direct array_set further below
                        if (expression->BinaryOperator == totemBinaryOperatorType_Assign || expression->BinaryOperator == totemBinaryOperatorType_Shift)
                        {
                            performAccess = totemBool_False;
                        }
                        
                        // if caller has given us a hint, we need to use it for the last array access
                        if (lValueHint)
                        {
                            memcpy(&lValue, lValueHint, sizeof(totemOperandRegisterPrototype));
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
                        if (shouldShift && TOTEM_HASBITS(exprFlags, totemEvalExpressionFlag_Shift))
                        {
                            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValueSrc, &arrIndex, totemOperationType_ComplexShift));
                        }
                        else
                        {
                            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValueSrc, &arrIndex, totemOperationType_ComplexGet));
                        }
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
                    totemOperandRegisterPrototype pointer;
                    memcpy(&pointer, &lValue, sizeof(totemOperandRegisterPrototype));
                    
                    // replace lvalue, we're done with it
                    if (lValueRegisterIsArrayMember && mutatedLValueRegisterUnary)
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
                    }
                    
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
                    
                    // if caller has given us a hint, we need to use it for the last array access
                    if (numlValueAccesses == accessesPerformed && lValueHint)
                    {
                        memcpy(&lValue, lValueHint, sizeof(totemOperandRegisterPrototype));
                    }
                    else
                    {
                        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                    }
                    
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionCall(build, op->InvocationParametersStart, &lValue, &pointer));
                    
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
    
    totemOperandRegisterPrototype preUnaryRegister, preUnaryLValue;
    totemString preUnaryNumber;
    totemString_FromLiteral(&preUnaryNumber, "0");
    
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
            case totemPreUnaryOperatorType_Dec:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &lValue, totemOperationType_LogicalNegate));
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Multiply));
                // A = B * -1
                break;
                
            case totemPreUnaryOperatorType_None:
                break;
        }
    }
    
    totemOperandRegisterPrototype postUnaryRegister;
    totemString postUnaryNumber;
    totemString_FromLiteral(&postUnaryNumber, "0");
    
    for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        switch(op->Type)
        {
            case totemPostUnaryOperatorType_Dec:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister, NULL));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister, NULL));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
            case totemPostUnaryOperatorType_Invocation:
                // already done above
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    totemOperandRegisterPrototype rValue;
    memset(&rValue, 0, sizeof(totemOperandRegisterPrototype));
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemBool recycleRValue = totemBool_False;
        totemBool recycleLValue = totemBool_False;
        
        if(expression->BinaryOperator == totemBinaryOperatorType_Assign || expression->BinaryOperator == totemBinaryOperatorType_Shift)
        {
            totemEvalExpressionFlag rValueEvalFlag = totemEvalExpressionFlag_None;
            if (expression->BinaryOperator == totemBinaryOperatorType_Shift)
            {
                rValueEvalFlag = totemEvalExpressionFlag_Shift;
            }
            
            if(expression->BinaryOperator != totemBinaryOperatorType_Shift && TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsTemporary))
            {
                // result of rValue expression can now be stored directly in lValue
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &lValue, rValueEvalFlag));
            }
            else
            {
                // otherwise we need the move op
                totemOperandRegisterPrototype *lValueToUse = NULL;
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsVariable) && expression->BinaryOperator != totemBinaryOperatorType_Shift)
                {
                    lValueToUse = &lValue;
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, lValueToUse, &rValue, rValueEvalFlag));
                
                // if expression wasn't eval'd directly to lValue, perform move
                if(lValueToUse == NULL || memcmp(lValueToUse, &rValue, sizeof(totemOperandRegisterPrototype)) != 0)
                {
                    recycleRValue = totemBool_True;
                    
                    totemRegisterPrototypeFlag rValueFlags;
                    totemRegisterListPrototype *rValueScope = totemBuildPrototype_GetRegisterList(build, rValue.RegisterScopeType);
                    totemRegisterListPrototype_GetRegisterFlags(rValueScope, rValue.RegisterIndex, &rValueFlags);
                    
                    if (expression->BinaryOperator == totemBinaryOperatorType_Shift)
                    {
                        if (TOTEM_HASANYBITS(rValueFlags, totemRegisterPrototypeFlag_IsConst | totemRegisterPrototypeFlag_IsValue))
                        {
                            return totemBuildPrototype_Break(build, totemEvalStatus_InvalidShiftSource, &expression->Position);
                        }
                    }
                    
                    totemOperandRegisterPrototype c;
                    memset(&c, 0, sizeof(c));
                    
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &rValue, &c, totemOperationType_Move));
                    
                    // overwrite original rValue when shifting
                    if (expression->BinaryOperator == totemBinaryOperatorType_Shift)
                    {
                        if (TOTEM_HASBITS(rValueFlags, totemRegisterPrototypeFlag_IsVariable))
                        {
                            totemOperandRegisterPrototype null;
                            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNull(build, &null, NULL));
                            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &rValue, &null, &c, totemOperationType_Move));
                        }
                    }
                }
                
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
            }
        }
        else
        {
            recycleRValue = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &rValue, totemEvalExpressionFlag_None));
            
            switch(expression->BinaryOperator)
            {
                case totemBinaryOperatorType_Plus:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Add));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_PlusAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Add));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Minus:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Subtract));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MinusAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Subtract));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Multiply:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Multiply));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MultiplyAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Multiply));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Divide:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Divide));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_DivideAssign:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Divide));
                    memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
                    break;
                    
                case totemBinaryOperatorType_Assign:
                case totemBinaryOperatorType_Shift:
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
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_MoreThan));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LessThan:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LessThan));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_MoreThanEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_MoreThanEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LessThanEquals:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LessThanEquals));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LogicalAnd:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LogicalAnd));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_LogicalOr:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_LogicalOr));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_IsType:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_Is));
                    recycleLValue = totemBool_True;
                    break;
                    
                case totemBinaryOperatorType_AsType:
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, result));
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &lValue, &rValue, totemOperationType_As));
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
        memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
    }
    
    if (mutatedLValueRegisterBinary | mutatedLValueRegisterUnary)
    {
        // this lvalue is now assigned
        totemRegisterListPrototype_SetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
    }
    
    // finish the array-access if the value was mutated
    if (lValueRegisterIsArrayMember && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary) && !TOTEM_HASBITS(exprFlags, totemEvalExpressionFlag_Shift))
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_EvalHint(totemBuildPrototype *build, totemOperandRegisterPrototype *value, totemOperandRegisterPrototype *hint)
{
    
    
    if (hint)
    {
        memcpy(value, hint, sizeof(totemOperandRegisterPrototype));
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, value));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandRegisterPrototype *hint,  totemOperandRegisterPrototype *value)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
            return totemVariablePrototype_Eval(argument->Variable, build, value);
            
        case totemArgumentType_String:
            return totemBuildPrototype_EvalString(build, argument->String, value, hint);
            
        case totemArgumentType_Null:
            return totemBuildPrototype_EvalNull(build, value, hint);
            
        case totemArgumentType_Boolean:
            return totemBuildPrototype_EvalBoolean(build, argument->Boolean, value, hint);
            
        case totemArgumentType_Number:
            return totemBuildPrototype_EvalNumber(build, argument->Number, value, hint);
            
        case totemArgumentType_Type:
            return totemBuildPrototype_EvalType(build, argument->DataType, value, hint);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, argument->FunctionDeclaration, value, hint);
            
        case totemArgumentType_Identifier:
            return totemBuildPrototype_EvalIdentifier(build, argument->Identifier, value, hint);
            
        case totemArgumentType_NewArray:
            return totemNewArrayPrototype_Eval(argument->NewArray, build, hint, value);
            
        case totemArgumentType_NewObject:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, value, hint));
            return totemBuildPrototype_EvalAbcInstruction(build, value, value, value, totemOperationType_NewObject);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemNewArrayPrototype_Eval(totemNewArrayPrototype *newArray, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *value)
{
    
    
    totemOperandRegisterPrototype arraySize, c;
    memset(&c, 0, sizeof(totemOperandRegisterPrototype));
    
    if (newArray->isInitList)
    {
        totemInt num = 0;
        for (totemExpressionPrototype *exp = newArray->Accessor; exp; exp = exp->Next)
        {
            num++;
        }
        
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, num, &arraySize, hint));
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(newArray->Accessor, build, NULL, &arraySize, totemEvalExpressionFlag_None));
    }
    
    
    TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, value, hint));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &arraySize, &c, totemOperationType_NewArray));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &arraySize));
    
    if (newArray->isInitList)
    {
        totemInt num = 0;
        totemOperandRegisterPrototype index, member;
        for (totemExpressionPrototype *exp = newArray->Accessor; exp; exp = exp->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, num, &index, NULL));
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(exp, build, NULL, &member, totemEvalExpressionFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &index, &member, totemOperationType_ComplexSet));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &index));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &member));
            num++;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandRegisterPrototype *index)
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
    totemInstruction *lastInstruction = totemMemoryBuffer_Top(&build->Instructions);
    if(lastInstruction == NULL || TOTEM_INSTRUCTION_GET_OP(*lastInstruction) != totemOperationType_Return)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, NULL));
        lastInstruction = totemMemoryBuffer_Top(&build->Instructions);
    }
    
    totemOperandXUnsigned flags = TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*lastInstruction);
    totemInstruction_SetBxUnsigned(lastInstruction, flags | totemReturnFlag_Last);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *src)
{
    // free moved global registers if any still exist in local scope
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    
    totemOperandXUnsigned flags = totemReturnFlag_Register;
    totemOperandRegisterPrototype def;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNull(build, &def, NULL));
    
    // implicit return
    if (src == NULL)
    {
        src = &def;
        flags = totemReturnFlag_None;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, src, flags, totemOperationType_Return));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RotateAllGlobalCaches(build, localScope, totemBool_False));
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InvalidShiftSource);
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