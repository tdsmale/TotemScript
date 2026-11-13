//
//  exec_interpreter.c
//  TotemScript
//
//  Created by Timothy Smale on 31/07/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>
#include <TotemScript/log.h>

static void totemInterpreter_Init(totemInterpreter* interpreter)
{
    totemBuildPrototype_Init(&interpreter->Build, &interpreter->ParseTree);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

void totemInterpreter_InitWithString(totemInterpreter *interpreter, const char *str, size_t strLen)
{
    totemParseTree_InitWithString(&interpreter->ParseTree, str, strLen);
    totemInterpreter_Init(interpreter);
}

void totemInterpreter_InitWithFile(totemInterpreter *interpreter, FILE *file)
{
    totemParseTree_InitWithFile(&interpreter->ParseTree, file);
    totemInterpreter_Init(interpreter);
}

void totemInterpreter_Reset(totemInterpreter *interpreter)
{
    totemParseTree_Reset(&interpreter->ParseTree);
    totemBuildPrototype_Reset(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

void totemInterpreter_Cleanup(totemInterpreter *interpreter)
{
    totemParseTree_Cleanup(&interpreter->ParseTree);
    totemBuildPrototype_Cleanup(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

static totemBool totemInterpreter_Eval(totemInterpreter* interpreter)
{
    // eval
    totemBuildPrototype_Reset(&interpreter->Build);
    interpreter->Result.SubStatus.EvalStatus = totemBuildPrototype_Eval(&interpreter->Build, &interpreter->ParseTree);
    if (interpreter->Result.SubStatus.EvalStatus != totemEvalStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_EvalError;
        return totemBool_False;
    }

    interpreter->Result.Status = totemInterpreterStatus_Success;
    return totemBool_True;
}

totemBool totemInterpreter_Interpret(totemInterpreter *interpreter)
{
    // parse
    totemParseTree_Reset(&interpreter->ParseTree);
    interpreter->Result.SubStatus.ParseStatus = totemParseTree_Parse(&interpreter->ParseTree);
    if (interpreter->Result.SubStatus.ParseStatus != totemParseStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_ParseError;
        return totemBool_False;
    }

    return totemInterpreter_Eval(interpreter);
}

static void internalError(totemLog *dst, const char *desc)
{
    totemLog_Write(dst, "Internal error: %s\n", desc);
}

static void scriptError(totemLog *dst, const char *desc, size_t line, size_t cha)
{
    totemLog_Write(dst, "Script error: %s on line %"PRISize":%"PRISize"\n", desc, line, cha);
}

static void outOfMemoryError(totemLog *dst)
{
    totemLog_Write(dst, "Not enough memory!\n");
}

void totemInterpreter_PrintResult(totemInterpreter *interpreter, totemLog *target)
{
    totemInterpreterResult *result = &interpreter->Result;
    
    switch (result->Status)
    {
        case totemInterpreterStatus_ParseError:
            switch (result->SubStatus.ParseStatus)
        {
            case totemParseStatus_OutOfMemory:
                outOfMemoryError(target);
                break;
                
            case totemParseStatus_UnexpectedToken:
            case totemParseStatus_ValueTooLarge:
                if (interpreter->ParseTree.ErrorAt.Start)
                {
					scriptError(target, totemParseStatus_Describe(result->SubStatus.ParseStatus),
                        interpreter->ParseTree.ErrorAt.LineNumber,
                        interpreter->ParseTree.ErrorAt.CharNumber);
                }
                else
                {
					scriptError(target, totemParseStatus_Describe(result->SubStatus.ParseStatus), 0, 0);
                }
                
                break;
                
            default:
				internalError(target, totemParseStatus_Describe(result->SubStatus.ParseStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_EvalError:
			switch (result->SubStatus.EvalStatus)
        {
            case totemEvalStatus_OutOfMemory:
                outOfMemoryError(target);
                break;
                
            case totemEvalStatus_InstructionOverflow:
            case totemEvalStatus_Success:
				internalError(target, totemEvalStatus_Describe(result->SubStatus.EvalStatus));
                break;
                
            default:
                if (interpreter->Build.ErrorAt)
                {
					scriptError(target, totemEvalStatus_Describe(result->SubStatus.EvalStatus),
                        interpreter->ParseTree.ErrorAt.LineNumber,
                        interpreter->ParseTree.ErrorAt.CharNumber);
                }
                else
                {
					scriptError(target, totemEvalStatus_Describe(result->SubStatus.EvalStatus), 0, 0);
                }
                
                break;
        }
            
            break;
            
        default:
            break;
    }
}
