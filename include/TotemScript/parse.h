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

#ifdef __cplusplus
extern "C" {
#endif

#define TOTEM_EMIT_SYMBOLS(_) \
	_(DoubleQuote,	'"') \
	_(Plus,			'+') \
	_(Minus,		'-') \
	_(Multiply,		'*') \
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
	_(PowerTo,		'^') \
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

	typedef enum totemLoadScriptStatus
    {
        totemLoadScriptStatus_Success,
        totemLoadScriptStatus_FileNotFound,
        totemLoadScriptStatus_OutOfMemory,
        totemLoadScriptStatus_CwdTooLarge,
        totemLoadScriptStatus_FChDirError,
        totemLoadScriptStatus_FilenameTooLarge
    }
    totemLoadScriptStatus;
    
    const char *totemLoadScriptStatus_Describe(totemLoadScriptStatus status);
     
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
        totemParseStatus_TooManyItems
    }
    totemParseStatus;
    
    const char *totemParseStatus_Describe(totemParseStatus status);
    
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
        totemArgumentType_Boolean,
        totemArgumentType_Null
    }
    totemArgumentType;
    
	typedef enum totemPreUnaryOperatorType
    {
        totemPreUnaryOperatorType_None = 0,
        totemPreUnaryOperatorType_Dec,
        totemPreUnaryOperatorType_Inc,
		totemPreUnaryOperatorType_LengthOf,
		totemPreUnaryOperatorType_CloneOf,
        totemPreUnaryOperatorType_Negate,
        totemPreUnaryOperatorType_LogicalNegate
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
        totemBinaryOperatorType_LeftShift,
		totemBinaryOperatorType_RightShift,
    }
    totemBinaryOperatorType;
    const char *totemBinaryOperatorType_Describe(totemBinaryOperatorType type);
    
	typedef enum totemStatementType
    {
        totemStatementType_WhileLoop = 1,
        totemStatementType_ForLoop,
        totemStatementType_IfBlock,
        totemStatementType_DoWhileLoop,
        totemStatementType_Return,
		totemStatementType_Yield,
        totemStatementType_Simple
    }
    totemStatementType;
    
	typedef enum totemBlockType
    {
        totemBlockType_FunctionDeclaration = 1,
        totemBlockType_Statement
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
		totemTokenType_None = 0,
		totemTokenType_Identifier,
		totemTokenType_Whitespace,
		totemTokenType_Null,
		totemTokenType_Var,
		totemTokenType_If,
		totemTokenType_Do,
		totemTokenType_While,
		totemTokenType_For,
		totemTokenType_Return,
		totemTokenType_Case,
		totemTokenType_Function,
		totemTokenType_EndScript,
		totemTokenType_Number,
		totemTokenType_Break,
		totemTokenType_Default,
		totemTokenType_Else,
		totemTokenType_True,
		totemTokenType_False,
		totemTokenType_Boolean,
		totemTokenType_Let,
		totemTokenType_Is,
		totemTokenType_Int,
		totemTokenType_Float,
		totemTokenType_Array,
		totemTokenType_String,
		totemTokenType_Type,
		totemTokenType_Object,
		totemTokenType_As,
		totemTokenType_Coroutine,
		totemTokenType_Userdata,
		totemTokenType_Include,
		totemTokenType_Import,
		totemTokenType_Yield,
#define TOTEM_EMIT_SYMBOL_TOKEN(name, symbol) totemTokenType_ ## name,
		TOTEM_EMIT_SYMBOLS(TOTEM_EMIT_SYMBOL_TOKEN)
		totemTokenType_Max,
    }
    totemTokenType;

    const char *totemTokenType_Describe(totemTokenType type);
	
	typedef enum totemTokenCategory
    {
        totemTokenCategory_None = 0,
        totemTokenCategory_Symbol,
        totemTokenCategory_ReservedWord,
        totemTokenCategory_Other
    }
	totemTokenCategory;

	typedef struct totemBufferPositionInfo
    {
        size_t LineNumber;
        size_t CharNumber;
        size_t Length;
        const char *Start;
    }
    totemBufferPositionInfo;
    
	typedef struct totemToken
    {
        totemBufferPositionInfo Position;
        totemTokenType Type;
        totemTokenCategory Category;
    }
    totemToken;

	void totemToken_Print(FILE *target, totemToken *token);
	void totemToken_PrintList(FILE *target, totemToken *token, size_t num);

    typedef struct totemVariablePrototype
    {
		totemBufferPositionInfo Position;
        totemString Identifier;
        struct totemVariablePrototype *Next;
        totemVariablePrototypeFlag Flags;
    }
    totemVariablePrototype;
    
    struct totemExpressionPrototype;
    
	typedef struct totemNewObjectPrototype
    {
        struct totemExpressionPrototype *Keys;
        struct totemExpressionPrototype *Values;
        totemInt Num;
    }
    totemNewObjectPrototype;
    
	typedef struct totemNewArrayPrototype
    {
		totemInt Num;
        struct totemExpressionPrototype *Accessor;
    }
    totemNewArrayPrototype;
    
    struct totemFunctionDeclarationPrototype;

	typedef union totemArgumentPrototypeValue
	{
		totemVariablePrototype *Variable;
		totemString *String;
		totemString *Number;
		totemString *Identifier;
		struct totemFunctionDeclarationPrototype *FunctionDeclaration;
		totemNewArrayPrototype *NewArray;
		totemNewObjectPrototype *NewObject;
		totemPublicDataType DataType;
		totemBool Boolean;
	}
	totemArgumentPrototypeValue;
    
    typedef struct totemArgumentPrototype
    {
		totemBufferPositionInfo Position;
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
        totemBufferPositionInfo Position;
		totemLValuePrototype LValue;
        struct totemExpressionPrototype *RValue;
        struct totemExpressionPrototype *Next;
        
        totemPreUnaryOperatorPrototype *PreUnaryOperators;
        totemPostUnaryOperatorPrototype *PostUnaryOperators;
        totemBinaryOperatorType BinaryOperator;
        totemLValueType LValueType;
    }
    totemExpressionPrototype;
    
    struct totemStatementPrototype;
    
	typedef struct totemWhileLoopPrototype
    {
		totemBufferPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
    }
    totemWhileLoopPrototype;
    
	typedef struct totemForLoopPrototype
    { 
		totemBufferPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Initialisation;
        totemExpressionPrototype *Condition;
        totemExpressionPrototype *AfterThought;
    }
    totemForLoopPrototype;
    
	typedef struct totemElseBlockPrototype
    {
		totemBufferPositionInfo Position;
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
		totemBufferPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
		totemIfBlockPrototypeNextBlockPtr Next;
        totemIfElseBlockType ElseType;
    }
    totemIfBlockPrototype;
     
	typedef struct totemDoWhileLoopPrototype
    {
		totemBufferPositionInfo Position;
        struct totemStatementPrototype *StatementsStart;
        totemExpressionPrototype *Expression;
    }
    totemDoWhileLoopPrototype;
    
    typedef struct totemFunctionDeclarationPrototype
    {
		totemBufferPositionInfo Position;
        totemVariablePrototype *ParametersStart;
        struct totemStatementPrototype *StatementsStart;
        totemString *Identifier;
        struct totemFunctionDeclarationPrototype *Next;
    }
    totemFunctionDeclarationPrototype;
    
	typedef union totemStatementPrototypePtr
	{
		totemWhileLoopPrototype *WhileLoop;
		totemForLoopPrototype *ForLoop;
		totemIfBlockPrototype *IfBlock;
		totemDoWhileLoopPrototype *DoWhileLoop;
		totemExpressionPrototype *Return;
		totemExpressionPrototype *Simple;
	}
	totemStatementPrototypePtr;

    typedef struct totemStatementPrototype
    {
		totemBufferPositionInfo Position;
		totemStatementPrototypePtr Ptr;
        struct totemStatementPrototype *Next;
        totemStatementType Type;
    }
    totemStatementPrototype;
    
	typedef union totemBlockPrototypePtr
	{
		totemFunctionDeclarationPrototype *FuncDec;
		totemStatementPrototype *Statement;
	}
	totemBlockPrototypePtr;

    typedef struct totemBlockPrototype
    {
		totemBufferPositionInfo Position;
		totemBlockPrototypePtr Ptr;
        struct totemBlockPrototype *Next;
        totemBlockType Type;
    }
    totemBlockPrototype;
    
	typedef struct totemScriptFileBlock
    {
        char *Value;
        size_t Length;
        size_t Offset;
        size_t Parent;
        size_t LineStart;
    }
    totemScriptFileBlock;

	TOTEM_CACHEBUFFER_DEFINITION(totemScriptFileBlock);
    
	typedef struct totemScriptFile
    {
        totemCharBuffer Buffer;
		totemScriptFileBlockBuffer FileBlock;
        char FilenameBuffer[PATH_MAX];
    }
	totemScriptFile;

	void totemScriptFile_Init(totemScriptFile *file);
	void totemScriptFile_Reset(totemScriptFile *file);
	void totemScriptFile_Cleanup(totemScriptFile *file);
	totemLoadScriptStatus totemScriptFile_Load(totemScriptFile *dst, totemString *filename);

	typedef struct totemTokenizer
	{
		totemToken CurrentToken;
		totemToken LastToken;
		size_t CurrentChar;
		size_t CurrentLine;
		const char *Current;
		const char *CEnd;
		totemBool InsideStringLiteral;
	}
	totemTokenizer;

	void totemTokenizer_Init(totemTokenizer *list);
	void totemTokenizer_Reset(totemTokenizer *list);
	void totemTokenizer_Cleanup(totemTokenizer *list);
	void totemTokenizer_Begin(totemTokenizer *list, const char *buffer, size_t length);
	void totemTokenizer_Step(totemTokenizer *list);
	void totemTokenizer_Rewind(totemTokenizer *list);

	typedef struct totemParseTree
	{
		totemBufferPositionInfo ErrorAt;
		totemTokenizer Tokenizer;
		totemJmpBuffer Jmp;
		totemMemoryBlock *LastMemBlock;
		totemBlockPrototype *FirstBlock;
		totemParseStatus JmpStatus;
	}
	totemParseTree;

	void totemParseTree_Init(totemParseTree *tree);
	void totemParseTree_Reset(totemParseTree *tree);
	void totemParseTree_Cleanup(totemParseTree *tree);
	totemParseStatus totemParseTree_Parse(totemParseTree *tree, const char *start, size_t len);
    
#ifdef __cplusplus
}
#endif

#endif