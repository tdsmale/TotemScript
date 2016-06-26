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

totemEvalStatus totemEvalStatus_Break(totemEvalStatus status)
{
    return status;
}

void totemBuildPrototype_Init(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Init(&build->GlobalRegisters, totemOperandType_GlobalRegister);
    totemHashMap_Init(&build->FunctionLookup);
    totemHashMap_Init(&build->NativeFunctionNamesLookup);
    totemHashMap_Init(&build->AnonymousFunctions);
    totemMemoryBuffer_Init(&build->Functions, sizeof(totemScriptFunctionPrototype));
    totemMemoryBuffer_Init(&build->Instructions, sizeof(totemInstruction));
    totemMemoryBuffer_Init(&build->NativeFunctionNames, sizeof(totemString));
    totemMemoryBuffer_Init(&build->FunctionArguments, sizeof(totemOperandRegisterPrototype));
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
}

void totemBuildPrototype_Reset(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
    totemHashMap_Reset(&build->FunctionLookup);
    totemHashMap_Reset(&build->NativeFunctionNamesLookup);
    totemHashMap_Reset(&build->AnonymousFunctions);
    totemMemoryBuffer_Reset(&build->Functions);
    totemMemoryBuffer_Reset(&build->Instructions);
    totemMemoryBuffer_Reset(&build->NativeFunctionNames);
    totemMemoryBuffer_Reset(&build->FunctionArguments);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
}

void totemBuildPrototype_Cleanup(totemBuildPrototype *build)
{
    totemRegisterListPrototype_Cleanup(&build->GlobalRegisters);
    totemHashMap_Cleanup(&build->FunctionLookup);
    totemHashMap_Cleanup(&build->NativeFunctionNamesLookup);
    totemHashMap_Cleanup(&build->AnonymousFunctions);
    totemMemoryBuffer_Cleanup(&build->Functions);
    totemMemoryBuffer_Cleanup(&build->Instructions);
    totemMemoryBuffer_Cleanup(&build->NativeFunctionNames);
    totemMemoryBuffer_Cleanup(&build->FunctionArguments);
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->LocalRegisters = NULL;
}

totemEvalStatus totemBuildPrototype_EvalFunctionArgumentsBegin(totemBuildPrototype *build, totemExpressionPrototype *parametersStart)
{
    // we need to eval the func-args before we call the function
    // 1. count num args
    totemOperandXUnsigned numArgs = 0;
    for(totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        if(numArgs == TOTEM_OPERANDX_UNSIGNED_MAX - 1)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyFunctionArguments);
        }
        
        numArgs++;
    }
    
    // 2. alloc mem
    totemMemoryBuffer_Reset(&build->FunctionArguments);
    totemOperandRegisterPrototype *funcArgs = totemMemoryBuffer_Secure(&build->FunctionArguments, numArgs);
    if(!funcArgs)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    // 3. eval args
    totemOperandXUnsigned currentArg = 0;
    for(totemExpressionPrototype *parameter = parametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype *operand = &funcArgs[currentArg];
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, NULL, operand, totemEvalVariableFlag_MustBeDefined));
        currentArg++;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalFunctionArgumentsEnd(totemBuildPrototype *build)
{
    totemOperandXUnsigned numObjects = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->FunctionArguments);
    
    for(size_t i = 0; i < numObjects; i++)
    {
        totemOperandRegisterPrototype *operand = totemMemoryBuffer_Get(&build->FunctionArguments, i);
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, operand, numObjects, totemOperationType_FunctionArg));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, operand));
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

totemEvalStatus totemBuildPrototype_FreeGlobalAssocs(totemBuildPrototype *build, size_t max)
{
    size_t numFreed = 0;
    
    // free moved global registers if any still exist in local scope
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    for (size_t i = 0; i < localScope->MoveToLocalVars.NumBuckets; i++)
    {
        for (totemHashMapEntry *entry = localScope->MoveToLocalVars.Buckets[i]; entry != NULL;)
        {
            size_t count = 0;
            totemOperandXUnsigned globalAssoc = 0;
            
            //printf("free %lu\n", entry->Value);
            
            if(totemRegisterListPrototype_GetRegisterRefCount(localScope, (totemOperandXUnsigned)entry->Value, &count)
               && (count == 0 || max == 0)
               && totemRegisterListPrototype_GetRegisterGlobalAssoc(localScope, (totemOperandXUnsigned)entry->Value, &globalAssoc))
            {
                totemOperandRegisterPrototype dummy;
                memset(&dummy, 0, sizeof(totemOperandRegisterPrototype));
                dummy.RegisterIndex = (totemOperandXUnsigned)entry->Value;
                dummy.RegisterScopeType = totemOperandType_LocalRegister;
                
                // remove
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &dummy, globalAssoc, totemOperationType_MoveToGlobal));
                
                totemRegisterListPrototype_SetRegisterFlags(localScope, dummy.RegisterIndex, totemRegisterPrototypeFlag_IsTemporary);
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &dummy));
                
                entry = entry->Next;
                totemHashMap_Remove(&localScope->MoveToLocalVars, (const char*)&dummy, sizeof(totemOperandRegisterPrototype));
                
                numFreed++;
            }
            else
            {
                entry = entry->Next;
            }
        }
        
        if(max != 0 && numFreed >= max)
        {
            break;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_GlobalAssocCheck(totemBuildPrototype *build, totemOperandRegisterPrototype *opOut)
{
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    // if we can't access this in a normal instruction, move to local-scope
    if(opOut->RegisterScopeType == totemOperandType_GlobalRegister && opOut->RegisterIndex >= TOTEM_MAX_LOCAL_REGISTERS && TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EvalGlobalAssocs))
    {
        // is this register already in local scope?
        totemHashMapEntry *entry = totemHashMap_Find(&localScope->MoveToLocalVars, (const char*)opOut, sizeof(totemOperandRegisterPrototype));
        if(entry != NULL)
        {
            opOut->RegisterIndex = (totemOperandXUnsigned)entry->Value;
            opOut->RegisterScopeType = totemOperandType_LocalRegister;
            totemRegisterListPrototype_IncRegisterRefCount(localScope, opOut->RegisterIndex, NULL);
            return totemEvalStatus_Success;
        }
        
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(localScope, &dummy));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &dummy, opOut->RegisterIndex, totemOperationType_MoveToLocal));
        
        totemRegisterListPrototype_SetRegisterGlobalAssoc(localScope, dummy.RegisterIndex, opOut->RegisterIndex);
        
        //printf("add %d %d\n", dummy.RegisterIndex, opOut->RegisterIndex);
        
        // add to lookup
        if (!totemHashMap_Insert(&localScope->MoveToLocalVars, (const char*)opOut, sizeof(totemOperandRegisterPrototype), dummy.RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        totemRegisterPrototypeFlag flags;
        totemRegisterListPrototype_GetRegisterFlags(&build->GlobalRegisters, opOut->RegisterIndex, &flags);
        
        opOut->RegisterIndex = dummy.RegisterIndex;
        opOut->RegisterScopeType = totemOperandType_LocalRegister;
        
        totemRegisterListPrototype_UnsetRegisterFlags(localScope, opOut->RegisterIndex, totemRegisterPrototypeFlag_IsTemporary);
        totemRegisterListPrototype_SetRegisterFlags(localScope, opOut->RegisterIndex, flags | totemRegisterPrototypeFlag_IsGlobalAssoc);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AddRegister(totemBuildPrototype *build, totemOperandType preferredScope, totemOperandRegisterPrototype *opOut)
{
    totemEvalStatus status = totemEvalStatus_Success;
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    switch(preferredScope)
    {
        case totemOperandType_LocalRegister:
            status = totemRegisterListPrototype_AddRegister(localScope, opOut);
            if(status == totemEvalStatus_TooManyRegisters)
            {
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_FreeGlobalAssocs(build, 1));
                status = totemRegisterListPrototype_AddRegister(localScope, opOut);
            }
            break;
            
        case totemOperandType_GlobalRegister:
        {
            status = totemRegisterListPrototype_AddRegister(&build->GlobalRegisters, opOut);
            if(status == totemEvalStatus_Success)
            {
                status = totemBuildPrototype_GlobalAssocCheck(build, opOut);
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
            if(TOTEM_HASBITS(flags, totemRegisterPrototypeFlag_IsTemporary))
            {
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(scope, op));
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalNumber(totemBuildPrototype *build, totemString *number, totemOperandRegisterPrototype *operand)
{
    totemEvalStatus status = totemRegisterListPrototype_AddNumberConstant(&build->GlobalRegisters, number, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, operand);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalString(totemBuildPrototype *build, totemString *buffer, totemOperandRegisterPrototype *operand)
{
    totemEvalStatus status = totemRegisterListPrototype_AddStringConstant(&build->GlobalRegisters, buffer, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, operand);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalFunctionName(totemBuildPrototype *build, totemString *name, totemFunctionPointerPrototype *func)
{
    // did we define this function in the script?
    totemHashMapEntry *searchResult = totemHashMap_Find(&build->FunctionLookup, name->Value, name->Length);
    if(searchResult)
    {
        func->Address = (totemOperandXUnsigned)searchResult->Value;
        func->Type = totemFunctionType_Script;
    }
    else
    {
        func->Type = totemFunctionType_Native;
        
        // add native function name
        totemHashMapEntry *result = totemHashMap_Find(&build->NativeFunctionNamesLookup, name->Value, name->Length);
        if(result)
        {
            func->Address = (totemOperandXUnsigned)result->Value;
        }
        else
        {
            if(totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames) >= TOTEM_MAX_NATIVEFUNCTIONS)
            {
                return totemEvalStatus_Break(totemEvalStatus_TooManyNativeFunctions);
            }
            
            func->Address = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->NativeFunctionNames);
            
            if(totemMemoryBuffer_Insert(&build->NativeFunctionNames, name, 1) == NULL)
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
            
            if(!totemHashMap_Insert(&build->NativeFunctionNamesLookup, name->Value, name->Length, func->Address))
            {
                return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
            }
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalFunctionPointer(totemBuildPrototype *build, totemFunctionPointerPrototype *value, totemOperandRegisterPrototype *op)
{
    totemEvalStatus status = totemRegisterListPrototype_AddFunctionPointer(&build->GlobalRegisters, value, op);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, op);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_EvalAnonymousFunction(totemBuildPrototype *build, totemFunctionDeclarationPrototype *func, totemOperandRegisterPrototype *op)
{
    totemFunctionPointerPrototype ptr;
    ptr.Type = totemFunctionType_Script;
    
    totemHashMapEntry *result = totemHashMap_Find(&build->AnonymousFunctions, &func, sizeof(totemFunctionDeclarationPrototype*));
    if(result)
    {
        ptr.Address = (totemOperandXUnsigned)result->Value;
    }
    else
    {
        // increment script function count
        if(totemMemoryBuffer_GetNumObjects(&build->Functions) + 1 >= TOTEM_MAX_SCRIPTFUNCTIONS)
        {
            return totemEvalStatus_Break(totemEvalStatus_TooManyScriptFunctions);
        }
        
        totemOperandXUnsigned funcIndex = (totemOperandXUnsigned)totemMemoryBuffer_GetNumObjects(&build->Functions);
        totemScriptFunctionPrototype *functionPrototype;
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
        
        if(!totemHashMap_Insert(&build->AnonymousFunctions, &func, sizeof(totemFunctionDeclarationPrototype*), funcIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        ptr.Address = funcIndex;
    }
    
    // enqueue function declaration to be evaluated after the current one
    func->Next = NULL;
    if(build->AnonymousFunctionHead)
    {
        build->AnonymousFunctionTail->Next = func;
    }
    else
    {
        build->AnonymousFunctionHead = func;
    }
    
    build->AnonymousFunctionTail = func;
    
    return totemBuildPrototype_EvalFunctionPointer(build, &ptr, op);
}

totemEvalStatus totemBuildPrototype_EvalNamedFunctionPointer(totemBuildPrototype *build, totemString *name, totemOperandRegisterPrototype *op)
{
    totemFunctionPointerPrototype value;
    memset(&value, 0, sizeof(totemFunctionPointerPrototype));
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionName(build, name, &value));
    return totemBuildPrototype_EvalFunctionPointer(build, &value, op);
}

totemEvalStatus totemBuildPrototype_EvalType(totemBuildPrototype *build, totemPublicDataType type, totemOperandRegisterPrototype *operand)
{
    totemEvalStatus status = totemRegisterListPrototype_AddType(&build->GlobalRegisters, type, operand);
    if(status == totemEvalStatus_Success)
    {
        status = totemBuildPrototype_GlobalAssocCheck(build, operand);
    }
    
    return status;
}

totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype)
{
    totemBuildPrototype_Reset(build);
    
    // global function is always at index 0
    totemScriptFunctionPrototype *globalFunction;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &globalFunction));
    
    // eval global values
    build->LocalRegisters = NULL;
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    totemRegisterListPrototype_Reset(&build->GlobalRegisters);
    build->Flags = totemBuildPrototypeFlag_EvalVariables | totemBuildPrototypeFlag_EnforceVariableDefinitions;
    
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
                    build->ErrorContext = block->FuncDec;
                    return totemEvalStatus_Break(totemEvalStatus_ScriptFunctionAlreadyDefined);
                }
                
                // secure named function
                size_t functionIndex = totemMemoryBuffer_GetNumObjects(&build->Functions);
                totemScriptFunctionPrototype *functionPrototype = NULL;
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
                memcpy(&functionPrototype->Name, block->FuncDec->Identifier, sizeof(totemString));
                
                if (!totemHashMap_Insert(&build->FunctionLookup, block->FuncDec->Identifier->Value, block->FuncDec->Identifier->Length, functionIndex))
                {
                    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
                }
                
                // setup the function pointer
                totemOperandRegisterPrototype dummy;
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNamedFunctionPointer(build, block->FuncDec->Identifier, &dummy));
                
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
    build->Flags = totemBuildPrototypeFlag_None;
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
    
    // eval anonymous functions
    while (build->AnonymousFunctionHead)
    {
        totemFunctionDeclarationPrototype *func = build->AnonymousFunctionHead;
        build->AnonymousFunctionHead = build->AnonymousFunctionHead->Next;
        
        for (totemStatementPrototype *stmt = func->StatementsStart; stmt != NULL; stmt = stmt->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_EvalValues(stmt, build));
        }
    }
    
    // eval "global" function instructions first
    build->AnonymousFunctionHead = NULL;
    build->AnonymousFunctionTail = NULL;
    build->Flags = totemBuildPrototypeFlag_EvalVariables | totemBuildPrototypeFlag_EvalGlobalAssocs;
    globalFunction->InstructionsStart = 0;
    
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
    globalFunction->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.Registers);
    
    // now eval all other function instructions
    totemRegisterListPrototype localRegisters;
    totemRegisterListPrototype_Init(&localRegisters, totemOperandType_LocalRegister);
    build->LocalRegisters = &localRegisters;
    build->Flags |= totemBuildPrototypeFlag_EnforceVariableDefinitions;
    
    totemOperandXUnsigned funcIndex = 1;
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

totemEvalStatus totemBuildPrototype_AllocInstruction(totemBuildPrototype *build, totemInstruction **instructionOut)
{
    *instructionOut = totemMemoryBuffer_Secure(&build->Instructions, 1);
    if(*instructionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemScriptFunctionPrototype *funcPrototype)
{
    funcPrototype->InstructionsStart = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    totemRegisterListPrototype_Reset(build->LocalRegisters);
    
    // parameters
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemVariablePrototype_Eval(parameter, build, &dummy, totemEvalVariableFlag_LocalOnly));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build));
    }
    
    totemEvalStatus status = totemBuildPrototype_EvalImplicitReturn(build);
    funcPrototype->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&build->LocalRegisters->Registers);
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
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Simple, build, NULL, &result, totemEvalVariableFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &result));
            break;
            
        case totemStatementType_Return:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Return, build, NULL, &result, totemEvalVariableFlag_MustBeDefined));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, &result));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalArrayAccessEnd(totemBuildPrototype *build, totemOperandRegisterPrototype *lValue, totemOperandRegisterPrototype *lValueSrc, totemOperandRegisterPrototype *arrIndex)
{
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, lValueSrc, arrIndex, lValue, totemOperationType_ComplexSet));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, arrIndex));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, lValueSrc));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemOperandRegisterPrototype *lValueHint, totemOperandRegisterPrototype *result, totemEvalVariableFlag varFlags)
{
    totemOperandRegisterPrototype lValueSrc;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValueArgument, build, lValueHint, &lValueSrc, varFlags));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValueExpression, build, lValueHint, &lValueSrc, varFlags));
            break;
    }
    
    totemBool mutatedLValueRegisterUnary = totemBool_False;
    totemBool mutatedLValueRegisterBinary = totemBool_False;
    totemBool lValueRegisterIsArrayMember = totemBool_False;
    size_t numlValueArrayAccesses = 0;
    totemRegisterPrototypeFlag lValueSrcFlags = totemRegisterPrototypeFlag_None;
    totemRegisterListPrototype *lValueSrcScope = totemBuildPrototype_GetRegisterList(build, lValueSrc.RegisterScopeType);
    totemRegisterListPrototype_GetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, &lValueSrcFlags);
    
    // check the operators used against this lvalue to see if it gets mutated
    for(totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        if(op->Type == totemPreUnaryOperatorType_Dec || op->Type == totemPreUnaryOperatorType_Inc)
        {
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
            numlValueArrayAccesses++;
            lValueRegisterIsArrayMember = totemBool_True;
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
        // array accesses cannot be modified
        if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsConst) && lValueRegisterIsArrayMember)
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // if already assigned, and is const, throw an error
        if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsAssigned | totemRegisterPrototypeFlag_IsConst))
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // assignment expressions can only happen on mutable lValues (variables)
        if(!TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsVariable) && !lValueRegisterIsArrayMember)
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // ONLY vars can be const
        if(!TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsVariable) && TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsConst))
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueCannotBeConst);
        }
        
        // ensure the array src is valid
        if(lValueRegisterIsArrayMember && TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsValue))
        {
            return totemEvalStatus_Break(totemEvalStatus_AssignmentLValueNotMutable);
        }
        
        // this lvalue is now assigned
        totemRegisterListPrototype_SetRegisterFlags(lValueSrcScope, lValueSrc.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
    }
    
    // when lValue is an array member, we must retrieve it first and place it in a register before any other op
    totemOperandRegisterPrototype lValue;
    totemOperandRegisterPrototype arrIndex;
    memcpy(&lValue, &lValueSrc, sizeof(totemOperandRegisterPrototype));
    totemRegisterPrototypeFlag lValueFlags = lValueSrcFlags;
    totemRegisterListPrototype *lValueScope = lValueSrcScope;
    
    if(lValueRegisterIsArrayMember)
    {
        size_t arrayAccessesPerformed = 0;
        
        for(totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
        {
            if(op->Type == totemPostUnaryOperatorType_ArrayAccess)
            {
                // we don't need to hang onto all the array srcs, just the last one
                if(TOTEM_HASBITS(lValueSrcFlags, totemRegisterPrototypeFlag_IsTemporary))
                {
                    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(lValueSrcScope, &lValueSrc));
                }
                
                // lvalue is now the src
                memcpy(&lValueSrc, &lValue, sizeof(totemOperandRegisterPrototype));
                lValueSrcFlags = lValueFlags;
                lValueSrcScope = lValueScope;
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->ArrayAccess, build, NULL, &arrIndex, totemEvalVariableFlag_MustBeDefined));
                arrayAccessesPerformed++;
                
                if(arrayAccessesPerformed == numlValueArrayAccesses)
                {
                    // if this is a move, we can skip this array_get - we'll be grabbing the new value and performing a direct array_set further below
                    if(expression->BinaryOperator == totemBinaryOperatorType_Assign)
                    {
                        break;
                    }
                    
                    // if caller has given us a hint, we need to use it for the last array access
                    if(lValueHint)
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
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValueSrc, &arrIndex, totemOperationType_ComplexGet));
                
                lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
                totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
                
                if(arrayAccessesPerformed == numlValueArrayAccesses)
                {
                    break;
                }
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
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                totemString_FromLiteral(&preUnaryNumber, "0");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Equals));
                // A = B == C(0)
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &preUnaryLValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
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
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Add));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
                // already done above
                break;
                
            case totemPostUnaryOperatorType_Invocation:
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsValue))
                {
                    totemPublicDataType dataType = 0;
                    if(totemRegisterListPrototype_GetRegisterType(lValueScope, lValue.RegisterIndex, &dataType))
                    {
                        if(dataType != totemPublicDataType_Function)
                        {
                            return totemEvalStatus_Break(totemEvalStatus_InvalidDataType);
                        }
                    }
                }
                
                // retain pointer address
                totemOperandRegisterPrototype pointer;
                memcpy(&pointer, &lValue, sizeof(totemOperandRegisterPrototype));
                
                // replace lvalue, we're done with it
                if(lValueRegisterIsArrayMember && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary))
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalArrayAccessEnd(build, &lValue, &lValueSrc, &arrIndex));
                }
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &lValue));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &lValue));
                lValueRegisterIsArrayMember = totemBool_False;
                lValueScope = totemBuildPrototype_GetRegisterList(build, lValue.RegisterScopeType);
                totemRegisterListPrototype_GetRegisterFlags(lValueScope, lValue.RegisterIndex, &lValueFlags);
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsBegin(build, op->InvocationParametersStart));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &pointer, &pointer, totemOperationType_Function));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsEnd(build));
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemOperandRegisterPrototype rValue;
        memset(&rValue, 0, sizeof(totemOperandRegisterPrototype));
        totemBool recycleRValue = totemBool_False;
        totemBool recycleLValue = totemBool_False;
        
        if(expression->BinaryOperator == totemBinaryOperatorType_Assign)
        {
            if(lValueRegisterIsArrayMember || TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsTemporary))
            {
                // result of rValue expression can now be stored directly in lValue
                if(!lValueRegisterIsArrayMember && TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsTemporary))
                {
                    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_FreeRegister(lValueScope, &lValue));
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &lValue, totemEvalVariableFlag_MustBeDefined));
            }
            else
            {
                // otherwise we need the move op
                recycleRValue = totemBool_True;
                totemOperandRegisterPrototype *lValueToUse = NULL;
                
                if(TOTEM_HASBITS(lValueFlags, totemRegisterPrototypeFlag_IsVariable))
                {
                    lValueToUse = &lValue;
                }
                
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, lValueToUse, &rValue, totemEvalVariableFlag_MustBeDefined));
                
                // expression was evaled directly to lValue, so we no longer need the move
                if(lValueToUse == NULL || memcmp(lValueToUse, &rValue, sizeof(totemOperandRegisterPrototype)))
                {
                    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &rValue, &rValue, totemOperationType_Move));
                }
                
                memcpy(result, &lValue, sizeof(totemOperandRegisterPrototype));
            }
        }
        else
        {
            recycleRValue = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, NULL, &rValue, totemEvalVariableFlag_MustBeDefined));
            
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
    
    // finish the array-access if the value was mutated
    if(lValueRegisterIsArrayMember && (mutatedLValueRegisterUnary || mutatedLValueRegisterBinary))
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

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemOperandRegisterPrototype *hint,  totemOperandRegisterPrototype *value, totemEvalVariableFlag flags)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
        {
            totemEvalStatus status = totemVariablePrototype_Eval(argument->Variable, build, value, flags);
            if(status != totemEvalStatus_Success)
            {
                build->ErrorContext = argument;
            }
            
            return status;
        }
            
        case totemArgumentType_String:
            return totemBuildPrototype_EvalString(build, argument->String, value);
            
        case totemArgumentType_Number:
            return totemBuildPrototype_EvalNumber(build, argument->Number, value);
            
        case totemArgumentType_Type:
            return totemBuildPrototype_EvalType(build, argument->DataType, value);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, argument->FunctionDeclaration, value);
            
        case totemArgumentType_FunctionPointer:
            return totemBuildPrototype_EvalNamedFunctionPointer(build, argument->FunctionPointer, value);
            
        case totemArgumentType_FunctionCall:
            return totemFunctionCallPrototype_Eval(argument->FunctionCall, build, hint, value);
            
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
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(newArray->Accessor, build, NULL, &arraySize, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, value, hint));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &arraySize, &c, totemOperationType_NewArray));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &arraySize));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemBuildPrototype *build, totemOperandRegisterPrototype *index, totemEvalVariableFlag flags)
{
    totemBool justCreated = totemBool_False;
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    
    // check local scope first
    if(!totemRegisterListPrototype_GetVariable(localScope, &variable->Identifier, index))
    {
        totemBool varFound = totemBool_False;
        
        // check global second
        if(!TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsLocal))
        {
            varFound = totemRegisterListPrototype_GetVariable(&build->GlobalRegisters, &variable->Identifier, index);
            
            // HAS to be local? eep.
            if(varFound && TOTEM_GETBITS(flags, totemEvalVariableFlag_LocalOnly))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
            }
        }
        
        // var not found, declare it locally
        if(!varFound)
        {
            // ensure proper definition semantics
            if(!TOTEM_HASANYBITS(variable->Flags, totemVariablePrototypeFlag_IsDeclaration | totemVariablePrototypeFlag_IsConst)
               || TOTEM_HASBITS(flags, totemEvalVariableFlag_MustBeDefined))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableNotDefined);
            }
            
            // create local
            justCreated = totemBool_True;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(localScope, &variable->Identifier, index));
        }
    }
    
    if(TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EnforceVariableDefinitions))
    {
        // can't redeclare vars
        if(!justCreated && TOTEM_HASANYBITS(variable->Flags, totemVariablePrototypeFlag_IsDeclaration | totemVariablePrototypeFlag_IsConst))
        {
            return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_GlobalAssocCheck(build, index));
    
    totemRegisterListPrototype *scope = totemBuildPrototype_GetRegisterList(build, index->RegisterScopeType);
    
    if(TOTEM_HASBITS(variable->Flags, totemVariablePrototypeFlag_IsConst))
    {
        totemRegisterListPrototype_SetRegisterFlags(scope, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstructionSigned(totemBuildPrototype *build, totemOperandXSigned ax, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxSigned(instruction, ax));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstructionUnsigned(totemBuildPrototype *build, totemOperandXUnsigned ax, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxUnsigned(instruction, ax));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionSigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXSigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxSigned(instruction, bx));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXUnsigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(instruction, bx));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, b->RegisterIndex, b->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterC(instruction, c->RegisterIndex, c->RegisterScopeType));
    
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

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *dest)
{
    // free moved global registers if any still exist in local scope
    totemBuildPrototype_FreeGlobalAssocs(build, 0);
    
    totemOperandXUnsigned flags = totemReturnFlag_Register;
    totemOperandRegisterPrototype def;
    def.RegisterScopeType = totemOperandType_LocalRegister;
    def.RegisterIndex = 0;
    
    // implicit return
    if(dest == NULL)
    {
        dest = &def;
        flags = totemReturnFlag_None;
    }
    
    return totemBuildPrototype_EvalAbxInstructionUnsigned(build, dest, flags, totemOperationType_Return);
}

totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemBuildPrototype *build, totemOperandRegisterPrototype *hint, totemOperandRegisterPrototype *result)
{
    // lookup function
    totemFunctionPointerPrototype funcInfo;
    totemOperandRegisterPrototype funcPtrReg;
    memset(&funcInfo, 0, sizeof(totemFunctionPointerPrototype));
    memset(&funcPtrReg, 0, sizeof(totemOperandRegisterPrototype));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionName(build, &functionCall->Identifier, &funcInfo));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionPointer(build, &funcInfo, &funcPtrReg));
    
    TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalHint(build, result, hint));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsBegin(build, functionCall->ParametersStart));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, result, &funcPtrReg, &funcPtrReg, totemOperationType_Function));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalFunctionArgumentsEnd(build));
    
    if(!hint)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, result));
    }
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InvalidDataType);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_ScriptFunctionAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_NativeFunctionAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_FunctionNotDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_Success);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyRegisters);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_InstructionOverflow);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableAlreadyDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableNotDefined);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_AssignmentLValueCannotBeConst);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_AssignmentLValueNotMutable);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableAlreadyAssigned);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyNativeFunctions);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyScriptFunctions);
            TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyFunctionArguments);
    }
    
    return "UNKNOWN";
}