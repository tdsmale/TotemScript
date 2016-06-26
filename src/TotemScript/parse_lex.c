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

typedef enum
{
    totemCommentType_None,
    totemCommentType_Line,
    totemCommentType_Block
}
totemCommentType;

#define TOTEM_DESC_TOKEN_SYMBOL(type, value) { type, totemTokenCategory_Symbol, value }
#define TOTEM_DESC_TOKEN_WORD(type, value) { type, totemTokenCategory_ReservedWord, value }

const static totemTokenDesc s_symbolTokenValues[] =
{
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Variable, "$"),
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
    TOTEM_DESC_TOKEN_SYMBOL(totemTokenType_Whitespace, " "),
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

const static totemTokenDesc s_reservedWordValues[] =
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
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Local, "local"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Userdata, "userdata"),
    TOTEM_DESC_TOKEN_WORD(totemTokenType_Var, "var")
};

#define TOTEM_LEX_CHECKRETURN(status, exp) status = exp; if(status == totemLexStatus_OutOfMemory) return totemLexStatus_Break(status);

void totemToken_Print(FILE *target, totemToken *token)
{
#if _WIN32
    fprintf(target, "%s %.*s at %Iu:%Iu\n", totemTokenType_Describe(token->Type), (int)token->Value.Length, token->Value.Value, token->Position.LineNumber, token->Position.CharNumber);
#else
    fprintf(target, "%s %.*s at %zu:%zu\n", totemTokenType_Describe(token->Type), (int)token->Value.Length, token->Value.Value, token->Position.LineNumber, token->Position.CharNumber);
#endif
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
    list->CurrentChar = 0;
    list->CurrentLine = 0;
    list->NextTokenStart = 0;
}

void totemTokenList_Cleanup(totemTokenList *list)
{
    totemMemoryBuffer_Cleanup(&list->Tokens);
}

totemLexStatus totemLexStatus_Break(totemLexStatus status)
{
    return status;
}

/**
 * Lex buffer into token list
 */
totemLexStatus totemTokenList_Lex(totemTokenList *list, const char *buffer, size_t length)
{
    size_t currentTokenLength = 0;
    list->CurrentLine = 1;
    list->CurrentChar = 0;
    const char *toCheck = NULL;
    totemLexStatus status = totemLexStatus_Success;
    totemCommentType comment = totemCommentType_None;
    
    for (size_t i = 0; i < length; ++i)
    {
        if (buffer[i] == '\r' || buffer[i] == '\t')
        {
            continue;
        }
        
        if (buffer[i] == '\n')
        {
            list->CurrentLine++;
            list->CurrentChar = 0;
            
            if (comment == totemCommentType_Line)
            {
                comment = totemCommentType_None;
            }
            
            continue;
        }
        else
        {
            list->CurrentChar++;
        }
        
        if (comment == totemCommentType_Block)
        {
            if (buffer[i] == '/' && buffer[i - 1] == '*')
            {
                comment = totemCommentType_None;
            }
            
            continue;
        }
        
        if (comment == totemCommentType_Line)
        {
            continue;
        }
        
        if (buffer[i] == '/' && i < length - 1)
        {
            switch (buffer[i + 1])
            {
                case '*':
                    comment = totemCommentType_Block;
                    currentTokenLength = 0;
                    continue;
                    
                case '/':
                    comment = totemCommentType_Line;
                    currentTokenLength = 0;
                    continue;
            }
        }
        
        // lexing new token
        if (currentTokenLength == 0)
        {
            list->NextTokenStart = list->CurrentChar - 1;
            toCheck = buffer + i;
        }
        else if (buffer[i] == ' ')
        {
            // presumed end of token
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, currentTokenLength));
            if (status != totemLexStatus_Success)
            {
                TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexNumberOrIdentifierToken(list, toCheck, currentTokenLength));
            }
            
            currentTokenLength = 0;
            --i;
            
            continue;
        }
        
        ++currentTokenLength;
        
        // do we have a one-char symbol?
        if (currentTokenLength == 1)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck));
            if (status == totemLexStatus_Success)
            {
                currentTokenLength = 0;
            }
            
            continue;
        }
        
        // we now have to presume that more than one token are sat right next to each other
        // let's look for our first one-char symbol, then split it from whatever it was sat after
        for (size_t j = 1; j < currentTokenLength; ++j)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck + j));
            if (status == totemLexStatus_Success)
            {
                // we found a symbol - now lex whatever was in front of it
                TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, j));
                if (status != totemLexStatus_Success)
                {
                    TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexNumberOrIdentifierToken(list, toCheck, j));
                }
                
                // swap previous two tokens around so they're in order
                size_t numTokens = totemMemoryBuffer_GetNumObjects(&list->Tokens);
                totemToken *current = totemMemoryBuffer_Get(&list->Tokens, numTokens - 1);
                totemToken *last = totemMemoryBuffer_Get(&list->Tokens, numTokens - 2);
                totemToken swap;
                memcpy(&swap, current, sizeof(totemToken));
                memcpy(current, last, sizeof(totemToken));
                memcpy(last, &swap, sizeof(totemToken));
                
                currentTokenLength = 0;
            }
        }
    }
    
    totemToken *endToken;
    TOTEM_LEX_ALLOC(endToken, list);
    endToken->Type = totemTokenType_EndScript;
    endToken->Category = totemTokenCategory_Other;
    endToken->Value.Value = NULL;
    endToken->Value.Length = 0;
    
    return totemLexStatus_Success;
}

totemToken *totemTokenList_Alloc(totemTokenList *list)
{
    size_t index = totemMemoryBuffer_GetNumObjects(&list->Tokens);
    if (totemMemoryBuffer_Secure(&list->Tokens, 1) != NULL)
    {
        totemToken *currentToken = totemMemoryBuffer_Get(&list->Tokens, index);
        memset(currentToken, 0, sizeof(totemToken));
        currentToken->Value.Value = NULL;
        currentToken->Value.Length = 0;
        currentToken->Type = totemTokenType_None;
        currentToken->Category = totemTokenCategory_None;
        currentToken->Position.CharNumber = list->NextTokenStart;
        currentToken->Position.LineNumber = list->CurrentLine;
        return currentToken;
    }
    
    return NULL;
}

totemLexStatus totemTokenList_LexNumberOrIdentifierToken(totemTokenList *list, const char *toCheck, size_t length)
{
    totemToken *currentToken = NULL;
    TOTEM_LEX_ALLOC(currentToken, list);
    currentToken->Type = totemTokenType_Number;
    currentToken->Category = totemTokenCategory_Other;
    currentToken->Value.Value = toCheck;
    currentToken->Value.Length = (uint32_t)length;
    
    for (size_t j = 0; j < length; ++j)
    {
        if (!isdigit(toCheck[j]))
        {
            currentToken->Type = totemTokenType_Identifier;
            break;
        }
    }
    
    //totemToken_Print(stdout, currentToken);
    
    return totemLexStatus_Success;
}

totemLexStatus totemTokenList_LexReservedWordToken(totemTokenList *list, const char *buffer, size_t length)
{
    for (size_t i = 0; i < TOTEM_ARRAYSIZE(s_reservedWordValues); ++i)
    {
        size_t tokenLength = strlen(s_reservedWordValues[i].Value);
        if (tokenLength == length && strncmp(buffer, s_reservedWordValues[i].Value, length) == 0)
        {
            totemToken *token = NULL;
            TOTEM_LEX_ALLOC(token, list);
            token->Type = s_reservedWordValues[i].Type;
            token->Category = totemTokenCategory_ReservedWord;
            token->Value.Value = buffer;
            token->Value.Length = (uint32_t)tokenLength;
            
            //totemToken_Print(stdout, token);
            
            return totemLexStatus_Success;
        }
    }
    
    return totemLexStatus_Failure;
}

totemLexStatus totemTokenList_LexSymbolToken(totemTokenList *list, const char *toCheck)
{
    for (size_t i = 0; i < TOTEM_ARRAYSIZE(s_symbolTokenValues); ++i)
    {
        if (toCheck[0] == s_symbolTokenValues[i].Value[0])
        {
            // combine whitespace
            if (s_symbolTokenValues[i].Type == totemTokenType_Whitespace)
            {
                size_t numTokens = totemMemoryBuffer_GetNumObjects(&list->Tokens);
                if (numTokens > 0)
                {
                    totemToken *last = totemMemoryBuffer_Get(&list->Tokens, numTokens - 1);
                    if (last->Type == totemTokenType_Whitespace)
                    {
                        last->Value.Length++;
                        return totemLexStatus_Success;
                    }
                }
            }
            
            totemToken *token = NULL;
            TOTEM_LEX_ALLOC(token, list);
            token->Type = s_symbolTokenValues[i].Type;
            token->Category = totemTokenCategory_Symbol;
            token->Value.Value = toCheck;
            token->Value.Length = 1;
            
            //totemToken_Print(stdout, token);
            
            return totemLexStatus_Success;
        }
    }
    
    return totemLexStatus_Failure;
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
            TOTEM_STRINGIFY_CASE(totemTokenType_Local);
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
            TOTEM_STRINGIFY_CASE(totemTokenType_Variable);
            TOTEM_STRINGIFY_CASE(totemTokenType_While);
            TOTEM_STRINGIFY_CASE(totemTokenType_Whitespace);
    }
    
    return "UNKNOWN";
}