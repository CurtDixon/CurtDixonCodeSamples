# Curt Dixon Code Samples
This repo is a small sample of code written by Curt Dixon.

FileHasher is a stand-alone, lightweight class that maintains a cache of file hashes. It is designed to be called from multiple threads. It does not create its own threads, but runs in the context of the calling thread. This means the hashing work is distributed across multiple threads (cores) instead of being serialized into only one. Reader/writer locks are used for increased performance.

The typical use case is a thread is created by a kernel event monitor (minifilter) when a binary file is being opened, and the hash needs to be checked before allowing the binary to load. The verification of the hash, and eventual clearing of the cache are handled by the caller.

FileHasher demonstrates std::map, std::mutex with reader/writer locks, (i.e. lock_shared), as well as the Windows crypto API (CryptCreateHash, etc.). It has a test main() and can be built and run in VS 2022. FileHasherTest.cpp was used by me for personal experimentation (i.e. it is not clean code) and was not intended to be checked in with the class code.  

------------

InvProcHandler is a Proof Of Concept (POC) C++ driver I wrote in 2013. If a process with the Internet Zone tag is started, the driver terminates it and restarts it in the Invincea sandbox. It requires the Invincea agent service, so this demo will not run currently.

The POC goals were to
  1) Experiment with writing a driver in C++ as opposed to C.
  2) Try the Sysprogs BazisLib C++ libraries for drivers.
  3) Use IRPs for communicating between user & kernel mode (as opposed to FilterSendMessage/FilterGetMessage API). There is only 1 IRP used in this POC.

InvProcHandler demonstrates use of PsSetCreateProcessNotifyRoutine, ZwQueryInformationProcess, KeStackAttachProcess, PsLookupProcessByProcessId, and kernel<->user mode communication using IRPs (IoCompleteRequest).

--------------

MultithreadingDemo is a demonstration of proper queue handling/locking with multiple threads in modern C++.

It demonstrates the use of std::thread, std::atomic, std::mutex, std::condition_variable, std::uniform_real_distribution<>, and std::unique_lock.

