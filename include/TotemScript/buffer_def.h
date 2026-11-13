//
//  buffer_def.h
//  TotemScript
//
//  Created by Timothy Smale on 26/09/2022
//  Copyright (c) 2022 Timothy Smale. All rights reserved.
//

#include <TotemScript/memory.h>

#ifndef TOTEM_DYNBUFFER_TYPE
#define TOTEM_DYNBUFFER_TYPE totemChar
TOTEM_STATIC_ASSERT(0, "Missing buffer type declaration");
#endif

#ifndef TOTEM_DYNBUFFER_REALLOC_FUNC
#define TOTEM_DYNBUFFER_REALLOC_FUNC TOTEM_REALLOC
TOTEM_STATIC_ASSERT(0, "Missing buffer realloc declaration");
#endif

#ifndef TOTEM_DYNBUFFER_FREE_FUNC
#define TOTEM_DYNBUFFER_FREE_FUNC TOTEM_FREE
TOTEM_STATIC_ASSERT(0, "Missing buffer free declaration");
#endif

#define TOTEM_DYNBUFFER(type) TOTEM_DYNBUFFER_NAME(type)

void TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Init) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer)
{
	buffer->Data = NULL;
	buffer->Length = 0;
	buffer->MaxLength = 0;
}

void TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Reset) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer)
{
	buffer->Length = 0;
}

TOTEM_DYNBUFFER_TYPE* TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Top) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer)
{
	if (buffer->Length)
	{
		return &buffer->Data[buffer->Length - 1];
	}
	
	return 0;
}

TOTEM_DYNBUFFER_TYPE* TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Bottom) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer)
{
	if (buffer->Length)
	{
		return buffer->Data;
	}

	return 0;
}

TOTEM_DYNBUFFER_TYPE *TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Push) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer, const TOTEM_DYNBUFFER_TYPE* elem, size_t num)
{
	void* ptr = TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Secure)(buffer, num);
	if (!ptr)
	{
		return NULL;
	}

	buffer->Length += num;
	memcpy(ptr, elem, sizeof(TOTEM_DYNBUFFER_TYPE) * num);
	return (TOTEM_DYNBUFFER_TYPE*)ptr;
}

totemBool TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Pop) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer, size_t num)
{
	if (buffer->Length > num)
	{
		buffer->Length -= num;
		return totemBool_True;
	}

	return totemBool_False;
}

TOTEM_DYNBUFFER_TYPE* TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, At) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer, size_t index)
{
	if (buffer->Length > index)
	{
		return buffer->Data + index;
	}

	return NULL;
}

TOTEM_DYNBUFFER_TYPE* TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Secure) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer, size_t amount)
{
	size_t needed = buffer->Length + amount;
	if (needed >= buffer->MaxLength)
	{
		size_t toAlloc = 0;
		if (buffer->MaxLength == 0)
		{
			toAlloc = 8;
		}
		else
		{
			toAlloc = buffer->MaxLength * 2;
		}
		if (needed > toAlloc)
		{
			toAlloc = needed;
		}
		TOTEM_DYNBUFFER_TYPE* newMem = (TOTEM_DYNBUFFER_TYPE*)TOTEM_DYNBUFFER_REALLOC_FUNC(buffer->Data, buffer->MaxLength * sizeof(TOTEM_DYNBUFFER_TYPE), toAlloc * sizeof(TOTEM_DYNBUFFER_TYPE));
		if (newMem == NULL)
		{
			return NULL;
		}
		buffer->MaxLength = toAlloc;
		buffer->Data = newMem;
	}
	
	return buffer->Data + buffer->Length;
}

totemBool TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Reserve) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer, size_t amount)
{
	if (buffer->MaxLength < amount)
	{
		if (TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Secure)(buffer, amount - buffer->MaxLength) == NULL)
		{
			return totemBool_False;
		}
	}

	return totemBool_True;
}

TOTEM_INLINE void TOTEM_DYNBUFFER_METHOD_NAME(TOTEM_DYNBUFFER_TYPE, Cleanup) (TOTEM_DYNBUFFER(TOTEM_DYNBUFFER_TYPE)* buffer)
{
	if (buffer->Data)
	{
		TOTEM_DYNBUFFER_FREE_FUNC(buffer->Data, buffer->MaxLength * sizeof(TOTEM_DYNBUFFER_TYPE));
		buffer->Data = NULL;
		buffer->Length = 0;
		buffer->MaxLength = 0;
	}
}

#undef TOTEM_DYNBUFFER
