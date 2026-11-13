//
//  parse.h
//  ColossusEngine
//
//  Created by Timothy Smale on 27/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef ColossusEngine_parse_h
#define ColossusEngine_parse_h

#include <TotemScript/base.h>
#include <TotemScript/memory.h>
#include <TotemScript/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOTEM_MAX_TOKEN_LENGTH (2048)

#define TOTEM_EMIT_SYMBOLS(_) \
	_(DoubleQuote,	'"') \
	_(Plus,			'+') \
	_(Minus,		'-') \
	_(Star,		    '*') \
	_(Not,			'!') \
	_(And,			'&') \
	_(Or,			'|') \
	_(Assign,		'=') \
	_(LBracket,		'(') \
	_(RBracket,		')') \
	_(LessThan,		'<') \
	_(MoreThan,		'>') \
	_(LCBracket,	'{') \
	_(RCBracket,	'}') \
	_(Dot,			'.') \
	_(Caret,		'^') \
	_(Semicolon,	';') \
	_(SingleQuote,	'\'') \
	_(LSBracket,	'[') \
	_(RSBracket,	']') \
	_(Comma,		',') \
	_(Colon,		':') \
	_(Backslash,	'\\') \
	_(Slash,		'/') \
	_(At,			'@') \
	_(Hash,			'#') \
    _(Tilde,        '~') \

#define TOTEM_EMIT_RESERVED_WORDS(_) \
    _(Null,         "null") \
    _(Var,          "var") \
    _(If,           "if") \
    _(Do,           "do") \
    _(While,        "while") \
    _(For,          "for") \
    _(Return,       "return") \
    _(Case,         "case") \
    _(Function,     "function") \
    _(Break,        "break") \
    _(Default,      "default") \
    _(Else,         "else") \
    _(True,         "true") \
    _(False,        "false") \
    _(Bool,         "bool") \
    _(Let,          "let") \
    _(Is,           "is") \
    _(Int,          "int") \
    _(Float,        "float") \
    _(Array,        "array") \
    _(String,       "string") \
    _(Type,         "type") \
    _(Object,       "object") \
    _(As,           "as") \
    _(Coroutine,    "coroutine") \
    _(Userdata,     "userdata") \
    _(Yield,        "yield") \
    _(Continue,     "continue") \
    _(Loop,         "loop") \

	typedef enum totemLexStatus
    {
        totemLexStatus_Success,
        totemLexStatus_Failure,
        totemLexStatus_OutOfMemory
    }
    totemLexStatus;
    
    const char *totemLexStatus_Describe(totemLexStatus status);
    
	typedef enum totemParseStatus
    {
        totemParseStatus_Success,
        totemParseStatus_UnexpectedToken,
        totemParseStatus_ValueTooLarge,
        totemParseStatus_OutOfMemory,
        totemParseStatus_TooManyItems,
        totemParseStatus_LexError
    }
    totemParseStatus;
    
    const char *totemParseStatus_Describe(totemParseStatus status);
    
    typedef enum totemParseStringFlag
    {
        totemParseStringFlag_None = 0,
        totemParseStringFlag_IsLink = 1,
        totemParseStringFlag_IsEnd = 1 << 1
    }
    totemParseStringFlag;

	typedef enum totemVariablePrototypeFlag
    {
        totemVariablePrototypeFlag_None = 0,
        totemVariablePrototypeFlag_IsConst = 1,
        totemVariablePrototypeFlag_IsDeclaration = 1 << 1
    }
    totemVariablePrototypeFlag;
    
	typedef enum totemArgumentType
    {
        totemArgumentType_Variable = 1,
        totemArgumentType_String,
        totemArgumentType_Number,
        totemArgumentType_NewArray,
        totemArgumentType_NewObject,
        totemArgumentType_Type,
        totemArgumentType_Identifier,
        totemArgumentType_FunctionDeclaration,
        totemArgumentType_CoroutineDeclaration,
        totemArgumentType_Bool,
        totemArgumentType_Null
    }
    totemArgumentType;
    
	typedef enum totemPreUnaryOperatorType
    {
        totemPreUnaryOperatorType_None = 0,
        totemPreUnaryOperatorType_Dec,
        totemPreUnaryOperatorType_Inc,
		totemPreUnaryOperatorType_LengthOf,
        totemPreUnaryOperatorType_Negate,
        totemPreUnaryOperatorType_LogicalNegate,
        totemPreUnaryOperatorType_BinaryNot,
    }
    totemPreUnaryOperatorType;
    
	typedef enum totemPostUnaryOperatorType
    {
        totemPostUnaryOperatorType_None = 0,
        totemPostUnaryOperatorType_Dec,
        totemPostUnaryOperatorType_Inc,
        totemPostUnaryOperatorType_ArrayAccess,
        totemPostUnaryOperatorType_Invocation
    }
    totemPostUnaryOperatorType;
    
	typedef enum totemBinaryOperatorType
    {
        totemBinaryOperatorType_None = 0,
        totemBinaryOperatorType_Plus,
        totemBinaryOperatorType_PlusAssign,
        totemBinaryOperatorType_Minus,
        totemBinaryOperatorType_MinusAssign,
        totemBinaryOperatorType_Multiply,
        totemBinaryOperatorType_MultiplyAssign,
        totemBinaryOperatorType_Divide,
        totemBinaryOperatorType_DivideAssign,
        totemBinaryOperatorType_Assign,
        totemBinaryOperatorType_Equals,
        totemBinaryOperatorType_NotEquals,
        totemBinaryOperatorType_MoreThan,
        totemBinaryOperatorType_LessThan,
        totemBinaryOperatorType_MoreThanEquals,
        totemBinaryOperatorType_LessThanEquals,
        totemBinaryOperatorType_LogicalAnd,
        totemBinaryOperatorType_LogicalOr,
        totemBinaryOperatorType_IsType,
        totemBinaryOperatorType_AsType,
        totemBinaryOperatorType_BinaryLeftShift,
        totemBinaryOperatorType_BinaryRightShift,
        totemBinaryOperatorType_BinaryAnd,
        totemBinaryOperatorType_BinaryOr,
        totemBinaryOperatorType_BinaryXor,
    }
    totemBinaryOperatorType;
    const char *totemBinaryOperatorType_Describe(totemBinaryOperatorType type);
    
	typedef enum totemStatementType
    {
        totemStatementType_Simple = 0,
        totemStatementType_WhileLoop,
        totemStatementType_ForLoop,
        totemStatementType_IfBlock,
        totemStatementType_DoWhileLoop,
        totemStatementType_Loop,
        totemStatementType_Return,
		totemStatementType_Yield,
        totemStatementType_Scope
    }
    totemStatementType;
    
	typedef enum totemBlockType
    {
        totemBlockType_Statement,
        totemBlockType_Export,
        totemBlockType_Import,
        totemBlockType_Function
    }
    totemBlockType;
    
	typedef enum totemLValueType
    {
        totemLValueType_Expression = 1,
        totemLValueType_Argument
    }
    totemLValueType;
    
	typedef enum totemIfElseBlockType
    {
        totemIfElseBlockType_None = 0,
        totemIfElseBlockType_ElseIf,
        totemIfElseBlockType_Else
    }
    totemIfElseBlockType;
    
	typedef enum totemTokenType
    {
		totemTokenType_Identifier,
		totemTokenType_Whitespace,
        totemTokenType_Digits,
        totemTokenType_EndScript,
#define TOTEM_EMIT_RESERVED_WORD_TOKEN(name, word) totemTokenType_ ## name,
        TOTEM_EMIT_RESERVED_WORDS(TOTEM_EMIT_RESERVED_WORD_TOKEN)
#undef TOTEM_EMIT_RESERVED_WORD_TOKEN
#define TOTEM_EMIT_SYMBOL_TOKEN(name, symbol) totemTokenType_ ## name,
		TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_TOKEN)
#undef TOTEM_EMIT_SYMBOL_TOKEN
		totemTokenType_Max,
    }
    totemTokenType;

    const char *totemTokenType_Describe(totemTokenType type);
	
	typedef enum totemTokenCategory
    {
        totemTokenCategory_Symbol,
        totemTokenCategory_ReservedWord,
        totemTokenCategory_Other
    }
	totemTokenCategory;

    totemTokenCategory totemTokenType_ToCategory(totemTokenType type);

	typedef struct totemTokenPositionInfo
    {
        size_t LineNumber;
        size_t CharNumber; // relative to current line
        size_t Length;
        size_t Start; // relative to beginning of source
    }
    totemTokenPositionInfo;

    struct totemParseTree;

    void totemTokenPositionInfo_Print(totemTokenPositionInfo *info, struct totemParseTree *src, totemLog *dst);

	typedef struct totemToken
    {
        totemTokenPositionInfo Position;
        totemTokenType Type;
    }
    totemToken;

	void totemToken_Print(totemLog *target, totemToken *token, struct totemParseTree *tree);
	void totemToken_PrintList(totemLog *target, totemToken *token, size_t num, struct totemParseTree *tree);

    typedef struct totemParseStringHeader
    {
        char Flags;
    }
    totemParseStringHeader;

    totemParseStringHeader *totemParseStringHeader_IntFromBuffer(char *buffer, uint8_t bufferLen, totemInt num);

    typedef struct totemParseString
    {
        totemParseStringHeader *Data;
    }
    totemParseString;

    size_t totemParseString_GetLength(totemParseString* str);
    size_t totemParseString_ExtractValue(totemParseString* str, char* valOut, size_t bufferLen);
    size_t totemParseString_Print(totemParseString *str, totemLog *to);
    totemBool totemParseString_Compare(totemParseString* str1, totemParseString* str2);
    totemBool totemParseString_Equals(totemParseString* str, const char* str2, size_t str2Len);
    totemHashValue totemParseString_Hash(totemParseString* str);

    typedef struct totemParseStringIterator
    {
        totemParseStringHeader *Itr;
        const char *Current;
        totemParseStringFlag CurrentFlags;
        uint8_t CurrentLength;
    }
    totemParseStringIterator;

    void totemParseStringIterator_Begin(totemParseStringIterator *it, totemParseString* str);
    totemBool totemParseStringIterator_Step(totemParseStringIterator *it);

    typedef struct totemNumberPrototype
    {
        totemParseString Value;
        totemBool IsFloat;
    }
    totemNumberPrototype;

    typedef struct totemVariablePrototype
    {
		totemTokenPositionInfo Position;
        totemParseString Identifier;
        struct totemVariablePrototype *Next;
        totemVariablePrototypeFlag Flags;
    }
    totemVariablePrototype;
    
    struct totemExpressionPrototype;
    
	typedef struct totemNewObjectPrototype
    {
        struct totemExpressionPrototype *Keys;
        struct totemExpressionPrototype *Values;
        uint32_t Num;
    }
    totemNewObjectPrototype;
    
	typedef struct totemNewArrayPrototype
    {
        struct totemExpressionPrototype *NumExpression;
        struct totemExpressionPrototype *Accessor;
        size_t NumElements;
    }
    totemNewArrayPrototype;
    
    struct totemStatementPrototype;

    typedef struct totemFunctionDeclarationPrototype
    {
        totemTokenPositionInfo Position;
        totemVariablePrototype* ParametersStart;
        struct totemStatementPrototype* StatementsStart;
        struct totemFunctionDeclarationPrototype* Next;
    }
    totemFunctionDeclarationPrototype;

	typedef union totemArgumentPrototypeValue
	{
        totemParseString String;
        totemParseString Identifier;
		totemVariablePrototype *Variable;
        totemNumberPrototype *Number;
        totemFunctionDeclarationPrototype *Func;
		totemNewArrayPrototype *NewArray;
		totemNewObjectPrototype *NewObject;
		totemPublicDataType DataType;
		totemBool Bool;
	}
	totemArgumentPrototypeValue;
    
    typedef struct totemArgumentPrototype
    {
		totemTokenPositionInfo Position;
		totemArgumentPrototypeValue Value;
        totemArgumentType Type;
    }
    totemArgumentPrototype;
    
    typedef struct totemPreUnaryOperatorPrototype
    {
        struct totemPreUnaryOperatorPrototype *Next;
        totemPreUnaryOperatorType Type;
    }
    totemPreUnaryOperatorPrototype;
    
    typedef struct totemPostUnaryOperatorPrototype
    {
        struct totemPostUnaryOperatorPrototype *Next;
		struct totemExpressionPrototype *Parameters;
        totemPostUnaryOperatorType Type;
    }
    totemPostUnaryOperatorPrototype;

	typedef union totemLValuePrototype
	{
		struct totemExpressionPrototype *Expression;
		totemArgumentPrototype *Argument;
	}
	totemLValuePrototype;
    
    typedef struct totemExpressionPrototype
    {
        totemTokenPositionInfo Position;
		totemLValuePrototype LValue;
        struct totemExpressionPrototype *RValue;
        struct totemExpressionPrototype *Next;
        
        totemPreUnaryOperatorPrototype *PreUnaryOperators;
        totemPostUnaryOperatorPrototype *PostUnaryOperators;
        totemBinaryOperatorType BinaryOperator;
        totemLValueType LValueType;
    }
    totemExpressionPrototype;
    
	typedef struct totemWhileLoopPrototype
    {
		totemTokenPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
    }
    totemWhileLoopPrototype;
    
	typedef struct totemForLoopPrototype
    { 
		totemTokenPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Initialisation;
        totemExpressionPrototype *Condition;
        totemExpressionPrototype *AfterThought;
    }
    totemForLoopPrototype;
    
	typedef struct totemElseBlockPrototype
    {
		totemTokenPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
    }
    totemElseBlockPrototype;
    
	typedef union totemIfBlockPrototypeNextBlockPtr
	{
		struct totemIfBlockPrototype *IfElseBlock;
		totemElseBlockPrototype *ElseBlock;
	}
	totemIfBlockPrototypeNextBlockPtr;

    typedef struct totemIfBlockPrototype
    {
		totemTokenPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
		totemIfBlockPrototypeNextBlockPtr Next;
        totemIfElseBlockType ElseType;
    }
    totemIfBlockPrototype;
     
	typedef struct totemDoWhileLoopPrototype
    {
		totemTokenPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
    }
    totemDoWhileLoopPrototype;

    typedef struct totemScopePrototype
    {
        struct totemStatementPrototype* StatementsStart;
    }
    totemScopePrototype;
    
    struct totemStatementPrototype;

	typedef union totemStatementPrototypePtr
	{
		totemWhileLoopPrototype *WhileLoop;
		totemForLoopPrototype *ForLoop;
		totemIfBlockPrototype *IfBlock;
		totemDoWhileLoopPrototype *DoWhileLoop;
		totemExpressionPrototype *Return;
		totemExpressionPrototype *Simple;
        struct totemStatementPrototype *Set;
	}
	totemStatementPrototypePtr;

    typedef struct totemStatementPrototype
    {
		totemTokenPositionInfo Position;
		totemStatementPrototypePtr Ptr;
        struct totemStatementPrototype *Next;
        totemStatementType Type;
    }
    totemStatementPrototype;

    typedef struct totemImportStatementPrototype
    {
        totemParseString ScriptName;
        totemParseString Alias;
        struct totemImportStatementPrototype* Next;
    }
    totemImportStatementPrototype;

    typedef struct totemImportBlockPrototype
    {
        totemImportStatementPrototype* Stmts;
    }
    totemImportBlockPrototype;

    typedef struct totemExportBlockPrototype
    {
        struct totemBlockPrototype* Blocks;
    }
    totemExportBlockPrototype;

    typedef struct totemFunctionBlockPrototype
    {
        totemParseString Identifier;
        totemFunctionDeclarationPrototype Dec;
    }
    totemFunctionBlockPrototype;
    
	typedef union totemBlockPrototypePtr
	{
		totemStatementPrototype *Statement;
        totemExportBlockPrototype *Export;
        totemImportBlockPrototype *Import;
        totemFunctionBlockPrototype *Function;
	}
	totemBlockPrototypePtr;

    typedef struct totemBlockPrototype
    {
		totemTokenPositionInfo Position;
		totemBlockPrototypePtr Ptr;
        struct totemBlockPrototype *Next;
        totemBlockType Type;
    }
    totemBlockPrototype;
    
    //typedef size_t(*totemTokenizerReaderCb)(char *buffer, size_t bufferLen, void*);

/*
	void totemTokenizer_Step(totemTokenizer *list, totemTokenizerReaderCb reader, void* readerCxt);
    
*/

    typedef struct totemParseMemBlock
    {
        char Data[512];
        size_t Remaining;
        struct totemParseMemBlock* Next;
    }
    totemParseMemBlock;

    typedef struct totemParseGlobalValue
    {
        totemArgumentPrototype *Arg;
        struct totemParseGlobalValue *Next;
    }
    totemParseGlobalValue;

    typedef size_t(*totemParseTree_StepCallback)(void *cxtIn, char *buffer, size_t len, size_t pos);
    typedef void(*totemParseTree_ResetCallback)(void* cxtIn);
    typedef void(*totemParseTree_PrintCallback)(void *cxtIn, totemLog *f, size_t len, size_t pos);

    typedef struct totemParseTreeClass
    {
        totemParseTree_StepCallback Step;
        totemParseTree_ResetCallback Reset;
        totemParseTree_PrintCallback Print;
    }
    totemParseTreeClass;

	typedef struct totemParseTree
	{
        totemToken CurrentToken;
        totemToken LastToken;
        size_t CurrentLineChar;
        size_t CurrentLine;
        size_t NumLineBreaksInCurrentToken;
        totemBool InsideStringLiteral;
        size_t CurrentPos;
        char ReadBuffer[TOTEM_MAX_TOKEN_LENGTH];
        
		totemTokenPositionInfo ErrorAt;
		//totemTokenizer Tokenizer;
		totemJmpBuffer Jmp;
		totemParseMemBlock *LastMemBlock;
        totemParseMemBlock *MemBlockFreeList;
		totemBlockPrototype *FirstBlock;
        totemParseStringHeader *StringParseHead;
        totemParseStringHeader *StringParseTail;
        totemParseGlobalValue *Globals;
        const struct totemParseTreeClass *Callbacks;
        void *SrcCxt;
        size_t SrcLen;
        size_t SrcPos;
        totemParseStatus JmpStatus;
	}
	totemParseTree;

	void totemParseTree_InitWithString(totemParseTree *tree, const char *str, totemStringLength len);
    void totemParseTree_InitWithFile(totemParseTree *tree, FILE *file);
	void totemParseTree_Reset(totemParseTree *tree);
	void totemParseTree_Cleanup(totemParseTree *tree);
    char totemParseTree_GetAbs(totemParseTree* tree, size_t pos);
    void totemParseTree_InitialRead(totemParseTree *tee);
    void totemParseTree_LexToken(totemParseTree *tree);
	totemParseStatus totemParseTree_Parse(totemParseTree *tree);
    
    totemToken* totemParseTree_Current(totemParseTree* tree);
    void totemParseTree_Break(totemParseTree *tree, totemParseStatus status, totemTokenPositionInfo *location);
    void totemParseTree_AllocMemBlock(totemParseTree* tree);

    void totemParseTree_StringParseBegin(totemParseTree* tree);
    void totemParseTree_StringParseEnd(totemParseTree* tree, totemParseString* result);
    void totemParseTree_StringParseStep(totemParseTree *tree);

#ifdef __cplusplus
}
#endif

#endif
