//
//  main.c
//  TotemScriptCmd
//
//  Created by Timothy Smale on 17/07/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/totem.h>
#include <stdlib.h>
#include <string.h>

// todo: file, line, char & length for errors

const char *const version = "TotemScriptCmd 0.1\n"
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

typedef totemBool(*totemCmd_EvalCb)(totemInterpreter*, totemString*);

int totemCmd_SimpleRun(totemCmd_EvalCb cb, totemString *str, const char **argv, uint32_t argvSize, totemCmdFlags flags)
{
    totem_Init();
    
    int ret = EXIT_SUCCESS;
    totemInterpreter interpreter;
    totemInterpreter_Init(&interpreter);
    
    if (!cb(&interpreter, str))
    {
        ret = EXIT_FAILURE;
        totemInterpreter_PrintResult(stderr, &interpreter);
    }
    else
    {
        if (TOTEM_HASBITS(flags, totemCmdFlags_DumpInstructions))
        {
            printf("******\n");
            printf("Instructions:\n");
            printf("******\n");
            totemInstruction_PrintList(stdout, totemMemoryBuffer_Get(&interpreter.Build.Instructions, 0), totemMemoryBuffer_GetNumObjects(&interpreter.Build.Instructions));
            printf("\n");
        }
        
        if (!TOTEM_HASBITS(flags, totemCmdFlags_DoNotRun))
        {
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
                totemLinkStatus linkStatus = totemRuntime_LinkBuild(&runtime, &interpreter.Build, &script);
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
                        printf("Could not create instance: %s\n", totemLinkStatus_Describe(linkStatus));
                        ret = EXIT_FAILURE;
                    }
                    else
                    {
                        // init exec state
                        linkStatus = totemRuntime_LinkExecState(&runtime, &execState, 256);
                        if (linkStatus != totemLinkStatus_Success)
                        {
                            printf("Could not create exec state: %s\n", totemLinkStatus_Describe(linkStatus));
                            ret = EXIT_FAILURE;
                        }
                        else
                        {
                            totemRegister returnRegister;
                            memset(&returnRegister, 0, sizeof(totemRegister));
                            
                            totemExecStatus execStatus = totemExecState_SetArgV(&execState, argv, argvSize);
                            if (execStatus == totemExecStatus_OutOfMemory)
                            {
                                printf("Could not set argv: %s\n", totemExecStatus_Describe(execStatus));
                                ret = EXIT_FAILURE;
                            }
                            else
                            {
                                // run script
                                totemInstanceFunction *func = totemMemoryBuffer_Get(&instance.LocalFunctions, 0);
                                totemExecStatus execStatus = totemExecState_ProtectedExec(&execState, func, &returnRegister);
                                if (execStatus != totemExecStatus_Continue)
                                {
                                    printf("runtime error: %s at line %i\n", totemExecStatus_Describe(execStatus), 0);
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
        }
    }
    
    totemInterpreter_Cleanup(&interpreter);
    totem_Cleanup();
    return ret;
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
    totemCmdFlags flags = totemCmdFlags_None;
    
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
            TOTEM_SETBITS(flags, totemCmdFlags_DumpInstructions);
        }
        else if (TOTEM_CMD_ISARG("--norun", arg) || TOTEM_CMD_ISARG("-p", arg))
        {
            TOTEM_SETBITS(flags, totemCmdFlags_DoNotRun);
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
    
    int ret = EXIT_SUCCESS;
    
    if (doVersion)
    {
        fprintf(stdout, "%s", version);
    }
    else if (doHelp)
    {
        fprintf(stdout, "%s", help);
    }
    else if (doFile)
    {
        totemString toParseStr = TOTEM_STRING_VAL(toParse);
        ret = totemCmd_SimpleRun(totemInterpreter_InterpretFile, &toParseStr, scriptArgs, numScriptArgs, flags);
    }
    else if (doString)
    {	
        totemString toParseStr = TOTEM_STRING_VAL(toParse);
        ret = totemCmd_SimpleRun(totemInterpreter_InterpretString, &toParseStr, scriptArgs, numScriptArgs, flags);
    }
    else
    {
        fprintf(stderr, "%s", help);
        ret = EXIT_FAILURE;
    }
    
    return ret;
}