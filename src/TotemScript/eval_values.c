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

totemEvalStatus totemArgumentPrototype_EvalValues(totemArgumentPrototype *arg, totemBuildPrototype *build)
{
    totemOperandRegisterPrototype dummy;
    
    switch (arg->Type)
    {
        case totemArgumentType_Null:
            return totemBuildPrototype_EvalNull(build, &dummy, NULL);
            
        case totemArgumentType_Boolean:
            return totemBuildPrototype_EvalBoolean(build, arg->Boolean, &dummy, NULL);
            
        case totemArgumentType_Number:
            return totemBuildPrototype_EvalNumber(build, arg->Number, &dummy, NULL);
            
        case totemArgumentType_String:
            return totemBuildPrototype_EvalString(build, arg->String, &dummy, NULL);
            
        case totemArgumentType_FunctionDeclaration:
            return totemBuildPrototype_EvalAnonymousFunction(build, arg->FunctionDeclaration, &dummy, NULL);
            
        case totemArgumentType_Type:
            return totemBuildPrototype_EvalType(build, arg->DataType, &dummy, NULL);
            
        case totemArgumentType_NewArray:
        {
            totemInt num = 0;
            for (totemExpressionPrototype *exp = arg->NewArray->Accessor; exp; exp = exp->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(exp, build));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, num, &dummy, NULL));
                num++;
            }
            
            break;
        }
            
        case totemArgumentType_NewObject:
        {
            totemExpressionPrototype *values = arg->NewObject->Values;
            totemExpressionPrototype *keys = arg->NewObject->Keys;
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, arg->NewObject->Num, &dummy, NULL));
            
            while (values)
            {
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(keys, build));
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(values, build));
                values = values->Next;
                keys = keys->Next;
            }
            
            break;
        }
            
        default:
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_EvalValues(totemExpressionPrototype *expression, totemBuildPrototype *build)
{
    switch (expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalValues(expression->LValueArgument, build));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->LValueExpression, build));
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
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                break;
                
            case totemPreUnaryOperatorType_Inc:
                totemString_FromLiteral(&preUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                break;
                
            case totemPreUnaryOperatorType_LogicalNegate:
                totemString_FromLiteral(&preUnaryNumber, "0");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                // A = B == C(0)
                break;
                
            case totemPreUnaryOperatorType_Negative:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
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
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister, NULL));
                break;
                
            case totemPostUnaryOperatorType_Inc:
                totemString_FromLiteral(&postUnaryNumber, "1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &postUnaryNumber, &postUnaryRegister, NULL));
                break;
                
            case totemPostUnaryOperatorType_ArrayAccess:
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(op->ArrayAccess, build));
                break;
                
            case totemPostUnaryOperatorType_Invocation:
                for (totemExpressionPrototype *parameter = op->InvocationParametersStart; parameter != NULL; parameter = parameter->Next)
                {
                    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(parameter, build));
                }
                break;
                
            case totemPostUnaryOperatorType_None:
                break;
        }
    }
    
    if (expression->RValue)
    {
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->RValue, build));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemStatementPrototype_EvalValues(totemStatementPrototype *statement, totemBuildPrototype *build)
{
    //totemBuildPrototypeFlag prevFlags = build->Flags;
    totemEvalStatus status = totemEvalStatus_Success;
    
    switch (statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            //TOTEM_UNSETBITS(build->Flags, totemBuildPrototypeFlag_EvalVariables);
            status = totemDoWhileLoopPrototype_EvalValues(statement->DoWhileLoop, build);
            //build->Flags = prevFlags;
            break;
            
        case totemStatementType_ForLoop:
            //TOTEM_UNSETBITS(build->Flags, totemBuildPrototypeFlag_EvalVariables);
            status = totemForLoopPrototype_EvalValues(statement->ForLoop, build);
            //build->Flags = prevFlags;
            break;
            
        case totemStatementType_IfBlock:
            //TOTEM_UNSETBITS(build->Flags, totemBuildPrototypeFlag_EvalVariables);
            status = totemIfBlockPrototype_EvalValues(statement->IfBlock, build);
            //build->Flags = prevFlags;
            break;
            
        case totemStatementType_WhileLoop:
            //TOTEM_UNSETBITS(build->Flags, totemBuildPrototypeFlag_EvalVariables);
            status = totemWhileLoopPrototype_EvalValues(statement->WhileLoop, build);
            //build->Flags = prevFlags;
            break;
            
        case totemStatementType_Simple:
            status = totemExpressionPrototype_EvalValues(statement->Simple, build);
            break;
            
        case totemStatementType_Return:
            status = totemExpressionPrototype_EvalValues(statement->Return, build);
            break;
    }
    
    return status;
}