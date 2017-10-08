#include "MapReduceFramework.h"
#include <pthread.h>
#include <vector>
#include<queue>
#include<map>
#include<iostream>
#include <mutex>
#include <sys/time.h>
#include <fstream>

//define
using namespace std;
#define CHUNK_SIZE 10
#define TIMEOUT 1000000
#define INITIAL_VAL -1
typedef pair<k2Base*,v2Base*> pair2;
struct comp
{
    bool operator()(const k2Base* key1, const k2Base* key2)
    {
        return (*key1)<(*key2);
    }
};


// function for handling errors
void systemFailure(int result, string failFunc)
{
    if (result != 0)
    {
        cout<<"MapReduceFramework Failure: "<< failFunc << " failed."<<endl;
        exit(1);
    }
}


//the global variables, the first 2 indicates the location that the thread
//works on, and so it will also have a mutex later on.
//also a flag for the shuffle
//and 2 int's that helps knowing which thread is running
int indexInPairsPool = 0;
int indexInPairsToReduce = 0;
int ourThreadLevel;
thread_local int threadID;
int threadCount = 0;
bool WORK_FOR_SHUFFLE =  true;
ofstream log;



// forward declerations
void performShuffleAllReaders();
void  createMapThreads(vector<pthread_t>*);
void createReduceThreads(vector<pthread_t>*);
void * shuffle(void*);
void* execMap(void*);
void* execReduce(void*);
void init();
void clearer();

//global datastructures, mutex's and cond.
OUT_ITEMS_LIST out;
IN_ITEMS_LIST pairsPool;
MapReduceBase* ourMapReduce;
map<k2Base *, V2_LIST,comp> inputToReduce;
deque<pair<deque<pair2>,pthread_mutex_t>> execMapThreadsPool;
deque<deque<OUT_ITEM>> execReduceThreadsPool;pthread_cond_t cv;
struct timespec abstime;
pthread_mutex_t lockPairPool;
pthread_mutex_t lockIndexReduce;
pthread_mutex_t lockTime;
pthread_mutex_t countMutex;



bool compareOut(const OUT_ITEM& out1, OUT_ITEM& out2)
{
    return (*out1.first) < (*out2.first);
}

/*
 * merges each of the threads output to one big output
 * and sorts them
 */
void mergeAndSort()
{
    int k=0;
    for (int i = 0 ; i <(int)execReduceThreadsPool.size();i++)
    {
        while (!execReduceThreadsPool[i].empty())
        {
            k++;
            OUT_ITEM item = execReduceThreadsPool[i].back();
            out.push_back(item);
            execReduceThreadsPool[i].pop_back();
        }
    }
    out.sort(compareOut);
}


//initalize
void init()
{
    lockPairPool = PTHREAD_MUTEX_INITIALIZER;
    lockIndexReduce = PTHREAD_MUTEX_INITIALIZER;
    lockTime = PTHREAD_MUTEX_INITIALIZER;
    countMutex = PTHREAD_MUTEX_INITIALIZER;
    cv = PTHREAD_COND_INITIALIZER;
    indexInPairsPool = 0;
    indexInPairsToReduce = 0;
    threadCount = 0;
    WORK_FOR_SHUFFLE = true;
    if (!out.empty())
    {
        out.clear();
    }
}

//clears
void clearer()
{
    int res;
    for (int i = 0; i<(int) execMapThreadsPool.size();i++)
    {
        res = pthread_mutex_destroy(&execMapThreadsPool[i].second);
        systemFailure(res,"pthread_mutex_destroy");
        execMapThreadsPool[i].first.clear();
        execReduceThreadsPool[i].clear();
    }
    execMapThreadsPool.clear();
    execReduceThreadsPool.clear();

    map<k2Base*,V2_LIST,comp>::iterator it = inputToReduce.begin();
    for ( ; it != inputToReduce.end(); it++)
    {
        it->second.clear();
    }

    inputToReduce.clear();

    res = pthread_mutex_destroy(&lockPairPool);
    systemFailure(res,"pthread_mutex_destroy");

    res = pthread_mutex_destroy(&lockIndexReduce);
    systemFailure(res,"pthread_mutex_destroy");

    res = pthread_mutex_destroy(&lockTime);
    systemFailure(res,"pthread_mutex_destroy");

    res = pthread_mutex_destroy(&countMutex);
    systemFailure(res,"pthread_mutex_destroy");

    pthread_cond_destroy(&cv);
    systemFailure(res,"pthread_mutex_destroy");

    pairsPool.empty();
}





/**
 *
 */
OUT_ITEMS_LIST runMapReduceFramework(MapReduceBase& mapReduce,
                                        IN_ITEMS_LIST& itemsList,
                                            int multiThreadLevel)
{
    //first thing we will do is to initalize log and variables.
    struct timeval beginTime, endTime;
    gettimeofday(&beginTime,NULL);
    string fileLoc(__FILE__);
    string path = fileLoc.substr(0,fileLoc.find_last_of("\\/")+1);
    log.open(path+"MapReduceFramework.log");
    log << "runMapReduceFramework started with " << multiThreadLevel <<
        " threads\n";
    init();

    ourThreadLevel = multiThreadLevel;
    vector<pthread_t> threads((unsigned long) ourThreadLevel);
    ourMapReduce = &mapReduce;
    pairsPool = itemsList;

    // the Map execution
    createMapThreads(&threads);

    // create a thread for shuffle
    pthread_t shuffleThread;
    int res = pthread_create(&shuffleThread, NULL, shuffle, NULL);
    systemFailure(res,"pthread_create");


    // use pthread_join in order to call to excecReduce() only
    // after excecMap and shuffle ended their work (terminated)
    for(int i = 0; i < ourThreadLevel; i++)
    {
        res = pthread_join(threads[i], NULL);
        systemFailure(res,"pthread_join");
    }

    WORK_FOR_SHUFFLE = false;
    res = pthread_join(shuffleThread, NULL);
    systemFailure(res,"pthread_join");
    gettimeofday(&endTime,NULL);
    log << "Map and Shuffle took "<<
            ((endTime.tv_sec-beginTime.tv_sec)*1000000 +
                    (endTime.tv_usec-beginTime.tv_usec)/1000)<<
            " ns\n";
    gettimeofday(&beginTime,NULL);
    // the Reduce excecution;
    threadCount = 0;
    threads.clear();
    createReduceThreads(&threads);

    for(int i = 0; i < ourThreadLevel; i++ )
    {
        res = pthread_join(threads[i], NULL);
        systemFailure(res,"pthread_join");

    }

    gettimeofday(&endTime,NULL);
    log << "Reduce took "<<
    ((endTime.tv_sec-beginTime.tv_sec)*1000000 +
     (endTime.tv_usec-beginTime.tv_usec)/1000)<<
    " ns\n";
    //merging and sorting the output, and calling a clearing function
    mergeAndSort();
    threads.clear();
    clearer();
    log << "runMapReduceFramework finished\n";
    log.close();
    return out;
}


void  createMapThreads(vector<pthread_t>* threads)
{
    for (int i = 0; i < ourThreadLevel; i++)
    {
        pthread_mutex_t threadMut = PTHREAD_MUTEX_INITIALIZER;
        deque<pair2> threadDeq;
        execMapThreadsPool.push_back(std::make_pair(threadDeq, threadMut));
        int t = pthread_create(&(*threads)[i], NULL, &execMap, NULL);
        systemFailure(t,"pthread_create");
    }
}


void createReduceThreads(vector<pthread_t>* threads)
{
    for (int i = 0; i < ourThreadLevel; i++)
    {
        deque<OUT_ITEM> tempDeq;
        execReduceThreadsPool.push_back(tempDeq);
        int t = pthread_create(&(*threads)[i], NULL, &execReduce, NULL);
        systemFailure(t,"pthread_create");
    }
}


string getTime()
{
    time_t t = time(NULL);
    tm timeNow = *localtime(&t);
    char returnValue[200] = {0};
    char dmyhms[] = "[%d.%m.%Y %H:%M:%S]";
    strftime(returnValue,199,dmyhms,&timeNow);
    return returnValue;
}

/**
 * the exececMap threads
 * this function is calling the map function we recieved from the user
 * on CHUNCK_SIZE pair every time it works
 */
void* execMap(void* i)
{
    log << "Thread ExecMap created " << getTime() <<"\n"<<endl;
    int res;
    //////////////////////////////////////////////////critical
    res = pthread_mutex_lock(&countMutex);
    systemFailure(res,"pthread_mutex_lock");

    threadID = threadCount++;

    res = pthread_mutex_unlock(&countMutex);
    systemFailure(res,"pthread_mutex_unlock");
    //////////////////////////////////////////////////end of critical
    int lengthPairs = (int) pairsPool.size();

    int beginIndex = INITIAL_VAL;
    int endIndex = INITIAL_VAL;

    while (beginIndex != lengthPairs)
    {
        //////////////////////////////////////////////////critical

        res = pthread_mutex_lock(&lockPairPool);
        systemFailure(res,"pthread_mutex_lock");

        beginIndex = indexInPairsPool;

        if(indexInPairsPool + CHUNK_SIZE > lengthPairs)
        {
            indexInPairsPool = lengthPairs;
            endIndex = indexInPairsPool;
        }
        else
        {
            indexInPairsPool = indexInPairsPool + CHUNK_SIZE;
            endIndex = indexInPairsPool;
        }

        res = pthread_mutex_unlock(&lockPairPool);
        systemFailure(res,"pthread_mutex_unlock");
        //////////////////////////////////////////////////end of critical

        IN_ITEMS_LIST::iterator it = pairsPool.begin();

        //advancing to the start
        advance(it, beginIndex);

        int i = beginIndex;
        //reads the next 10 pairs from the list or less pairs if 10 not exist
        while (i < endIndex)
        {
            IN_ITEM pair = *it;
            const k1Base *const key = pair.first;
            const v1Base *const val = pair.second;
            ourMapReduce->Map(key,val);
            i++;
            ++it;
        }
        //sends signal to the shuffle to wake up because it gets new pairs
        pthread_cond_signal(&cv);
    }
    log << "Thread ExecMap terminated " << getTime() <<"\n"<<endl;

    return NULL;
}

/*
 * this function does the actual shuffle work.
 * it runs on all the output the emit2 function created.
 * and pushs it to our internal datastructure that we implemented
 * using map for easy concatenation of values to keys.
 */
void performShuffleAllReaders()
{
    int res;
    int size = (int) execMapThreadsPool.size();
    for (int i = 0; i < size;i++)
    {
        while (!execMapThreadsPool[i].first.empty())
        {
            //////////////////////////////////////////////////critical
            res = pthread_mutex_lock(&execMapThreadsPool[i].second);
            systemFailure(res,"pthread_mutex_lock");
            pair2 currPair = execMapThreadsPool[i].first.front();
            inputToReduce[currPair.first].push_back(currPair.second);
            execMapThreadsPool[i].first.pop_front();
            res = pthread_mutex_unlock(&execMapThreadsPool[i].second);
            systemFailure(res,"pthread_mutex_unlock");
            //////////////////////////////////////////////////end of critical
        }
    }
}
/**
* this is the main shuffle function, it mainly takes care
 * of the conditional timewait. and calls for the function that actually
 * does the work
*/
void * shuffle(void* param)
{
    log << "Thread Shuffle created " << getTime() <<"\n"<<endl;

    struct timespec waitingTime;


    while (WORK_FOR_SHUFFLE)
    {
        waitingTime.tv_sec = time(NULL);
        waitingTime.tv_nsec =  TIMEOUT;
        pthread_cond_timedwait(&cv, &lockTime, &abstime);
        performShuffleAllReaders();
    }
    performShuffleAllReaders();
    log << "Thread Shuffle terminated " << getTime() <<"\n"<<endl;
    return NULL;
}

/**
 * the excecReduce threads
 * this function is calling them reduce function we recieve from the user
 * on CHUNCK_SIZE pair every time it work
 */
void* execReduce(void* i)
{
    log << "Thread execReduce created " << getTime() <<"\n"<<endl;
    int res;
    ////////////////////////////////critical
    res =pthread_mutex_lock(&countMutex);
    systemFailure(res,"pthread_mutex_lock");
    threadID = threadCount++;
    res = pthread_mutex_unlock(&countMutex);
    systemFailure(res,"pthread_mutex_unlock");
    /////////////////////////////// end of critical

    int beginIndex = INITIAL_VAL;
    int length = (int)inputToReduce.size();

    while (beginIndex != length)
    {
        ////////////////////////////////critical

        res = pthread_mutex_lock(&lockIndexReduce);
        systemFailure(res,"pthread_mutex_lock");

        beginIndex = indexInPairsToReduce;
        int endIndex;
        if ((indexInPairsToReduce + CHUNK_SIZE) > length) {
            indexInPairsToReduce = length;
            endIndex = indexInPairsToReduce;
        }
        else {
            indexInPairsToReduce = indexInPairsToReduce + CHUNK_SIZE;
            endIndex = indexInPairsToReduce;
        }
        res = pthread_mutex_unlock(&lockIndexReduce);
        systemFailure(res,"pthread_mutex_lock");

        /////////////////////////////// end of critical
        std::map<k2Base *, V2_LIST,comp>::iterator it = inputToReduce.begin();
        std::map<k2Base *, V2_LIST,comp>::iterator it2 = inputToReduce.end();

        int i = beginIndex;
        advance(it,beginIndex);

        // reads chunk of 10 pairs into data structure chunkPairsToReduce
        while (i < endIndex && it != it2) {
            pair<k2Base *, V2_LIST> currPair = *it;
            const k2Base *key = currPair.first;
            V2_LIST vals = currPair.second;
            ourMapReduce->Reduce(key, vals);
            it++;
            i++;
        }
    }
    log << "Thread execReduce terminated " << getTime() <<"\n"<<endl;
}

/**
* called by the map function in order to add a new pair to the framework
 * internal data structure
*/
void Emit2 (k2Base* key2, v2Base* val2)
{
    int res;
    pair2 p = std::make_pair(key2,val2);
    ////////////////////////// critical
    res = pthread_mutex_lock(&execMapThreadsPool[threadID].second);
    systemFailure(res,"pthread_mutex_lock");
    execMapThreadsPool[threadID].first.push_back(p);
    res = pthread_mutex_unlock(&execMapThreadsPool[threadID].second);
    systemFailure(res,"pthread_mutex_lock");
    ////////////////////////////end of critical
}


/**
*called by the reduce function in order to add  new pair to the the framework
 * internal data structure
*/
void Emit3 (k3Base* key3, v3Base* val3)
{
    OUT_ITEM item = std::make_pair(key3,val3);
    execReduceThreadsPool[threadID].push_back(item);
}
