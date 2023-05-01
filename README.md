# CurtDixonCodeSamples
This repos is a small sample of code written by Curt Dixon.

FileHasher is a stand-alone, lightweight class that maintains a cache of file hashes. It is designed to be called from multiple threads. It does not create its own threads,
but runs in the context of the calling thread. The typical use case is a thread is created by a kernel event monitor (mini-filter) when a binary file is
being opened, and the hash needs to be checked before allowing the binary to load. This means the hashing work is distributed across multiple threads (cores)
instead of being serialized into only one.

FileHasher demonstrates std::map, std::mutex with reader/writer locks, (i.e. lock_shared), as well as the Windows crypto API (CryptCreateHash, etc.). It has a test main() and can be built and run in VS 2022.

------------

InvProcHandler is a Proof Of Concept (POC) of a driver the intercepts processes starting, and redirects them to the Invincea service application. It requires the Invincea agent service, so this demo will not run currently.

InvProcHandler demonstrates use of PsSetCreateProcessNotifyRoutine, ZwQueryInformationProcess, KeStackAttachProcess, PsLookupProcessByProcessId, and kernel<->user mode communication using IRPs (IoCompleteRequest).

--------------

MultithreadingDemo is a demonstration of proper queue handling/locking with multiple threads in modern C++.

It demonstrates the use of std::thread, std::atomic, std::mutex, std::condition_variable, std::uniform_real_distribution<>, and std::unique_lock.

