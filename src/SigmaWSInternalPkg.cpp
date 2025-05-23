#include "SigmaWSInternalPkg.h"
#include <libb64/cdecode.h>
#include <libb64/cencode.h>

String SigmaWSInternalPkg::GetEncoded(byte *data, int length)
{
    String msg = "";
    char *buffer;
    int bufferLen = base64_encode_expected_len(length);
    buffer = (char *)malloc(bufferLen);
    base64_encode_chars((char *)data, length, buffer);
    msg = String(buffer);
    free(buffer);

    return msg;
}

int SigmaWSInternalPkg::GetDecodedLength(String data)
{
    return base64_decode_expected_len(data.length());
}

int SigmaWSInternalPkg::GetDecoded(String data, byte *encodedData)
{
    int length = GetDecodedLength(data);
    return base64_decode_chars(data.c_str(), length, (char *)encodedData);
}