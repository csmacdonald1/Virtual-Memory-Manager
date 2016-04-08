/*
 * vm_app.h
 *
 * Public routines for clients of the external pager
 */

#ifndef _VM_APP_H_
#define _VM_APP_H_

/*
 * vm_extend() -- ask for the lowest invalid virtual page in the process's
 * arena to be declared valid.  Returns the lowest-numbered byte of the new
 * valid virtual page.  E.g., if the valid part of the arena before calling
 * vm_extend is 0x60000000-0x60003fff, the return value will be 0x60004000,
 * and the resulting valid part of the arena will be 0x60000000-0x60005fff.
 * vm_extend will return NULL if the disk is out of swap space.
 */
extern void *vm_extend(void);

/* 
 * vm_syslog() -- ask external pager to log a message (message data must
 * be in address space controlled by external pager).  Logs message of length
 * len.  Returns 0 on success, -1 on failure,
 */
extern int vm_syslog(void *message, unsigned int len);

/* 
 * vm_yield() -- ask operating system to yield the CPU to another process.
 * The infrastructure's scheduler is non-preemptive, so a process runs until
 * it calls vm_yield() or exits.
 */
extern void vm_yield(void);

#define VM_PAGESIZE 8192

#endif /* _VM_APP_H_ */
