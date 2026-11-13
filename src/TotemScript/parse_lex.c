//
//  parse_lex.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <TotemScript/log.h>
#include <string.h>
#include <ctype.h>

#define EMIT_WHITESPACE(_) \
	_(' ') \
	_('\n') \
	_('\t') \
	_('\r') \
	_('\f') \
	_('\v') \
	_('\0') \
	

#define EMIT_DIGITS(_) \
	_('0') \
	_('1') \
	_('2') \
	_('3') \
	_('4') \
	_('5') \
	_('6') \
	_('7') \
	_('8') \
	_('9') \

typedef enum CharFlag
{
    CharFlag_None,
	CharFlag_IsWhitespace,
	CharFlag_IsDigit,
	CharFlag_IsSymbol
}
CharFlag;

const static char s_lookup[UINT8_MAX + 1] =
{
#define EMIT(x) [x] = CharFlag_IsWhitespace,
	EMIT_WHITESPACE(EMIT)
#undef EMIT
#define EMIT(x) [x] = CharFlag_IsDigit,
	EMIT_DIGITS(EMIT)
#undef EMIT
#define EMIT(x, y) [y] = CharFlag_IsSymbol,
	TOTEM_EMIT_SYMBOLS(EMIT)
#undef EMIT
};

TOTEM_STATIC_ASSERT(TOTEM_IS_POWOF2(TOTEM_SIZEOF_STATIC(totemParseTree, ReadBuffer)), "ReadBuffer size must be power of 2");
#define TOTEM_TOKENIZER_READ_INDEX(absPos) ((absPos) & (TOTEM_SIZEOF_STATIC(totemParseTree, ReadBuffer) - 1));

void totemTokenPositionInfo_Print(totemTokenPositionInfo *info, totemParseTree *src, totemLog *dst)
{
	totemLog_Write(dst, "\"");
    src->Callbacks->Print(src->SrcCxt, dst, info->Length, info->Start);
    totemLog_Write(dst, "\"\nline(%"PRISize") char(%"PRISize") start(%"PRISize") length(%"PRISize")",
                   info->LineNumber,
                   info->CharNumber,
                   info->Start,
                   info->Length);
}

void totemToken_Print(totemLog *target, totemToken *token, totemParseTree *tree)
{
    totemLog_Write(target, "%s ", totemTokenType_Describe(token->Type));
    totemTokenPositionInfo_Print(&token->Position, tree, target);
}

void totemToken_PrintList(totemLog *target, totemToken *tokens, size_t num, totemParseTree *tree)
{
    for (size_t i = 0; i < num; ++i)
    {
        totemToken *token = tokens + i;
        totemToken_Print(target, token, tree);
    }
}

static void totemParseTree_SetToken(totemParseTree *worker, size_t length, totemTokenType type)
{
	worker->CurrentToken.Type = type;
	worker->CurrentToken.Position.LineNumber = worker->CurrentLine;
	worker->CurrentToken.Position.CharNumber = worker->CurrentLineChar;
	worker->CurrentToken.Position.Length = length;
	worker->CurrentToken.Position.Start = worker->CurrentPos;
}

static void totemParseTree_FinishToken(totemParseTree *worker)
{
    worker->CurrentToken.Position.Length = worker->CurrentPos - worker->CurrentToken.Position.Start;
	worker->InsideStringLiteral ^= worker->CurrentToken.Type == totemTokenType_DoubleQuote && worker->LastToken.Type != totemTokenType_Slash;

	TOTEM_LOG_BEGIN(Parse, Lex, Debug, logger);
		totemLog_Write(logger, "Lexed token: ");
		totemToken_Print(logger, &worker->CurrentToken, worker);
	TOTEM_LOG_END();
}

static size_t totemParseTree_LeftToReadInBuffer(totemParseTree *worker)
{
    size_t leftToRead = worker->SrcPos - worker->CurrentPos; //- worker->CurrentToken.Position.Start;
    return leftToRead;
}

char totemParseTree_GetAbs(totemParseTree* worker, size_t pos)
{
    size_t index = TOTEM_TOKENIZER_READ_INDEX(pos);
    return worker->ReadBuffer[index];
}

static char totemParseTree_Get(totemParseTree* worker, size_t pos)
{
    return totemParseTree_GetAbs(worker, worker->CurrentPos + pos);
}

static void totemParseTree_Read(totemParseTree* worker, size_t toRead)
{
	// try to fill the read buffer with the rest of the current token
    size_t currentPos = worker->CurrentPos;
    
	while (toRead)
	{
        size_t pos = TOTEM_TOKENIZER_READ_INDEX(currentPos);
        size_t maxReadPos = TOTEM_TOKENIZER_READ_INDEX(worker->SrcPos);
        
        size_t toReadLen = 0;
        
        if(maxReadPos >= pos)
        {
            // increase maxReadPos to end of ring buffer
            toReadLen = sizeof(worker->ReadBuffer) - maxReadPos;
        }
        else if(pos > maxReadPos)
        {
            // increase maxReadPos to just behind currentPos
            toReadLen = pos - maxReadPos;
        }
        
        if(!toReadLen)
        {
            totemParseTree_Break(worker, totemParseStatus_LexError, NULL);
        }
        
        // perform read
		char* whereToWriteTo = worker->ReadBuffer + maxReadPos;
		size_t read = worker->Callbacks->Step(worker->SrcCxt, whereToWriteTo, toReadLen, worker->SrcPos);
		worker->SrcPos += read;
        currentPos += read;
        
		if (read != toReadLen)
		{
			break;
		}
        
        toReadLen -= read;
	}
}

static void totemParseTree_Ensure(totemParseTree* worker, size_t length)
{
    // ensure we have already read at least the given length of the current token into the ring buffer
	if (totemParseTree_LeftToReadInBuffer(worker) < length)
	{
		totemParseTree_Read(worker, length);
	}
}

static size_t GetLargestReservedWordLength()
{
	// compiler should reduce this to a single value
	size_t maxLen = 0;
#define FOR_WORD(type, word) if((sizeof(word) - 1) > maxLen) maxLen = sizeof(word) - 1;
	TOTEM_EMIT_RESERVED_WORDS(FOR_WORD);
#undef FOR_WORD

	return maxLen;
}

static totemBool totemParseTree_Compare(totemParseTree* worker, const char* word, size_t wordLen)
{
	for (size_t i = 0; i < wordLen; i++)
	{
		if (word[i] != totemParseTree_Get(worker, i))
		{
			return totemBool_False;
		}
	}

	return totemBool_True;
}

static void totemParseTree_Inc(totemParseTree *worker, size_t i)
{
	// increment the current read position

	size_t totalLeft = worker->SrcLen - worker->CurrentPos;
	if (totalLeft < i)
	{
		i = totalLeft;
	}

	size_t remaining = sizeof(worker->ReadBuffer) - (worker->CurrentPos - worker->CurrentToken.Position.Start);
	if (remaining < i)
	{
		i = remaining;
	}

	size_t oldCurrent = worker->CurrentPos;
    
	if (worker->CurrentPos + i >= worker->SrcPos)
	{
		totemParseTree_Read(worker, i);
	}
    
    worker->CurrentPos += i;

	for (; oldCurrent < worker->CurrentPos; oldCurrent++)
	{
		char c = totemParseTree_GetAbs(worker, oldCurrent);

		if (c == '\n' || c == '\r')
		{
			worker->CurrentLine++;
			worker->CurrentLineChar = 1;
			worker->NumLineBreaksInCurrentToken++;
		}
		else
		{
			worker->CurrentLineChar++;
		}
	}
}

static void totemParseTree_SkipComment(totemParseTree* worker, char c, char c2)
{
	// skip comments
	char type = c2;
	
	if (type == '/')
	{
        totemParseTree_Inc(worker, 1);

		// single-line comment
		while (worker->CurrentPos < worker->SrcLen)
		{
			c = totemParseTree_Get(worker, 0);
			totemParseTree_Inc(worker, 1);

			if (c == '\n' || c == '\r')
			{
				break;
			}
		}
	}
	else if (type == '*')
	{
        totemParseTree_Inc(worker, 2);

		// multiple-line comment
		while (worker->CurrentPos < worker->SrcLen - 2)
		{
			c = totemParseTree_Get(worker, 0);
			c2 = totemParseTree_Get(worker, 1);

			if (c == '*' && c2 == '/')
			{
				totemParseTree_Inc(worker, 2);
				break;
			}

			totemParseTree_Inc(worker, 1);
		}
	}
}

static void totemParseTree_Lex(totemParseTree* worker, totemTokenType type, CharFlag whileIs)
{
	totemParseTree_SetToken(worker, 1, type);

	do
	{
		totemParseTree_Inc(worker, 1);
	} 	
	while ((worker->CurrentPos < worker->SrcLen) && (s_lookup[totemParseTree_Get(worker, 0)] == whileIs));

	totemParseTree_FinishToken(worker);
}

static totemBool totemParseTree_TryLexWord(totemParseTree *worker, const char *word, size_t wordLen, totemTokenType tokenType)
{
    size_t end = worker->CurrentPos + wordLen;
    char endChar = totemParseTree_GetAbs(worker, end);
    if(end == worker->SrcLen || s_lookup[endChar])
    {
        if(totemParseTree_Compare(worker, (word), wordLen))
        {
            totemParseTree_SetToken(worker, wordLen, tokenType);
            totemParseTree_Inc(worker, wordLen);
            totemParseTree_FinishToken(worker);
            return totemBool_True;
        }
    }
    
    return totemBool_False;
}

void totemParseTree_InitialRead(totemParseTree *tree)
{
    tree->SrcPos = tree->Callbacks->Step(tree->SrcCxt, tree->ReadBuffer, sizeof(tree->ReadBuffer), tree->SrcPos);
}

/**
 * Lex a single token
 */
void totemParseTree_LexToken(totemParseTree *worker)
{
	while(1)
	{
		worker->LastToken = worker->CurrentToken;
		worker->NumLineBreaksInCurrentToken = 0;
		
		// end
		if (worker->CurrentPos >= worker->SrcLen)
		{
			totemParseTree_SetToken(worker, 0, totemTokenType_EndScript);
			totemParseTree_FinishToken(worker);
			return;
		}

		totemParseTree_Ensure(worker, 1);
		char c = totemParseTree_Get(worker, 0);

		switch (c)
		{
		// whitespace
#define EMIT(x) case x:
			EMIT_WHITESPACE(EMIT)
			totemParseTree_Lex(worker, totemTokenType_Whitespace, CharFlag_IsWhitespace);
			return;

		// digits
			EMIT_DIGITS(EMIT)
			totemParseTree_Lex(worker, totemTokenType_Digits, CharFlag_IsDigit);
			return;
#undef EMIT

		// symbols
#define TOTEM_EMIT_SYMBOL_CASE(name, symbol)  \
		case symbol: \
		{ \
			if(symbol == '/') \
			{ \
				totemParseTree_Ensure(worker, 2); \
				char c2 = totemParseTree_Get(worker, 1); \
				if (worker->CurrentPos < (worker->SrcLen - 1) && (c2 == '/' || c2 == '*') && !worker->InsideStringLiteral) \
				{ \
					totemParseTree_SkipComment(worker, c, c2); \
					continue; \
				} \
			} \
			totemParseTree_SetToken(worker, 1, totemTokenType_ ## name); \
			totemParseTree_Inc(worker, 1); \
			totemParseTree_FinishToken(worker); \
			return; \
		}
		TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_CASE)
#undef TOTEM_EMIT_SYMBOL_CASE

			// reserved words
#define TOTEM_TRY_LEX_WORD(type, word) \
			{ \
				const static size_t wordLen = sizeof((word)) - 1; \
				if((word)[0] == beginChar && amountRead > wordLen) \
				{ \
                    if(totemParseTree_TryLexWord(worker, word, wordLen, totemTokenType_ ## type)) \
                    { \
                        return; \
                    } \
				} \
			}

#define TOTEM_TRY_LEX_WORD_FROM_CHAR(fc) \
		case (fc): \
		{ \
			totemParseTree_Ensure(worker, GetLargestReservedWordLength()); \
            const size_t amountRead = totemParseTree_LeftToReadInBuffer(worker); \
			const static char beginChar = (fc); \
			TOTEM_EMIT_RESERVED_WORDS(TOTEM_TRY_LEX_WORD) \
			goto LexIdentifier; \
		}

		TOTEM_TRY_LEX_WORD_FROM_CHAR('a');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('b');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('c');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('d');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('e');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('f');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('g');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('h');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('i');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('j');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('k');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('l');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('m');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('n');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('o');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('p');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('q');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('r');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('s');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('t');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('u');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('v');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('w');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('x');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('y');
		TOTEM_TRY_LEX_WORD_FROM_CHAR('z');

#undef TOTEM_TRY_LEX_WORD_FROM_CHAR
#undef TOTEM_TRY_LEX_WORD

		default:
		LexIdentifier:
			totemParseTree_Lex(worker, totemTokenType_Identifier, 0);
			return;
		}
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
        TOTEM_STRINGIFY_CASE(totemTokenType_Digits);
		TOTEM_STRINGIFY_CASE(totemTokenType_Identifier);
		TOTEM_STRINGIFY_CASE(totemTokenType_Whitespace);
		TOTEM_STRINGIFY_CASE(totemTokenType_EndScript);
		TOTEM_STRINGIFY_CASE(totemTokenType_Max);
#define TOTEM_EMIT_RESERVED_WORD_TOKEN(name, word) TOTEM_STRINGIFY_CASE(totemTokenType_ ## name);
		TOTEM_EMIT_RESERVED_WORDS(TOTEM_EMIT_RESERVED_WORD_TOKEN)
#undef TOTEM_EMIT_RESERVED_WORD_TOKEN
#define TOTEM_EMIT_SYMBOL_TOKEN(name, symbol) TOTEM_STRINGIFY_CASE(totemTokenType_ ## name);
			TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_TOKEN)
#undef TOTEM_EMIT_SYMBOL_TOKEN
    }
    
    return "UNKNOWN";
}

totemTokenCategory totemTokenType_ToCategory(totemTokenType type)
{
	switch (type)
	{
#define TOTEM_EMIT_RESERVED_WORD_TOKEN(name, word) case totemTokenType_ ## name: return totemTokenCategory_ReservedWord;
		TOTEM_EMIT_RESERVED_WORDS(TOTEM_EMIT_RESERVED_WORD_TOKEN)
#undef TOTEM_EMIT_RESERVED_WORD_TOKEN
#define TOTEM_EMIT_SYMBOL_TOKEN(name, symbol) case totemTokenType_ ## name: return totemTokenCategory_Symbol;
			TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_TOKEN)
#undef TOTEM_EMIT_SYMBOL_TOKEN
	default: return totemTokenCategory_Other;
	}
}
