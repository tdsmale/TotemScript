//
//  exec_str.c
//  TotemScript
//
//  Created by Timothy Smale on 2025/10/28.
//  Copyright © 2025 Timothy Smale. All rights reserved.
//

#include <stdio.h>
#include <TotemScript/exec.h>

static size_t totemInternedStringHeader_Size(size_t len)
{
    return len + sizeof(totemInternedStringHeader) - TOTEM_SIZEOF_STATIC(totemInternedStringHeader, Data);
}

static totemInternedStringHeader *totemInternedStringHeader_Alloc(size_t len)
{
    size_t toAllocate = totemInternedStringHeader_Size(len);
    return (totemInternedStringHeader*)TOTEM_MALLOC(toAllocate);
}

void totemInternedStringHeader_Free(totemInternedStringHeader *str)
{
    TOTEM_FREE(str, totemInternedStringHeader_Size(str->Length));
}

totemLinkStatus totemRuntime_InternParseString(totemRuntime *runtime, totemParseString *str, totemRuntimeStringValue *valOut)
{
    // todo - combine both intern string functions into one
    
    size_t strLen = totemParseString_GetLength(str);
    
    if(strLen <= TOTEM_MINISTRING_MAXLENGTH)
    {
        char buffer[sizeof(valOut->Src.MiniString)];
        valOut->Type = totemPrivateDataType_MiniString;
        totemParseString_ExtractValue(str, buffer, sizeof(buffer));
        totemRuntimeStringValue_SetMiniString(buffer, (uint8_t)strLen, valOut);
        
        return totemLinkStatus_Success;
    }
    
    totemLinkStatus status = totemLinkStatus_Success;
    totemLock_Acquire(&runtime->InternedStringsLock);
    
    totemRuntimeStringSetParseKey key;
    key.Key = str;
    if(totemRuntimeStringSet_GetParse(&runtime->InternedStrings, &key))
    {
        *valOut = *key.Value;
    }
    else
    {
        totemInternedStringHeader *newStr = totemInternedStringHeader_Alloc(strLen);
        if (!newStr)
        {
            status = totemLinkStatus_OutOfMemory;
        }
        else
        {
            newStr->Length = strLen;

            newStr->Hash = totemParseString_Hash(str);
            totemParseString_ExtractValue(str, newStr->Data, strLen);

            totemRuntimeStringValue_FromInternedString(newStr, valOut);

            if (!totemRuntimeStringSet_InsertNoSearch(&runtime->InternedStrings, valOut))
            {
                totemInternedStringHeader_Free(newStr);
                status = totemLinkStatus_OutOfMemory;
            }
            else
            {
                valOut->Type = totemPrivateDataType_InternedString;
                valOut->Src.InternedString = newStr;
            }
        }
    }
    
    totemLock_Release(&runtime->InternedStringsLock);
    
    if (status != totemLinkStatus_Success)
    {
        return totemLinkStatus_Break(status);
    }
    
    return status;
}

totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRuntimeStringValue *valOut)
{
    if (str->Length <= TOTEM_MINISTRING_MAXLENGTH)
    {
        totemRuntimeStringValue_SetMiniString(str->Value, (uint8_t)str->Length, valOut);
        return totemLinkStatus_Success;
    }
    
    totemLinkStatus status = totemLinkStatus_Success;
    totemLock_Acquire(&runtime->InternedStringsLock);

    totemRuntimeStringSetLiteralKey key;
    key.Key = *str;
    if(totemRuntimeStringSet_GetLiteral(&runtime->InternedStrings, &key))
    {
        *valOut = *key.Value;
    }
    else
    {
        totemInternedStringHeader *newStr = totemInternedStringHeader_Alloc(str->Length);
        if (!newStr)
        {
            status = totemLinkStatus_OutOfMemory;
        }
        else
        {
            newStr->Length = str->Length;

            newStr->Hash = totem_Hash(str->Value, newStr->Length);
            memcpy(newStr->Data, str->Value, newStr->Length);
            newStr->Data[newStr->Length] = 0;

            totemRuntimeStringValue_FromInternedString(newStr, valOut);

            if (!totemRuntimeStringSet_InsertNoSearch(&runtime->InternedStrings, valOut))
            {
                totemInternedStringHeader_Free(newStr);
                status = totemLinkStatus_OutOfMemory;
            }
            else
            {
                valOut->Type = totemPrivateDataType_InternedString;
                valOut->Src.InternedString = newStr;
            }
        }
    }

    totemLock_Release(&runtime->InternedStringsLock);

    if (status != totemLinkStatus_Success)
    {
        return totemLinkStatus_Break(status);
    }

    return status;
}
