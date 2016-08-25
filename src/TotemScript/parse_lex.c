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

#define TOTEM_DESC_TOKEN_SYMBOL(type, value) { type, totemTokenCategory_Symbol, value, TOTEM_STRING_LITERAL_SIZE(value) }
#define TOTEM_DESC_TOKEN_WORD(type, value) { type, totemTokenCategory_ReservedWord, value, TOTEM_STRING_LITERAL_SIZE(value) }

static totemTokenDesc s_symbolTokenValues[] =
{
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Plus, "+"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Minus, "-"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Multiply, "*"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Divide, "/"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Not, "!"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_And, "&"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Or, "|"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Assign, "="),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_LBracket, "("),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_RBracket, ")"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_LessThan, "<"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_MoreThan, ">"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_LCBracket, "{"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_RCBracket, "}"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Dot, "."),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_PowerTo, "^"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Semicolon, ";"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_SingleQuote, "'"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_DoubleQuote, "\""),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_LSBracket, "["),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_RSBracket, "]"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Comma, ","),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Colon, ":"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Backslash, "\\"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Slash, "/"),
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_At, "@")
};

static totemTokenDesc s_reservedWordValues[] =
{
    TOTEM_DESC_TOKEN_WORD(totemTokenType_If, "if"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Do, "do"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_While, "while"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_For, "for"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Return, "return"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Case, "case"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Break, "break"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Function, "function"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Default, "default"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Else, "else"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_True, "true"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_False, "false"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Boolean, "boolean"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Null, "null"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Let, "let"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Is, "is"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Float, "float"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Int, "int"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Array, "array"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_String, "string"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Type, "type"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_As, "as"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Coroutine, "coroutine"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Object, "object"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Userdata, "userdata"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Var, "var")
};

#define TOTEM_LEX_NEWLINECHECK(state, c) if(*c == '\n') { state->CurrentLine++; state->CurrentChar = 1; }
#define TOTEM_LEX_INC(state, c) c++; state->CurrentChar++; TOTEM_LEX_NEWLINECHECK(state, c);

#define TOTEM_LEX_ISWHITESPACE(c) (*c == '\n' || *c == '\t' || *c == '\r' || *c == ' ')
#define TOTEM_LEX_ALLOC(dest, list, start, len, cat, type) \
dest = totemTokenList_Alloc(list, start, len, list->CurrentLine, list->CurrentChar, cat, type); \
if(!dest) return totemLexStatus_OutOfMemory;
#define TOTEM_LEX_CHECKRETURN(status, exp) status = exp; if(status == totemLexStatus_OutOfMemory) return totemLexStatus_Break(status);

void totemToken_Print(FILE *target, totemToken *token)
{
    fprintf(target, "%s %.*s at %"PRId64":%"PRId64"\n",
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

void totemTokenList_Init(totemTokenList *list)
{
    memset(list, 0, sizeof(totemTokenList));
    totemMemoryBuffer_Init(&list->Tokens, sizeof(totemToken));
}

void totemTokenList_Reset(totemTokenList *list)
{
    totemMemoryBuffer_Reset(&list->Tokens);
}

void totemTokenList_Cleanup(totemTokenList *list)
{
    totemMemoryBuffer_Cleanup(&list->Tokens);
}

totemLexStatus totemLexStatus_Break(totemLexStatus status)
{
    return status;
}

totemToken *totemTokenList_Alloc(totemTokenList *list, const char *start, size_t length, size_t lineNumber, size_t charNumber, totemTokenCategory category, totemTokenType type)
{
    totemToken *currentToken = totemMemoryBuffer_Secure(&list->Tokens, 1);
    if (currentToken != NULL)
    {
        currentToken->Category = category;
        currentToken->Type = type;
        
        currentToken->Position.Start = start;
        currentToken->Position.LineNumber = lineNumber;
        currentToken->Position.CharNumber = charNumber;
        currentToken->Position.Length = length;
    }
    
    return currentToken;
}

totemLexStatus totemTokenList_LexNumberAndIdentifierTokens(totemTokenList *list, const char *toCheck, size_t length)
{
    totemToken *currentToken = NULL;
    TOTEM_LEX_ALLOC(currentToken, list, toCheck, 0, totemTokenCategory_Other, isdigit(toCheck[0]) ? totemTokenType_Number : totemTokenType_Identifier);
    
    char *c = (char*)toCheck;
    char *cend = (char*)toCheck + length;
    while (c != cend)
    {
        if (!isdigit(*c))
        {
            if (currentToken->Type != totemTokenType_Identifier)
            {
                TOTEM_LEX_ALLOC(currentToken, list, c, 0, totemTokenCategory_Other, totemTokenType_Identifier);
            }
        }
        else
        {
            if (currentToken->Type != totemTokenType_Number)
            {
                TOTEM_LEX_ALLOC(currentToken, list, c, 0, totemTokenCategory_Other, totemTokenType_Number);
            }
        }
        
        currentToken->Position.Length++;
        c++;
    }
    
    //totemToken_Print(stdout, currentToken);
    
    return totemLexStatus_Success;
}

totemLexStatus totemTokenList_LexReservedWordToken(totemTokenList *list, const char *buffer, size_t length)
{
    for (size_t i = 0; i < TOTEM_ARRAY_SIZE(s_reservedWordValues); ++i)
    {
        totemTokenDesc *desc = &s_reservedWordValues[i];
        
        if (length == desc->Length && strncmp(buffer, desc->Value, length) == 0)
        {
            totemToken *token = NULL;
            TOTEM_LEX_ALLOC(token, list, buffer, desc->Length, totemTokenCategory_ReservedWord, desc->Type);
            
            return totemLexStatus_Success;
        }
    }
    
    return totemLexStatus_Failure;
}

totemLexStatus totemTokenList_LexSymbolToken(totemTokenList *list, const char *toCheck, totemBool *insideStringLiteral)
{
    char c = *toCheck;
    
    for (size_t i = 0; i < TOTEM_ARRAY_SIZE(s_symbolTokenValues); ++i)
    {
        totemTokenDesc *desc = &s_symbolTokenValues[i];
        if (c == desc->Value[0])
        {
            totemToken *token = NULL;
            TOTEM_LEX_ALLOC(token, list, toCheck, 1, totemTokenCategory_Symbol, desc->Type);
            
            size_t numTokens = totemMemoryBuffer_GetNumObjects(&list->Tokens);
            if (numTokens > 0)
            {
                totemToken *lastToken = token - 1;
                *insideStringLiteral ^= token->Type == totemTokenType_DoubleQuote && lastToken->Type != totemTokenType_Slash;
            }
            
            return totemLexStatus_Success;
        }
    }
    
    return totemLexStatus_Failure;
}

totemLexStatus totemTokenList_LexString(totemTokenList *list, const char *toCheck, size_t length)
{
    totemLexStatus status = totemLexStatus_Success;
    
    TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, length));
    if (status != totemLexStatus_Success)
    {
        TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexNumberAndIdentifierTokens(list, toCheck, length));
    }
    
    return status;
}

totemLexStatus totemTokenList_Resolve(totemTokenList *list, const char *toCheck, size_t len, totemBool *insideStringLiteral)
{
    totemLexStatus status = totemLexStatus_Success;
    
    if (len == 1)
    {
        TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck, insideStringLiteral));
        if (status != totemLexStatus_Success)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexString(list, toCheck, len));
        }
    }
    else if (len > 0)
    {
        TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexString(list, toCheck, len));
    }
    
    return status;
}

/**
 * Lex buffer into token list
 */
totemLexStatus totemTokenList_Lex(totemTokenList *list, const char *buffer, size_t length)
{
    if (!length)
    {
        return totemLexStatus_Success;
    }
    
    totemLexStatus status = totemLexStatus_Success;
    totemBool insideStringLiteral = totemBool_False;
    const char *toCheck = buffer;
    const char *c = buffer;
    const char *cend = buffer + length;
    list->CurrentChar = 1;
    list->CurrentLine = 1;
    
    while (1)
    {
        size_t len = c - toCheck;
        
        // end
        if (c >= cend)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_Resolve(list, toCheck, len, &insideStringLiteral));
            
            totemToken *endToken;
            TOTEM_LEX_ALLOC(endToken, list, NULL, 0, totemTokenCategory_Other, totemTokenType_EndScript);
            /*
             FILE *f = fopen("lex.txt", "w");
             totemToken_PrintList(f, list->Tokens.Data, totemMemoryBuffer_GetNumObjects(&list->Tokens));
             fclose(f);
             */
            return totemLexStatus_Success;
        }
        
        // whitespace
        if (TOTEM_LEX_ISWHITESPACE(c))
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_Resolve(list, toCheck, len, &insideStringLiteral));
            
            totemToken *endToken;
            TOTEM_LEX_ALLOC(endToken, list, c, 1, totemTokenCategory_Other, totemTokenType_Whitespace);
            
            while (TOTEM_LEX_ISWHITESPACE(c))
            {
                TOTEM_LEX_INC(list, c);
            }
            
            endToken->Position.Length = (size_t)(c - endToken->Position.Start);
            toCheck = c;
            continue;
        }
        
        // start of comment
        if ((c[0] == '/' && c < cend - 1 && (c[1] == '/' || c[1] == '*')) && !insideStringLiteral)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_Resolve(list, toCheck, len, &insideStringLiteral));
            
            if (!insideStringLiteral)
            {
                switch (c[1])
                {
                    case '/':
                        TOTEM_LEX_INC(list, c);
                        TOTEM_LEX_INC(list, c);
                        
                        while (1)
                        {
                            if (c >= cend)
                            {
                                break;
                            }
                            
                            if (c[0] == '\n')
                            {
                                TOTEM_LEX_INC(list, c);
                                break;
                            }
                            
                            TOTEM_LEX_INC(list, c);
                        }
                        break;
                        
                    case '*':
                        TOTEM_LEX_INC(list, c);
                        TOTEM_LEX_INC(list, c);
                        
                        while (1)
                        {
                            if (c == cend - 1)
                            {
                                TOTEM_LEX_INC(list, c);
                                break;
                            }
                            
                            if (c >= cend)
                            {
                                break;
                            }
                            
                            if (c[0] == '*' && c[1] == '/')
                            {
                                TOTEM_LEX_INC(list, c);
                                TOTEM_LEX_INC(list, c);
                                break;
                            }
                            
                            TOTEM_LEX_INC(list, c);
                        }
                        break;
                }
            }
            
            toCheck = c;
            continue;
        }
        
        if (len == 0)
        {
            TOTEM_LEX_INC(list, c);
            continue;
        }
        
        if (len == 1)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck, &insideStringLiteral));
            if (status == totemLexStatus_Success)
            {
                toCheck = c;
                continue;
            }
        }
        
        TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, c, &insideStringLiteral));
        if (status == totemLexStatus_Success)
        {
            size_t symbolIndex = totemMemoryBuffer_GetNumObjects(&list->Tokens) - 1;
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexString(list, toCheck, len));
            
            // shift symbol to be in front
            totemToken *symbol = totemMemoryBuffer_Get(&list->Tokens, symbolIndex);
            totemToken *last = totemMemoryBuffer_Top(&list->Tokens);
            if (symbol != last)
            {
                totemToken swap;
                memcpy(&swap, symbol, sizeof(totemToken));
                
                for (totemToken *toCopy = symbol + 1, *copyTo = symbol; toCopy <= last; toCopy++, copyTo++)
                {
                    memcpy(copyTo, toCopy, sizeof(totemToken));
                }
                
                memcpy(last, &swap, sizeof(totemToken));
            }
            
            TOTEM_LEX_INC(list, c);
            toCheck = c;
        }
        else
        {
            TOTEM_LEX_INC(list, c);
        }
    }
    
    return totemLexStatus_Success;
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
            TOTEM_STRINGIFY_CASE(totemTokenType_Divide);
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
    }
    
    return "UNKNOWN";
}
