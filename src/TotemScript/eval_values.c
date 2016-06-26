//
//  eval_values.c
//  TotemScript
//
//  Created by Timothy Smale on 10/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>

totemEvalStatus totemFunctionCallPrototype_EvalValues(totemFunctionCallPrototype *call, totemBuildPrototype *build)
{
    for (totemExpressionPrototype *exp = call->ParametersStart; exp != NULL; exp = exp->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(exp, build, totemEvalVariableFlag_MustBeDefined));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_EvalValues(totemArgumentPrototype *arg, totemBuildPrototype *build, totemEvalVariableFlag varFlags)
{
    totemOperandRegisterPrototype dummy;
    
    switch (arg->Type)
    {
        case totemArgumentType_Number:
            return totemBuildPrototype_EvalNumber(build, arg->Number, &dummy);
            
        case totemArgumentType_String:
            return totemBuildPrototype_EvalString(build, arg->String, &dummy);
            
        case totemArgumentType_FunctionCall:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNamedFunctionPointer(build, &arg->FunctionCall->Identifier, &dummy));
            return totemFunctionCallPrototype_EvalValues(arg->FunctionCall, build);
            
        case totemArgumentType_FunctionPointer:
            return totemBuildPrototype_EvalNamedFunctionPointer(build, arg->FunctionPointer, &dummy);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, arg->FunctionDeclaration, &dummy);
            
        case totemArgumentType_Type:
            return totemBuildPrototype_EvalType(build, arg->DataType, &dummy);
            
        case totemArgumentType_Variable:
            if (TOTEM_HASBITS(build->Flags, totemBuildPrototypeFlag_EvalVariables))
            {
                return totemVariablePrototype_Eval(arg->Variable, build, &dummy, varFlags);
            }
            break;
            
        case totemArgumentType_NewArray:
        case totemArgumentType_NewObject:
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_EvalValues(totemExpressionPrototype *expression, totemBuildPrototype *build, totemEvalVariableFlag varFlags)
{
    switch (expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalValues(expression->LValueArgument, build, varFlags));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->LValueExpression, build, varFlags));
            break;
    }
    
    for (totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        totemString preUnaryNumber;
        memset(&preUnaryNumber, 0, sizeof(totemString));
        
        totemOperandRegisterPrototype preUnaryRegister;
        memset(&preUnaryNumber, 0, sizeof(totemOperandRegisterPrototype));
        
        switch (op->Type)
        {
            case totemPreUnaryOperatorType_Dec:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                totemString_FromLiteral(&preUnaryNumber, "0");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                // A = B == C(0)
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister));
                // A = B * -1
                break;
                
            case totemPreUnaryOperatorType_None:
                break;
        }
    }
    
    for (totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        totemString postUnaryNumber;
        memset(&postUnaryNumber, 0, sizeof(totemString));
        
        totemOperandRegisterPrototype postUnaryRegister;
        memset(&postUnaryNumber, 0, sizeof(totemOperandRegisterPrototype));
        
        switch (op->Type)
        {
            case totemPostUnaryOperatorType_Dec:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(op->ArrayAccess, build, NULL, &postUnaryRegister, totemEvalVariableFlag_MustBeDefined));
                break;
                
            case totemPostUnaryOperatorType_Invocation:
                for (totemExpressionPrototype *parameter = op->InvocationParametersStart; parameter != NULL; parameter = parameter->Next)
                {
                    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(parameter, build, totemEvalVariableFlag_MustBeDefined));
                }
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    if (expression->RValue)
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->RValue, build, totemEvalVariableFlag_MustBeDefined));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemStatementPrototype_EvalValues(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    switch (statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            return totemDoWhileLoopPrototype_EvalValues(statement->DoWhileLoop, build);
            
        case totemStatementType_ForLoop:
            return totemForLoopPrototype_EvalValues(statement->ForLoop, build);
            
        case totemStatementType_IfBlock:
            return totemIfBlockPrototype_EvalValues(statement->IfBlock, build);
            
        case totemStatementType_WhileLoop:
            return totemWhileLoopPrototype_EvalValues(statement->WhileLoop, build);
            
        case totemStatementType_Simple:
            return totemExpressionPrototype_EvalValues(statement->Simple, build, totemEvalVariableFlag_None);
            
        case totemStatementType_Return:
            return totemExpressionPrototype_EvalValues(statement->Return, build, totemEvalVariableFlag_MustBeDefined);
    }
    
    return totemEvalStatus_Success;
}