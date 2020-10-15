//
//  exec_interpreter.c
//  TotemScript
//
//  Created by Timothy Smale on 31/07/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <TotemScript/exec.h>

void totemInterpreter_Init(totemInterpreter *interpreter)
{
    totemScriptFile_Init(&interpreter->Script);
    totemParseTree_Init(&interpreter->ParseTree);
    totemBuildPrototype_Init(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

void totemInterpreter_Reset(totemInterpreter *interpreter)
{
    totemScriptFile_Reset(&interpreter->Script);
    totemParseTree_Reset(&interpreter->ParseTree);
    totemBuildPrototype_Reset(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

void totemInterpreter_Cleanup(totemInterpreter *interpreter)
{
    totemScriptFile_Cleanup(&interpreter->Script);
    totemParseTree_Cleanup(&interpreter->ParseTree);
    totemBuildPrototype_Cleanup(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

totemBool totemInterpreter_InterpretFile(totemInterpreter *interpreter, totemString *filename)
{
    totemScriptFile_Reset(&interpreter->Script);
    
	interpreter->Result.SubStatus.FileStatus = totemScriptFile_Load(&interpreter->Script, filename);
	if (interpreter->Result.SubStatus.FileStatus != totemLoadScriptStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_FileError;
        return totemBool_False;
    }
    
    totemString toParse;
	toParse.Length = totemCharBuffer_Num(&interpreter->Script.Buffer);
    toParse.Value = totemCharBuffer_Bottom(&interpreter->Script.Buffer);
    return totemInterpreter_InterpretString(interpreter, &toParse);
}

totemBool totemInterpreter_InterpretString(totemInterpreter *interpreter, totemString *string)
{
    // parse
    totemParseTree_Reset(&interpreter->ParseTree);
	interpreter->Result.SubStatus.ParseStatus = totemParseTree_Parse(&interpreter->ParseTree, string->Value, string->Length);
	if (interpreter->Result.SubStatus.ParseStatus != totemParseStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_ParseError;
        return totemBool_False;
    }
    
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

void internalError(char *buffer, size_t bufferLen, const char *desc)
{
    totem_snprintf(buffer, bufferLen, "Internal error: %s\n", desc);
}

void scriptError(char *buffer, size_t bufferLen, const char *desc, const char *src, const char *start, size_t len, size_t line, size_t cha)
{
    totem_snprintf(buffer, bufferLen, "Script error: %s in file %s on line %"PRISize":%"PRISize" : \"...%.*s...\"\n", desc, src, line, cha, len, start);
}

void outOfMemoryError(char *buffer, size_t bufferLen)
{
    totem_snprintf(buffer, bufferLen, "Not enough memory!\n");
}

totemScriptFileBlock *totemInterpreter_LookupFile(totemInterpreter *interpreter, const char *check)
{
	const char *start = totemCharBuffer_Bottom(&interpreter->Script.Buffer);
    size_t offsetInd = check - start;
    
	size_t max = totemScriptFileBlockBuffer_Num(&interpreter->Script.FileBlock);
    for (size_t i = 0; i < max; i++)
    {
		totemScriptFileBlock *offset = totemScriptFileBlockBuffer_At(&interpreter->Script.FileBlock, i);
        if (offset->Offset > offsetInd)
        {
            if (i == 0)
            {
                return NULL;
            }
            
            return offset - 1;
        }
    }
    
    return NULL;
}

void totemInterpreter_PrintResult(FILE *target, totemInterpreter *interpreter)
{
    char buffer[250];
    buffer[0] = 0;
    
    totemInterpreterResult *result = &interpreter->Result;
    
    switch (result->Status)
    {
        case totemInterpreterStatus_FileError:
			switch (result->SubStatus.FileStatus)
        {
            case totemLoadScriptStatus_FileNotFound:
            {
				totemScriptFileBlock *offendingOffset = totemScriptFileBlockBuffer_Top(&interpreter->Script.FileBlock);
                totemScriptFileBlock *parentOffset = NULL;
                
                const char *missingFile = NULL;
                const char *parentFile = NULL;
                
                if (offendingOffset)
                {
                    missingFile = offendingOffset->Value;
					parentOffset = totemScriptFileBlockBuffer_At(&interpreter->Script.FileBlock, offendingOffset->Parent);
                    
                    if (parentOffset && offendingOffset->Offset != 0)
                    {
                        parentFile = parentOffset->Value;
                    }
                }
                
                if (parentFile)
                {
                    totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "Could not find file %s - included in file %s\n", missingFile, parentFile);
                }
                else
                {
                    totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), "Could not find file %s\n", missingFile);
                }
                
                break;
            }
                
            case totemLoadScriptStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            default:
				internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemLoadScriptStatus_Describe(result->SubStatus.FileStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_LexError:
			switch (result->SubStatus.LexStatus)
        {
            case totemLexStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            default:
				internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemLexStatus_Describe(result->SubStatus.LexStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_ParseError:
            switch (result->SubStatus.ParseStatus)
        {
            case totemParseStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            case totemParseStatus_UnexpectedToken:
            case totemParseStatus_ValueTooLarge:
                if (interpreter->ParseTree.ErrorAt.Start)
                {
                    totemScriptFileBlock *offset = totemInterpreter_LookupFile(interpreter, interpreter->ParseTree.ErrorAt.Start);
                    
					scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemParseStatus_Describe(result->SubStatus.ParseStatus),
                                offset ? offset->Value : NULL,
                                interpreter->ParseTree.ErrorAt.Start,
                                interpreter->ParseTree.ErrorAt.Length,
                                offset ? interpreter->ParseTree.ErrorAt.LineNumber - offset->LineStart : interpreter->ParseTree.ErrorAt.LineNumber,
                                interpreter->ParseTree.ErrorAt.CharNumber);
                }
                else
                {
					scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemParseStatus_Describe(result->SubStatus.ParseStatus),
                                NULL,
                                NULL,
                                0,
                                0,
                                0);
                }
                
                break;
                
            default:
				internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemParseStatus_Describe(result->SubStatus.ParseStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_EvalError:
			switch (result->SubStatus.EvalStatus)
        {
            case totemEvalStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            case totemEvalStatus_InstructionOverflow:
            case totemEvalStatus_Success:
				internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemEvalStatus_Describe(result->SubStatus.EvalStatus));
                break;
                
            default:
                if (interpreter->Build.ErrorAt)
                {
                    totemScriptFileBlock *offset = totemInterpreter_LookupFile(interpreter, interpreter->Build.ErrorAt->Start);
                    
					scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemEvalStatus_Describe(result->SubStatus.EvalStatus),
                                offset ? offset->Value : NULL,
                                interpreter->Build.ErrorAt->Start,
                                interpreter->Build.ErrorAt->Length,
                                offset ? interpreter->Build.ErrorAt->LineNumber - offset->LineStart : interpreter->Build.ErrorAt->LineNumber,
                                interpreter->Build.ErrorAt->CharNumber);
                }
                else
                {
					scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemEvalStatus_Describe(result->SubStatus.EvalStatus),
                                NULL,
                                NULL,
                                0,
                                0,
                                0);
                }
                
                break;
        }
            
            break;
            
        default:
            break;
    }
    
    fprintf(target, "%s\n", buffer);
}
