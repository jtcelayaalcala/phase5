extern void mbox_create(USLOSS_Sysargs *args_ptr);
extern void mbox_release(USLOSS_Sysargs *args_ptr);
extern void mbox_send(USLOSS_Sysargs *args_ptr);
extern void mbox_receive(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive(USLOSS_Sysargs *args_ptr);

extern int Mbox_Create(int numslots, int slotsize, int *mboxID);
extern int Mbox_Release(int mboxID);
extern int Mbox_Send(int mboxID, void *msgPtr, int msgSize);
extern int Mbox_Receive(int mboxID, void *msgPtr, int msgSize);
extern int Mbox_CondSend(int mboxID, void *msgPtr, int msgSize);
extern int Mbox_CondReceive(int mboxID, void *msgPtr, int msgSize);

extern int  Spawn(char *name, int (*func)(char *), char *arg, int stack_size,
                  int priority, int *pid);
extern int  Wait(int *pid, int *status);
extern void Terminate(int status);
extern void GetTimeofDay(int *tod);
extern void CPUTime(int *cpu);
extern void GetPID(int *pid);
extern int  SemCreate(int value, int *semaphore);
extern int  SemP(int semaphore);
extern int  SemV(int semaphore);

typedef struct frame {
    void* addr;
    int state;
    int inUse;
} frame;