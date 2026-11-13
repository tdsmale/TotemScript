//
//  main.c
//  TotemScriptCmd
//
//  Created by Timothy Smale on 17/07/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/totem.h>

#define TOTEM_MAX_TERMINAL_INPUT 2048

// todo: file, line, char & length for errors

#define TOTEM_EMIT_CMDS(_) \
    _(h, help,      "Display this information and exit") \
    _(a, printargs, "Display input arguments") \
    _(v, version,   "Display version info and exit") \
    _(t, terminal,  "Run terminal (max length: " TOTEM_STRINGIFY(TOTEM_MAX_TERMINAL_INPUT) ")") \
    _(f, file,      "Parse file specified by \"filename\"") \
    _(s, string,    "Parse \"string\"") \
    _(d, dump,      "Display bytecode before running") \
    _(p, parseonly, "Only parse bytecode") \

const char *const version = 
"TotemScriptCmd " TOTEM_VERSION_STRING "\n"
"The MIT License (MIT)\n"
"Copyright (C) 2016 Timothy Smale\n"
"\n";

#define TOTEM_EMIT_CMD(char, name, info) "-" #char "/ -" #name " (" info ")\n"
const char *const help = "usage: TotemScriptCmd [options] [filename|string]\n"
"Options:\n"
TOTEM_EMIT_CMDS(TOTEM_EMIT_CMD)
"\n"
"http://github.com/tdsmale/TotemScript/\n";
#undef TOTEM_EMIT_CMD

#define TOTEM_CMD_ISARG(a, arg) (strncmp(a, arg, strlen(a)) == 0)

typedef struct totemCmdState
{
    totemInterpreter Interpreter;
    totemRuntime Runtime;
    totemExecState ExecState;
    totemScript Script;
    FILE *Src;
}
totemCmdState;

void totemCmdState_Init(totemCmdState *state, const char **argv, int argc)
{
    totemRuntime_Init(&state->Runtime);
    totemExecState_Init(&state->ExecState);
    totemScript_Init(&state->Script);

    // load libs
    totemLinkStatus linkStatus = totemRuntime_LinkStdLib(&state->Runtime);
    if (linkStatus != totemLinkStatus_Success)
    {
        printf("Could not load native libraries: %s\n", totemLinkStatus_Describe(linkStatus));
    }

    // set arguments
    totemExecState_SetArgV(&state->ExecState, argv, argc);
}

void totemCmdState_InitWithFile(totemCmdState *state, FILE *file, const char **argv, int argc)
{
    totemInterpreter_InitWithFile(&state->Interpreter, file);
    totemCmdState_Init(state, argv, argc);
}

void totemCmdState_InitWithString(totemCmdState *state, const char *str, size_t len, const char **argv, int argc)
{
    totemInterpreter_InitWithString(&state->Interpreter, str, len);
    totemCmdState_Init(state, argv, argc);
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
    // link build
    totemLinkStatus linkStatus = totemRuntime_LinkBuild(&state->Runtime, &state->Interpreter.Build, &state->Script);
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

    // init & run instance
    totemGCObject *instance = NULL;
    totemExecStatus execStatus = totemExecState_CreateInstance(&state->ExecState, &state->Script, totemBool_True, &instance);
    
    if (execStatus != totemExecStatus_Continue)
    {
        printf("Could not create & run instance: %s\n", totemExecStatus_Describe(execStatus));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, const char **argv)
{
    totemCmdState state;
    FILE *file = NULL;
    const char* toParse = NULL;
    const char** scriptArgs = NULL;
    int numScriptArgs = 0;
    char terminalInput[TOTEM_MAX_TERMINAL_INPUT];

#define TOTEM_EMIT_CMD(char, name, info) static totemBool do_ ##name = totemBool_False;
    TOTEM_EMIT_CMDS(TOTEM_EMIT_CMD);
#undef TOTEM_EMIT_CMD

    if (argc <= 1)
    {
        printf("%s", help);
        return EXIT_FAILURE;
    }
    
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        totemBool recognised = totemBool_False;
        
#define TOTEM_EMIT_CMD(cha, name, info) \
        if(TOTEM_CMD_ISARG("-"#cha, arg) || TOTEM_CMD_ISARG(#name, arg)) \
        { \
            do_ ##name = totemBool_True; \
            recognised = totemBool_True; \
        }
        TOTEM_EMIT_CMDS(TOTEM_EMIT_CMD)
#undef TOTEM_EMIT_CMD

        if(!recognised)
        {
            // assume this is either string or filename
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

    if (do_printargs)
    {
#define EMIT(a, b, c) fprintf(stdout, "%s %s %i\n", #a, #b, do_ ## b);
        TOTEM_EMIT_CMDS(EMIT)
        printf("toParse: %s\n", toParse);
#undef EMIT
    }
    
    if (do_version)
    {
        printf("%s", version);
    }
    else if (do_help)
    {
        printf("%s", help);
    }
    else if(((do_file || do_string) && toParse) || do_terminal)
    {
        totem_Init();
        
        while (1)
        {
            totemBool parseResult = totemBool_False;
            
            if (do_terminal)
            {
                if (fgets(terminalInput, sizeof(terminalInput), stdin) == NULL)
                {
                    continue;
                }

                terminalInput[TOTEM_MAX_TERMINAL_INPUT - 1] = 0;
                toParse = terminalInput;
            }

            if (toParse)
            {
                totemBool didInit = totemBool_False;
                
                if (do_string || do_terminal)
                {
                    totemCmdState_InitWithString(&state, toParse, strlen(toParse), argv, argc);
                    didInit = totemBool_True;
                }
                else if (do_file)
                {
                    file = totem_fopen(toParse, "rb");
                    if(file)
                    {
                        totemCmdState_InitWithFile(&state, file, argv, argc);
                        didInit = totemBool_True;
                    }
                    else
                    {
                        ret = EXIT_FAILURE;
                    }
                }
                
                if(didInit)
                {
                    parseResult = totemInterpreter_Interpret(&state.Interpreter);

                    if (!parseResult)
                    {
                        TOTEM_LOG_DO(Cmd, Result, Error, logger, totemInterpreter_PrintResult(&state.Interpreter, logger));
                        ret = EXIT_FAILURE;
                    }
                    else
                    {
                        if (do_dump)
                        {
                            TOTEM_LOG_DO(Cmd, Result, Output, dst, totemInstruction_PrintList(dst, totemInstructionDynBuffer_Bottom(&state.Interpreter.Build.Instructions), totemInstructionDynBuffer_Length(&state.Interpreter.Build.Instructions)));
                        }

                        if (!do_parseonly)
                        {
                            ret = totemCmdState_Run(&state, scriptArgs, numScriptArgs);
                        }
                    }
                    
                    totemCmdState_Cleanup(&state);
                }
            }

            if (!do_terminal)
            {
                break;
            }
        }
        
        totem_Cleanup();
    }
    else
    {
        fprintf(stdout, "%s", help);
        ret = EXIT_FAILURE;
    }
    
    if(file)
    {
        fclose(file);
    }
    
    return ret;
}
