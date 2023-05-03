// MultithreadingDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// Description:
// Write a multithreaded program that :
// 1. Generates N random integers between 0 - 9
// 2. Counts how many times each integer was repeated
// The program shall use :
// 1. A queue of max 10 elements to store the integers
// 2. A tracking array of exactly 10 elements to store the repetition counts.The array index correspond to the numbers 0 - 9.
//
// What you need to do :
//    -Use C / C++ language.
//    - Create 4 threads.
//    - Threads #1 and #2 generate a total of N random integers between 0 - 9 and put them into
//    the queue with a maximum size of 10 elements.If queue is full - the threads wait.
//    - Threads #3 and #4 read from the queue and increment the array value at the
//    corresponding index.If the queue is empty - the threads wait.
//    - When all N numbers are generated and counted, the threads must exit gracefully and
//    the program then outputs the resulting tracking array.
//    - You may use any editor / compiler of your choice.We prefer if Visual Studio is used but
//    anything that gets the job done is acceptable.
//
//    What we are looking for (in order of importance) :
//    1. Correct multithreading synchronization :
//      a.We will examine your choice of synchronization primitives
//      b.How you use these primitives to achieve the best performance.
//    2. Clean readable code
//      3. Comments
// Example :
// Input(for N = 7) : [1, 1, 2, 2, 2, 9, 0]
// Output : [1, 2, 3, 0, 0, 0, 0, 0, 0, 1


#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cstdlib>
#include <atomic>
#include <chrono>  
#include <random>

#include <functional>


const int MAX_QUEUE_SIZE = 10;
const int MAX_INTEGERS_TO_GENERATE = 12;
const int NUM_THREADS = 4;


class MultithreadingDemo
{
public:
    MultithreadingDemo() :
        m_nIntegerCounter(MAX_INTEGERS_TO_GENERATE),
        m_nIntegerTotals(MAX_QUEUE_SIZE, 0),
        m_boolStopConsumerThreads(false),
        m_RandomDist(0, MAX_QUEUE_SIZE),
        m_RandomGen(m_RandomDev())
    {
    };

    void Init()
    {
        m_nIntegerTotals = std::vector<int>(MAX_QUEUE_SIZE, 0);
        m_queueIntegers = {};
        m_nIntegerCounter = MAX_INTEGERS_TO_GENERATE;
        m_boolStopConsumerThreads = false;
    }

    bool WaitAndPop(int threadnum);
    bool PushOrWait(int value, int threadnum);
    void ProducerThread(int threadnum);
    void ConsumerThread(int nThreadNum);

    bool RunTest();

    std::queue<int> 		m_queueIntegers;
    std::mutex 				m_mutexQueue;
    std::condition_variable m_cvQueueReady;
    std::atomic<int>		m_nIntegerCounter;
    std::vector<int>		m_nIntegerTotals;

    std::uniform_real_distribution<>    m_RandomDist;
    std::mt19937        m_RandomGen;
    std::random_device  m_RandomDev;

    bool m_boolStopConsumerThreads;

 };


// Wait for items to appear in the queue. Increment the appropriate value total in the array
// and pop the item from the queue.

bool MultithreadingDemo::WaitAndPop(int threadnum)
{
    std::unique_lock<std::mutex> lk(m_mutexQueue);

    // Wait until the queue contains elements OR stop is signaled
    m_cvQueueReady.wait(lk, [&]()
        {
            return (m_boolStopConsumerThreads || !m_queueIntegers.empty());	// condition var predicate
        });

    while (m_queueIntegers.size())
    {
        int value = m_queueIntegers.front();
        ++(m_nIntegerTotals[value]);
        printf("consumer %d popping <<<< value %d\n", threadnum, value);
        m_queueIntegers.pop();
    }

    lk.unlock();		// manually unlock mutex so another thread can acquire immediately after notify below
    m_cvQueueReady.notify_all();

    // if stop was signaled, we're done
    if (m_boolStopConsumerThreads)
    {
        //printf("consumer %d ending\n", threadnum);
        return false;
    }
    return true;
}


// Push an item on the queue. If the queue is full, wait for room.

bool MultithreadingDemo::PushOrWait(int value, int threadnum)
{
    std::unique_lock<std::mutex> lk(m_mutexQueue);

    // Wait until the queue has room
    m_cvQueueReady.wait(lk, [&]()
        {
            return (m_queueIntegers.size() < MAX_QUEUE_SIZE);		// condition var predicate
        });

    m_queueIntegers.push(value);
    printf("producer %d pushing >>>> value %d\n", threadnum, value);

    lk.unlock();		// manually unlock mutex so another thread can acquire immediately after notify below
    m_cvQueueReady.notify_one();

    return true;
}


// The producer thread generates random #'s and pushes them on the queue

void MultithreadingDemo::ProducerThread(int threadnum)
{
    while (m_nIntegerCounter.fetch_sub(1) > 0)		// loop for desired # of integers
    {
        int randomnum = (int)m_RandomDist(m_RandomGen);
        // This sleep is only to allow all threads a chance to run for the test. In general, code runs faster when it doesn't sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!PushOrWait(randomnum, threadnum))
            break;
    }

    printf("producer %d ending\n", threadnum);
}

// consumer thread pops queue items until told to stop

void MultithreadingDemo::ConsumerThread(int threadnum)
{
    // keep waiting and popping queue elements until told to stop
    while (WaitAndPop(threadnum))
        ;
    printf("consumer %d ending\n", threadnum);
}


bool MultithreadingDemo::RunTest()
{
    Init();

    // Start a group of consumer & producer threads

    std::vector<std::thread> consumers;
    std::vector<std::thread> producers;

    for (int i = 1; i <= NUM_THREADS; ++i)
    {
        consumers.emplace_back(std::thread(&MultithreadingDemo::ConsumerThread, this, i));
        producers.emplace_back(std::thread(&MultithreadingDemo::ProducerThread, this, i));
    }

    // wait for all producer threads to complete
    for (auto& t : producers)
        t.join();


    // signal all consumer threads to stop
    m_boolStopConsumerThreads = true;
    std::unique_lock<std::mutex> lk(m_mutexQueue);	// this guarantees all threads are sleeping and waiting and can't miss the notify call below
    m_cvQueueReady.notify_all();
    lk.unlock();

    // wait for consumers to stop
    for (auto& t : consumers)
        t.join();

    // Validate that we got the correct result
    int total = 0;

    printf("\nCounters:\n");
    total = 0;
    for (int i = 0; i < MAX_QUEUE_SIZE; ++i)
    {
        printf("%d", m_nIntegerTotals[i]);
        total += m_nIntegerTotals[i];
        if (i < MAX_QUEUE_SIZE - 1)
            printf(", ");
    }
    printf("\nTotal = %d\n", total);

    return true;
}


int main(int argc, char** argv)
{
    MultithreadingDemo test;

    int testCount = 0;
    while (++testCount < 5 &&
        test.RunTest())
    {
        if (testCount % 100 == 0)
            printf("Test %d\n", testCount);
    }

    return 0;
}
