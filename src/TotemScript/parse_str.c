//
//  parse_str.c
//  TotemScript
//
//  Created by Timothy Smale on 29/05/2021
//  Copyright (c) 2021 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <string.h>

// parse strings are built as a linked list of headers, each header representing a segment of the actual string value
// to iterate over a parse string, we iterate over these headers
// this is done because memory allocation within a parse tree is done over a linked list of uniform memory blocks
// to jump from one memory block to another, a pointer is encoded directly after the last header's value
// laid out in memory, each parse string "segment" is:
// header | string value | optional ptr link

// todo - do we even need to copy the value, or can we just reference the original source? could be overkill...
// todo - works really well for small strings, but not for large ones...

#define TOTEM_PARSE_SEGMENTLEN_WIDTH 6
#define TOTEM_PARSE_STR_MAXSEGMENTLEN TOTEM_MAXVAL_UNSIGNED(uint8_t, TOTEM_PARSE_SEGMENTLEN_WIDTH)
#define TOTEM_PARSE_SEGMENTLEN_MASK TOTEM_BITMASK(uint8_t, 2, TOTEM_PARSE_SEGMENTLEN_WIDTH)
#define TOTEM_PARSE_SEGMENTLEN_CLAMP TOTEM_BITMASK(uint8_t, 0, TOTEM_PARSE_SEGMENTLEN_WIDTH)

TOTEM_STATIC_ASSERT(sizeof(totemParseStringHeader) == 1, "assuming the header is 1 byte");

static char *totemParseStringHeader_GetValue(totemParseStringHeader *hdr) {
    // get value for this segment
    char *data = (char*)hdr;
    return data + sizeof(*hdr);
}

static uint8_t totemParseStringHeader_GetLength(totemParseStringHeader *hdr) {
    // get length for this segment
    return TOTEM_GETBITS_OFFSET(hdr->Flags, TOTEM_PARSE_SEGMENTLEN_MASK, 2);
}

static void totemParseStringHeader_SetLength(totemParseStringHeader *hdr, uint8_t length) {
    // set length for this segment
    length &= TOTEM_PARSE_SEGMENTLEN_CLAMP;
    TOTEM_UNSETBITS(hdr->Flags, TOTEM_PARSE_SEGMENTLEN_MASK);
    TOTEM_SETBITS_OFFSET(hdr->Flags, length, 2);
}

totemParseStringHeader *totemParseStringHeader_IntFromBuffer(char *buffer, uint8_t bufferLen, totemInt num)
{
    if(bufferLen <= sizeof(totemParseStringHeader))
    {
        return NULL;
    }
    
    totemParseStringHeader *hdr = (totemParseStringHeader*)buffer;
    buffer += sizeof(totemParseStringHeader);
    bufferLen -= sizeof(totemParseStringHeader);
    
    int result = totem_snprintf(buffer, bufferLen, "%"TOTEM_INT_PRINTF, num);

    if (result < 0 || result >= bufferLen)
    {
        return NULL;
    }

    totemParseStringHeader_SetLength(hdr, result);
    TOTEM_SETBITS(hdr->Flags, totemParseStringFlag_IsEnd);
    return hdr;
}

static void totemParseStringIterator_Read(totemParseStringIterator* it)
{
	it->CurrentFlags = it->Itr->Flags;
	it->CurrentLength = totemParseStringHeader_GetLength(it->Itr);
	it->Current = ((char*)it->Itr) + 1;
}

void totemParseStringIterator_Begin(totemParseStringIterator* it, totemParseString* str)
{
    it->Itr = str->Data;
	totemParseStringIterator_Read(it);
}

totemBool totemParseStringIterator_Step(totemParseStringIterator* it)
{
	if (TOTEM_GETBITS(it->CurrentFlags, totemParseStringFlag_IsEnd))
	{
		return totemBool_False;
	}

    // skip header & segment value to reach next header in-memory
    it->Itr = (totemParseStringHeader*)(totemParseStringHeader_GetValue(it->Itr) + it->CurrentLength);

	if (TOTEM_GETBITS(it->CurrentFlags, totemParseStringFlag_IsLink))
	{
        // extract ptr to next header (which is sitting in next memory block)
		uintptr_t link = 0;
		memcpy(&link, it->Itr, sizeof(uintptr_t));
		it->Itr = (totemParseStringHeader*)link;
	}

	totemParseStringIterator_Read(it);
	return totemBool_True;
}

size_t totemParseString_GetLength(totemParseString* str)
{
    totemParseStringIterator itr;
    totemParseStringIterator_Begin(&itr, str);
    
    size_t len = 0;
    do
    {
        len += itr.CurrentLength;
    }
    while(totemParseStringIterator_Step(&itr));
    
    return len;
}

static totemHashValue totemParseString_HashStep(totemParseStringIterator *it, totemHashValue hash)
{
    return totem_HashStep(it->Current, it->CurrentLength, hash);
}

totemHashValue totemParseString_Hash(totemParseString* str)
{
    totemParseStringIterator itr;
    totemParseStringIterator_Begin(&itr, str);
    
    totemHashValue hash = totem_HashBegin();
    
    do
    {
        hash = totemParseString_HashStep(&itr, hash);
    }
    while(totemParseStringIterator_Step(&itr));
    
    return hash;
}

size_t totemParseString_ExtractValue(totemParseString* str, char* valOut, size_t valOutLen)
{
	size_t len = totemParseString_GetLength(str);
    
    if (len > valOutLen)
    {
        return 0;
    }
	
	totemParseStringIterator it;
	totemParseStringIterator_Begin(&it, str);

	do 
	{
		memcpy(valOut, it.Current, it.CurrentLength);
		valOut += it.CurrentLength;
	} 	
	while (totemParseStringIterator_Step(&it));

	valOut[0] = 0;
    return len;
}

totemBool totemParseString_Equals(totemParseString* str, const char* str2, size_t str2Len)
{
	totemParseStringIterator it;
	totemParseStringIterator_Begin(&it, str);
	size_t it2 = 0;

	do
	{
		if (strncmp(it.Current, str2 + it2, it.CurrentLength) != 0)
		{
			return totemBool_False;
		}

		it2 += it.CurrentLength;
	} 
	while (totemParseStringIterator_Step(&it));

	return it2 == str2Len;
}

totemBool totemParseString_Compare(totemParseString* str1, totemParseString* str2)
{
	size_t len1 = totemParseString_GetLength(str1);
	size_t len2 = totemParseString_GetLength(str2);

	if (!len1 || len1 != len2)
	{
		return totemBool_False;
	}

	totemParseStringIterator it1, it2;
	size_t pos1 = 0, pos2 = 0, read = 0;
	
	totemParseStringIterator_Begin(&it1, str1);
	totemParseStringIterator_Begin(&it2, str2);
	
	while(1)
	{
		if (it1.Current[pos1] != it2.Current[pos2])
		{
			return totemBool_False;
		}

		read++;
        
        if(read >= len1)
        {
            return totemBool_True;
        }

		if (++pos1 >= it1.CurrentLength)
		{
			if(!totemParseStringIterator_Step(&it1))
            {
                return totemBool_False;
            }
            
			pos1 = 0;
		}

		if (++pos2 >= it2.CurrentLength)
		{
            if(!totemParseStringIterator_Step(&it2))
            {
                return totemBool_False;
            }
            
			pos2 = 0;
		}
	}
}

size_t totemParseString_Print(totemParseString *str, totemLog *to)
{
    totemParseStringIterator itr;
    totemParseStringIterator_Begin(&itr, str);
    size_t res = 0;
    
    totemHashValue hash = totem_HashBegin();
    
    totemLog_Write(to, "\"");
    
    do
    {
        res += totemLog_Write(to, "%.*s", itr.CurrentLength, itr.Current);
        hash = totemParseString_HashStep(&itr, hash);
    }
    while(totemParseStringIterator_Step(&itr));
    
    totemLog_Write(to, "\" (hash: %"TOTEM_HASH_PRINTF")", hash);
    
    return res;
}

static void totemParseTree_StringParseAlloc(totemParseTree* tree)
{
    TOTEM_LOG_DO(Parse, String, Debug, logger, totemLog_Write(logger, "StringParseAlloc"));
    
    // allocate a new parse string header
    TOTEM_ASSERT(tree->LastMemBlock->Remaining > sizeof(totemParseStringHeader));
	tree->StringParseTail = (totemParseStringHeader*)(tree->LastMemBlock->Data + sizeof(tree->LastMemBlock->Data) - tree->LastMemBlock->Remaining);
	tree->LastMemBlock->Remaining -= sizeof(totemParseStringHeader);
    tree->StringParseTail->Flags = totemParseStringFlag_None;
}

void totemParseTree_StringParseBegin(totemParseTree* tree)
{
	TOTEM_LOG_DO(Parse, String, Debug, logger, totemLog_Write(logger, "StringParseBegin"));
    
	if (tree->StringParseHead)
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}

	if (!tree->LastMemBlock || tree->LastMemBlock->Remaining < sizeof(totemParseStringHeader) + 1 + sizeof(uintptr_t))
	{
		// ensure at least space for header, link ptr and 1 byte of data
		totemParseTree_AllocMemBlock(tree);
	}

	totemParseTree_StringParseAlloc(tree);
	tree->StringParseHead = tree->StringParseTail;
}

void totemParseTree_StringParseEnd(totemParseTree* tree, totemParseString* result)
{
	TOTEM_LOG_DO(Parse, String, Debug, logger, totemLog_Write(logger, "StringParseEnd"));
    
	if (tree->StringParseTail)
	{
		TOTEM_SETBITS(tree->StringParseTail->Flags, totemParseStringFlag_IsEnd);

		result->Data = tree->StringParseHead;
		tree->StringParseHead = NULL;
		tree->StringParseTail = NULL;
        
		TOTEM_LOG_BEGIN(Parse, String, Debug, logger);
			totemLog_Write(logger, "New parse string: ");
			totemParseString_Print(result, logger);
		TOTEM_LOG_END();
	}
	else
	{
		totemParseTree_Break(tree, totemParseStatus_UnexpectedToken, &totemParseTree_Current(tree)->Position);
	}
}

void totemParseTree_StringParseStep(totemParseTree *tree)
{
    // copy source of this token into curent segment, creating a new one if neccessary
    
	size_t tokenCopyFrom = totemParseTree_Current(tree)->Position.Start;
	size_t tokenLenRemaining = totemParseTree_Current(tree)->Position.Length;
	while (tokenLenRemaining)
	{
		uint8_t currentSegmentLen = totemParseStringHeader_GetLength(tree->StringParseTail);
		TOTEM_ASSERT(tree->LastMemBlock->Remaining >= sizeof(uintptr_t));

		if (tree->LastMemBlock->Remaining == sizeof(uintptr_t))
		{
            // need to move to next memory block
			TOTEM_SETBITS(tree->StringParseTail->Flags, totemParseStringFlag_IsLink);
			totemParseStringHeader* prevTail = tree->StringParseTail;

			totemParseTree_AllocMemBlock(tree);
			totemParseTree_StringParseAlloc(tree);

            // copy ptr link onto end of last segment (after value)
            void *copyTo = totemParseStringHeader_GetValue(prevTail) + currentSegmentLen;
            uintptr_t toCopy = (uintptr_t)tree->StringParseTail;
			memcpy(copyTo, &toCopy, sizeof(toCopy));
            currentSegmentLen = 0;
		}
		else if (currentSegmentLen == TOTEM_PARSE_STR_MAXSEGMENTLEN)
		{
            // start a new segment here
			totemParseTree_StringParseAlloc(tree);
            currentSegmentLen = 0;
		}

		uint8_t toCopy = (uint8_t)tokenLenRemaining;
		uint8_t lenRemaining = TOTEM_PARSE_STR_MAXSEGMENTLEN - currentSegmentLen;

		if (toCopy > lenRemaining)
		{
			toCopy = lenRemaining;
		}

		size_t maxWritable = tree->LastMemBlock->Remaining - sizeof(uintptr_t);
		if (toCopy > maxWritable)
		{
			toCopy = (uint8_t)maxWritable;
		}

		char *data = totemParseStringHeader_GetValue(tree->StringParseTail) + currentSegmentLen;

		for (size_t i = 0; i < toCopy; i++)
		{
			data[i] = totemParseTree_GetAbs(tree, tokenCopyFrom);
            tokenCopyFrom++;
		}

		tokenLenRemaining -= toCopy;
        tree->LastMemBlock->Remaining -= toCopy;
		totemParseStringHeader_SetLength(tree->StringParseTail, currentSegmentLen + toCopy);
	}
}
