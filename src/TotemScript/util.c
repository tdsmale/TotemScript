//
//  util.c
//  TotemScript
//
//  Created by Timothy Smale on 22/12/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/base.h>
#include <TotemScript/parse.h>
#include <string.h>

static totemMallocCb mallocCb = NULL;
static totemFreeCb freeCb = NULL;
static totemHashCb hashCb = NULL;

void totem_SetGlobalCallbacks(totemMallocCb newMallocCb, totemFreeCb newFreeCb, totemHashCb newHashCb)
{
    mallocCb = newMallocCb;
    freeCb = newFreeCb;
    hashCb = newHashCb;
}

void *totem_Malloc(size_t len)
{
    if(mallocCb)
    {
        return mallocCb(len);
    }
    
    return malloc(len);
}

void totem_Free(void *mem)
{
    if(freeCb)
    {
        return freeCb(mem);
    }
    
    return free(mem);
}

uint32_t totem_Hash(const char *data, size_t len)
{
    if(hashCb)
    {
        return hashCb(data, len);
    }
    
    uint32_t hash = 5831;
    for(uint32_t i = 0; i < len; ++i)
    {
        hash = 33 * hash + data[i];
    }
    
    return hash;
}

void totemString_FromLiteral(totemString *strOut, const char *str)
{
    strOut->Value = str;
    strOut->Length = (uint32_t)strlen(str);
}

void *totemMemoryBlock_Alloc(totemMemoryBlock **blockHead, size_t objectSize)
{
    size_t allocSize = objectSize;
    size_t extra = allocSize % sizeof(uintptr_t);
    if(extra != 0)
    {
        allocSize += sizeof(uintptr_t) - extra;
    }
    
    totemMemoryBlock *chosenBlock = NULL;
    
    for(totemMemoryBlock *block = *blockHead; block != NULL; block = block->Prev)
    {
        if(block->Remaining > allocSize)
        {
            chosenBlock = block;
            break;
        }
    }
    
    if(chosenBlock == NULL)
    {
        // alloc new
        chosenBlock = (totemMemoryBlock*)totem_Malloc(sizeof(totemMemoryBlock));
        if(chosenBlock == NULL)
        {
            return NULL;
        }
        chosenBlock->Remaining = TOTEM_MEMORYBLOCK_DATASIZE;
        chosenBlock->Prev = *blockHead;
        *blockHead = chosenBlock;
    }
    
    void *ptr = chosenBlock->Data + (TOTEM_MEMORYBLOCK_DATASIZE - chosenBlock->Remaining);
    memset(ptr, 0, objectSize);
    chosenBlock->Remaining -= allocSize;
    return ptr;
}

void totemMemoryBuffer_Reset(totemMemoryBuffer *buffer, size_t objectSize)
{
    buffer->Length = 0;
    buffer->ObjectSize = objectSize;
}

void totemMemoryBuffer_Cleanup(totemMemoryBuffer *buffer)
{
    if(buffer->Data)
    {
        totem_Free(buffer->Data);
        buffer->Data = NULL;
        buffer->Length = 0;
        buffer->MaxLength = 0;
        buffer->ObjectSize = 0;
    }
}

totemBool totemMemoryBuffer_Secure(totemMemoryBuffer *buffer, size_t amount)
{
    char *memEnd = buffer->Data + buffer->MaxLength;
    char *memCurrent = buffer->Data + buffer->Length;
    
    amount *= buffer->ObjectSize;
    
    if(memCurrent + amount > memEnd)
    {
        size_t toAlloc = buffer->MaxLength * 1.5;
        if(amount > toAlloc)
        {
            toAlloc += amount;
        }
        char *newMem = totem_Malloc(toAlloc);
        if(newMem == NULL)
        {
            return totemBool_False;
        }
        
        memcpy(newMem, buffer->Data, buffer->MaxLength);
        memset(newMem + buffer->MaxLength, 0, toAlloc - buffer->MaxLength);
        totem_Free(buffer->Data);
        buffer->MaxLength = toAlloc;
        buffer->Data = newMem;
    }
    
    buffer->Length += amount;
    return totemBool_True;
}

void *totemMemoryBuffer_Get(totemMemoryBuffer *buffer, size_t index)
{
    index *= buffer->ObjectSize;
    if(index < buffer->MaxLength)
    {
        return (void*)&buffer->Data[index];
    }

    return NULL;
}

size_t totemMemoryBuffer_GetNumObjects(totemMemoryBuffer *buffer)
{
    return buffer->Length / buffer->ObjectSize;
}

size_t totemMemoryBuffer_GetMaxObjects(totemMemoryBuffer *buffer)
{
    return buffer->MaxLength / buffer->ObjectSize;
}

void totemHashMap_InsertDirect(totemHashMapEntry **buckets, size_t numBuckets, totemHashMapEntry *entry)
{
    size_t index = entry->Hash % numBuckets;
    if(buckets[index] == NULL)
    {
        buckets[index] = entry;
    }
    else
    {
        for(totemHashMapEntry *bucket = buckets[index]; bucket != NULL; bucket = bucket->Next)
        {
            if(bucket->Next == NULL)
            {
                bucket->Next = entry;
                break;
            }
        }
    }
}

totemBool totemHashMap_Insert(totemHashMap *hashmap, const char *key, size_t keyLen, size_t value)
{
    totemHashMapEntry *existingEntry = totemHashMap_Find(hashmap, key, keyLen);
    if(existingEntry)
    {
        existingEntry->Value = value;
        return totemBool_True;
    }
    else
    {
        if(hashmap->NumKeys >= hashmap->NumBuckets)
        {
            // buckets realloc
            size_t newNumBuckets = hashmap->NumKeys * 1.5;
            totemHashMapEntry **newBuckets = totem_Malloc(sizeof(totemHashMapEntry**) * newNumBuckets);
            if(!newBuckets)
            {
                return totemBool_False;
            }
            
            // reassign
            memset(newBuckets, 0, newNumBuckets * sizeof(totemHashMapEntry**));
            for(size_t i = 0; i < hashmap->NumBuckets; i++)
            {
                totemHashMapEntry *next = NULL;
                for(totemHashMapEntry *entry = hashmap->Buckets[i]; entry != NULL; entry = next)
                {
                    next = entry->Next;
                    entry->Next = NULL;
                    totemHashMap_InsertDirect(newBuckets, newNumBuckets, entry);
                }
            }
            
            // replace buckets
            totem_Free(hashmap->Buckets);
            hashmap->Buckets = newBuckets;
            hashmap->NumBuckets = newNumBuckets;
        }
        
        // hashmap insert
        totemHashMapEntry *entry = NULL;
        if (hashmap->FreeList)
        {
            entry = hashmap->FreeList;
            hashmap->FreeList = entry->Next;
        }
        else
        {
            entry = totem_Malloc(sizeof(totemHashMapEntry));
            if(entry == NULL)
            {
                return totemBool_False;
            }
        }
        
        entry->Next = NULL;
        entry->Value = value;
        entry->Key = key;
        entry->KeyLen = keyLen;
        entry->Hash = totem_Hash(key, keyLen);
        totemHashMap_InsertDirect(hashmap->Buckets, hashmap->NumBuckets, entry);
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemHashMapEntry *totemHashMap_Find(totemHashMap *hashmap, const char *key, size_t keyLen)
{
    uint32_t hash = totem_Hash((char*)key, keyLen);
    int index = hash % hashmap->NumBuckets;
    
    for(totemHashMapEntry *entry = hashmap->Buckets[index]; entry != NULL; entry = entry->Next)
    {
        if(strncmp(entry->Key, key, entry->KeyLen))
        {
            return entry;
        }
    }
    
    return NULL;
}

void totemHashMap_MoveKeysToFreeList(totemHashMap *hashmap)
{
    for(size_t i = 0; i < hashmap->NumKeys; i++)
    {
        while(hashmap->Buckets[i])
        {
            totemHashMapEntry *entry = hashmap->Buckets[i];
            hashmap->Buckets[i] = entry->Next;
            entry->Next = hashmap->FreeList;
            hashmap->FreeList = entry;
        }
    }
}

void totemHashMap_Reset(totemHashMap *hashmap)
{
    totemHashMap_MoveKeysToFreeList(hashmap);
    hashmap->NumKeys = 0;
}

void totemHashMap_Cleanup(totemHashMap *map)
{
    totemHashMap_MoveKeysToFreeList(map);
    while(map->FreeList)
    {
        totemHashMapEntry *entry = map->FreeList;
        map->FreeList = entry->Next;
        totem_Free(entry);
    }
    
    totem_Free(map->Buckets);
}

const char *totemOperation_GetName(totemOperation op)
{
    switch(op)
    {
        case totemOperation_Add:
            return "ADD";
            break;
            
        case totemOperation_ConditionalGoto:
            return "CONDITIONALGOTO";
            break;
            
        case totemOperation_Divide:
            return "DIVIDE";
            break;
            
        case totemOperation_Equals:
            return "EQUALS";
            break;
            
        case totemOperation_FunctionArg:
            return "FUNCTIONARG";
            break;
            
        case totemOperation_Goto:
            return "GOTO";
            break;
            
        case totemOperation_LessThan:
            return "LESSTHAN";
            break;
            
        case totemOperation_LessThanEquals:
            return "LESSTHANEQUALS";
            break;
            
        case totemOperation_LogicalAnd:
            return "LOGICALAND";
            break;
            
        case totemOperation_LogicalOr:
            return "LOGICALOR";
            break;
            
        case totemOperation_MoreThan:
            return "MORETHAN";
            break;
            
        case totemOperation_MoreThanEquals:
            return "MORETHANEQUALS";
            break;
            
        case totemOperation_Move:
            return "MOVE";
            break;
            
        case totemOperation_Multiply:
            return "MULTIPLY";
            break;
            
        case totemOperation_NativeFunction:
            return "NATIVEFUNCTION";
            break;
            
        case totemOperation_None:
            return "NONE";
            break;
            
        case totemOperation_NotEquals:
            return "NOTEQUALS";
            break;
            
        case totemOperation_Power:
            return "POWER";
            break;
            
        case totemOperation_Return:
            return "RETURN";
            break;
            
        case totemOperation_ScriptFunction:
            return "SCRIPTFUNCTION";
            break;
            
        case totemOperation_Subtract:
            return "SUBTRACT";
            break;
            
        default:
            return "UNKNOWN";
            break;
    }
}


const char *totemTokenType_GetName(totemTokenType token)
{
    switch(token) {
        case totemTokenType_Switch:
            return "SWITCH";
            break;
            
        case totemTokenType_And:
            return "AND";
            break;
            
        case totemTokenType_Assign:
            return "ASSIGN";
            break;
            
        case totemTokenType_Divide:
            return "DIVIDE";
            break;
            
        case totemTokenType_Do:
            return "DO";
            break;
            
        case totemTokenType_Dot:
            return "DOT";
            break;
            
        case totemTokenType_Semicolon:
            return "SEMICOLON";
            break;
            
        case totemTokenType_For:
            return "FOR";
            break;
            
        case totemTokenType_Identifier:
            return "IDENTIFIER";
            break;
            
        case totemTokenType_If:
            return "IF";
            break;
            
        case totemTokenType_LBracket:
            return "LBRACKET";
            break;
            
        case totemTokenType_LCBracket:
            return "LCBRACKET";
            break;
            
        case totemTokenType_LessThan:
            return "LESSTHAN";
            break;
            
        case totemTokenType_Minus:
            return "MINUS";
            break;
            
        case totemTokenType_MoreThan:
            return "MORETHAN";
            break;
            
        case totemTokenType_Multiply:
            return "MULTIPLY";
            break;
            
        case totemTokenType_Not:
            return "NOT";
            break;
            
        case totemTokenType_Or:
            return "OR";
            break;
            
        case totemTokenType_Plus:
            return "PLUS";
            break;
            
        case totemTokenType_PowerTo:
            return "POWERTO";
            break;
            
        case totemTokenType_RBracket:
            return "RBRACKET";
            break;
            
        case totemTokenType_RCBracket:
            return "RCBRACKET";
            break;
            
        case totemTokenType_Return:
            return "RETURN";
            break;
            
        case totemTokenType_Variable:
            return "VAR";
            break;
            
        case totemTokenType_While:
            return "WHILE";
            break;
            
        case totemTokenType_Whitespace:
            return "WHITESPACE";
            break;
            
        case totemTokenType_SingleQuote:
            return "SQUOTE";
            break;
            
        case totemTokenType_DoubleQuote:
            return "DQUOTE";
            break;
            
        case totemTokenType_LSBracket:
            return "LSBRACKET";
            break;
            
        case totemTokenType_RSBracket:
            return "RSBRACKET";
            break;
            
        case totemTokenType_Function:
            return "FUNCTION";
            break;
            
        case totemTokenType_Break:
            return "BREAK";
            break;
            
        case totemTokenType_Comma:
            return "COMMA";
            break;
            
        case totemTokenType_Colon:
            return "COLON";
            break;
            
        case totemTokenType_EndScript:
            return "ENDSCRIPT";
            break;
            
        case totemTokenType_Number:
            return "NUMBER";
            break;
            
        case totemTokenType_Case:
            return "CASE";
            break;
            
        case totemTokenType_Default:
            return "DEFAULT";
            break;
            
        case totemTokenType_Else:
            return "ELSE";
            break;
            
        case totemTokenType_True:
            return "TRUE";
        	break;
        	
        case totemTokenType_False:
            return "FALSE";
        	break;
            
        case totemTokenType_Backslash:
            return "BACKSLASH";
            break;
            
        case totemTokenType_Slash:
            return "FORWARDSLASH";
            break;
        	
        default:
            return "UNKNOWN";
            break;
    }
}

const char *totemDataType_GetName(totemDataType type)
{
    switch(type)
    {
        case totemDataType_Number:
            return "NUMBER (64-Bit DOUBLE)";
            
        case totemDataType_Reference:
            return "REFERENCE";
            
        case totemDataType_String:
            return "STRING";
            
        default:
            return "UNKNOWN";
    }
}

void totemInstruction_PrintList(FILE *file, totemInstruction *instructions, size_t num)
{
    for(size_t i = 0; i < num; ++i)
    {
        totemInstruction_Print(file, instructions[i]);
    }
}

void totemInstruction_Print(FILE *file, totemInstruction instruction)
{
    switch(instruction.Abc.Operation)
    {
        case totemOperation_Add:
        case totemOperation_Divide:
        case totemOperation_Equals:
        case totemOperation_LessThan:
        case totemOperation_LessThanEquals:
        case totemOperation_LogicalAnd:
        case totemOperation_LogicalOr:
        case totemOperation_MoreThan:
        case totemOperation_MoreThanEquals:
        case totemOperation_Multiply:
        case totemOperation_Power:
        case totemOperation_Subtract:
        case totemOperation_Move:
        case totemOperation_NotEquals:
            totemInstruction_PrintAbcInstruction(file, instruction);
            break;
            
        case totemOperation_ConditionalGoto:
        case totemOperation_FunctionArg:
        case totemOperation_NativeFunction:
        case totemOperation_ScriptFunction:
            totemInstruction_PrintAbxInstruction(file, instruction);
            break;
            
        case totemOperation_Goto:
        case totemOperation_Return:
            totemInstruction_PrintAxxInstruction(file, instruction);
            break;
            
        default:
            fprintf(file, "Unrecognised operation %i\n", instruction.Abc.Operation);
            break;
    }
}

void totemInstruction_PrintAbcInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%s %s%d %s%d %s%d",
            totemOperation_GetName(instruction.Abc.Operation),
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abc.OperandAType),
            instruction.Abc.OperandAIndex,
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abc.OperandBType),
            instruction.Abc.OperandBIndex,
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abc.OperandCType),
            instruction.Abc.OperandCIndex);
}

void totemInstruction_PrintAbxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%s %s%d %d",
            totemOperation_GetName(instruction.Abx.Operation),
            totemRegisterScopeType_GetOperandTypeCode(instruction.Abx.OperandAType),
            instruction.Abx.OperandAIndex,
            instruction.Abx.OperandBx);
}

void totemInstruction_PrintAxxInstruction(FILE *file, totemInstruction instruction)
{
    fprintf(file, "%s %d",
            totemOperation_GetName(instruction.Axx.Operation),
            instruction.Axx.OperandAxx);
}

const char *totemRegisterScopeType_GetOperandTypeCode(totemRegisterScopeType type)
{
    switch(type)
    {
        case totemRegisterScopeType_Global:
            return "g";
            
        case totemRegisterScopeType_Local:
            return "l";
            
        default:
            return "x";
    }
}

void totemToken_PrintList(FILE *target, totemToken *tokens, size_t num)
{
    for(size_t i = 0 ; i < num; ++i)
    {
        totemToken *token = tokens + i;
        fprintf(target, "%zu %s %.*s", i, totemTokenType_GetName(token->Type), (int)token->Value.Length, token->Value.Value);
    }
}