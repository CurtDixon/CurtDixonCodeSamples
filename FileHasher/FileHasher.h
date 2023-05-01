#pragma once


#include <string>


class FileHasher
{
public:
    
    static DWORD GetFileHash(
        const std::wstring& wstrFileName,
        std::wstring& wstrHashValue,
        ALG_ID Algid = CALG_MD5,
        bool boolForceRecalc = false);

    static bool ConvertHexStringToBinary(const std::wstring &wstrSource, byte *pDest, DWORD dwLen);
    static std::wstring ConvertHashBufferToHexString(const BYTE * pInputBuffer, const size_t aInputBufferSize);
};