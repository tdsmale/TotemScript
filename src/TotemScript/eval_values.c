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
    totemOperandPrototype dummy;
    
    switch (arg->Type)
    {
        case totemArgumentType_Null:
            return totemBuildPrototype_EvalNull(build, &dummy, NULL);
            
        case totemArgumentType_Boolean:
			return totemBuildPrototype_EvalBoolean(build, arg->Value.Boolean, &dummy, NULL);
            
        case totemArgumentType_Number:
			return totemBuildPrototype_EvalNumber(build, arg->Value.Number, &dummy, NULL);
            
        case totemArgumentType_String:
			return totemBuildPrototype_EvalString(build, arg->Value.String, &dummy, NULL);
            
        case totemArgumentType_FunctionDeclaration:
			return totemBuildPrototype_EvalAnonymousFunction(build, arg->Value.FunctionDeclaration, &dummy, NULL);
            
        case totemArgumentType_Type:
			return totemBuildPrototype_EvalType(build, arg->Value.DataType, &dummy, NULL);
            
        case totemArgumentType_NewArray:
        {
            totemInt num = 0;
			for (totemExpressionPrototype *exp = arg->Value.NewArray->Accessor; exp; exp = exp->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(exp, build));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, num, &dummy, NULL));
                num++;
            }
            
            break;
        }
            
        case totemArgumentType_NewObject:
        {
			totemExpressionPrototype *values = arg->Value.NewObject->Values;
			totemExpressionPrototype *keys = arg->Value.NewObject->Keys;
			TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalInt(build, arg->Value.NewObject->Num, &dummy, NULL));
            
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
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_EvalValues(expression->LValue.Argument, build));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(expression->LValue.Expression, build));
            break;
    }
    
    for (totemPreUnaryOperatorPrototype *op = expression->PreUnaryOperators; op != NULL; op = op->Next)
    {
        totemString preUnaryNumber;
		TOTEM_ZERO(&preUnaryNumber);
        
        totemOperandPrototype preUnaryRegister;
		TOTEM_ZERO(&preUnaryNumber);
        
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
                break;
                
            case totemPreUnaryOperatorType_Negate:
                totemString_FromLiteral(&preUnaryNumber, "-1");
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalNumber(build, &preUnaryNumber, &preUnaryRegister, NULL));
                break;
                
            case totemPreUnaryOperatorType_None:
                break;
        }
    }
    
    for (totemPostUnaryOperatorPrototype *op = expression->PostUnaryOperators; op != NULL; op = op->Next)
    {
        totemString postUnaryNumber;
		TOTEM_ZERO(&postUnaryNumber);
        
        totemOperandPrototype postUnaryRegister;
		TOTEM_ZERO(&postUnaryNumber);
        
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
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_EvalValues(op->Parameters, build));
                break;
                
            case totemPostUnaryOperatorType_Invocation:
                for (totemExpressionPrototype *parameter = op->Parameters; parameter != NULL; parameter = parameter->Next)
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
    totemEvalStatus status = totemEvalStatus_Success;
    
    switch (statement->Type)
    {
        case totemStatementType_DoWhileLoop:
			status = totemDoWhileLoopPrototype_EvalValues(statement->Ptr.DoWhileLoop, build);
            break;
            
        case totemStatementType_ForLoop:
            status = totemForLoopPrototype_EvalValues(statement->Ptr.ForLoop, build);
            break;
            
        case totemStatementType_IfBlock:
			status = totemIfBlockPrototype_EvalValues(statement->Ptr.IfBlock, build);
            break;
            
        case totemStatementType_WhileLoop:
			status = totemWhileLoopPrototype_EvalValues(statement->Ptr.WhileLoop, build);
            break;
            
        case totemStatementType_Simple:
			status = totemExpressionPrototype_EvalValues(statement->Ptr.Simple, build);
            break;
            
        case totemStatementType_Return:
			status = totemExpressionPrototype_EvalValues(statement->Ptr.Return, build);
            break;
    }
    
    return status;
}