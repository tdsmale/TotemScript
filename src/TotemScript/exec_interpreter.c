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
    totemTokenList_Init(&interpreter->TokenList);
    totemParseTree_Init(&interpreter->ParseTree);
    totemBuildPrototype_Init(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

void totemInterpreter_Reset(totemInterpreter *interpreter)
{
    totemScriptFile_Reset(&interpreter->Script);
    totemTokenList_Reset(&interpreter->TokenList);
    totemParseTree_Reset(&interpreter->ParseTree);
    totemBuildPrototype_Reset(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

void totemInterpreter_Cleanup(totemInterpreter *interpreter)
{
    totemScriptFile_Cleanup(&interpreter->Script);
    totemTokenList_Cleanup(&interpreter->TokenList);
    totemParseTree_Cleanup(&interpreter->ParseTree);
    totemBuildPrototype_Cleanup(&interpreter->Build);
    interpreter->Result.Status = totemInterpreterStatus_Success;
}

totemBool totemInterpreter_InterpretFile(totemInterpreter *interpreter, totemString *filename)
{
    totemScriptFile_Reset(&interpreter->Script);
    
    interpreter->Result.FileStatus = totemScriptFile_Load(&interpreter->Script, filename);
    if (interpreter->Result.FileStatus != totemLoadScriptStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_FileError;
        return totemBool_False;
    }
    
    totemString toParse;
    toParse.Length = interpreter->Script.Buffer.Length;
    toParse.Value = interpreter->Script.Buffer.Data;
    return totemInterpreter_InterpretString(interpreter, &toParse);
}

totemBool totemInterpreter_InterpretString(totemInterpreter *interpreter, totemString *string)
{
    // lex
    totemTokenList_Reset(&interpreter->TokenList);
    interpreter->Result.LexStatus = totemTokenList_Lex(&interpreter->TokenList, string->Value, string->Length);
    if (interpreter->Result.LexStatus != totemLexStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_LexError;
        return totemBool_False;
    }
    
    // parse
    totemParseTree_Reset(&interpreter->ParseTree);
    interpreter->Result.ParseStatus = totemParseTree_Parse(&interpreter->ParseTree, &interpreter->TokenList);
    if (interpreter->Result.ParseStatus != totemParseStatus_Success)
    {
        interpreter->Result.Status = totemInterpreterStatus_ParseError;
        return totemBool_False;
    }
    
    // eval
    totemBuildPrototype_Reset(&interpreter->Build);
    interpreter->Result.EvalStatus = totemBuildPrototype_Eval(&interpreter->Build, &interpreter->ParseTree);
    if (interpreter->Result.EvalStatus != totemEvalStatus_Success)
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
    totem_snprintf(buffer, bufferLen, "Script error: %s in file %s on line %"PRISize":%"PRISize" : \"...%.*s...\"\n", desc, src, line, cha, (int)15, start);
}

void outOfMemoryError(char *buffer, size_t bufferLen)
{
    totem_snprintf(buffer, bufferLen, "Not enough memory!\n");
}

totemScriptFileBlock *totemInterpreter_LookupFile(totemInterpreter *interpreter, const char *check)
{
    const char *start = interpreter->Script.Buffer.Data;
    size_t offsetInd = check - start;
    
    size_t max = totemMemoryBuffer_GetNumObjects(&interpreter->Script.FileBlock);
    for (size_t i = 0; i < max; i++)
    {
        totemScriptFileBlock *offset = totemMemoryBuffer_Get(&interpreter->Script.FileBlock, i);
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
            switch (result->FileStatus)
        {
            case totemLoadScriptStatus_FileNotFound:
            {
                totemScriptFileBlock *offendingOffset = totemMemoryBuffer_Top(&interpreter->Script.FileBlock);
                totemScriptFileBlock *parentOffset = NULL;
                
                const char *missingFile = NULL;
                const char *parentFile = NULL;
                
                if (offendingOffset)
                {
                    missingFile = offendingOffset->Value;
                    parentOffset = totemMemoryBuffer_Get(&interpreter->Script.FileBlock, offendingOffset->Parent);
                    
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
                internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemLoadScriptStatus_Describe(result->FileStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_LexError:
            switch (result->LexStatus)
        {
            case totemLexStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            default:
                internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemLexStatus_Describe(result->LexStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_ParseError:
            switch (result->ParseStatus)
        {
            case totemParseStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            case totemParseStatus_UnexpectedToken:
            case totemParseStatus_ValueTooLarge:
                if (interpreter->ParseTree.ErrorAt)
                {
                    totemScriptFileBlock *offset = totemInterpreter_LookupFile(interpreter, interpreter->ParseTree.ErrorAt->Start);
                    
                    scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemParseStatus_Describe(result->ParseStatus),
                                offset ? offset->Value : NULL,
                                interpreter->ParseTree.ErrorAt->Start,
                                interpreter->ParseTree.ErrorAt->Length,
                                offset ? interpreter->ParseTree.ErrorAt->LineNumber - offset->LineStart : interpreter->ParseTree.ErrorAt->LineNumber,
                                interpreter->ParseTree.ErrorAt->CharNumber);
                }
                else
                {
                    scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemParseStatus_Describe(result->ParseStatus),
                                NULL,
                                NULL,
                                0,
                                0,
                                0);
                }
                
                break;
                
            default:
                internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemParseStatus_Describe(result->ParseStatus));
                break;
        }
            
            break;
            
        case totemInterpreterStatus_EvalError:
            switch (result->EvalStatus)
        {
            case totemEvalStatus_OutOfMemory:
                outOfMemoryError(buffer, TOTEM_ARRAY_SIZE(buffer));
                break;
                
            case totemEvalStatus_InstructionOverflow:
            case totemEvalStatus_Success:
                internalError(buffer, TOTEM_ARRAY_SIZE(buffer), totemEvalStatus_Describe(result->EvalStatus));
                break;
                
            default:
                if (interpreter->Build.ErrorAt)
                {
                    totemScriptFileBlock *offset = totemInterpreter_LookupFile(interpreter, interpreter->Build.ErrorAt->Start);
                    
                    scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemEvalStatus_Describe(result->EvalStatus),
                                offset ? offset->Value : NULL,
                                interpreter->Build.ErrorAt->Start,
                                interpreter->Build.ErrorAt->Length,
                                offset ? interpreter->Build.ErrorAt->LineNumber - offset->LineStart : interpreter->Build.ErrorAt->LineNumber,
                                interpreter->Build.ErrorAt->CharNumber);
                }
                else
                {
                    scriptError(buffer, TOTEM_ARRAY_SIZE(buffer), totemEvalStatus_Describe(result->EvalStatus),
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
