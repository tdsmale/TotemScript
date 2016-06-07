//
//  parse_load.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <string.h>

typedef struct totemScriptName
{
    const char *Filename;
    struct totemScriptName *Next;
}
totemScriptName;

#define TOTEM_LOADSCRIPT_SKIPWHITESPACE(str) while(str[0] == ' ' || str[0] == '\n' || str[0] == '\t' || str[0] == '\r') str++;

totemBool totemScriptName_Push(totemScriptName **tree, const char *name)
{
    totemScriptName *treeEntry = totem_CacheMalloc(sizeof(totemScriptName));
    if (treeEntry == NULL)
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
    while (treeEntry != NULL)
    {
        if (strcmp(treeEntry->Filename, filename) == 0)
        {
            return totemBool_True;
        }
        
        treeEntry = treeEntry->Next;
    }
    
    return totemBool_False;
}

const char *totemScriptName_Pop(totemScriptName **tree)
{
    if (*tree)
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
    if (!file)
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
    if (totemScriptName_Push(tree, srcPath) == totemBool_False)
    {
        err->Status = totemLoadScriptStatus_OutOfMemory;
        fclose(file);
        return totemBool_False;
    }
    
    // look for include statements
    while (totemBool_True)
    {
        long resetPos = ftell(file);
        char cha = fgetc(file);
        
        while ((cha == ' ' || cha == '\n' || cha == '\t' || cha == '\r') && cha != EOF)
        {
            resetPos = ftell(file);
            cha = fgetc(file);
        }
        
        fseek(file, resetPos, SEEK_SET);
        
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
        
        char *filename = totem_CacheMalloc(filenameSize + 1);
        if (filename == NULL)
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
        if (totemScriptName_Search(tree, filename) == totemBool_True)
        {
            continue;
        }
        
        totemScriptName *restore = *tree;
        
        totemBool result = totemScriptFile_LoadRecursive(dst, filename, err, tree);
        totem_CacheFree(filename, filenameSize + 1);
        if (!result)
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
    if (!buffer)
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
    
    if (result == totemBool_True)
    {
        size_t bufferSize = totemMemoryBuffer_GetNumObjects(&dst->Buffer);
        if (bufferSize > 0)
        {
            if (!totemMemoryBuffer_Secure(&dst->Buffer, 1))
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

const char *totemLoadScriptStatus_Describe(totemLoadScriptStatus status)
{
    switch (status)
    {
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_OutOfMemory);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_FileNotFound);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_Recursion);
            TOTEM_STRINGIFY_CASE(totemLoadScriptStatus_Success);
    }
    
    return "UNKNOWN";
}