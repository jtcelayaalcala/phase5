/*
 * Function prototypes from Patrick's phase3 solution. These can be called
 * when in *kernel* mode to get access to phase3 functionality.
 */


#ifndef PROVIDED_PROTOTYPES_H

#define PROVIDED_PROTOTYPES_H

extern int  spawnReal(char *name, int (*func)(char *), char *arg,
                       int stack_size, int priority);
extern int  waitReal(int *status);
extern void terminateReal(int exit_code);
extern int  semcreateReal(int init_value);
extern int  sempReal(int semaphore);
extern int  semvReal(int semaphore);
extern int  semfreeReal(int semaphore);
extern int  gettimeofdayReal(int *time);
extern int  cputimeReal(int *time);
extern int  getPID_real(int *pid);

extern int sleepReal(int seconds);
extern int diskReadReal(void *diskBuffer, int unit, int track, int first, int sectors, int *status);
extern int diskWriteReal(void *diskBuffer, int unit, int track, int first, int sectors, int *status);
extern int diskSizeReal(int unit, int *sector, int *track, int *disk);
extern int termreadReal(char *buffer, int bufferSize, int unitID, int *numCharsRead);
extern int termwriteReal(char *buffer, int bufferSize, int unitID, int *numCharsRead);

#endif  /* PROVIDED_PROTOTYPES_H */
