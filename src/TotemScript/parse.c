//
//  parse.c
//  TotemScript
//
//  Created by Timothy Smale on 19/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <TotemScript/base.h>
#include <string.h>
#include <ctype.h>

#define TOTEM_PARSE_SKIPWHITESPACE(token) while(token->Type == totemTokenType_Whitespace) { token++; }
#define TOTEM_PARSE_COPYPOSITION(token, dest) memcpy(&dest->Position, &token->Position, sizeof(totemBufferPositionInfo))
#define TOTEM_PARSE_ALLOC(dest, type, tree) dest = (type*)totemParseTree_Alloc(tree, sizeof(type)); if(!dest) return totemParseStatus_Break(totemParseStatus_OutOfMemory);
#define TOTEM_PARSE_CHECKRETURN(exp) { totemParseStatus s = exp; if(s != totemParseStatus_Success) return s; }
#define TOTEM_PARSE_ENFORCETOKEN(token, type) if(token->Type != type) return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
#define TOTEM_PARSE_ENFORCENOTTOKEN(token, type) if(token->Type == type) return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
#define TOTEM_PARSE_INC_NOT_ENDSCRIPT(token) token++; TOTEM_PARSE_ENFORCENOTTOKEN(token, totemTokenType_EndScript);

#define TOTEM_DESC_TOKEN_SYMBOL(type, value) { type, totemTokenCategory_Symbol, value }
#define TOTEM_DESC_TOKEN_WORD(type, value) { type, totemTokenCategory_ReservedWord, value }

const static totemTokenDesc s_symbolTokenValues[] =
{
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Variable, "$" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Plus, "+" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Minus, "-" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Multiply, "*" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Divide, "/" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Not, "!" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_And, "&" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Or, "|" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Assign, "=" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_LBracket, "(" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_RBracket, ")" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_LessThan, "<" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_MoreThan, ">" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_LCBracket, "{" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_RCBracket, "}" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Dot, "." ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_PowerTo, "^" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Semicolon, ";" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Whitespace, " " ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_SingleQuote, "'" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_DoubleQuote, "\"" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_LSBracket, "[" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_RSBracket, "]" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Comma, "," ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Colon, ":" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Backslash, "\\" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_Slash, "/" ),
    TOTEM_DESC_TOKEN_SYMBOL( totemTokenType_At, "@" )
};

const static totemTokenDesc s_reservedWordValues[] =
{
    TOTEM_DESC_TOKEN_WORD( totemTokenType_If, "if" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Do, "do" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_While, "while" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_For, "for" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Return, "return" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Case, "case" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Break, "break" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Function, "function" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Default, "default" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Else, "else" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_True, "true" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_False, "false" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Null, "null" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Const, "const" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Is, "is" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Float, "float" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Int, "int" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Array, "array" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_String, "string" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Type, "type" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_As, "as" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Coroutine, "coroutine" ),
    TOTEM_DESC_TOKEN_WORD( totemTokenType_Object, "object" )
};

#define TOTEM_LEX_CHECKRETURN(status, exp) status = exp; if(status == totemLexStatus_OutOfMemory) return totemLexStatus_Break(status);

typedef enum
{
    totemCommentType_None,
    totemCommentType_Line,
    totemCommentType_Block
}
totemCommentType;

typedef struct totemScriptName
{
    const char *Filename;
    struct totemScriptName *Next;
}
totemScriptName;

#define TOTEM_LOADSCRIPT_SKIPWHITESPACE(str) while(str[0] == ' ' || str[0] == '\n' || str[0] == '\t' || str[0] == '\r') str++;

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
    for(size_t i = 0 ; i < num; ++i)
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

const char *totemLoadScriptStatus_Describe(totemLoadScriptStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_FileNotFound);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_Recursion);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_Success);
    }
    
    return "UNKNOWN";
}

totemBool totemScriptName_Push(totemScriptName **tree, const char *name)
{
    totemScriptName *treeEntry = totem_CacheMalloc(sizeof(totemScriptName));
    if(treeEntry == NULL)
    {
        return totemBool_False;
    }
    
    treeEntry->Filename = name;
    treeEntry->Next = *tree;
    *tree = treeEntry;
    
    return totemBool_True;
}

totemBool totemScriptName_Search(totemScriptName **tree, const char *filename)
{
    totemScriptName *treeEntry = *tree;
    while(treeEntry != NULL)
    {
        if(strcmp(treeEntry->Filename, filename) == 0)
        {
            return totemBool_True;
        }
        
        treeEntry = treeEntry->Next;
    }
    
    return totemBool_False;
}

const char *totemScriptName_Pop(totemScriptName **tree)
{
    if(*tree)
    {
        totemScriptName *toDelete = *tree;
        *tree = (*tree)->Next;
        
        const char *filename = toDelete->Filename;
        totem_CacheFree(toDelete, sizeof(totemScriptName));
        
        return filename;
    }
    
    return NULL;
}

void totemScriptFile_Init(totemScriptFile *script)
{
    totemMemoryBuffer_Init(&script->Buffer, 1);
}

void totemScriptFile_Reset(totemScriptFile *script)
{
    totemMemoryBuffer_Reset(&script->Buffer);
}

void totemScriptFile_Cleanup(totemScriptFile *script)
{
    totemMemoryBuffer_Cleanup(&script->Buffer);
}

totemBool totemScriptFile_LoadRecursive(totemScriptFile *dst, const char *srcPath, totemLoadScriptError *err, totemScriptName **tree)
{
    FILE *file = totem_fopen(srcPath, "r");
    if(!file)
    {
        err->Status = totemLoadScriptStatus_FileNotFound;
        totemString_FromLiteral(&err->Description, srcPath);
        return totemBool_False;
    }
    
    fseek(file, 0, SEEK_END);
    long fSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (!totem_fchdir(file))
    {
        return totemBool_False;
    }
    
    // add to tree
    if(totemScriptName_Push(tree, srcPath) == totemBool_False)
    {
        err->Status = totemLoadScriptStatus_OutOfMemory;
        fclose(file);
        return totemBool_False;
    }
    
    // look for include statements
    while(totemBool_True)
    {
        long resetPos = ftell(file);
        char cha = fgetc(file);
        
        while((cha == ' ' || cha == '\n' || cha == '\t' || cha == '\r') && cha != EOF)
        {
            resetPos = ftell(file);
            cha = fgetc(file);
        }
        
        fseek(file, resetPos, SEEK_SET);
        
        // any include statements?
        char include[8];
        include[7] = 0;
        size_t numRead = fread(include, 1, 7, file);
        if(numRead != 7)
        {
            fseek(file, resetPos, SEEK_SET);
            break;
        }
        
        if(strncmp("include", include, 7) != 0)
        {
            fseek(file, resetPos, SEEK_SET);
            break;
        }
        
        resetPos = ftell(file);
        for(cha = fgetc(file); cha == ' ' && cha != EOF; /* nada */)
        {
            resetPos = ftell(file);
            cha = fgetc(file);
        }
        
        fseek(file, resetPos, SEEK_SET);
        
        int filenameSize = 0;
        for(cha = fgetc(file); cha != ';' && cha != EOF; /* nada */)
        {
            if (filenameSize + 1 < 0)
            {
                break;
            }
            
            filenameSize++;
            cha = fgetc(file);
        }
        
        char *filename = totem_CacheMalloc(filenameSize + 1);
        if(filename == NULL)
        {
            fclose(file);
            totemScriptName_Pop(tree);
            err->Status = totemLoadScriptStatus_OutOfMemory;
            return totemBool_False;
        }
        
        fseek(file, resetPos, SEEK_SET);
        fread(filename, 1, filenameSize, file);
        filename[filenameSize] = 0;
        fseek(file, 2, SEEK_CUR);
        
        // already in tree? skip this 'un
        if(totemScriptName_Search(tree, filename) == totemBool_True)
        {
            continue;
        }
        
        totemScriptName *restore = *tree;
        
        totemBool result = totemScriptFile_LoadRecursive(dst, filename, err, tree);
        totem_CacheFree(filename, filenameSize + 1);
        if(!result)
        {
            fclose(file);
            totemScriptName_Pop(tree);
            return totemBool_False;
        }
        
        if (!totem_fchdir(file))
        {
            return totemBool_False;
        }
        
        *tree = restore;
    }
    
    char *buffer = totemMemoryBuffer_Secure(&dst->Buffer, fSize);
    if(!buffer)
    {
        fclose(file);
        totemScriptName_Pop(tree);
        err->Status = totemLoadScriptStatus_OutOfMemory;
        totemString_FromLiteral(&err->Description, "");
        return totemBool_False;
    }
    
    fread(buffer, 1, fSize, file);
    
    fclose(file);
    totemScriptName_Pop(tree);
    return totemBool_True;
}

totemBool totemScriptFile_Load(totemScriptFile *dst, const char *srcPath, totemLoadScriptError *err)
{
    totemScriptFile_Reset(dst);
    totemScriptName *nameTree = NULL;
    const char *currentDir = totem_getcwd();
    
    totemBool result = totemScriptFile_LoadRecursive(dst, srcPath, err, &nameTree);
    
    if(result == totemBool_True)
    {
        size_t bufferSize = totemMemoryBuffer_GetNumObjects(&dst->Buffer);
        if(bufferSize > 0)
        {
            if(!totemMemoryBuffer_Secure(&dst->Buffer, 1))
            {
                return totemBool_False;
            }
            
            dst->Buffer.Data[bufferSize] = 0;
        }
    }
    
    totem_chdir(currentDir);
    totem_freecwd(currentDir);
    return result;
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
    
    for(size_t i = 0; i < length; ++i)
    {
        if(buffer[i] == '\r' || buffer[i] == '\t')
        {
            continue;
        }
        
        if(buffer[i] == '\n')
        {
            list->CurrentLine++;
            list->CurrentChar = 0;
            
            if(comment == totemCommentType_Line)
            {
                comment = totemCommentType_None;
            }
            
            continue;
        }
        else
        {
            list->CurrentChar++;
        }
        
        if(comment == totemCommentType_Block)
        {
            if(buffer[i] == '/' && buffer[i - 1] == '*')
            {
                comment = totemCommentType_None;
            }
            
            continue;
        }
        
        if(comment == totemCommentType_Line)
        {
            continue;
        }
        
        if(buffer[i] == '/' && i < length - 1)
        {
            switch(buffer[i + 1])
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
        if(currentTokenLength == 0)
        {
            list->NextTokenStart = list->CurrentChar - 1;
            toCheck = buffer + i;
        }
        else if(buffer[i] == ' ')
        {
            // presumed end of token
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, currentTokenLength));
            if(status != totemLexStatus_Success)
            {
                TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexNumberOrIdentifierToken(list, toCheck, currentTokenLength));
            }
            
            currentTokenLength = 0;
            --i;
            
            continue;
        }
        
        ++currentTokenLength;
        
        // do we have a one-char symbol?
        if(currentTokenLength == 1)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck));
            if(status == totemLexStatus_Success)
            {
                currentTokenLength = 0;
            }
            
            continue;
        }
        
        // we now have to presume that more than one token are sat right next to each other
        // let's look for our first one-char symbol, then split it from whatever it was sat after
        for(size_t j = 1; j < currentTokenLength; ++j)
        {
            TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexSymbolToken(list, toCheck + j));
            if(status == totemLexStatus_Success)
            {
                // we found a symbol - now lex whatever was in front of it
                TOTEM_LEX_CHECKRETURN(status, totemTokenList_LexReservedWordToken(list, toCheck, j));
                if(status != totemLexStatus_Success)
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
    if(totemMemoryBuffer_Secure(&list->Tokens, 1) != NULL)
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
    
    for(size_t j = 0; j < length; ++j)
    {
        if(!isdigit(toCheck[j]))
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
    for(size_t i = 0 ; i < TOTEM_ARRAYSIZE(s_reservedWordValues); ++i)
    {
        size_t tokenLength = strlen(s_reservedWordValues[i].Value);
        if(tokenLength == length && strncmp(buffer, s_reservedWordValues[i].Value, length) == 0)
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
    for(size_t i = 0 ; i < TOTEM_ARRAYSIZE(s_symbolTokenValues); ++i)
    {
        if(toCheck[0] == s_symbolTokenValues[i].Value[0])
        {
            // combine whitespace
            if(s_symbolTokenValues[i].Type == totemTokenType_Whitespace)
            {
                size_t numTokens = totemMemoryBuffer_GetNumObjects(&list->Tokens);
                if(numTokens > 0)
                {
                    totemToken *last = totemMemoryBuffer_Get(&list->Tokens, numTokens - 1);
                    if(last->Type == totemTokenType_Whitespace)
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

void *totemParseTree_Alloc(totemParseTree *tree, size_t objectSize)
{
    return totemMemoryBlock_Alloc(&tree->LastMemBlock, objectSize);
}

void totemParseTree_Cleanup(totemParseTree *tree)
{
    totemMemoryBlock_Cleanup(&tree->LastMemBlock);
    tree->FirstBlock = NULL;
    tree->CurrentToken = NULL;
}

void totemParseTree_Reset(totemParseTree *tree)
{
    totemParseTree_Cleanup(tree);
}

void totemParseTree_Init(totemParseTree *tree)
{
    memset(tree, 0, sizeof(totemParseTree));
    totemParseTree_Reset(tree);
}

totemParseStatus totemParseStatus_Break(totemParseStatus status)
{
    return status;
}

/**
 * Create parse tree from token list
 */
totemParseStatus totemParseTree_Parse(totemParseTree *tree, totemTokenList *tokens)
{
    totemBlockPrototype *lastBlock = NULL;
    
    for(tree->CurrentToken = (totemToken*)tokens->Tokens.Data; tree->CurrentToken->Type != totemTokenType_EndScript; )
    {
        totemBlockPrototype *block;
        TOTEM_PARSE_ALLOC(block, totemBlockPrototype, tree);
        
        totemParseStatus status = totemBlockPrototype_Parse(block, tree);
        if(status != totemParseStatus_Success)
        {
            return status;
        }
        
        if(lastBlock == NULL)
        {
            tree->FirstBlock = block;
        }
        else
        {
            lastBlock->Next = block;
        }
        
        lastBlock = block;
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemBlockPrototype_Parse(totemBlockPrototype *block, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, block);
    
    switch (tree->CurrentToken->Type)
    {
        case totemTokenType_Function:
        {
            block->Type = totemBlockType_FunctionDeclaration;
            TOTEM_PARSE_ALLOC(block->FuncDec, totemFunctionDeclarationPrototype, tree);
            return totemFunctionDeclarationPrototype_Parse(block->FuncDec, tree, totemBool_False);
        }
            
        default:
        {
            block->Type = totemBlockType_Statement;
            TOTEM_PARSE_ALLOC(block->Statement, totemStatementPrototype, tree);
            return totemStatementPrototype_Parse(block->Statement, tree);
        }
    }
}

totemParseStatus totemStatementPrototype_Parse(totemStatementPrototype *statement, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, statement);
    
    memcpy(&statement->Position, &tree->CurrentToken->Position, sizeof(totemBufferPositionInfo));
    
    switch (tree->CurrentToken->Type)
    {
        case totemTokenType_While:
        {
            statement->Type = totemStatementType_WhileLoop;
            TOTEM_PARSE_ALLOC(statement->WhileLoop, totemWhileLoopPrototype, tree);
            return totemWhileLoopPrototype_Parse(statement->WhileLoop, tree);
        }
            
        case totemTokenType_Do:
        {
            statement->Type = totemStatementType_DoWhileLoop;
            TOTEM_PARSE_ALLOC(statement->DoWhileLoop, totemDoWhileLoopPrototype, tree);
            return totemDoWhileLoopPrototype_Parse(statement->DoWhileLoop, tree);
        }
            
        case totemTokenType_For:
        {
            statement->Type = totemStatementType_ForLoop;
            TOTEM_PARSE_ALLOC(statement->ForLoop, totemForLoopPrototype, tree);
            return totemForLoopPrototype_Parse(statement->ForLoop, tree);
        }
            
        case totemTokenType_If:
        {
            statement->Type = totemStatementType_IfBlock;
            TOTEM_PARSE_ALLOC(statement->IfBlock, totemIfBlockPrototype, tree);
            return totemIfBlockPrototype_Parse(statement->IfBlock, tree);
        }
            
        case totemTokenType_Return:
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            
            statement->Type = totemStatementType_Return;
            TOTEM_PARSE_ALLOC(statement->Return, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Return, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
            tree->CurrentToken++;
            return totemParseStatus_Success;
        }
            
        default:
        {
            statement->Type = totemStatementType_Simple;
            TOTEM_PARSE_ALLOC(statement->Simple, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Simple, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
            tree->CurrentToken++;
            return totemParseStatus_Success;
        }
    }
}

totemParseStatus totemStatementPrototype_ParseInSet(totemStatementPrototype **first, totemStatementPrototype **last, totemParseTree *tree)
{
    totemStatementPrototype *statement = NULL;
    TOTEM_PARSE_ALLOC(statement, totemStatementPrototype, tree);
    
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_Parse(statement, tree))
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(*first == NULL)
    {
        *first = statement;
    }
    else
    {
        (*last)->Next = statement;
    }
    
    *last = statement;
    return totemParseStatus_Success;
}

totemParseStatus totemStatementPrototype_ParseSet(totemStatementPrototype **first, totemStatementPrototype **last, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LCBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    while (tree->CurrentToken->Type != totemTokenType_RCBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseInSet(first, last, tree));
    }
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterList(totemVariablePrototype **first, totemVariablePrototype **last, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    while (tree->CurrentToken->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterInList(first, last, tree));
        
        if (tree->CurrentToken->Type == totemTokenType_Comma)
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCENOTTOKEN(tree->CurrentToken, totemTokenType_RBracket);
        }
    }
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    while (tree->CurrentToken->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterInList(first, last, tree));
        
        if (tree->CurrentToken->Type == totemTokenType_Comma)
        {
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCENOTTOKEN(tree->CurrentToken, totemTokenType_RBracket);
        }
    }
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterInList(totemVariablePrototype **first, totemVariablePrototype **last, totemParseTree *tree)
{
    totemVariablePrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemVariablePrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_Parse(parameter, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemTokenType nextType = tree->CurrentToken->Type;
    
    if(nextType != totemTokenType_RBracket && nextType != totemTokenType_Comma)
    {
        return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
    }
    
    if(*first == NULL)
    {
        *first = parameter;
    }
    else
    {
        (*last)->Next = parameter;
    }
    
    *last = parameter;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterInList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemParseTree *tree)
{
    totemExpressionPrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(parameter, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type != totemTokenType_RBracket && tree->CurrentToken->Type != totemTokenType_Comma)
    {
        return totemParseStatus_UnexpectedToken;
    }
    
    if(*first == NULL)
    {
        *first = parameter;
    }
    else
    {
        (*last)->Next = parameter;
    }
    
    *last = parameter;
    return totemParseStatus_Success;
}

totemParseStatus totemWhileLoopPrototype_Parse(totemWhileLoopPrototype *loop, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, loop);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemDoWhileLoopPrototype_Parse(totemDoWhileLoopPrototype *loop, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, loop);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Do);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    loop->StatementsStart = firstStatement;
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, tree));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
    
    tree->CurrentToken++;
    return totemParseStatus_Success;
}

totemParseStatus totemIfBlockPrototype_Parse(totemIfBlockPrototype *block, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, block);
    block->ElseType = totemIfElseBlockType_None;
    
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_If);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, tree));
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    block->StatementsStart = firstStatement;
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_Else)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
        
        switch(tree->CurrentToken->Type)
        {
            case totemTokenType_If:
            {
                block->ElseType = totemIfElseBlockType_ElseIf;
                TOTEM_PARSE_ALLOC(block->IfElseBlock, totemIfBlockPrototype, tree);
                TOTEM_PARSE_CHECKRETURN(totemIfBlockPrototype_Parse(block->IfElseBlock, tree));
                break;
            }
                
            case totemTokenType_LCBracket:
            {
                block->ElseType = totemIfElseBlockType_Else;
                TOTEM_PARSE_ALLOC(block->ElseBlock, totemElseBlockPrototype, tree);
                TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, block->ElseBlock);
                
                totemStatementPrototype *firstElseStatement = NULL, *lastElseStatement = NULL;
                TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstElseStatement, &lastElseStatement, tree));
                block->ElseBlock->StatementsStart = firstElseStatement;
                break;
            }
                
            default:
                return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
        }
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemForLoopPrototype_Parse(totemForLoopPrototype *loop, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, loop);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_For);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    TOTEM_PARSE_ALLOC(loop->Initialisation, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Initialisation, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->Condition, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Condition, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(loop->AfterThought, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->AfterThought, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_RBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionDeclarationPrototype_Parse(totemFunctionDeclarationPrototype *func, totemParseTree *tree, totemBool isAnonymous)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Function);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, func);
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(!isAnonymous)
    {
        TOTEM_PARSE_ALLOC(func->Identifier, totemString, tree);
        TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(func->Identifier, tree, totemBool_True));
    }
    
    totemVariablePrototype *firstParameter = NULL, *lastParameter = NULL;
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterList(&firstParameter, &lastParameter, tree));
    func->ParametersStart = firstParameter;
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, tree));
    func->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_Parse(totemExpressionPrototype *expression, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, expression);
    
    // pre-unary operators
    for(totemPreUnaryOperatorPrototype **preUnaryType = &expression->PreUnaryOperators; totemBool_True; /* nada */)
    {
        TOTEM_PARSE_CHECKRETURN(totemPreUnaryOperatorPrototype_Parse(preUnaryType, tree));
        if(!(*preUnaryType))
        {
            break;
        }
        else
        {
            preUnaryType = &(*preUnaryType)->Next;
        }
    }
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    if(tree->CurrentToken->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
        expression->LValueType = totemLValueType_Expression;
        TOTEM_PARSE_ALLOC(expression->LValueExpression, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->LValueExpression, tree));
        
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
        TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RBracket);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    else
    {
        expression->LValueType = totemLValueType_Argument;
        TOTEM_PARSE_ALLOC(expression->LValueArgument, totemArgumentPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemArgumentPrototype_Parse(expression->LValueArgument, tree));
    }
    
    // post-unary operators
    for(totemPostUnaryOperatorPrototype **postUnaryType = &expression->PostUnaryOperators; totemBool_True; /* nada */)
    {
        TOTEM_PARSE_CHECKRETURN(totemPostUnaryOperatorPrototype_Parse(postUnaryType, tree));
        if(!(*postUnaryType))
        {
            break;
        }
        else
        {
            postUnaryType = &(*postUnaryType)->Next;
        }
    }
    
    TOTEM_PARSE_CHECKRETURN(totemBinaryOperatorType_Parse(&expression->BinaryOperator, tree));
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        TOTEM_PARSE_ALLOC(expression->RValue, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->RValue, tree));
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemPreUnaryOperatorPrototype_Parse(totemPreUnaryOperatorPrototype **type, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Plus:
            TOTEM_PARSE_ALLOC(*type, totemPreUnaryOperatorPrototype, tree);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Plus);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            (*type)->Type = totemPreUnaryOperatorType_Inc;
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_ALLOC(*type, totemPreUnaryOperatorPrototype, tree);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            if(tree->CurrentToken->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                (*type)->Type = totemPreUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            (*type)->Type = totemPreUnaryOperatorType_Negative;
            return totemParseStatus_Success;
            
        case totemTokenType_Not:
            TOTEM_PARSE_ALLOC(*type, totemPreUnaryOperatorPrototype, tree);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            (*type)->Type = totemPreUnaryOperatorType_LogicalNegate;
            return totemParseStatus_Success;
            
        default:
            return totemParseStatus_Success;
    }
}

totemParseStatus totemPostUnaryOperatorPrototype_Parse(totemPostUnaryOperatorPrototype **type, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Plus:
            if ((tree->CurrentToken + 1)->Type == totemTokenType_Plus)
            {
                TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
                
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                (*type)->Type = totemPostUnaryOperatorType_Inc;
                return totemParseStatus_Success;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            if((tree->CurrentToken + 1)->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                (*type)->Type = totemPostUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_LBracket:
            TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
            (*type)->Type = totemPostUnaryOperatorType_Invocation;
            
            // parse parameters
            totemExpressionPrototype *first = NULL, *last = NULL;
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterList(&first, &last, tree));
            (*type)->InvocationParametersStart = first;
            
            return totemParseStatus_Success;
            
        case totemTokenType_LSBracket:
        {
            TOTEM_PARSE_ALLOC(*type, totemPostUnaryOperatorPrototype, tree);
            (*type)->Type = totemPostUnaryOperatorType_ArrayAccess;
            
            // parse index
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ALLOC((*type)->ArrayAccess, totemExpressionPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse((*type)->ArrayAccess, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RSBracket);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            
            return totemParseStatus_Success;
        }
            
        default:
            return totemParseStatus_Success;
    }
}

totemParseStatus totemBinaryOperatorType_Parse(totemBinaryOperatorType *type, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Assign:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_Equals;
                break;
                
            default:
                *type = totemBinaryOperatorType_Assign;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_MoreThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_MoreThanEquals;
                break;
                
            default:
                *type = totemBinaryOperatorType_MoreThan;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_LessThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_LessThanEquals;
                break;
                
            default:
                *type = totemBinaryOperatorType_LessThan;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Divide:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_DivideAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Divide;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_MinusAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Minus;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Multiply:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_MultiplyAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Multiply;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Plus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch(tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_PlusAssign;
                break;
                
            default:
                *type = totemBinaryOperatorType_Plus;
                break;
        }
            
            return totemParseStatus_Success;
            
        case totemTokenType_And:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_And);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_LogicalAnd;
            return totemParseStatus_Success;
            
        case totemTokenType_Or:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Or);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_LogicalOr;
            return totemParseStatus_Success;
            
        case totemTokenType_Is:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_IsType;
            return totemParseStatus_Success;
            
        case totemTokenType_As:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            *type = totemBinaryOperatorType_AsType;
            return totemParseStatus_Success;
            
        case totemTokenType_Not:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            switch (tree->CurrentToken->Type)
        {
            case totemTokenType_Assign:
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                *type = totemBinaryOperatorType_NotEquals;
                break;
                
            default:
                *type = totemBinaryOperatorType_None;
                tree->CurrentToken--;
                break;
        }
            
            return totemParseStatus_Success;
            
        default:
            *type = totemBinaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemVariablePrototype_Parse(totemVariablePrototype *variable, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, variable);
    
    if(tree->CurrentToken->Type == totemTokenType_Const)
    {
        variable->IsConst = totemBool_True;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    }
    else
    {
        variable->IsConst = totemBool_False;
    }
    
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Variable);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&variable->Identifier, tree, totemBool_False));
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionCallPrototype_Parse(totemFunctionCallPrototype *call, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&call->Identifier, tree, totemBool_True));
    
    totemExpressionPrototype *first = NULL, *last = NULL;
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterList(&first, &last, tree));
    call->ParametersStart = first;
    return totemParseStatus_Success;
}

totemParseStatus totemString_ParseNumber(totemString *number, totemParseTree *tree)
{
    number->Length = 0;
    number->Value = tree->CurrentToken->Value.Value;
    
    while(tree->CurrentToken->Type == totemTokenType_Number || tree->CurrentToken->Type == totemTokenType_Dot)
    {
        number->Length += tree->CurrentToken->Value.Length;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    tree->CurrentToken--;
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Number);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemArgumentPrototype_Parse(totemArgumentPrototype *argument, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_COPYPOSITION(tree->CurrentToken, argument);
    
    switch(tree->CurrentToken->Type)
    {
        case totemTokenType_Function:
        {
            totemToken *reset = tree->CurrentToken;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            
            if(tree->CurrentToken->Type == totemTokenType_LBracket)
            {
                // anonymous function
                tree->CurrentToken = reset;
                argument->Type = totemArgumentType_FunctionDeclaration;
                TOTEM_PARSE_ALLOC(argument->FunctionDeclaration, totemFunctionDeclarationPrototype, tree);
                TOTEM_PARSE_CHECKRETURN(totemFunctionDeclarationPrototype_Parse(argument->FunctionDeclaration, tree, totemBool_True));
            }
            else
            {
                // function type
                argument->DataType = totemPublicDataType_Function;
                argument->Type = totemArgumentType_Type;
            }
            
            break;
        }
            
            // function pointer
        case totemTokenType_At:
            argument->Type = totemArgumentType_FunctionPointer;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_ALLOC(argument->FunctionPointer, totemString, tree);
            TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(argument->FunctionPointer, tree, totemBool_True));
            break;
            
            // types
        case totemTokenType_Array:
            argument->DataType = totemPublicDataType_Array;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Int:
            argument->DataType = totemPublicDataType_Int;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Float:
            argument->DataType = totemPublicDataType_Float;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_String:
            argument->DataType = totemPublicDataType_String;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Type:
            argument->DataType = totemPublicDataType_Type;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Object:
            argument->DataType = totemPublicDataType_Object;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        case totemTokenType_Coroutine:
            argument->DataType = totemPublicDataType_Coroutine;
            argument->Type = totemArgumentType_Type;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
            // new object
        case totemTokenType_LCBracket:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
            TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RCBracket);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            argument->Type = totemArgumentType_NewObject;
            break;
            
            // new array
        case totemTokenType_LSBracket:
            argument->Type = totemArgumentType_NewArray;
            TOTEM_PARSE_ALLOC(argument->NewArray, totemNewArrayPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemNewArrayPrototype_Parse(argument->NewArray, tree));
            break;
            
            // variable
        case totemTokenType_Variable:
        case totemTokenType_Const:
            argument->Type = totemArgumentType_Variable;
            TOTEM_PARSE_ALLOC(argument->Variable, totemVariablePrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_Parse(argument->Variable, tree));
            break;
            
            // null
        case totemTokenType_Null:
            argument->Type = totemArgumentType_Number;
            TOTEM_PARSE_ALLOC(argument->Number, totemString, tree);
            totemString_FromLiteral(argument->Number, "0");
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
            // number
        case totemTokenType_Number:
            argument->Type = totemArgumentType_Number;
            TOTEM_PARSE_ALLOC(argument->Number, totemString, tree);
            TOTEM_PARSE_CHECKRETURN(totemString_ParseNumber(argument->Number, tree));
            break;
            
            // string
        case totemTokenType_DoubleQuote:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            
            argument->Type = totemArgumentType_String;
            const char *begin = tree->CurrentToken->Value.Value;
            uint32_t len = 0;
            
            while(tree->CurrentToken->Type != totemTokenType_DoubleQuote)
            {
                len += tree->CurrentToken->Value.Length;
                
                if(tree->CurrentToken->Type == totemTokenType_Backslash)
                {
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                    
                    if(tree->CurrentToken->Type == totemTokenType_DoubleQuote)
                    {
                        len += tree->CurrentToken->Value.Length;
                        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                    }
                }
                else
                {
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
                }
            }
            
            TOTEM_PARSE_ALLOC(argument->String, totemString, tree);
            argument->String->Value = begin;
            argument->String->Length = len;
            
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
            // function call
        case totemTokenType_Identifier:
            argument->Type = totemArgumentType_FunctionCall;
            TOTEM_PARSE_ALLOC(argument->FunctionCall, totemFunctionCallPrototype, tree);
            TOTEM_PARSE_CHECKRETURN(totemFunctionCallPrototype_Parse(argument->FunctionCall, tree));
            break;
            
            // boolean
        case totemTokenType_True:
        case totemTokenType_False:
            argument->Type = totemArgumentType_Number;
            TOTEM_PARSE_ALLOC(argument->Number, totemString, tree);
            totemString_FromLiteral(argument->Number, tree->CurrentToken->Type == totemTokenType_True ? "1" : "0");
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
            break;
            
        default:
            return totemParseStatus_Break(totemParseStatus_UnexpectedToken);
    }
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemNewArrayPrototype_Parse(totemNewArrayPrototype *arr, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_LSBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    TOTEM_PARSE_ALLOC(arr->Accessor, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(arr->Accessor, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_RSBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    return totemParseStatus_Success;
}

totemParseStatus totemString_ParseIdentifier(totemString *string, totemParseTree *tree, totemBool strict)
{
    TOTEM_PARSE_SKIPWHITESPACE(tree->CurrentToken);
    
    const char *start = tree->CurrentToken->Value.Value;
    uint32_t length = 0;
    
    if(strict)
    {
        TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Identifier);
        
        while(tree->CurrentToken->Type == totemTokenType_Identifier)
        {
            length += tree->CurrentToken->Value.Length;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        }
    }
    else
    {
        while(tree->CurrentToken->Type != totemTokenType_Whitespace && (tree->CurrentToken->Category == totemTokenCategory_ReservedWord || tree->CurrentToken->Type == totemTokenType_Identifier))
        {
            length += tree->CurrentToken->Value.Length;
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
        }
    }
    
    string->Length = length;
    string->Value = start;
    
    if(strict)
    {
        tree->CurrentToken--;
        TOTEM_PARSE_ENFORCETOKEN(tree->CurrentToken, totemTokenType_Identifier);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(tree->CurrentToken);
    }
    
    return totemParseStatus_Success;
}

const char *totemParseStatus_Describe(totemParseStatus status)
{
    switch (status)
    {
            TOTEM_STRINGIFY_CASE(totemParseStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemParseStatus_Success);
            TOTEM_STRINGIFY_CASE(totemParseStatus_UnexpectedToken);
            TOTEM_STRINGIFY_CASE(totemParseStatus_ValueTooLarge);
    }
    
    return "UNKNOWN";
}

const char *totemLexStatus_Describe(totemLexStatus status)
{
    switch(status)
    {
            TOTEM_STRINGIFY_CASE(totemLexStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLexStatus_Success);
            TOTEM_STRINGIFY_CASE(totemLexStatus_Failure);
    }
    
    return "UNKNOWN";
}

const char *totemTokenType_Describe(totemTokenType type)
{
    switch(type)
    {
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
            TOTEM_STRINGIFY_CASE(totemTokenType_Const);
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
            TOTEM_STRINGIFY_CASE(totemTokenType_Variable);
            TOTEM_STRINGIFY_CASE(totemTokenType_While);
            TOTEM_STRINGIFY_CASE(totemTokenType_Whitespace);
    }
    
    return "UNKNOWN";
}