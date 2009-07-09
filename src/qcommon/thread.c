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
	return NULL;
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
	threads[i].handle = CreateThread(NULL, 0, ThreadStart, (void *)i, 0);
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
	WaitForSingleObject(Com_GetThreadHandle(id), INFINITE):
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
static qthread_t workerThreads[MAX_THREADS];
static THREAD_LOCAL int workerThreadID;

static mutex_t jobListMutex;
static cond_t newJobs;
static growList_t jobList;
static int numJobs = 0;
static qboolean stopThreads = qfalse;

/*
=================
WorkerThreadStart
=================
*/
static void WorkerThreadStart(void *id)
{
	jobHeader_t *job;
	int i;

	workerThreadID = (qthread_t)id;
	workerThreads[workerThreadID] = threadID;

	while (1) {
		Com_MutexLock(&jobListMutex);
		while (!numJobs)
			Com_CondWait(&newJobs, &jobListMutex);
		if (stopThreads) {
			Com_MutexUnlock(&jobListMutex);
			return;
		}

		// Fetch a job, remove it from the list, and run it
		for (i = 0; i < jobList.currentElements; i++) {
			if (!jobList.elements[i])
				continue;
			job = jobList.elements[i];
			jobList.elements[i] = NULL;
			numJobs--;
			Com_MutexUnlock(&jobListMutex);
			job->jobfunc(job);
			break;
		}
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

	Com_InitGrowList(&jobList, 32);
	memset(jobList.elements, 0, 32 * sizeof(void *));
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
	int i;

	Com_MutexLock(&jobListMutex);
	numJobs++;
	stopThreads = qtrue;
	Com_CondSignalAll(&newJobs);
	Com_MutexUnlock(&jobListMutex);

	for (i = 0; i < numWorkerThreads; i++) {
		Com_JoinThread(workerThreads[i]);
	}
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
Com_AddJobToPool
=================
*/
void Com_AddJobToPool(jobHeader_t *job)
{
	int i;

	Com_MutexLock(&jobListMutex);
	numJobs++;

	// Try to reuse an existing slot
	for (i = 0; i < jobList.currentElements; i++) {
		if (!jobList.elements[i])
			break;
	}
	if (i == jobList.currentElements)
		Com_AddToGrowList(&jobList, job);

	Com_CondSignal(&newJobs);
	Com_MutexUnlock(&jobListMutex);
}

/*
=================
Com_ProcessJobPool
=================
*/
qboolean Com_ProcessJobPool(int jobType)
{
	jobHeader_t *job;
	int i;

	Com_MutexLock(&jobListMutex);
	if (!numJobs) {
		Com_MutexUnlock(&jobListMutex);
		return qfalse;
	}

	// Fetch a job, remove it from the list, and run it
	for (i = 0; i < jobList.currentElements; i++) {
		if (!jobList.elements[i])
			continue;
		job = jobList.elements[i];
		if (jobType != JOBTYPE_ANY && job->jobType != jobType)
			continue;
		jobList.elements[i] = NULL;
		numJobs--;
		Com_MutexUnlock(&jobListMutex);
		job->jobfunc(job);
		return qtrue;
	}

	Com_MutexUnlock(&jobListMutex);
	return qfalse;
}
