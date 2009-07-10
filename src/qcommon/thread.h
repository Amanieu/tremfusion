/*
===========================================================================
Copyright (C) 2009 Amanieu d'Antras

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// thread.h -- helper functions for multithreading support

#ifndef _THREAD_H_
#define _THREAD_H_

#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#include <windef.h>
#include <winbase.h>
typedef HANDLE qthread_handle_t;
#else
#include <pthread.h>
typedef pthread_t qthread_handle_t;
#endif
typedef long qthread_t;

// NOTE: This doesn't work in dlls
#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#elif __GNUC__
#define THREAD_LOCAL __thread
#else
#error "No multithreading support!"
#endif


// Atomic operations, always returns the old value
static ID_INLINE int32_t Com_AtomicExchange(int32_t *ptr, int32_t value)
{
#ifdef _MSC_VER
	return InterlockedExchange(ptr, value);
#else
	return __sync_lock_test_and_set(ptr, value);
#endif
}
static ID_INLINE int32_t Com_AtomicIncrement(int32_t *ptr)
{
#ifdef _MSC_VER
	return InterlockedIncrement(ptr);
#else
	return __sync_fetch_and_add(ptr, 1);
#endif
}
static ID_INLINE int32_t Com_AtomicDecrement(int32_t *ptr)
{
#ifdef _MSC_VER
	return InterlockedDecrement(ptr);
#else
	return __sync_fetch_and_sub(ptr, 1);
#endif
}
static ID_INLINE int32_t Com_AtomicAdd(int32_t *ptr, int32_t value)
{
#ifdef _MSC_VER
	return InterlockedExchangeAdd(ptr, value);
#else
	return __sync_fetch_and_add(ptr, value);
#endif
}
static ID_INLINE int32_t Com_AtomicSubtract(int32_t *ptr, int32_t value)
{
#ifdef _MSC_VER
	return InterlockedExchangeAdd(ptr, -value);
#else
	return __sync_fetch_and_sub(ptr, value);
#endif
}
static ID_INLINE int32_t Com_AtomicCompareExchange(int32_t *ptr, int32_t test, int32_t value)
{
#ifdef _MSC_VER
	return InterlockedCompareExchange(ptr, value, test);
#else
	return __sync_val_compare_and_swap(ptr, test, value);
#endif
}
static ID_INLINE qboolean Com_AtomicTestAndSet(qboolean *ptr)
{
#ifdef _MSC_VER
	return InterlockedBitTestAndSet((LONG *)ptr, 0);
#else
	return __sync_lock_test_and_set(ptr, 1);
#endif
}


// Mutexes, condition variables and read/write locks
#ifdef _WIN32
typedef CRITICAL_SECTION mutex_t;
typedef struct {
    HANDLE semaphore;
    mutex_t mutex;
    uint32_t numSleeping;
    uint32_t numAwake;
    uint32_t generation;
} cond_t;
typedef struct {
	mutex_t mutex;
	HANDLE readEvent;
	int32_t readers;
} rwlock_t;
#else
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
typedef pthread_rwlock_t rwlock_t;
#endif

static ID_INLINE void Com_MutexInit(mutex_t *mutex)
{
#ifdef _WIN32
	InitializeCriticalSection(mutex);
#else
	pthread_mutex_init(mutex, NULL);
#endif
}
static ID_INLINE void Com_MutexDestroy(mutex_t *mutex)
{
#ifdef _WIN32
	DeleteCriticalSection(mutex);
#else
	pthread_mutex_destroy(mutex);
#endif
}
static ID_INLINE void Com_MutexLock(mutex_t *mutex)
{
#ifdef _WIN32
	EnterCriticalSection(mutex);
#else
	pthread_mutex_lock(mutex);
#endif
}
static ID_INLINE qboolean Com_MutexTryLock(mutex_t *mutex)
{
#ifdef _WIN32
	return (TryEnterCriticalSection(mutex) != 0);
#else
	return (pthread_mutex_trylock(mutex) == 0);
#endif
}
static ID_INLINE void Com_MutexUnlock(mutex_t *mutex)
{
#ifdef _WIN32
	LeaveCriticalSection(mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}

#ifdef _WIN32
void Com_CondInit(cond_t *cond);
void Com_CondDestroy(cond_t *cond);
void Com_CondWait(cond_t *cond, mutex_t *mutex);
void Com_CondSignal(cond_t *cond);
void Com_CondBroadcast(cond_t *cond);
#else
static ID_INLINE void Com_CondInit(cond_t *cond)
{
	pthread_cond_init(cond, NULL);
}
static ID_INLINE void Com_CondDestroy(cond_t *cond)
{
	pthread_cond_destroy(cond);
}
static ID_INLINE void Com_CondWait(cond_t *cond, mutex_t *mutex)
{
	pthread_cond_wait(cond, mutex);
}
static ID_INLINE void Com_CondSignal(cond_t *cond)
{
	// Only wakes up one thread
	pthread_cond_signal(cond);
}
static ID_INLINE void Com_CondBroadcast(cond_t *cond)
{
	// Wakes up all threads
	pthread_cond_broadcast(cond);
}
#endif

#ifdef _WIN32
void Com_RWL_Init(rwlock_t *rwlock);
void Com_RWL_Destroy(rwlock_t *rwlock);
void Com_RWL_LockRead(rwlock_t *rwlock);
qboolean Com_RWL_TryLockRead(rwlock_t *rwlock);
void Com_RWL_UnlockRead(rwlock_t *rwlock);
void Com_RWL_LockWrite(rwlock_t *rwlock);
qboolean Com_RWL_TryLockWrite(rwlock_t *rwlock);
void Com_RWL_UnlockWrite(rwlock_t *rwlock);
#else
static ID_INLINE void Com_RWL_Init(rwlock_t *rwlock)
{
	pthread_rwlock_init(rwlock, NULL);
}
static ID_INLINE void Com_RWL_Destroy(rwlock_t *rwlock)
{
	pthread_rwlock_destroy(rwlock);
}
static ID_INLINE void Com_RWL_LockRead(rwlock_t *rwlock)
{
	pthread_rwlock_rdlock(rwlock);
}
static ID_INLINE qboolean Com_RWL_TryLockRead(rwlock_t *rwlock)
{
	return pthread_rwlock_tryrdlock(rwlock) == 0;
}
static ID_INLINE void Com_RWL_UnlockRead(rwlock_t *rwlock)
{
	pthread_rwlock_unlock(rwlock);
}
static ID_INLINE void Com_RWL_LockWrite(rwlock_t *rwlock)
{
	pthread_rwlock_wrlock(rwlock);
}
static ID_INLINE qboolean Com_RWL_TryLockWrite(rwlock_t *rwlock)
{
	return pthread_rwlock_wrlock(rwlock) == 0;
}
static ID_INLINE void Com_RWL_UnlockWrite(rwlock_t *rwlock)
{
	pthread_rwlock_unlock(rwlock);
}
#endif


// Thread control functions
#define MAX_THREADS 256
#define INVALID_THREAD -1
typedef void (*thread_func_t)(void *);
qthread_t Com_SpawnThread(thread_func_t func, void *arg);
void Com_JoinThread(qthread_t id);
qthread_t Com_GetThreadID(void);
qthread_handle_t Com_GetThreadHandle(qthread_t id);


// Thread pool and job management
#define JOBTYPE_ANY -1
typedef struct jobHeader_s {
	void (*jobfunc)(struct jobHeader_s *header);
	int jobType;
	// Add your own stuff after this header
} jobHeader_t;
int Com_GetNumCPUs(void);
void Com_InitThreadPool(int numThreads);
void Com_ShutdownThreadPool(void);
int Com_GetNumThreadsInPool(void);
// The job pointer must stay allocated for the duration of existance of the job.
// The job function should free this structure once it is done using it.
void Com_AddJobToPool(jobHeader_t *job);
// This function will a single job from the pool with the specified jobType.
// Use JOBTYPE_ANY for a job of any type. Returns qtrue if a job was run.
qboolean Com_ProcessJobPool(int jobType);

#endif // _THREAD_H_
