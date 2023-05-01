// FileHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.PROMISE_TYPE
//

#include "stdafx.h"

#include <stdio.h>
#include <windows.h>
#include <wincrypt.h>

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "FileHasher.h"



// FileHasher is a lightweight class with only static member functions. It is designed to be called from multiple threads.
// It does not create its own thread, but runs in the context of the calling thread. So the hashing work is distributed across
// multiple threads instead of being serialized into only one.

// It is a stand-alone class.

// Currently, threads are created via std::async that call FileHasher::GetFileHash. Whichever thread makes the 1st request
// for a particular file computes the hash. If other threads request the same file hash, instead of starting a new hash calculation,
// they will wait until the earlier thread completes the hash.

// For reference, in a test Win 10-64 VM with 6 virtual cores, calculating the MD5 hash of a 1.4GB file takes about 3.4 seconds.

// The computed file hashes are stored in a internal static map cache for fast future retrieval.
// This is the layout of the std::map
//
// map key = std::wstring                           // full file path
// map value = MAP_HASH_VALUE_STRUCT< std::shared_mutex,  // synchronizes access to the file's hash value string below
//                        std::wstring >            // wstring of file hash


// Note: A map of std::promise will not work here because a std::promise can only be accessed once.


typedef struct
{
    std::shared_mutex   m_mtxHashValueMutex;
    std::wstring        m_strHashValue;
} MAP_HASH_VALUE_STRUCT;


// file hash map
typedef std::map <
    const std::wstring,                     // file path string used as key
    std::shared_ptr<MAP_HASH_VALUE_STRUCT>  // hash value pair (mutex and hash string)
> HASH_MAP_TYPE;

static HASH_MAP_TYPE    g_mapFileHashesCache;   // global map of file hashes cache
static std::mutex       g_mtxFileHashesMap;     // mutex that protects the file hash map


static const size_t kFileReadBufferSize = 24 * 1024; // in bytes. This value gave the best performance in testing.


static DWORD CalcFileHash(
    const std::wstring& wstrFilePath,
    std::wstring& wstrHashValue,
    ALG_ID Algid)
{
    HANDLE  hFile = INVALID_HANDLE_VALUE;
    HCRYPTPROV  hCryptProviderHandle = NULL;
    HCRYPTHASH  hHashHandle = NULL;

    // This is a "scope guard" on exit cleanup routine.
    std::shared_ptr<int> OnExit(NULL, [&](int *)
    {
        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle(hFile);
        if (hHashHandle)
            CryptDestroyHash(hHashHandle);
        if (hCryptProviderHandle)
            CryptReleaseContext(hCryptProviderHandle, 0);
    });

    hFile = CreateFileW(wstrFilePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }

    if (!hCryptProviderHandle)
    {
        // Setup the crypto provider
        if (!CryptAcquireContextW(&hCryptProviderHandle,
            NULL,
            NULL,
            PROV_RSA_FULL,
            CRYPT_VERIFYCONTEXT))
        {
            return GetLastError();
        }
    }

    if (!CryptCreateHash(hCryptProviderHandle, Algid, 0, 0, &hHashHandle))
    {
        return GetLastError();
    }

    while (TRUE)
    {
        DWORD dwBytesRead = 0;
        BYTE  bReadBuffer[kFileReadBufferSize];

        if (!ReadFile(hFile, bReadBuffer, kFileReadBufferSize, &dwBytesRead, NULL))
            return GetLastError();

        if (dwBytesRead == 0)
        {
            // Reached the end of the file
            DWORD dwHashSize = sizeof(DWORD);   // note: MSDN is wrong on how to use HP_HASHSIZE
            CryptGetHashParam(hHashHandle, HP_HASHVAL, NULL, &dwHashSize, 0);
            std::unique_ptr<byte[]> bHashBuffer(new byte[dwHashSize]);

            // Get the result of the hashing and convert to string
            if (CryptGetHashParam(hHashHandle, HP_HASHVAL, bHashBuffer.get(), &dwHashSize, 0))
            {
                wstrHashValue = FileHasher::ConvertHashBufferToHexString(bHashBuffer.get(), dwHashSize);
                return NO_ERROR;
            }
        }
        else
        {
            if (!CryptHashData(hHashHandle, bReadBuffer, dwBytesRead, 0))
            {
                return GetLastError();
            }
        }
    }
}   // end while true


DWORD FileHasher::GetFileHash(
    const std::wstring& wstrFilePath,
    std::wstring& wstrHashValue,
    ALG_ID Algid,
    bool boolForceRecalc)
{
    std::unique_lock<std::mutex> lockMap(g_mtxFileHashesMap, std::defer_lock);
    lockMap.lock();     // Lock map while we call find()

    auto iter = g_mapFileHashesCache.find(wstrFilePath);
    if (boolForceRecalc || (iter == g_mapFileHashesCache.end()))      // recalc requested or file name not not in map
    {
        // create a new blank map hash value struct to insert into the global map
        auto  pMapHashValueStruct = std::make_shared<MAP_HASH_VALUE_STRUCT>();
        pMapHashValueStruct->m_mtxHashValueMutex.lock();         // lock the hash value mutex in exclusive (writer) mode

        // Insert or replace the map hash value into the map.
        auto iter2 = g_mapFileHashesCache.insert_or_assign(wstrFilePath, pMapHashValueStruct);

        // The pMapHashValueStruct that we just inserted into the map is a std::shared_ptr. So, no matter what happens to this map
        // entry, our copy will live until we are done with it here.
        
        lockMap.unlock();       // unlock global hash map while we compute the hash

        DWORD rc = CalcFileHash(wstrFilePath, pMapHashValueStruct->m_strHashValue, Algid);

        if (rc != NO_ERROR)
        {
            pMapHashValueStruct->m_strHashValue = L"Error: ";
            pMapHashValueStruct->m_strHashValue += std::to_wstring(rc);
        }
        
        wstrHashValue = pMapHashValueStruct->m_strHashValue;
        pMapHashValueStruct->m_mtxHashValueMutex.unlock();        // unlock the hash value shared mutex

        return rc;
    }

    // File name is in the map

    // Wait if necessary for the future hash value
    std::shared_ptr<MAP_HASH_VALUE_STRUCT>  pMapHashValueStruct = iter->second;
    lockMap.unlock();       // unlock the map while we wait for the hash

    // We have a shared_ptr for the map hash value pair, so even if entry is erased from global map, our ptrs are good
    pMapHashValueStruct->m_mtxHashValueMutex.lock_shared();     // lock the hash value mutex in shared (reader) mode
    wstrHashValue = pMapHashValueStruct->m_strHashValue;        // copy hash value string for caller
    pMapHashValueStruct->m_mtxHashValueMutex.unlock_shared();

    return NO_ERROR;
}


std::wstring FileHasher::ConvertHashBufferToHexString(
    const BYTE * pInputBuffer,
    const size_t aInputBufferSize)
{
    static const wchar_t kAlphabit[] = L"0123456789abcdef";
    std::wstring  wstrResult;
    wstrResult.reserve(aInputBufferSize * 2);   // wstring always wants extra space. * 2 avoids extra internal string buf realloc

    for (int i = 0; i < aInputBufferSize; ++i)
    {
        wstrResult += kAlphabit[(pInputBuffer[i] >> 4) & 0xf];
        wstrResult += kAlphabit[pInputBuffer[i] & 0xf];
    }

    return wstrResult;
}


bool FileHasher::ConvertHexStringToBinary(const std::wstring &wstrSource, byte *pDest, DWORD dwLen)
{
    DWORD len = min(dwLen, (DWORD)wstrSource.length() / 2);
    for (DWORD i = 0; i < len; i++)
    {
        DWORD dwTmp;
        int nFields = swscanf_s(wstrSource.data() + (i * 2), L"%02x", &dwTmp);
        if (nFields != 1)
            return false;           // invalid hex digits
        pDest[i] = (byte)dwTmp;
    }
    return true;
}
