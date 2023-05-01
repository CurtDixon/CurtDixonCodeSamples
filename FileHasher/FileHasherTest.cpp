// FileHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.PROMISE_TYPE
//

//#include "stdafx.h"

#include <stdio.h>
#include <windows.h>
#include <Wincrypt.h>

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "FileHasher.h"

#include <thread>       // testing

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

// this type creates a smart handle with automatic close
typedef std::unique_ptr <std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> AUTO_HANDLE;


const wchar_t *strTestFile = L"C:\\Users\\cdixon\\Downloads\\en_sql_server_2019_enterprise_x64_dvd_5e1ecc6b.iso";
const wchar_t *strTestFile2 = L"C:\\Users\\cdixon\\Downloads\\SSMS-Setup-ENU.exe";
const wchar_t *strTestFile3 = L"C:\\Users\\cdixon\\Downloads\\Logfile.PML";
const wchar_t *strTestFile4 = L"C:\\Users\\cdixon\\Downloads\\CSD AES-587.log";
const wchar_t *strTestFile5 = L"C:\\Users\\cdixon\\Downloads\test1.etl";
const wchar_t *strTestFile6 = L"C:\\Users\\cdixon\\Downloads\\test2.etl";


FileHasher  *g_FileHasher;

void TestThread1()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread2()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread3()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile3);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread4()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile4);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread5()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile5);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread6()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread7()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile6);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread8()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile2);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}

void TestThread9()
{
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile6);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue, CALG_MD5, true);
    wprintf(L"Hash = %s\n", wstrHashValue.c_str());
}



int RunTest()
{
    //std::thread test(TestThread);
    std::wstring    wstrHashValue;
    std::wstring    wstrTestFile(strTestFile);

//#if defined(_DEBUG)
    LARGE_INTEGER liFreq, liStartTime, liStopTime;
    UINT64 uuElapsedTime;

    ::QueryPerformanceFrequency(&liFreq);
    ::QueryPerformanceCounter(&liStartTime);
//#endif
    std::thread test1(TestThread1);
    std::thread test2(TestThread2);
    std::thread test3(TestThread3);
    std::thread test4(TestThread4);
    std::thread test5(TestThread5);
    std::thread test6(TestThread6);         /////////////
    std::thread test7(TestThread7);
    std::thread test8(TestThread8);
    std::thread test9(TestThread9);

    g_FileHasher->GetFileHash(wstrTestFile, wstrHashValue);
    byte dest[16];
    FileHasher::ConvertHexStringToBinary(wstrHashValue, dest, sizeof(dest));

    test1.join();
    test2.join();
    test3.join();
    test4.join();
    test5.join();
    test6.join();
    test7.join();
    test8.join();
    test9.join();

//#if defined(_DEBUG)
    ::QueryPerformanceCounter(&liStopTime);
    uuElapsedTime = 0;
    if (liFreq.QuadPart)
        uuElapsedTime = ((liStopTime.QuadPart - liStartTime.QuadPart) * 1000000ull) / liFreq.QuadPart;
    wprintf(L"main Hash = %s\n", wstrHashValue.c_str());
    wprintf(L"duration %llu microseconds\n", uuElapsedTime);
//#endif

    return 0;

}


int main()
{
    //_CrtSetBreakAlloc(155);

    //g_mtxFileHashesMap = new std::mutex;
    //g_FileHasher = new FileHasher;
    RunTest();

    //delete g_FileHasher;
    //delete g_mtxFileHashesMap;
    _CrtDumpMemoryLeaks();
}

