//
//  log.c
//  TotemScript
//
//  Created by Timothy Smale on 2026/10/16.
//  Copyright © 2026 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/log.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef struct totemVaList
{
    va_list list;
}
totemVaList;

typedef struct totemLogDefaultLambdaContext
{
    const char *Format;
    totemVaList *Args;
}
totemLogDefaultLambdaContext;

typedef struct totemLogOutput
{
    totemLock Lock;
    FILE *File;
    totemLogContext *Context;
}
totemLogOutput;

static totemLogOutput s_LogStdOut;
static totemLock s_LogConfigLock;
static TOTEM_BITARRAY(s_logLevels, totemLogLevel_Max);
static TOTEM_BITARRAY(s_logCategories, totemLogCategory_Max);
static TOTEM_BITARRAY(s_logParentCategories, totemLogCategory_Max);

void totem_InitLogging()
{
    s_LogStdOut.File = stdout;
    s_LogStdOut.Context = NULL;
    totemLock_Init(&s_LogStdOut.Lock);
    totemLock_Init(&s_LogConfigLock);
    
    TOTEM_ZERO(s_logLevels);
    TOTEM_ZERO(s_logCategories);
    TOTEM_ZERO(s_logParentCategories);
    
    // set default log levels
#define EMIT_LEVEL(a, b, c) \
    { \
        if(b) \
        { \
            totemBitArray_Set(s_logLevels, totemLogLevel_ ## a, b); \
        } \
    }
TOTEM_EMIT_LOGGING_LEVELS(EMIT_LEVEL)
#undef EMIT_LEVEL
    
    // set default log categories
#define EMIT_CAT(a, b, c, d, e) \
    { \
        totemBitArray_Set(s_logCategories, totemLogCategory_ ## a ## _ ## b ## _ ## c, d && e); \
    }
TOTEM_EMIT_LOGGING_CATEGORIES(EMIT_CAT)
#undef EMIT_CAT
    
    // set default parent categories
#define EMIT_PARENT(a) \
    { \
        totemBitArray_Set(s_logParentCategories, totemLogParentCategory_ ## a, 1); \
    }
TOTEM_EMIT_LOGGING_PARENT_CATEGORIES(EMIT_PARENT)
#undef EMIT_PARENT
}

void totem_CleanupLogging()
{
    totemLock_Cleanup(&s_LogStdOut.Lock);
}

void totem_ToggleLogLevel(totemLogLevel level, totemBool activated)
{
    if(level < totemLogLevel_Max)
    {
        totemLock_Acquire(&s_LogConfigLock);
        totemBitArray_Set(s_logLevels, level, activated);
        totemLock_Release(&s_LogConfigLock);
    }
}

void totem_ToggleLogCategory(totemLogCategory category, totemBool activated)
{
    if(category < totemLogCategory_Max)
    {
        totemLock_Acquire(&s_LogConfigLock);
        totemBitArray_Set(s_logCategories, category, activated);
        totemLock_Release(&s_LogConfigLock);
    }
}

void totem_ToggleLogParentCategory(totemLogParentCategory category, totemBool activated)
{
    if(category < totemLogParentCategory_Max)
    {
        totemLock_Acquire(&s_LogConfigLock);
        totemBitArray_Set(s_logParentCategories, category, activated);
        totemLock_Release(&s_LogConfigLock);
    }
}

totemBool totemLog_IsActive(totemLogParentCategory parent, totemLogCategory category, totemLogLevel level)
{
    if (parent < totemLogParentCategory_Max && category < totemLogCategory_Max && level < totemLogLevel_Max)
    {
        totemBool result = totemBool_True;
        result &= totemBitArray_Get(s_logParentCategories, parent);
        result &= totemBitArray_Get(s_logCategories, category);
        result &= totemBitArray_Get(s_logLevels, level);
        return result;
    }

    return totemBool_False;
}

static size_t totemLogVfprintf(FILE *f, const char *format, totemVaList *args)
{
    int written = vfprintf(f, format, args->list);
    if(written < 0)
    {
        fprintf(stderr, "Log write error %i (written %i)\n", errno, written);
        return 0;
    }
    
    return (size_t)written;
}

size_t totemLog_Write(totemLog *dst, const char *format, ...)
{
    size_t written = 0;
    totemVaList args;
    va_start(args.list, format);
    written = totemLogVfprintf(dst->File, format, &args);
    va_end(args.list);
    return written;
}

size_t totemLog_WriteLine(totemLog* dst, const char* format, ...)
{
    size_t written = 0;
    totemVaList args;
    va_start(args.list, format);
    written = totemLogVfprintf(dst->File, format, &args);
    va_end(args.list);

    fprintf(dst->File, "\n");

    return written;
}

static void totemLogContextStart(totemLog *f, totemLogContext *cxt)
{
    totemLog_Write(f, "%s\n%s(%i)\n", cxt->Category, cxt->File, cxt->Line);
}

static void totemLogContextFinish(totemLog *f, totemLogContext *cxt)
{
    totemLog_Write(f, "\n\n");
}

totemLog *totemLogContext_Begin(totemLogContext *cxt, const char *category, const char *file, int line)
{
    totemLog *dst = &s_LogStdOut;
    totemLock_Acquire(&dst->Lock);
    
    totemLogContext *prevContext = dst->Context;
    if(prevContext)
    {
        // just interrupt the previous context for now
        totemLogContextFinish(dst, prevContext);
    }
    
    // setup new context
    cxt->Line = line;
    cxt->File = file;
    cxt->Category = category;
    cxt->PrevContext = prevContext;
    cxt->Log = dst;
    dst->Context = cxt;
    totemLogContextStart(dst, cxt);
    return dst;
}

void totemLogContext_End(totemLogContext *cxt)
{
    totemLog *dst = cxt->Log;
    
    totemLogContextFinish(dst, cxt);
    
    if(cxt->PrevContext)
    {
        // restore previous context
        dst->Context = cxt->PrevContext;
        totemLogContextStart(dst, dst->Context);
    }
    else
    {
        dst->Context = NULL;
    }
    
    totemLock_Release(&dst->Lock);
}
