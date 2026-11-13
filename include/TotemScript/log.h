//
//  log.h
//  TotemScript
//
//  Created by Timothy Smale on 2026/10/11.
//  Copyright (c) 2026 Timothy Smale. All rights reserved.
//

#ifndef TOTEM_SCRIPT_LOG_H
#define TOTEM_SCRIPT_LOG_H

#include <TotemScript/base.h>
#include <stdarg.h>

// log level, is compiled, is active by default
#define TOTEM_EMIT_LOGGING_LEVELS(_) \
    _(Debug, TOTEM_IS_DEBUG, 1) \
    _(Output, 1, 1) \
    _(Warning, 1, 1) \
    _(Error, 1, 1)

#define TOTEM_EMIT_LOGGING_PARENT_CATEGORIES(_) \
    _(Parse) \
    _(Eval) \
    _(Exec) \
    _(Log) \
    _(Cmd) \
    _(Memory)

// parent category, category name, log level, is compiled, is active by default
#define TOTEM_EMIT_LOGGING_CATEGORIES(_) \
    _(Log, Assert, Error, TOTEM_IS_DEBUG, 1) \
    _(Log, Generic, Debug, 1, 1) \
    _(Memory, Free, Debug, 1, 1) \
    _(Parse, File, Error, 1, 1) \
    _(Parse, Lex, Debug, 1, 1) \
    _(Parse, String, Error, 1, 1) \
    _(Parse, String, Debug, 1, 1) \
    _(Parse, Memory, Debug, 1, 1) \
    _(Eval, Arith, Debug, 1, 1) \
    _(Eval, Expression, Debug, 1, 1) \
    _(Eval, Identifier, Debug, 1, 1) \
    _(Eval, Register, Debug, 1, 1) \
    _(Exec, VM, Debug, 1, 1) \
    _(Exec, GC, Debug, 1, 1) \
    _(Exec, Lib, Output, 1, 1) \
    _(Exec, Lib, Error, 1, 1) \
    _(Exec, Lib, Debug, 1, 1) \
    _(Exec, File, Error, 1, 1) \
    _(Exec, String, Debug, 1, 1) \
    _(Exec, Runtime, Error, 1, 1) \
    _(Cmd, Result, Error, 1, 1) \
    _(Cmd, Result, Output, 1, 1)

typedef enum totemLogLevel
{
#define EMIT_LEVEL(a, b, c) totemLogLevel_ ## a,
    TOTEM_EMIT_LOGGING_LEVELS(EMIT_LEVEL)
    totemLogLevel_Max
#undef EMIT_LEVEL
}
totemLogLevel;

typedef enum totemLogParentCategory
{
#define EMIT_PARENT(a) totemLogParentCategory_ ## a,
    TOTEM_EMIT_LOGGING_PARENT_CATEGORIES(EMIT_PARENT)
#undef EMIT_PARENT
    totemLogParentCategory_Max
}
totemLogParentCategory;

typedef enum totemLogCategory
{
#define EMIT_CAT(a, b, c, d, e) totemLogCategory_ ## a ## _ ## b ## _ ## c,
    TOTEM_EMIT_LOGGING_CATEGORIES(EMIT_CAT)
    totemLogCategory_Max
#undef EMIT_CAT
}
totemLogCategory;

typedef struct totemLogContext
{
    totemLog *Log;
    struct totemLogContext *PrevContext;
    const char *Category;
    const char *File;
    int Line;
}
totemLogContext;

void totem_InitLogging(void);
void totem_CleanupLogging(void);

void totem_ToggleLogLevel(totemLogLevel level, totemBool activated);
void totem_ToggleLogParentCategory(totemLogParentCategory category, totemBool activated);
void totem_ToggleLogCategory(totemLogCategory category, totemBool activated);
totemBool totemLog_IsActive(totemLogParentCategory parent, totemLogCategory category, totemLogLevel level);

typedef void(*totemLogCallback)(totemLog *output, void *cxt);
totemLog *totemLogContext_Begin(totemLogContext *cxt, const char *category, const char *file, int line);
void totemLogContext_End(totemLogContext *cxt);
size_t totemLog_Write(totemLog *dst, const char *format, ...);

#define TOTEM_LOG_LEVEL_IS_COMPILED(a) totemLogLevel_ ## a ## _IsCompiled
#define TOTEM_LOG_CATEGORY_IS_COMPILED(a, b, c) totemLogCategory_ ## a ## _ ## b ## _ ## c ## _IsCompiled
#define TOTEM_LOG_CATEGORY_STRING_NAME(a, b, c) totemLog_CategoryString_ ## a ## _ ## b ## _ ## c
#define TOTEM_LOG_IS_ACTIVE(a, b, c) totemLog_IsActive(totemLogParentCategory_ ## a, totemLogCategory_ ## a ## _ ## b ## _ ## c, totemLogLevel_ ## c)
#define TOTEM_LOG_IS_COMPILED_AND_ACTIVE(a, b, c) TOTEM_LOG_LEVEL_IS_COMPILED(c) && TOTEM_LOG_CATEGORY_IS_COMPILED(a, b, c) && TOTEM_LOG_IS_ACTIVE(a, b, c)

// emit logging constants
#define EMIT_LEVEL(a, b, c) \
    static const totemBool TOTEM_LOG_LEVEL_IS_COMPILED(a) = b;
TOTEM_EMIT_LOGGING_LEVELS(EMIT_LEVEL)
#undef EMIT_LEVEL

#define EMIT_CAT(a, b, c, d, e) \
    static const totemBool TOTEM_LOG_CATEGORY_IS_COMPILED(a, b, c) = d; \
    static const char *TOTEM_LOG_CATEGORY_STRING_NAME(a, b, c) = #a "." #b "." #c ;
TOTEM_EMIT_LOGGING_CATEGORIES(EMIT_CAT)
#undef EMIT_CAT

#define TOTEM_LOG_CONTEXT_BEGIN(a, b, c, logger) \
    totemLogContext logContext; \
    const char* category = TOTEM_LOG_CATEGORY_STRING_NAME(a, b, c); \
    totemLog* logger = totemLogContext_Begin(&logContext, category, __FILE__, __LINE__); \
    if (logger) \
    {

#define TOTEM_LOG_CONTEXT_END() \
    }

#define TOTEM_LOG_BEGIN(a, b, c, logger) \
    if(TOTEM_LOG_IS_COMPILED_AND_ACTIVE(a, b, c)) \
    { \
        TOTEM_LOG_CONTEXT_BEGIN(a, b, c, logger); \

#define TOTEM_LOG_END() \
        TOTEM_LOG_CONTEXT_END(); \
    } \

#define TOTEM_LOG_DO(a, b, c, logger, stmt) \
    TOTEM_LOG_BEGIN(a, b, c, logger) \
        stmt; \
    TOTEM_LOG_END()

#define TOTEM_ASSERT_BEGIN(check, logger) \
    if(TOTEM_LOG_IS_COMPILED_AND_ACTIVE(Log, Assert, Error)) \
    { \
        if(!(check)) \
        { \
            TOTEM_LOG_CONTEXT_BEGIN(Log, Assert, Error, logger); \

#define TOTEM_ASSERT_END() \
            TOTEM_LOG_CONTEXT_END(); \
            totem_Fatal(); \
        } \
    }

#define TOTEM_ASSERT_DO(check, logger, stmt) \
    TOTEM_ASSERT_BEGIN(check, logger) \
        stmt; \
    TOTEM_ASSERT_END()

#define TOTEM_LOG(a, b, c, string) TOTEM_LOG_DO(a, b, c, logger, totemLog_Write(logger, string))
#define TOTEM_LOG_V(a, b, c, string, ...) TOTEM_LOG_DO(a, b, c, logger, totemLog_Write(logger, string, __VA_ARGS__))

#define TOTEM_ASSERT_F(check, format) TOTEM_ASSERT_DO(check, logger, totemLog_Write(logger, format))
#define TOTEM_ASSERT_V(check, format, ...) TOTEM_ASSERT_DO(check, logger, totemLog_Write(logger, format, __VA_ARGS__))
#define TOTEM_ASSERT(check) TOTEM_ASSERT_V(check, "assert failed: \"%s\"", #check)

#define TOTEM_LOG_GENERIC_DO(logger, stmt) TOTEM_LOG_DO(Log, Generic, Debug, logger, stmt)
#define TOTEM_LOG_GENERIC(str) TOTEM_LOG(Log, Generic, Debug, str)
#define TOTEM_LOG_GENERIC_V(str, ...) TOTEM_LOG_V(Log, Generic, Debug, str, __VA_ARGS__)

#endif /* TOTEM_SCRIPT_LOG_H */