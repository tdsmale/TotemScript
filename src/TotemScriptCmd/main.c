//
//  main.c
//  TotemScriptCmd
//
//  Created by Timothy Smale on 17/07/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <TotemScript/totem.h>

// todo: file, line, char & length for errors

const char *const version = "TotemScriptCmd\n"
"The MIT License (MIT)\n"
"Copyright (C) 2016 Timothy Smale\n"
"\n";

const char *const help = "usage: TotemScriptCmd [options] [filename|string]\n"
"Options:\n"
"-h / --help		Display this information and exit\n"
"-v / --version		Display version info and exit\n"
"-f / --file		Parse file specified by \"filename\"\n"
"-s / --string		Parse \"string\"\n"
"-d / --dump		Display bytecode before running\n"
"-p / --norun		Only parse bytecode\n"
"\n"
"http://github.com/tdsmale/TotemScript/\n";

#define TOTEM_CMD_ISARG(a, arg) (strncmp(a, arg, strlen(a)) == 0)

void internalError(const char *desc)
{
    fprintf(stderr, "Internal error: %s\n", desc);
}

void scriptError(const char *desc, const char *file, const char *start, size_t len)
{
    fprintf(stderr, "Script error: %s in file %s: %.*s\n", desc, file, (int)len, start);
}

void outOfMemoryError()
{
    fprintf(stderr, "Not enough memory!\n");
}

typedef enum
{
    totemCmdEvalFlags_None = 0,
    totemCmdEvalFlags_DumpInstructions = 1,
    totemCmdEvalFlags_DoNotRun = 1 << 1
}
totemCmdEvalFlags;

int run(totemInterpreter *interpreter, const char **argv, uint32_t argvSize)
{
    int ret = EXIT_SUCCESS;
    
    totemRuntime runtime;
    totemExecState execState;
    totemScript script;
    totemInstance instance;
    
    totemRuntime_Init(&runtime);
    totemExecState_Init(&execState);
    totemScript_Init(&script);
    totemInstance_Init(&instance);
    
    // load libs
    totemLinkStatus linkStatus = totemRuntime_LinkStdLib(&runtime);
    if (linkStatus != totemLinkStatus_Success)
    {
        printf("Could not load native libraries: %s\n", totemLinkStatus_Describe(linkStatus));
        ret = EXIT_FAILURE;
    }
    else
    {
        // link build
        totemLinkStatus linkStatus = totemRuntime_LinkBuild(&runtime, &interpreter->Build, &script);
        if (linkStatus != totemLinkStatus_Success)
        {
            printf("Could not register script: %s\n", totemLinkStatus_Describe(linkStatus));
            ret = EXIT_FAILURE;
        }
        else
        {
            // init instance
            linkStatus = totemScript_LinkInstance(&script, &instance);
            if (linkStatus != totemLinkStatus_Success)
            {
                printf("Could not create instance\n");
                ret = EXIT_FAILURE;
            }
            else
            {
                // init exec state
                linkStatus = totemRuntime_LinkExecState(&runtime, &execState, 256);
                if (linkStatus != totemLinkStatus_Success)
                {
                    printf("Could not create exec state\n");
                    ret = EXIT_FAILURE;
                }
                else
                {
                    totemRegister returnRegister;
                    memset(&returnRegister, 0, sizeof(totemRegister));
                    
                    totemExecStatus execStatus = totemExecState_SetArgV(&execState, argv, argvSize);
                    if (execStatus == totemExecStatus_OutOfMemory)
                    {
                        outOfMemoryError();
                        ret = EXIT_FAILURE;
                    }
                    else
                    {
                        // run script
                        totemInstanceFunction *func = totemMemoryBuffer_Get(&instance.LocalFunctions, 0);
                        totemExecStatus execStatus = totemExecState_ProtectedExec(&execState, func, &returnRegister);
                        switch (execStatus)
                        {
                            case totemExecStatus_OutOfMemory:
                                outOfMemoryError();
                                break;
                                
                            case totemExecStatus_Continue:
                                break;
                                
                            default:
                                scriptError(totemExecStatus_Describe(execStatus), "", "", 0);
                                break;
                        }
                    }
                }
            }
        }
    }
    
    totemInstance_Cleanup(&instance);
    totemScript_Cleanup(&script);
    totemExecState_Cleanup(&execState);
    totemRuntime_Cleanup(&runtime);
    
    return ret;
}

int eval(totemBool(*cb)(totemInterpreter*, const char*), const char *str, const char **argv, uint32_t argvSize, totemCmdEvalFlags flags)
{
    totem_Init();
    
    int ret = EXIT_SUCCESS;
    totemInterpreter interpreter;
    totemInterpreter_Init(&interpreter);
    
    if (!cb(&interpreter, str))
    {
        ret = EXIT_FAILURE;
        
        switch (interpreter.Result.Status)
        {
            case totemInterpreterStatus_FileError:
                switch (interpreter.Result.FileStatus)
            {
                case totemLoadScriptStatus_FileNotFound:
                    fprintf(stderr, "file not found!\n");
                    break;
                    
                case totemLoadScriptStatus_OutOfMemory:
                    outOfMemoryError();
                    break;
                    
                default:
                    internalError(totemLoadScriptStatus_Describe(interpreter.Result.FileStatus));
                    break;
            }
                
                break;
                
            case totemInterpreterStatus_LexError:
                switch (interpreter.Result.LexStatus)
            {
                case totemLexStatus_OutOfMemory:
                    outOfMemoryError();
                    break;
                    
                default:
                    internalError(totemLexStatus_Describe(interpreter.Result.LexStatus));
                    break;
            }
                
                break;
                
            case totemInterpreterStatus_ParseError:
                switch (interpreter.Result.ParseStatus)
            {
                case totemParseStatus_OutOfMemory:
                    outOfMemoryError();
                    break;
                    
                case totemParseStatus_UnexpectedToken:
                case totemParseStatus_ValueTooLarge:
                    if (interpreter.ParseTree.ErrorAt)
                    {
                        scriptError(totemParseStatus_Describe(interpreter.Result.ParseStatus), "", "", 0);
                    }
                    else
                    {
                        scriptError(totemParseStatus_Describe(interpreter.Result.ParseStatus), "", "", 0);
                    }
                    
                    break;
                    
                default:
                    internalError(totemParseStatus_Describe(interpreter.Result.ParseStatus));
                    break;
            }
                
                break;
                
            case totemInterpreterStatus_EvalError:
                switch (interpreter.Result.EvalStatus)
            {
                case totemEvalStatus_OutOfMemory:
                    outOfMemoryError();
                    break;
                    
                case totemEvalStatus_InstructionOverflow:
                case totemEvalStatus_Success:
                    internalError(totemEvalStatus_Describe(interpreter.Result.EvalStatus));
                    break;
                    
                default:
                    scriptError(totemEvalStatus_Describe(interpreter.Result.EvalStatus), "", "", 0);
                    break;
            }
                
                break;
                
            default:
                break;
        }
    }
    else
    {
        if (TOTEM_HASBITS(flags, totemCmdEvalFlags_DumpInstructions))
        {
            printf("******\n");
            printf("Instructions:\n");
            printf("******\n");
            totemInstruction_PrintList(stdout, totemMemoryBuffer_Get(&interpreter.Build.Instructions, 0), totemMemoryBuffer_GetNumObjects(&interpreter.Build.Instructions));
            printf("\n");
        }
        
        if (!TOTEM_HASBITS(flags, totemCmdEvalFlags_DoNotRun))
        {
            ret = run(&interpreter, argv, argvSize);
        }
    }
    
    totemInterpreter_Cleanup(&interpreter);
    totem_Cleanup();
    return ret;
}

totemBool evalFile(totemInterpreter *inter, const char *str)
{
    return totemInterpreter_InterpretFile(inter, str);
}

totemBool evalString(totemInterpreter *inter, const char *str)
{
    totemString val = TOTEM_STRING_VAL(str);
    return totemInterpreter_InterpretString(inter, &val);
}

int main(int argc, const char * argv[])
{
    const char *toParse = NULL;
    const char **scriptArgs = NULL;
    int numScriptArgs = 0;
    totemBool doHelp = totemBool_False;
    totemBool doVersion = totemBool_False;
    totemBool doFile = totemBool_False;
    totemBool doString = totemBool_False;
    totemCmdEvalFlags flags = totemCmdEvalFlags_None;
    
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        
        if (TOTEM_CMD_ISARG("--help", arg) || TOTEM_CMD_ISARG("-h", arg))
        {
            doHelp = totemBool_True;
        }
        else if (TOTEM_CMD_ISARG("--version", arg) || TOTEM_CMD_ISARG("-v", arg))
        {
            doVersion = totemBool_True;
        }
        else if (TOTEM_CMD_ISARG("--file", arg) || TOTEM_CMD_ISARG("-f", arg))
        {
            doFile = totemBool_True;
        }
        else if (TOTEM_CMD_ISARG("--string", arg) || TOTEM_CMD_ISARG("-s", arg))
        {
            doString = totemBool_True;
        }
        else if (TOTEM_CMD_ISARG("--dump", arg) || TOTEM_CMD_ISARG("-d", arg))
        {
            TOTEM_SETBITS(flags, totemCmdEvalFlags_DumpInstructions);
        }
        else if (TOTEM_CMD_ISARG("--norun", arg) || TOTEM_CMD_ISARG("-p", arg))
        {
            TOTEM_SETBITS(flags, totemCmdEvalFlags_DoNotRun);
        }
        else
        {
            toParse = arg;
            
            if (i < argc - 1)
            {
                numScriptArgs = argc - i - 1;
                scriptArgs = argv + i + 1;
            }
            
            break;
        }
    }
    
    if (doVersion)
    {
        fprintf(stdout, "%s", version);
        return EXIT_SUCCESS;
    }
    else if (doHelp)
    {
        fprintf(stdout, "%s", help);
        return EXIT_SUCCESS;
    }
    else if (doFile)
    {
        return eval(evalFile, toParse, scriptArgs, numScriptArgs, flags);
    }
    else if (doString)
    {
        return eval(evalString, toParse, scriptArgs, numScriptArgs, flags);
    }
    else
    {
        fprintf(stderr, "%s", help);
        return EXIT_FAILURE;
    }
}