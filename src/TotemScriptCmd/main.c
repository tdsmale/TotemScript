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

typedef struct
{
    totemInterpreter Interpreter;
    totemRuntime Runtime;
    totemExecState ExecState;
    totemScript Script;
}
totemCmdState;

void totemCmdState_Init(totemCmdState *state)
{
    totemInterpreter_Init(&state->Interpreter);
    totemRuntime_Init(&state->Runtime);
    totemExecState_Init(&state->ExecState);
    totemScript_Init(&state->Script);
}

void totemCmdState_Cleanup(totemCmdState *state)
{
    totemScript_Cleanup(&state->Script);
    totemExecState_Cleanup(&state->ExecState);
    totemRuntime_Cleanup(&state->Runtime);
    totemInterpreter_Cleanup(&state->Interpreter);
}

int totemCmdState_Run(totemCmdState *state, const char **argv, int argc)
{
    // load libs
    totemLinkStatus linkStatus = totemRuntime_LinkStdLib(&state->Runtime);
    if (linkStatus != totemLinkStatus_Success)
    {
        printf("Could not load native libraries: %s\n", totemLinkStatus_Describe(linkStatus));
        return EXIT_FAILURE;
    }
    
    // link build
    linkStatus = totemRuntime_LinkBuild(&state->Runtime, &state->Interpreter.Build, &state->Script);
    if (linkStatus != totemLinkStatus_Success)
    {
        printf("Could not register script: %s\n", totemLinkStatus_Describe(linkStatus));
        return EXIT_FAILURE;
    }
    
    // init exec state
    linkStatus = totemRuntime_LinkExecState(&state->Runtime, &state->ExecState, 256);
    if (linkStatus != totemLinkStatus_Success)
    {
        printf("Could not create exec state: %s\n", totemLinkStatus_Describe(linkStatus));
        return EXIT_FAILURE;
    }
    
    // init instance
    totemGCObject *instance = NULL;
    totemExecStatus execStatus = totemExecState_CreateInstance(&state->ExecState, &state->Script, &instance);
    if (execStatus != totemExecStatus_Continue)
    {
        printf("Could not create instance: %s\n", totemExecStatus_Describe(execStatus));
        return EXIT_FAILURE;
    }
    
    // set arguments
    totemExecState_SetArgV(&state->ExecState, argv, argc);
    
    // run script
    totemInstanceFunction *func = totemMemoryBuffer_Get(&instance->Instance->LocalFunctions, 0);
    execStatus = totemExecState_Exec(&state->ExecState, func);
    if (execStatus != totemExecStatus_Continue)
    {
        printf("runtime error: %s at line %i\n", totemExecStatus_Describe(execStatus), 0);
    }
    
    return EXIT_SUCCESS;
}

int main(int argc, const char **argv)
{
    totemCmdState state;
    const char *toParse = NULL;
    const char **scriptArgs = NULL;
    int numScriptArgs = 0;
    totemBool doHelp = totemBool_False;
    totemBool doVersion = totemBool_False;
    totemBool doFile = totemBool_False;
    totemBool doString = totemBool_False;
    totemBool dumpInstructions = totemBool_False;
    totemBool doNotRun = totemBool_False;
    
    if (argc <= 1)
    {
        fprintf(stderr, "%s", help);
        return EXIT_FAILURE;
    }
    
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
            dumpInstructions = totemBool_True;
        }
        else if (TOTEM_CMD_ISARG("--norun", arg) || TOTEM_CMD_ISARG("-p", arg))
        {
            doNotRun = totemBool_True;
        }
        else
        {
            toParse = arg;
            
            if (i < argc - 1)
            {
                scriptArgs = argv + i + 1;
                numScriptArgs = argc - i - 1;
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
    else if (toParse)
    {
        totem_Init();
        totemCmdState_Init(&state);
        totemBool parseResult = totemBool_False;
        totemString toParseStr = TOTEM_STRING_VAL(toParse);
        
        if (doFile)
        {
            parseResult = totemInterpreter_InterpretFile(&state.Interpreter, &toParseStr);
        }
        else if (doString)
        {
            parseResult = totemInterpreter_InterpretString(&state.Interpreter, &toParseStr);
        }
        
        if (!parseResult)
        {
            totemInterpreter_PrintResult(stderr, &state.Interpreter);
            ret = EXIT_FAILURE;
        }
        else
        {
            if (dumpInstructions)
            {
                printf("******\n");
                printf("Instructions:\n");
                printf("******\n");
                totemInstruction_PrintList(stdout, totemMemoryBuffer_Bottom(&state.Interpreter.Build.Instructions), totemMemoryBuffer_GetNumObjects(&state.Interpreter.Build.Instructions));
                printf("\n");
            }
            
            if (!doNotRun)
            {
                ret = totemCmdState_Run(&state, scriptArgs, numScriptArgs);
            }
        }
        
        totemCmdState_Cleanup(&state);
        totem_Cleanup();
    }
    else
    {
        fprintf(stderr, "%s", help);
        ret = EXIT_FAILURE;
    }
    
    return ret;
}