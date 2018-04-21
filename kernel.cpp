#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "llist.h"
#include "prioll.h"
#include "kernel.h"

/*

	STUDENT 1 NUMBER: A0161321X
	STUDENT 1 NAME: Tran Nhat Quang

	STUDENT 2 NUMBER: A0158438U
	STUDENT 2 NAME: Nicholas Ang

	STUDENT 3 NUMBER: A0174721H
	STUDENT 3 NAME: Loïc Vandenberghe

	*/

/* OS variables */

// Current number of processes
int procCount;

// Current timer tick
int timerTick=0;
int currProcess, currPrio;

#if SCHEDULER_TYPE == 0

/* Process Control Block for LINUX scheduler*/
typedef struct tcb
{
	int procNum;
	int prio;
	int quantum;
	int timeLeft;
} TTCB;

#elif SCHEDULER_TYPE == 1

/* Process Control Block for RMS scheduler*/

typedef struct tcb
{
	int procNum;
	int timeLeft;
	int deadline;
	int c;
	int p;
} TTCB;

#endif


TTCB processes[NUM_PROCESSES];

#if SCHEDULER_TYPE == 0

// Lists of processes according to priority levels.
TNode *queueList1[PRIO_LEVELS];
TNode *queueList2[PRIO_LEVELS];

// Active list and expired list pointers
TNode **activeList = queueList1;
TNode **expiredList = queueList2;

#elif SCHEDULER_TYPE == 1

// Ready queue and blocked queue
TPrioNode *readyQueue, *blockedQueue;

// This stores the data for pre-empted processes.
TPrioNode *suspended; // Suspended process due to pre-emption

// Currently running process
TPrioNode *currProcessNode; // Current process
#endif


#if SCHEDULER_TYPE == 0

// Searches the active list for the next priority level with processes
int findNextPrio(int currPrio)
{
	int i;

	for(i=0; i<PRIO_LEVELS; i++)
		if(activeList[i] != NULL)
			return i;

	return -1;

}

int linuxScheduler()
{
	//select current process
	int processNumber = currProcess;

	//if the process has no time left change the current process
	if(processes[processNumber].timeLeft == 0){

		//reinitialize timeLeft
		processes[processNumber].timeLeft = processes[processNumber].quantum;

		//insert process in the expireList
		insert(&expiredList[processes[processNumber].prio],processNumber,processes[processNumber].quantum);

		//find next prio
		int prio = findNextPrio(0);

		//if the active list is empty, we swap active list and expired list
		if(prio == -1){

			//swap active and expired list
			TNode** temp = activeList;
			activeList = expiredList;
			expiredList = temp;

			//we find the new priority
			prio = findNextPrio(0);

			//if the list is still empty then return an error because there is no process to run
			if(prio == -1){
				return -1;
			}
		}

		//select new process to run
		processNumber = remove(&activeList[prio]);
	}

	//decrement time left
	processes[processNumber].timeLeft--;

	//return current process
	return processNumber;
}
#elif SCHEDULER_TYPE == 1

int RMSScheduler()
{	
	/*
	 * Check the blockedQueue for any ready process
	 * then move all the ready processes to the readyQueue
	 */
	TPrioNode *readyNode = checkReady(blockedQueue, timerTick);

	while (readyNode!=NULL){
		prioRemoveNode(&blockedQueue, readyNode);
		prioInsertNode(&readyQueue, readyNode);
		readyNode = checkReady(blockedQueue, timerTick);
	}

	/*
	 * check the head node of the readyQueue:
	 * i.e: the process at the head of the readyQueue
	 */
	TPrioNode *node = peek(readyQueue);

	// If the current process has finished running for this quantum
	if (currProcessNode != NULL && processes[currProcessNode->procNum].timeLeft == 0){
		
		// Update the process deadline and reset the timeLeft
		int prev_deadline = processes[currProcessNode->procNum].deadline;
		processes[currProcessNode->procNum].deadline += processes[currProcessNode->procNum].p;
		processes[currProcessNode->procNum].timeLeft = processes[currProcessNode->procNum].c;
		
		/*
		 * If the process couldn't finish before its deadline
		 * 		Insert the process in the readyQueue
		 * Else (if the process finished before its deadline)
		 * 		Insert it in the blocked queue. So it will block until the next period
		 */
		if(timerTick >= prev_deadline){
			prioInsertNode(&readyQueue, currProcessNode);
		}
		else{
			prioInsertNode(&blockedQueue, currProcessNode);
		}

		/*
		 * if there is no suspended process, get the head process of the 
		 * readyQueue to be the current process
		 */
		if (suspended==NULL){
			currProcessNode = prioRemove(&readyQueue);
			
			//If there is no ready process left return -1
			if (currProcessNode == NULL){
				return -1;
			}
		}
		/*
		 * else, if there is a suspended process with higher priority 
		 * than the head ready process:
		 * 		 we chose to run the suspended process
		 */
		else if (node == NULL || (node!=NULL && suspended->p < node->p)){
			currProcessNode = suspended;
			suspended = NULL;
		} 
		/*
		 * if the head ready process has higher priority than the suspended
		 * process:
		 * 		we chose to run the head ready process
		 */
		else if (node!= NULL && suspended->p > node->p){
			currProcessNode = prioRemove(&readyQueue);
		}
	}
	
	/*
	 * if the head ready node has higher priority than the current 
	 * running process:
	 * 		we suspend the running process and run the head ready process
	 */
	else if (node != NULL && (currProcessNode == NULL || node->p < currProcessNode->p)){
		
		//if there is already a suspended process:
		//		add it to the readyQueue
		if(suspended != NULL){
			prioInsertNode(&readyQueue,suspended);
		}
		
		suspended = currProcessNode;
		currProcessNode = prioRemove(&readyQueue);
	} 
	/*
	 * If there is no process to run left return -1
	 */
	else if (currProcessNode == NULL && node == NULL && suspended == NULL){
		return -1;
	}

	//decrement timeLeft for the running process
	processes[currProcessNode->procNum].timeLeft --;

	return currProcessNode->procNum;
}

#endif

void timerISR()
{

#if SCHEDULER_TYPE == 0
	currProcess = linuxScheduler();
#elif SCHEDULER_TYPE == 1
	currProcess = RMSScheduler();
#endif

#if SCHEDULER_TYPE == 0
	static int prevProcess=-1;

	// To avoid repetitiveness for hundreds of cycles, we will only print when there's
	// a change of processes
	if(currProcess != prevProcess)
	{

		// Print process details for LINUX scheduler
		printf("Time: %d Process: %d Prio Level: %d Quantum : %d\n", timerTick, processes[currProcess].procNum+1,
			processes[currProcess].prio, processes[currProcess].quantum);
		prevProcess=currProcess;
	}
#elif SCHEDULER_TYPE == 1

	// Print process details for RMS scheduler

	printf("Time: %d ", timerTick);
	if(currProcess == -1)
		printf("---\n");
	else
	{
		// If we have busted a processe's deadline, print !! first
		int bustedDeadline = (timerTick >= processes[currProcess].deadline);

		if(bustedDeadline)
			printf("!! ");

		printf("P%d Deadline: %d", currProcess+1, processes[currProcess].deadline);

		if(bustedDeadline)
			printf(" !!\n");
		else
			printf("\n");
	}

#endif

	// Increment timerTick. You will use this for scheduling decisions.
	timerTick++;
}

void startTimer()
{
	// In an actual OS this would make hardware calls to set up a timer
	// ISR, start an actual physical timer, etc. Here we will simulate a timer
	// by calling timerISR every millisecond

	int i;

#if SCHEDULER_TYPE==0
	int total = processes[currProcess].quantum;

	for(i=0; i<PRIO_LEVELS; i++)
	{
		total += totalQuantum(activeList[i]);
	}

	for(i=0; i<NUM_RUNS * total; i++)
	{
		timerISR();
		usleep(1000);
	}
#elif SCHEDULER_TYPE==1

	// Find LCM of all periods
	int lcm = prioLCM(readyQueue);

	for(i=0; i<NUM_RUNS*lcm; i++)
	{
		timerISR();
		usleep(1000);
	}
#endif
}

void startOS()
{
#if SCHEDULER_TYPE == 0
	// There must be at least one process in the activeList
	currPrio = findNextPrio(0);

	if(currPrio < 0)
	{
		printf("ERROR: There are no processes to run!\n");
		return;
	}

	// set the first process
	currProcess = remove(&activeList[currPrio]);

#elif SCHEDULER_TYPE == 1
	currProcessNode = prioRemove(&readyQueue);
	currProcess = currProcessNode->procNum;
#endif

	// Start the timer
	startTimer();

#if SCHEDULER_TYPE == 0
	int i;

	for(i=0; i<PRIO_LEVELS; i++)
		destroy(&activeList[i]);

#elif SCHEDULER_TYPE == 1
	prioDestroy(&readyQueue);
	prioDestroy(&blockedQueue);

	if(suspended != NULL)
		free(suspended);
#endif
}

void initOS()
{
	// Initialize all variables
	procCount=0;
	timerTick=0;
	currProcess = 0;
	currPrio = 0;
#if SCHEDULER_TYPE == 0
	int i;

	// Set both queue lists to NULL
	for(i=0; i<PRIO_LEVELS; i++)
	{
		queueList1[i]=NULL;
		queueList2[i]=NULL;
	}
#elif SCHEDULER_TYPE == 1

	// Set readyQueue and blockedQueue to NULL
	readyQueue=NULL;
	blockedQueue=NULL;

	// The suspended variable is used to store
	// which process was pre-empted.
	suspended = NULL;
#endif

}

#if SCHEDULER_TYPE == 0

// Returns the quantum in ms for a particular priority level.
int findQuantum(int priority)
{
	return ((PRIO_LEVELS - 1) - priority) * QUANTUM_STEP + QUANTUM_MIN;
}

// Adds a process to the process table
int addProcess(int priority)
{
	if(procCount >= NUM_PROCESSES)
		return -1;

	// Insert process data into the process table
	processes[procCount].procNum = procCount;
	processes[procCount].prio = priority;
	processes[procCount].quantum = findQuantum(priority);
	processes[procCount].timeLeft = processes[procCount].quantum;

	// Add to the active list
	insert(&activeList[priority], processes[procCount].procNum, processes[procCount].quantum);


	procCount++;
	return 0;
}
#elif SCHEDULER_TYPE == 1

// Adds a process to the process table
int addProcess(int p, int c)
{
	if(procCount >= NUM_PROCESSES)
		return -1;

		// Insert process data into the process table
		processes[procCount].p = p;
		processes[procCount].c = c;
		processes[procCount].timeLeft=c;
		processes[procCount].deadline = p;

		// And add to the ready queue.
		prioInsert(&readyQueue, procCount, p, p);
	procCount++;
	return 0;
}


#endif
