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

// thread.c -- helper functions for multithreading support

#include "q_shared.h"
#include "qcommon.h"

/*
==============================================================
GENERIC THREAD CONTROL FUNCTION
==============================================================
*/

// Per thread ID, set on thread init
static THREAD_LOCAL qthread_t threadID;

// All of the information about a running thread
typedef struct {
#ifdef _WIN32
	HANDLE handle;
#else
	pthread_t handle;
#endif
	qboolean inUse;
	void *arg;
	thread_func_t func;
} threadinfo_t;

// Array containing all of the thread IDs
static threadinfo_t threads[MAX_THREADS];

/*
=================
ThreadStart

Platform-specific thread start wrapper
=================
*/
#ifdef _WIN32
static DWORD WINAPI ThreadStart(void *arg)
#else
static void *ThreadStart(void *arg)
#endif
{
	threadID = (qthread_t)arg;
	threads[threadID].func(threads[threadID].arg);
#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

/*
=================
Com_SpawnThread
=================
*/
qthread_t Com_SpawnThread(thread_func_t func, void *arg)
{
	qthread_t i;

	// Find an unused spot
	for (i = 0; i < MAX_THREADS; i++) {
		if (!Com_AtomicTestAndSet(&threads[i].inUse))
			break;
	}

	if (i >= MAX_THREADS)
		Com_Error(ERR_FATAL, "Exceeded maximum number of threads");

	threads[i].func = func;
	threads[i].arg = arg;

#ifdef _WIN32
	threads[i].handle = CreateThread(NULL, 0, ThreadStart, (void *)i, 0, NULL);
	if (!threads[i].handle)
		Com_Error(ERR_FATAL, "Error spawning thread %d", (int)i);
#else
	if (pthread_create(&threads[i].handle, NULL, ThreadStart, (void *)i))
		Com_Error(ERR_FATAL, "Error spawning thread %d", (int)i);
#endif

	return i;
}

/*
=================
Com_JoinThread
=================
*/
void Com_JoinThread(qthread_t id)
{
#ifdef _WIN32
	WaitForSingleObject(Com_GetThreadHandle(id), INFINITE);
#else
	pthread_join(Com_GetThreadHandle(id), NULL);
#endif
}

/*
=================
Com_GetThreadID
=================
*/
qthread_t Com_GetThreadID(void)
{
	return threadID;
}

/*
=================
Com_GetThreadHandle
=================
*/
qthread_handle_t Com_GetThreadHandle(qthread_t id)
{
	return threads[id].handle;
}

/*
=================
Com_GetNumCPUs
=================
*/
int Com_GetNumCPUs(void)
{
#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
	int nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs < 1)
		return 1;
	else
		return nprocs;
#else
	return 1;
#endif
#endif
}

/*
==============================================================
THREAD POOL AND JOB MANAGEMENT
==============================================================
*/

static int numWorkerThreads = 0;
static THREAD_LOCAL qthread_t workerID;

static mutex_t jobListMutex;
static cond_t newJobs;
static jobHeader_t *jobList = NULL;
static qboolean stopThreads = qfalse;

/*
=================
WorkerThreadStart
=================
*/
static void WorkerThreadStart(void *id)
{
	jobHeader_t *job;

	workerID = (qthread_t)id;

	while (1) {
		Com_MutexLock(&jobListMutex);
		while (!jobList && !stopThreads)
			Com_CondWait(&newJobs, &jobListMutex);
		if (stopThreads) {
			Com_MutexUnlock(&jobListMutex);
			return;
		}

		// Fetch a job, remove it from the list, and run it
		job = jobList;
		jobList = job->next;
		Com_MutexUnlock(&jobListMutex);
		job->jobfunc(job);
	}
}

/*
=================
Com_InitThreadPool
=================
*/
void Com_InitThreadPool(int numThreads)
{
	qthread_t i;

	Com_MutexInit(&jobListMutex);
	Com_CondInit(&newJobs);

	numWorkerThreads = numThreads;
	for (i = 0; i < numThreads; i++)
		Com_SpawnThread(WorkerThreadStart, (void *)i);
}

/*
=================
Com_ShutdownThreadPool
=================
*/
void Com_ShutdownThreadPool(void)
{
	stopThreads = qtrue;
	Com_CondBroadcast(&newJobs);
}

/*
=================
Com_GetNumThreadsInPool
=================
*/
int Com_GetNumThreadsInPool(void)
{
	return numWorkerThreads;
}

/*
=================
Com_GetWorkerID
=================
*/
qthread_t Com_GetWorkerID(void)
{
	return workerID;
}

/*
=================
Com_AddJobToPool
=================
*/
void Com_AddJobToPool(jobHeader_t *job)
{
	jobHeader_t *next, *prev;

	Com_MutexLock(&jobListMutex);

	if (!jobList) {
		jobList = job;
		job->next = NULL;
	} else {
		prev = jobList;
		next = jobList->next;
		while (next && next->priority >= job->priority) {
			prev = next;
			next = next->next;
		}
		prev->next = job;
		job->next = next;
	}

	Com_MutexUnlock(&jobListMutex);
	Com_CondSignal(&newJobs);
}

/*
==============================================================
SUPPORT FUNCTIONS FOR WIN32
==============================================================
*/

#ifdef _WIN32

/*
=================
Com_CondInit
=================
*/
void Com_CondInit(cond_t *cond)
{
	cond->semaphore = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
	Com_MutexInit(&cond->mutex);
	cond->numSleeping = 0;
	cond->numAwake = 0;
	cond->generation = 0;
}

/*
=================
Com_CondDestroy
=================
*/
void Com_CondDestroy(cond_t *cond)
{
	CloseHandle(cond->semaphore);
	Com_MutexDestroy(&cond->mutex);
}

/*
=================
Com_CondWait
=================
*/
void Com_CondWait(cond_t *cond, mutex_t *mutex)
{
	uint32_t generation;
	qboolean wake = qfalse;

	Com_MutexLock(&cond->mutex);
	cond->numSleeping++;
	generation = cond->generation;
	Com_MutexUnlock(&cond->mutex);

	Com_MutexUnlock(mutex);
	while (1) {
		WaitForSingleObject(cond->semaphore, INFINITE);
		Com_MutexLock(&cond->mutex);
		if (cond->numAwake) {
			if (cond->generation != generation) {
				cond->numAwake--;
				cond->numSleeping--;
				break;
			} else
				wake = qtrue;
		}
		Com_MutexUnlock(&cond->mutex);
        if (wake) {
            wake = qfalse;
            ReleaseSemaphore(cond->semaphore, 1, NULL);
        }
	}
	Com_MutexUnlock(&cond->mutex);
	Com_MutexLock(mutex);
}

/*
=================
Com_CondSignal
=================
*/
void Com_CondSignal(cond_t *cond)
{
	qboolean wake = qfalse;

	Com_MutexLock(&cond->mutex);
    if (cond->numSleeping > cond->numAwake) {
		wake = qtrue;
        cond->numAwake++;
        cond->generation++;
    }
	Com_MutexUnlock(&cond->mutex);

	if (wake)
		ReleaseSemaphore(cond->semaphore, 1, NULL);
}

/*
=================
Com_CondBroadcast
=================
*/
void Com_CondBroadcast(cond_t *cond)
{
	uint32_t wake = 0;

	Com_MutexLock(&cond->mutex);
    if (cond->numSleeping > cond->numAwake) {
		wake = cond->numSleeping - cond->numAwake;
        cond->numAwake = cond->numSleeping;
        cond->generation++;
    }
	Com_MutexUnlock(&cond->mutex);

	if (wake)
		ReleaseSemaphore(cond->semaphore, wake, NULL);
}


/*
=================
Com_RWL_Init
=================
*/
void Com_RWL_Init(rwlock_t *rwlock)
{
	Com_MutexInit(&rwlock->mutex);
	rwlock->readEvent = CreateMutex(NULL, FALSE, NULL);
	rwlock->readers = 0;
}

/*
=================
Com_RWL_Destroy
=================
*/
void Com_RWL_Destroy(rwlock_t *rwlock)
{
	CloseHandle(rwlock->readEvent);
	Com_MutexDestroy(&rwlock->mutex);
}

/*
=================
Com_RWL_LockRead
=================
*/
void Com_RWL_LockRead(rwlock_t *rwlock)
{
	Com_MutexLock(&rwlock->mutex);
	Com_AtomicIncrement(&rwlock->readers);
	ResetEvent(rwlock->readEvent);
	Com_MutexUnlock(&rwlock->mutex);
}

/*
=================
Com_RWL_TryLockRead
=================
*/
qboolean Com_RWL_TryLockRead(rwlock_t *rwlock)
{
	if (!Com_MutexTryLock(&rwlock->mutex))
		return qfalse;
	Com_AtomicIncrement(&rwlock->readers);
	ResetEvent(rwlock->readEvent);
	Com_MutexUnlock(&rwlock->mutex);
	return qtrue;
}

/*
=================
Com_RWL_UnlockRead
=================
*/
void Com_RWL_UnlockRead(rwlock_t *rwlock)
{
	if (!Com_AtomicDecrement(&rwlock->readers))
		SetEvent(rwlock->readEvent);
}

/*
=================
Com_RWL_LockWrite
=================
*/
void Com_RWL_LockWrite(rwlock_t *rwlock)
{
	Com_MutexLock(&rwlock->mutex);
	if (rwlock->readers)
		WaitForSingleObject(rwlock->readEvent, INFINITE);
}

/*
=================
Com_RWL_TryLockWrite
=================
*/
qboolean Com_RWL_TryLockWrite(rwlock_t *rwlock)
{
	if (!Com_MutexTryLock(&rwlock->mutex))
		return qfalse;
	if (rwlock->readers) {
		Com_MutexUnlock(&rwlock->mutex);
		return qfalse;
	}
	return qtrue;
}

/*
=================
Com_RWL_UnlockWrite
=================
*/
void Com_RWL_UnlockWrite(rwlock_t *rwlock)
{
	Com_MutexUnlock(&rwlock->mutex);
}

#endif
