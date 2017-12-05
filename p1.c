
#include <usloss.h>
#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>
#include <driver.h>
#include <providedPrototypes.h>
#define debug if(p1debugging) USLOSS_Console

int p1debugging = 0;

extern Process ProcTable[MAXPROC];
extern int vmInitialized;
extern frame* frameTable;
extern int statSem;
extern void* vmRegion;

void p1_fork(int pid)  {
    debug("p1_fork() called: pid = %d\n", pid);

    /*
    * Initialize page tables.
    */

    if(vmInitialized){ //vm Region has been initialized

	    debug("p1_fork(): Initializing page table for process %d\n", pid);



	    ProcTable[pid % MAXPROC].numPages = vmStats.pages; //number of pages set

	    ProcTable[pid % MAXPROC].pageTable = (PTE*) malloc(sizeof(PTE)*vmStats.pages); //malloc page table for each 

	    memset(ProcTable[pid % MAXPROC].pageTable, 0, sizeof(PTE)*vmStats.pages);

	    ProcTable[pid % MAXPROC].inVM = 1;

	}else{
		; //do nothing
	}

	debug("p1_fork(): Done!\n");
} /* p1_fork */

void p1_switch(int old, int new) {
    debug("p1_switch() called: old = %d, new = %d\n", old, new);

    Process *oldProc = &ProcTable[old % MAXPROC];
    Process *newProc = &ProcTable[new % MAXPROC];

    if(!oldProc || !newProc)
    	USLOSS_Halt(1);

    if(vmInitialized){
    	sempReal(statSem);
    	vmStats.switches++; //increment number of switches
    	semvReal(statSem);
    }

    if(vmInitialized && oldProc->inVM && newProc->inVM){ //vm Region has been initialized

    	debug("p1_switch(): Number of pages is: %d\n", vmStats.pages);


    	debug("p1_switch(): Unmapping...\n");
    	for(int i = 0;i < vmStats.pages;i++){ //go through and unmap all of the old mappings
    		if(oldProc->pageTable[i].inUse){
    			USLOSS_MmuUnmap(TAG, i);
    		}
    	}
		debug("p1_switch(): Mapping...\n");
    	for(int i = 0;i < vmStats.pages;i++){ //go through and map all of the new mappings
    		if(newProc->pageTable[i].inUse){

    			debug("p1_switch(): Frame held by page table entry of process %d, in slot %d: %d\n", new, i, newProc->pageTable[i].frame);

    			if(frameTable[newProc->pageTable[i].frame].state == UNUSED){ //mapping previously unused frame
    				debug("p1_switch(): Unused frame being mapped, frame %d\n", i);

    				sempReal(statSem);
    				vmStats.new++; //increment stats
    				semvReal(statSem);

    				debug("p1_switch(): vmStats is %d\n", vmStats.new);

    				frameTable[newProc->pageTable[i].frame].state = USED; //mark frame as used

    			}

    			USLOSS_MmuMap(TAG, i, newProc->pageTable[i].frame, USLOSS_MMU_PROT_RW);
    		}
    	}

	}else{
		debug("p1_switch(): VM region hasn't been initialized...doing nothing\n");
		; //do nothing
	}

	debug("p1_switch(): Done!\n");
} /* p1_switch */

void p1_quit(int pid) {
    debug("p1_quit() called: pid = %d\n", pid);

    if(vmInitialized){ //vm Region has been initialized

	    debug("p1_quit(): Freeing page table for %d\n", pid);

	    free(ProcTable[pid % MAXPROC].pageTable); //free page table

	}else{
		; //do nothing
	}
} /* p1_quit */

















