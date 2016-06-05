//
//  test
//  TotemScript
//
//  Created by Timothy Smale on 05/11/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <TotemScript/totem.h>

totemExecStatus totemPrint(totemExecState *state)
{
    totemRegister *reg = &state->Registers[totemOperandType_LocalRegister][0];
    totemRegister_Print(stdout, reg);
    return totemExecStatus_Continue;
}

totemExecStatus totemAssert(totemExecState *state)
{
    totemRegister *reg = &state->Registers[totemOperandType_LocalRegister][0];
    if (reg->Value.Data)
    {
        return totemExecStatus_Continue;
    }
    else
    {
        return totemExecStatus_Stop;
    }
}

void totemFileTestDestructor(totemExecState *state, totemUserdata *data)
{
	FILE *f = (FILE*)data->Data;
	fclose(f);
}

totemExecStatus totemFileTest(totemExecState *state)
{
	FILE *f = totem_fopen("test.totem", "r");
	if (!f)
	{
		return totemExecStatus_Stop;
	}

	totemGCObject *gc = NULL;
	totemExecStatus status = totemExecState_CreateUserdata(state, (uint64_t)f, totemFileTestDestructor, &gc);
	if (status != totemExecStatus_Continue)
	{
		fclose(f);
		return status;
	}

	totemExecState_AssignNewUserdata(state, state->CallStack->ReturnRegister, gc);
	return status;
}

typedef struct
{
	totemString Name;
	totemNativeFunction Func;
	totemOperandXUnsigned Addr;
}
totemFunctionToRegister;

int main(int argc, const char * argv[])
{
    totem_Init();
    
    // load file
    totemScriptFile scriptContents;
    totemScriptFile_Init(&scriptContents);
    
    totemLoadScriptError err;
    if(totemScriptFile_Load(&scriptContents, "test.totem", &err) != totemBool_True)
    {
        printf("Load script error: %s: %.*s\n", totemLoadScriptStatus_Describe(err.Status), err.Description.Length, err.Description.Value);
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Script:\n");
    printf("******\n");
    for(size_t i = 0; i < scriptContents.Buffer.Length; i++)
    {
        printf("%c", scriptContents.Buffer.Data[i]);
    }
    
    printf("\n");
    
    // init runtime
    totemRuntime runtime;
    totemRuntime_Init(&runtime);
    
	// link functions
	totemFunctionToRegister funcs[] = 
	{
		{ TOTEM_STRING_VAL("print"), totemPrint, 0 },
		{ TOTEM_STRING_VAL("assert"), totemAssert, 0 },
		{ TOTEM_STRING_VAL("fopen"), totemFileTest, 0 },
	};

	for (size_t i = 0; i < TOTEM_ARRAYSIZE(funcs); i++)
	{
		totemFunctionToRegister *func = &funcs[i];

		totemLinkStatus linkStatus = totemRuntime_LinkNativeFunction(&runtime, func->Func, &func->Name, &func->Addr);
		if (linkStatus != totemLinkStatus_Success)
		{
			printf("Could not register %.*s: %s\n", func->Name.Length, func->Name.Value, totemLinkStatus_Describe(linkStatus));
			return EXIT_FAILURE;
		}
	}
    
    // lex
    totemTokenList tokens;
    totemTokenList_Init(&tokens);
    
    totemLexStatus lexStatus = totemTokenList_Lex(&tokens, scriptContents.Buffer.Data, scriptContents.Buffer.Length);
    if(lexStatus != totemLexStatus_Success)
    {
        printf("Lex error: %s\n", totemLexStatus_Describe(lexStatus));
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Tokens:\n");
    printf("******\n");
    totemToken_PrintList(stdout, (totemToken*)tokens.Tokens.Data, totemMemoryBuffer_GetNumObjects(&tokens.Tokens));
    
    // parse
    totemParseTree parseTree;
    totemParseTree_Init(&parseTree);
    
    totemParseStatus parseStatus = totemParseTree_Parse(&parseTree, &tokens);
    if(parseStatus != totemParseStatus_Success)
    {
        printf("Parse error %s (%s) at %.*s \n", totemParseStatus_Describe(parseStatus), totemTokenType_Describe(parseTree.CurrentToken->Type), 50, parseTree.CurrentToken->Value.Value);
        return EXIT_FAILURE;
    }
    
    // eval
    totemBuildPrototype build;
    totemBuildPrototype_Init(&build);
    
    totemEvalStatus evalStatus = totemBuildPrototype_Eval(&build, &parseTree);
    if(evalStatus != totemEvalStatus_Success)
    {
        printf("Eval error %s\n", totemEvalStatus_Describe(evalStatus));
        return EXIT_FAILURE;
    }
    
    // link
    totemScript script;
    totemScript_Init(&script);
    totemLinkStatus linkStatus = totemRuntime_LinkBuild(&runtime, &build, &script);
    if(linkStatus != totemLinkStatus_Success)
    {
        printf("Could not register script: %s\n", totemLinkStatus_Describe(linkStatus));
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Instructions:\n");
    printf("******\n");
    totemInstruction_PrintList(stdout, totemMemoryBuffer_Get(&build.Instructions, 0), totemMemoryBuffer_GetNumObjects(&build.Instructions));
    printf("\n");
    
    // init actor
    totemActor actor;
    totemActor_Init(&actor);
    if (totemScript_LinkActor(&script, &actor) != totemLinkStatus_Success)
    {
        printf("Could not create actor\n");
        return EXIT_FAILURE;
    }
    
    printf("******\n");
    printf("Globals:\n");
    printf("******\n");
    totemRegister_PrintList(stdout, totemMemoryBuffer_Get(&actor.GlobalRegisters, 0), totemMemoryBuffer_GetNumObjects(&actor.GlobalRegisters));
    printf("\n");
    
    // init exec state
    totemExecState execState;
    totemExecState_Init(&execState);
    if(totemRuntime_LinkExecState(&runtime, &execState, 128) != totemLinkStatus_Success)
    {
        printf("Could not create exec state\n");
        return EXIT_FAILURE;
    }
    
    totemRegister returnRegister;
    memset(&returnRegister, 0, sizeof(totemRegister));
    
    printf("******\n");
    printf("Run script:\n");
    printf("******\n");
    
    // run script
    totemExecStatus execStatus = totemExecState_Exec(&execState, &actor, 0, &returnRegister);
    if(execStatus != totemExecStatus_Return)
    {
        printf("exec error %s\n", totemExecStatus_Describe(execStatus));
        return EXIT_FAILURE;
    }
    
    // free
    totemExecState_Cleanup(&execState);
    totemActor_Cleanup(&actor);
    totemScript_Cleanup(&script);
    
    totemBuildPrototype_Cleanup(&build);
    totemParseTree_Cleanup(&parseTree);
    totemTokenList_Cleanup(&tokens);
    
    totemRuntime_Cleanup(&runtime);
    totemScriptFile_Cleanup(&scriptContents);
    
    totem_Cleanup();
    
    return 0;
}