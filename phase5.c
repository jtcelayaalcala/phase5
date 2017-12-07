/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */

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

#define debug if(debugging) USLOSS_Console

#define SWAPDISK 1

extern int  SemFree(int semaphore);

extern int start5(char *);

Process ProcTable[MAXPROC];

void* vmRegion; //global pointer to VM region
int numPagers = 0;
int* pagerIDs;
int debugging = 0; //debugging variable
int clockHand = 0; //current clock position

int faultMBox; //Mbox where pagers will look for faults to service

int vmInitialized = 0; //global to let all know if VM region has been intialized

int sectorSize = 0;
int trackSize = 0;
int diskSize = 0;

int blocks = 0;
int* freePages;

int statSem; //mutex on vmStats struct
int frameSem; //mutex on frame table

int* freeBlocks;

void* tempBuf;

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;

frame* frameTable;


static void FaultHandler(int type /* MMU_INT */, void* arg  /* Offset within VM region */);
//static void FaultHandler(int type, void *offset);

static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);

void* vmInitReal(int mappings, int pages, int frames, int pagers);
void vmDestroyReal(void);
static int Pager(char *buf);

void disableInterrupts(){
    int currentpsr;
    int newstatus;

    currentpsr = USLOSS_PsrGet();
    newstatus  = USLOSS_PsrSet(currentpsr & ~(1 << (USLOSS_PSR_CURRENT_INT-1) ) );

  if (newstatus) newstatus++;
}

void enableInterrupts(){
    int currentpsr;
    int newstatus;

    currentpsr = USLOSS_PsrGet();
    newstatus  = USLOSS_PsrSet(currentpsr | (1 << (USLOSS_PSR_CURRENT_INT-1) ) );

  if (newstatus) newstatus++;
}


/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    //intialize necessary semaphores
    SemCreate(1, &statSem);
    SemCreate(1, &frameSem); 

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    //initialize processes
     for(int i = 0;i < MAXPROC;i++){
        memset(&ProcTable[i], 0, sizeof(Process));
        Mbox_Create(0,0, &ProcTable[i].privateMbox);
     }

    enableInterrupts();

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy; 
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr) {
    CheckMode();

    debug("vmInit(): started!\n");
    int mappings = 0, pages = 0, frames = 0, pagers = 0; //arguments for vmInitReal


    /*
    extract values from Sysargs struct
    */
    debug("vmInit(): Extracting values from Sysargs struct...\n");

    mappings = (int) (long) USLOSS_SysargsPtr->arg1;
    pages = (int) (long) USLOSS_SysargsPtr->arg2;
    frames = (int) (long) USLOSS_SysargsPtr->arg3;
    pagers = (int) (long) USLOSS_SysargsPtr->arg4;

    USLOSS_SysargsPtr->arg1 = (void*) (long) vmInitReal(mappings, pages, frames, pagers); //return pointer to VM region

    USLOSS_SysargsPtr->arg4 = 0;
    debug("vmInit(): Done!\n");
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr) {
   CheckMode();

   debug("vmDestroy(): started!\n");

   vmDestroyReal();

   debug("vmDestroy(): Done!\n");
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void* vmInitReal(int mappings, int pages, int frames, int pagers) {
   int status;
   int dummy;
   char name[128], buf[10];

   debug("vmInitReal(): started!\n");


   CheckMode();

   if(mappings != pages || pagers > MAXPAGERS){ //make sure number of mappings equal to number of virtual pages
      return (void*) (long) -1;
   }

   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB); //initialize MMU

   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
      abort();
   }

   vmInitialized = 1;

   diskSizeReal(SWAPDISK, &sectorSize, &trackSize, &diskSize); //get disk Size

   tempBuf = malloc(USLOSS_MmuPageSize()); //malloc temporary buffer

   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler; //install handler

   blocks = diskSize*trackSize*sectorSize/USLOSS_MmuPageSize();

   freeBlocks = (int*) malloc(sizeof(int)*blocks); //initialize free blocks
   memset(freeBlocks, 0, sizeof(int)*blocks); //zero it out

   freePages = (int*) malloc(sizeof(int)*pages); //initialize free blocks
   memset(freePages, 0, sizeof(int)*pages); //zero it out


  /*Initialize frame table*/

    frameTable = (frame*) malloc(frames*sizeof(frame)); //frame table

    for(int i = 0;i < frames;i++){ //zero out frames
      frameTable[i].state = UNUSED;
      frameTable[i].inUse = 0;
      frameTable[i].addr = NULL;
    }


   /* 
    * Create the fault mailbox.
    */
    faultMBox = MboxCreate(0,sizeof(int)); //mailbox to wake up pager


   /*
    * Fork the pagers.
    */

    debug("vmInitReal(): Forking pagers...\n");

    pagerIDs = malloc(pagers*sizeof(int));
    numPagers = pagers;

    enableInterrupts();

    for(int i = 0; i < pagers;i++){
      sprintf(name, "Pager %d", i);
      sprintf(buf, "%d", i);

      pagerIDs[i] = fork1(name, Pager, buf, USLOSS_MIN_STACK, 2); //pager id recorded in array pagerIDs
    }

   /*
    * Zero out, then initialize, the vmStats structure
    */


   memset((char *) &vmStats, 0, sizeof(VmStats));

   vmStats.pages = pages;
   vmStats.frames = frames;
   vmStats.diskBlocks = blocks;
   vmStats.freeFrames = frames;
   vmStats.freeDiskBlocks = blocks;
   vmStats.switches = 0;       // # of context switches
   vmStats.faults = 0;         // # of page faults
   vmStats.new = 0;            // # faults caused by previously unused pages
   vmStats.pageIns = 0;        // # faults that required reading page from disk
   vmStats.pageOuts = 0;       // # faults that required writing a page to disk
   vmStats.replaced = 0; // # pages replaced; i.e., frame had a page and we

    debug("vmInitReal(): Returning!\n");

   vmRegion = USLOSS_MmuRegion(&dummy);

   debug("vmInitReal(): vmRegion is: %ld\n", vmRegion);

   //dumpProcesses();

   return vmRegion;
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void PrintStats(void) {
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches-2);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void vmDestroyReal(void) {
  int result = 0;
  int mssg = -1;

  debug("vmDestroyReal(): started!\n");

   CheckMode();

   result = USLOSS_MmuDone();
   /*
    * Kill the pagers here.
    */

    for(int i = 0;i < numPagers;i++){
      MboxSend(faultMBox, &mssg, sizeof(int));
      zap(pagerIDs[i]);
    }



   /* 
    * Print vm statistics.
    */
   PrintStats();

   debug("vmDestroyReal(): Done!\n");

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void FaultHandler(int type /* MMU_INT */, void* arg  /* Offset within VM region */) {
   int cause;
   int offset = (int) (long) arg;
   int pid = getpid();


   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);

   sempReal(statSem);
   vmStats.faults++;
   semvReal(statSem);


   debug("FaultHandler(): started!\n");

   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */

    faults[pid % MAXPROC].pid = pid;
    faults[pid % MAXPROC].addr = (void*) (long) offset;
    faults[pid % MAXPROC].replyMbox = ProcTable[pid % MAXPROC].privateMbox;

    debug("FaultHandler(): Sending message to pager...\n");
    MboxSend(faultMBox, &pid, sizeof(int));

    debug("FaultHandler(): Waiting...\n");
    MboxReceive(ProcTable[pid % MAXPROC].privateMbox, NULL, 0);

    debug("FaultHandler(): Done!\n");
} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int Pager(char *buf){
  int faultedProc;
  int page = 0;
  int pageSize = USLOSS_MmuPageSize();
  int access = 0; //access bits

  //dumpProcesses();

  int justDone = 0;

  debug("Pager(): started!\n");

  while(!isZapped()) {
      /* Wait for fault to occur (receive from mailbox) */
      debug("Pager(): waiting for fault...\n");

      MboxReceive(faultMBox, &faultedProc, sizeof(int));

      if(faultedProc < 0){
        continue;
      }

      debug("Pager(): Received fault from process %d\n", faultedProc);
      debug("Pager(): Address that caused the fault was %ld\n", (long) faults[faultedProc % MAXPROC].addr);

      page = (int) (long) faults[faultedProc % MAXPROC].addr/pageSize; //find page that caused faults

      /* Look for free frame */
      int i = 0;

      for(;i < vmStats.frames;i++){
          sempReal(frameSem);
          if(!frameTable[i].inUse){
              frameTable[i].inUse = 1;
              semvReal(frameSem);
              break;
          }
          semvReal(frameSem);
      }

      if(i >= vmStats.frames){ /* If there isn't one then use clock algorithm to replace a page (perhaps write to disk) */

          while(1){
              USLOSS_MmuGetAccess(clockHand, &access);

              debug("Pager(): Access is %d for frame %d\n", access, clockHand);

              if(access == 0 || access == USLOSS_MMU_DIRTY){ //not referenced
                  i = clockHand;
                  debug("Pager(): Clock algorithm found frame %d\n", i);


                  int k = 0;
                  int lastPage = 0;
                  for(;k < vmStats.pages;k++){
                      if(ProcTable[faultedProc % MAXPROC].pageTable[k].frame == i){
                          ProcTable[faultedProc % MAXPROC].pageTable[k].state = REPLACED;
                          ProcTable[faultedProc % MAXPROC].pageTable[k].frame = -1;

                          lastPage = k;
                          k = 100000;
                      }
                  }

                  //write to disk if dirty
                  if(access == USLOSS_MMU_DIRTY){
                      debug("Pager(): Frame %d is dirty\n", i);

                      debug("Pager(): Copying frame %d to intermediate buffer\n", i);

                      USLOSS_MmuMap(TAG, 0, i, USLOSS_MMU_PROT_RW); //temporary mapping, map page 0 to frame i

                      memcpy(tempBuf /*destiniation*/, vmRegion /*source*/, USLOSS_MmuPageSize()); //move to buffer

                      USLOSS_MmuUnmap(TAG, 0); //undo mapping


                      //look for free space in disk
                      int n = 0;
                      for(;n < blocks;n++){
                          if(!freeBlocks[n]){
                            break;
                          }
                      }

                      debug("Pager(): About to write, have that pageSize is %d, sector size is %d, track size is %d, and disk size is %d\n", pageSize, sectorSize, trackSize, diskSize);
                      debug("Pager(): So, we will write %d sectors to track %d, starting at sector %d\n", pageSize/(sectorSize), pageSize*n/(sectorSize*trackSize), pageSize*n/(sectorSize) % trackSize);

                      diskWriteReal(SWAPDISK, pageSize*n/(sectorSize*trackSize) /*track*/, pageSize*n/(sectorSize) % trackSize /*sector*/, pageSize/(sectorSize)/*num sectors*/ , tempBuf);
                      // if(ProcTable[faultedProc % MAXPROC].pageTable[page].state == UNUSED){
                      //   justDone = 1;
                      // }

                      ProcTable[faultedProc % MAXPROC].pageTable[lastPage].state = INCORE; //tell process its page is now on disk
                      ProcTable[faultedProc % MAXPROC].pageTable[lastPage].diskBlock = n; //tell it where it is

                      debug("Pager(): Incrementing pageOuts to %d\n", vmStats.pageOuts+1);

                      sempReal(statSem);
                      vmStats.pageOuts++; //increment number of pageOuts
                      semvReal(statSem);

                  }
                  break;
              }else{ //referenced
                  USLOSS_MmuSetAccess(clockHand, access-1); //set reference bit
              }

              clockHand = (clockHand+1) % vmStats.frames;
          }
      }

      debug("Pager(): Free frame found! Frame %d\n", i);

      debug("Pager(): Assigning frame %d to process %d\n", i, faultedProc);

      ProcTable[faultedProc].pageTable[page].frame = i; //set frame to be mapped to right page in process' proc table entry
      ProcTable[faultedProc].pageTable[page].inUse = 1;

      debug("Pager(): Zeroing out frame %d\n", i);

      USLOSS_MmuMap(TAG, 0, i, USLOSS_MMU_PROT_RW); //temporary mapping, map page i to frame 0

      memset(vmRegion, 0, USLOSS_MmuPageSize()); //zero out page

      USLOSS_MmuSetAccess(i, USLOSS_MMU_REF); //set reference bit to 1, i.e referenced

      USLOSS_MmuUnmap(TAG, 0); //undo mapping


      /* Load page into frame from disk, if necessary */
      if(ProcTable[faultedProc % MAXPROC].pageTable[page].state == INCORE && !justDone){

          debug("Pager(): Reading from disk...\n");

          sempReal(statSem);
          vmStats.pageIns++; //increment number of pageOuts
          semvReal(statSem);

          //read from disk
          USLOSS_MmuMap(TAG, 0, i, USLOSS_MMU_PROT_RW); //temporary mapping, map page 0 to frame i

          int n = pageSize*ProcTable[faultedProc % MAXPROC].pageTable[page].diskBlock;

          debug("Pager(): About to read, have that pageSize is %d, sector size is %d, track size is %d, and disk size is %d\n", pageSize, sectorSize, trackSize, diskSize);
          debug("Pager(): So, we will read %d sectors from track %d, starting at sector %d\n", pageSize/(sectorSize), pageSize*n/(sectorSize*trackSize), pageSize*n/(sectorSize) % trackSize);
          debug("Pager(): Address we will read into is: %ld\n", vmRegion);


          diskReadReal(SWAPDISK, pageSize*n/(sectorSize*trackSize) /*track*/, pageSize*n/(sectorSize) % trackSize /*sector*/, pageSize/(sectorSize)/*num sectors*/ , tempBuf);


          memcpy(vmRegion /*destiniation*/, tempBuf /*source*/, USLOSS_MmuPageSize()); //move to buffer

          //diskReadReal(vmRegion, SWAPDISK, pageSize*ProcTable[faultedProc % MAXPROC].pageTable[page].diskBlock/(sectorSize*trackSize), pageSize*ProcTable[faultedProc % MAXPROC].pageTable[page].diskBlock/(sectorSize), pageSize/sectorSize, NULL); //write frame to disk

          USLOSS_MmuUnmap(TAG, 0); //undo mapping

          ProcTable[faultedProc % MAXPROC].pageTable[page].state = USED;
          freeBlocks[ProcTable[faultedProc % MAXPROC].pageTable[page].diskBlock] = 0;

      }

      /* Unblock waiting (faulting) process */
      MboxSend(ProcTable[faultedProc % MAXPROC].privateMbox, NULL, 0);
  }

  debug("Pager(): Done!\n");

    return 0;
} /* Pager */


















