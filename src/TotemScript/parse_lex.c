//
//  parse_lex.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <string.h>
#include <ctype.h>

void totemToken_Print(FILE *target, totemToken *token)
{
    fprintf(target, "%s %.*s at %"PRISize":%"PRISize"\n",
            totemTokenType_Describe(token->Type),
            (int)token->Position.Length,
            token->Position.Start,
            token->Position.LineNumber,
            token->Position.CharNumber);
}

void totemToken_PrintList(FILE *target, totemToken *tokens, size_t num)
{
    for (size_t i = 0; i < num; ++i)
    {
        totemToken *token = tokens + i;
        totemToken_Print(target, token);
    }
}

void totemTokenizer_Init(totemTokenizer *worker)
{
	TOTEM_ZERO(worker);
}

void totemTokenizer_Reset(totemTokenizer *worker)
{
	TOTEM_ZERO(worker);
}

void totemTokenizer_Cleanup(totemTokenizer *worker)
{
	TOTEM_ZERO(worker);
}

void totemTokenizer_Begin(totemTokenizer *worker, const char *buffer, size_t length)
{
	worker->Current = buffer;
	worker->InsideStringLiteral = totemBool_False;
	worker->CEnd = buffer + length;
	worker->CurrentChar = 1;
	worker->CurrentLine = 1;
	TOTEM_ZERO(&worker->CurrentToken);
	TOTEM_ZERO(&worker->LastToken);
}

static totemLexStatus totemLexStatus_Break(totemLexStatus status)
{
    return status;
}

static void totemTokenizer_Set(totemTokenizer *worker, size_t length, totemTokenCategory category, totemTokenType type, totemToken *output)
{
    output->Category = category;
	output->Type = type;
	output->Position.Start = worker->Current;
	output->Position.LineNumber = worker->CurrentLine;
	output->Position.CharNumber = worker->CurrentChar;
	output->Position.Length = length;
}

static void totemTokenizer_FinishToken(totemTokenizer *worker, totemToken *output)
{
	output->Position.Length = (size_t)(worker->Current - output->Position.Start);
}

static totemBool IsSymbol(char c)
{
	switch (c)
	{
#define TOTEM_EMIT_SYMBOL_CASE(name, symbol) case symbol:
		TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_CASE)
#undef TOTEM_EMIT_SYMBOL_CASE
			return totemBool_True;

		default:
			return totemBool_False;
	}
}

static void totemTokenizer_Inc(totemTokenizer *worker)
{
    worker->Current++;
    
    if (*worker->Current == '\n' || *worker->Current == '\r')
    {
        worker->CurrentLine++;
        worker->CurrentChar = 1;
    }
    else
    {
        worker->CurrentChar++;
    }
}

void totemTokenizer_Rewind(totemTokenizer *list)
{
	list->CurrentToken = list->LastToken;
}

/**
 * Lex a single token
 */
void totemTokenizer_Step(totemTokenizer *worker)
{
	while (1)
	{
		worker->LastToken = worker->CurrentToken;
		totemToken *output = &worker->CurrentToken;

		// end
		if (worker->Current >= worker->CEnd)
		{
			totemTokenizer_Set(worker, 0, totemTokenCategory_Other, totemTokenType_EndScript, output);
			break;
		}

		// whitespace
		else if (isspace(*worker->Current))
		{
			totemTokenizer_Set(worker, 1, totemTokenCategory_Other, totemTokenType_Whitespace, output);

			do
			{
				totemTokenizer_Inc(worker);
			} 
			while (worker->Current < worker->CEnd && isspace(worker->Current[0]));

			totemTokenizer_FinishToken(worker, output);
		}

		// numbers
		else if (isdigit(*worker->Current))
		{
			totemTokenizer_Set(worker, 1, totemTokenCategory_Other, totemTokenType_Number, output);

			do
			{
				totemTokenizer_Inc(worker);
			} 
			while (worker->Current < worker->CEnd && isdigit(worker->Current[0]));

			totemTokenizer_FinishToken(worker, output);
		}

		// symbols
		else if (IsSymbol(*worker->Current))
		{
			// comment?
			if (*worker->Current == '/' && worker->Current < (worker->CEnd - 1) && (worker->Current[1] == '/' || worker->Current[1] == '*') && !worker->InsideStringLiteral)
			{
				// skip comments
				char type = worker->Current[1];
				totemTokenizer_Inc(worker);
				totemTokenizer_Inc(worker);

				if (type == '/')
				{
					while (worker->Current < worker->CEnd)
					{
						if (worker->Current[0] == '\n')
						{
							totemTokenizer_Inc(worker);
							break;
						}

						totemTokenizer_Inc(worker);
					}
				}
				else if (type == '*')
				{
					while (worker->Current < worker->CEnd)
					{
						if (worker->Current == worker->CEnd - 2 || (worker->Current[0] == '*' && worker->Current[1] == '/'))
						{
							totemTokenizer_Inc(worker);
							totemTokenizer_Inc(worker);
							break;
						}

						totemTokenizer_Inc(worker);
					}
				}

				continue;
			}
			else
			{
				switch (*worker->Current)
				{
#define TOTEM_EMIT_SYMBOL_CASE(name, symbol) case symbol: \
					{ \
						totemTokenizer_Set(worker, 1, totemTokenCategory_Symbol, totemTokenType_ ## name, output); \
						totemTokenizer_Inc(worker); \
						break; \
					}
					TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_CASE)
#undef TOTEM_EMIT_SYMBOL_CASE
				}
			}
		}
		else
		{
			switch (*worker->Current)
			{
				// reserved words
#define TOTEM_TRY_LEX_WORD(worker, type, word)  \
				if(worker->Current + strlen(word) <= worker->CEnd) \
				{ \
					if(worker->Current + strlen(word) == worker->CEnd || isspace(worker->Current[strlen(word)]) || isdigit(worker->Current[strlen(word)]) || IsSymbol(worker->Current[strlen(word)])) \
					{ \
						if(strncmp(worker->Current, word, strlen(word)) == 0) \
						{ \
							totemTokenizer_Set(worker, strlen(word), totemTokenCategory_ReservedWord, type, output); \
							for (size_t i = 0; i < strlen(word); i++) \
							{ \
								totemTokenizer_Inc(worker); \
							} \
							break; \
						} \
					} \
				}

				case 'a':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Array, "array");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_As, "as");
					goto lex_identifier;

				case 'b':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Boolean, "boolean");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Break, "break");
					goto lex_identifier;

				case 'c':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Case, "case");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Coroutine, "coroutine");
					goto lex_identifier;

				case 'd':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Default, "default");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Do, "do");
					goto lex_identifier;

				case 'e':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Else, "else");
					goto lex_identifier;

				case 'f':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_False, "false");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Float, "float");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_For, "for");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Function, "function");
					goto lex_identifier;

				case 'i':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_If, "if");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Import, "import");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Include, "include");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Int, "int");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Is, "is");
					goto lex_identifier;

				case 'l':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Let, "let");
					goto lex_identifier;

				case 'n':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Null, "null");
					goto lex_identifier;

				case 'o':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Object, "object");
					goto lex_identifier;

				case 'r':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Return, "return");
					goto lex_identifier;

				case 's':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_String, "string");
					goto lex_identifier;

				case 't':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_True, "true");
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Type, "type");
					goto lex_identifier;

				case 'u':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Userdata, "userdata");
					goto lex_identifier;

				case 'v':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Var, "var");
					goto lex_identifier;

				case 'w':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_While, "while");
					goto lex_identifier;

				case 'y':
					TOTEM_TRY_LEX_WORD(worker, totemTokenType_Yield, "yield");
					goto lex_identifier;

				default:
				lex_identifier :
				{
					// assume an identifier
					totemTokenizer_Set(worker, 1, totemTokenCategory_Other, totemTokenType_Identifier, output);

					do
					{
						totemTokenizer_Inc(worker);
					} 
					while (!isspace(*worker->Current) && !isdigit(*worker->Current) && !IsSymbol(*worker->Current) && worker->Current < worker->CEnd);

					totemTokenizer_FinishToken(worker, output);
					break;
				}
			}
		}

		worker->InsideStringLiteral ^= output->Type == totemTokenType_DoubleQuote && worker->LastToken.Type != totemTokenType_Slash;
		worker->LastToken.Type = output->Type;
	}
}

const char *totemLexStatus_Describe(totemLexStatus status)
{
    switch (status)
    {
            TOTEM_STRINGIFY_CASE(totemLexStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLexStatus_Success);
            TOTEM_STRINGIFY_CASE(totemLexStatus_Failure);
    }
    
    return "UNKNOWN";
}

const char *totemTokenType_Describe(totemTokenType type)
{
    switch (type)
    {
            TOTEM_STRINGIFY_CASE(totemTokenType_Var);
            TOTEM_STRINGIFY_CASE(totemTokenType_At);
            TOTEM_STRINGIFY_CASE(totemTokenType_As);
            TOTEM_STRINGIFY_CASE(totemTokenType_Array);
            TOTEM_STRINGIFY_CASE(totemTokenType_Int);
            TOTEM_STRINGIFY_CASE(totemTokenType_Float);
            TOTEM_STRINGIFY_CASE(totemTokenType_Type);
            TOTEM_STRINGIFY_CASE(totemTokenType_String);
            TOTEM_STRINGIFY_CASE(totemTokenType_Null);
            TOTEM_STRINGIFY_CASE(totemTokenType_And);
            TOTEM_STRINGIFY_CASE(totemTokenType_Assign);
            TOTEM_STRINGIFY_CASE(totemTokenType_Backslash);
            TOTEM_STRINGIFY_CASE(totemTokenType_Break);
            TOTEM_STRINGIFY_CASE(totemTokenType_Case);
            TOTEM_STRINGIFY_CASE(totemTokenType_Colon);
            TOTEM_STRINGIFY_CASE(totemTokenType_Comma);
            TOTEM_STRINGIFY_CASE(totemTokenType_Let);
            TOTEM_STRINGIFY_CASE(totemTokenType_Coroutine);
            TOTEM_STRINGIFY_CASE(totemTokenType_Default);
            TOTEM_STRINGIFY_CASE(totemTokenType_Do);
            TOTEM_STRINGIFY_CASE(totemTokenType_Dot);
            TOTEM_STRINGIFY_CASE(totemTokenType_DoubleQuote);
            TOTEM_STRINGIFY_CASE(totemTokenType_Else);
            TOTEM_STRINGIFY_CASE(totemTokenType_EndScript);
            TOTEM_STRINGIFY_CASE(totemTokenType_False);
            TOTEM_STRINGIFY_CASE(totemTokenType_For);
            TOTEM_STRINGIFY_CASE(totemTokenType_Function);
            TOTEM_STRINGIFY_CASE(totemTokenType_Identifier);
            TOTEM_STRINGIFY_CASE(totemTokenType_If);
            TOTEM_STRINGIFY_CASE(totemTokenType_Is);
            TOTEM_STRINGIFY_CASE(totemTokenType_LBracket);
            TOTEM_STRINGIFY_CASE(totemTokenType_LCBracket);
            TOTEM_STRINGIFY_CASE(totemTokenType_LessThan);
            TOTEM_STRINGIFY_CASE(totemTokenType_LSBracket);
            TOTEM_STRINGIFY_CASE(totemTokenType_Max);
            TOTEM_STRINGIFY_CASE(totemTokenType_Minus);
            TOTEM_STRINGIFY_CASE(totemTokenType_MoreThan);
            TOTEM_STRINGIFY_CASE(totemTokenType_Multiply);
            TOTEM_STRINGIFY_CASE(totemTokenType_None);
            TOTEM_STRINGIFY_CASE(totemTokenType_Not);
            TOTEM_STRINGIFY_CASE(totemTokenType_Number);
            TOTEM_STRINGIFY_CASE(totemTokenType_Object);
            TOTEM_STRINGIFY_CASE(totemTokenType_Or);
            TOTEM_STRINGIFY_CASE(totemTokenType_Plus);
            TOTEM_STRINGIFY_CASE(totemTokenType_PowerTo);
            TOTEM_STRINGIFY_CASE(totemTokenType_RBracket);
            TOTEM_STRINGIFY_CASE(totemTokenType_RCBracket);
            TOTEM_STRINGIFY_CASE(totemTokenType_Return);
            TOTEM_STRINGIFY_CASE(totemTokenType_RSBracket);
            TOTEM_STRINGIFY_CASE(totemTokenType_Semicolon);
            TOTEM_STRINGIFY_CASE(totemTokenType_SingleQuote);
            TOTEM_STRINGIFY_CASE(totemTokenType_Slash);
            TOTEM_STRINGIFY_CASE(totemTokenType_True);
            TOTEM_STRINGIFY_CASE(totemTokenType_Userdata);
            TOTEM_STRINGIFY_CASE(totemTokenType_While);
            TOTEM_STRINGIFY_CASE(totemTokenType_Whitespace);
            TOTEM_STRINGIFY_CASE(totemTokenType_Boolean);
            TOTEM_STRINGIFY_CASE(totemTokenType_Hash);
            TOTEM_STRINGIFY_CASE(totemTokenType_Import);
            TOTEM_STRINGIFY_CASE(totemTokenType_Include);
    }
    
    return "UNKNOWN";
}
