//
//  parse_load.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <string.h>

#define TOTEM_LOADSCRIPT_SKIPWHITESPACE(str) while(str[0] == ' ' || str[0] == '\n' || str[0] == '\t' || str[0] == '\r') str++;

void totemScriptFile_Init(totemScriptFile *script)
{
    totemMemoryBuffer_Init(&script->Buffer, 1);
    totemMemoryBuffer_Init(&script->FileBlock, sizeof(totemScriptFileBlock));
}

void totemScriptFile_PopOffset(totemScriptFile *file)
{
    totemScriptFileBlock *off = totemMemoryBuffer_Top(&file->FileBlock);
    totem_CacheFree(off->Value, off->Length);
    totemMemoryBuffer_Pop(&file->FileBlock, 1);
}

void totemScriptFile_CleanupOffsets(totemScriptFile *file)
{
    size_t max = totemMemoryBuffer_GetNumObjects(&file->FileBlock);
    for (size_t i = 0; i < max; i++)
    {
        totemScriptFile_PopOffset(file);
    }
}

void totemScriptFile_Reset(totemScriptFile *script)
{
    totemScriptFile_CleanupOffsets(script);
    totemMemoryBuffer_Reset(&script->Buffer);
    totemMemoryBuffer_Reset(&script->FileBlock);
}

void totemScriptFile_Cleanup(totemScriptFile *script)
{
    totemScriptFile_CleanupOffsets(script);
    totemMemoryBuffer_Cleanup(&script->Buffer);
    totemMemoryBuffer_Cleanup(&script->FileBlock);
}

totemScriptFileBlock *totemScriptFile_AddOffset(totemScriptFile *dst, const char *filename, size_t filenameLen, size_t parent)
{
    totemScriptFileBlock *offset = totemMemoryBuffer_Secure(&dst->FileBlock, 1);
    if (!offset)
    {
        return NULL;
    }
    
    offset->Value = totem_CacheMalloc(filenameLen + 1);
    if (!offset->Value)
    {
        totemMemoryBuffer_Pop(&dst->FileBlock, 1);
        return NULL;
    }
    
    memcpy(offset->Value, filename, filenameLen);
    offset->Value[filenameLen] = 0;
    offset->Length = filenameLen;
    offset->Parent = parent;
    offset->LineStart = 0;
    return offset;
}

totemLoadScriptStatus totemScriptFile_LoadRecursive(totemScriptFile *dst)
{
    size_t blockIndex = totemMemoryBuffer_GetNumObjects(&dst->FileBlock) - 1;
    totemScriptFileBlock *block = totemMemoryBuffer_Get(&dst->FileBlock, blockIndex);
    
    FILE *file = totem_fopen(block->Value, "rb");
    if (!file)
    {
        return totemLoadScriptStatus_FileNotFound;
    }
    
    fseek(file, 0, SEEK_END);
    long fSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (!totem_fchdir(file))
    {
        fclose(file);
        return totemLoadScriptStatus_FChDirError;
    }
    
    // look for include statements
    while (1)
    {
        long resetPos = 0;
        char cha = 0;
        
        do
        {
            resetPos = ftell(file);
            cha = fgetc(file);
        }
        while ((cha == ' ' || cha == '\n' || cha == '\t' || cha == '\r') && cha != EOF);
        
        if (cha != '#')
        {
            fseek(file, resetPos, SEEK_SET);
            break;
        }
        
        // any include statements?
        char include[8];
        include[7] = 0;
        size_t numRead = fread(include, 1, 7, file);
        if (numRead != 7)
        {
            fseek(file, resetPos, SEEK_SET);
            break;
        }
        
        if (strncmp("include", include, 7) != 0)
        {
            fseek(file, resetPos, SEEK_SET);
            break;
        }
        
        resetPos = ftell(file);
        for (cha = fgetc(file); cha == ' ' && cha != EOF; /* nada */)
        {
            resetPos = ftell(file);
            cha = fgetc(file);
        }
        
        fseek(file, resetPos, SEEK_SET);
        
        int filenameSize = 0;
        for (cha = fgetc(file); cha != ';' && cha != EOF; /* nada */)
        {
            if (filenameSize + 1 < 0)
            {
                break;
            }
            
            filenameSize++;
            cha = fgetc(file);
        }
        
        if (filenameSize >= TOTEM_ARRAY_SIZE(dst->FilenameBuffer))
        {
            fclose(file);
            return totemLoadScriptStatus_FilenameTooLarge;
        }
        
        fseek(file, resetPos, SEEK_SET);
        fread(dst->FilenameBuffer, 1, filenameSize, file);
        dst->FilenameBuffer[filenameSize] = 0;
        fseek(file, 2, SEEK_CUR);
        
        totemBool loadFile = totemBool_True;
        size_t maxFiles = totemMemoryBuffer_GetNumObjects(&dst->FileBlock);
        for (size_t i = 0; i < maxFiles; i++)
        {
            totemScriptFileBlock *off = totemMemoryBuffer_Get(&dst->FileBlock, i);
            if (strncmp(off->Value, dst->FilenameBuffer, off->Length) == 0)
            {
                loadFile = totemBool_False;
                break;
            }
        }
        
        // already in tree? skip this 'un
        if (!loadFile)
        {
            continue;
        }
        
        // add to tree
        totemScriptFileBlock *newOffset = totemScriptFile_AddOffset(dst, dst->FilenameBuffer, filenameSize + 1, blockIndex);
        if (!newOffset)
        {
            fclose(file);
            return totemLoadScriptStatus_OutOfMemory;
        }
        
        totemLoadScriptStatus status = totemScriptFile_LoadRecursive(dst);
        if (status != totemLoadScriptStatus_Success)
        {
            fclose(file);
            return status;
        }
        
        if (!totem_fchdir(file))
        {
            fclose(file);
            return totemLoadScriptStatus_FChDirError;
        }
    }
    
    block = totemMemoryBuffer_Get(&dst->FileBlock, blockIndex);
    
    // num lines start
    if (totemMemoryBuffer_GetNumObjects(&dst->FileBlock) > 1)
    {
        totemScriptFileBlock *lastBlock = block - 1;
        block->LineStart = lastBlock->LineStart;
        
        const char *toCheck = totemMemoryBuffer_Get(&dst->Buffer, block->Offset);
        const char *end = totemMemoryBuffer_Top(&dst->Buffer);
        
        while (toCheck != end)
        {
            if (toCheck[0] == '\n')
            {
                block->LineStart++;
            }
            
            toCheck++;
        }
    }
    else
    {
        block->LineStart = 0;
    }
    
    size_t existingLength = dst->Buffer.Length;
    char *buffer = totemMemoryBuffer_Secure(&dst->Buffer, fSize + 1);
    if (!buffer)
    {
        fclose(file);
        return totemLoadScriptStatus_OutOfMemory;
    }
    
    // set file offset
    block->Offset = existingLength;
    
    size_t read = fread(buffer, 1, fSize, file);
    buffer[read] = 0;
    dst->Buffer.Length = existingLength + read;
    
    fclose(file);
    return totemLoadScriptStatus_Success;
}

totemLoadScriptStatus totemScriptFile_Load(totemScriptFile *dst, totemString *srcPath)
{
    totemScriptFile_Reset(dst);
    
    char cwd[PATH_MAX];
    if (!totem_getcwd(cwd, TOTEM_ARRAY_SIZE(cwd)))
    {
        return totemLoadScriptStatus_CwdTooLarge;
    }
    
    totemScriptFileBlock *newOffset = totemScriptFile_AddOffset(dst, srcPath->Value, srcPath->Length, 0);
    if (!newOffset)
    {
        return totemLoadScriptStatus_OutOfMemory;
    }
    
    totemLoadScriptStatus result = totemScriptFile_LoadRecursive(dst);
    totem_chdir(cwd);
    
    /*
     FILE *file = fopen("load.txt", "w");
     fprintf(file, "%.*s", dst->Buffer.Length, dst->Buffer.Data);
     fclose(file);
     */
    
    return result;
}

const char *totemLoadScriptStatus_Describe(totemLoadScriptStatus status)
{
    switch (status)
    {
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_FChDirError);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_CwdTooLarge);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_FileNotFound);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_FilenameTooLarge);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_Success);
    }
    
    return "UNKNOWN";
}