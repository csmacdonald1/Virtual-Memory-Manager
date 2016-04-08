/*
 * vm_pager.h
 *
 * Header file for the external pager
 */

#ifndef _VM_PAGER_H_
#define _VM_PAGER_H_

#include <sys/types.h>

/*
 * ****************************************************
 * * Interface for student portion of Project 2 pager *
 * ****************************************************
 */

/*
 * vm_init
 *
 * Called when the pager starts.  It should set up any internal data structures
 * needed by the pager, e.g. physical page bookkeeping, process table, disk
 * usage table, etc.
 *
 * vm_init is passed both the number of physical memory pages and the number
 * of disk blocks in the raw disk.
 */
extern void vm_init(unsigned int memory_pages, unsigned int disk_blocks);

/*
 * vm_create
 *
 * Called when a new process, with process identifier "pid", is added to the
 * system.  It should create whatever new elements are required for each of
 * your data structures.  The new process will only run when it's switched
 * to via vm_switch().
 */
extern void vm_create(pid_t pid);

/*
 * vm_switch
 *
 * Called when the kernel is switching to a new process, with process
 * identifier "pid".  This allows the pager to do any bookkeeping needed to
 * register the new process.
 */
extern void vm_switch(pid_t pid);

/*
 * vm_fault
 *
 * Called when current process has a fault at virtual address addr.  write_flag
 * is true if the access that caused the fault is a write.
 * Should return 0 on success, -1 on failure.
 */
extern int vm_fault(void *addr, bool write_flag);

/*
 * vm_destroy
 *
 * Called when current process exits.  It should deallocate all resources
 * held by the current process (page table, physical pages, disk blocks, etc.)
 */
extern void vm_destroy();

/*
 * vm_extend
 *
 * A request by current process to declare as valid the lowest invalid virtual
 * page in the arena.  It should return the lowest-numbered byte of the new
 * valid virtual page.  E.g., if the valid part of the arena before calling
 * vm_extend is 0x60000000-0x60003fff, the return value will be 0x60004000,
 * and the resulting valid part of the arena will be 0x60000000-0x60005fff.
 * vm_extend should return NULL on error, e.g., if the disk is out of swap
 * space.
 */
extern void * vm_extend();

/*
 * vm_syslog
 *
 * A request by current process to log a message that is stored in the process'
 * arena at address "message" and is of length "len".
 *
 * Should return 0 on success, -1 on failure.
 */
extern int vm_syslog(void *message, unsigned int len);


/*
 * *********************************************
 * * Public interface for the disk abstraction *
 * *********************************************
 *
 * Disk blocks are numbered from 0 to (disk_blocks-1), where disk_blocks
 * is the parameter passed in vm_init().
 */

/*
 * disk_read
 *
 * read block "block" from the disk into physical memory page "ppage".
 */
extern void disk_read(unsigned int block, unsigned int ppage);

/*
 * disk_write
 *
 * write the contents of physical memory page "ppage" onto disk block "block".
 */
extern void disk_write(unsigned int block, unsigned int ppage);

/*
 * ********************************************************
 * * Public interface for the physical memory abstraction *
 * ********************************************************
 *
 * Physical memory pages are numbered from 0 to (memory_pages-1), where
 * memory_pages is the parameter passed in vm_init().
 *
 * Your pager accesses the data in physical memory through the variable
 * pm_physmem, e.g. ((char *)pm_physmem)[5] is byte 5 in physical memory.
 */
extern void * pm_physmem;

/*
 * ***********************
 * * Definition of arena *
 * ***********************
 */

/* pagesize for the machine */
#define VM_PAGESIZE 8192

/* virtual address at which application's arena starts */
#define VM_ARENA_BASEADDR    ((void *) 0x60000000)

/* virtual page number at which application's arena starts */
#define VM_ARENA_BASEPAGE    ((uintptr_t) VM_ARENA_BASEADDR / VM_PAGESIZE)

/* size (in bytes) of arena */
#define VM_ARENA_SIZE    0x20000000

/*
 * **************************************
 * * Definition of page table structure *
 * **************************************
 */

/*
 * Format of page table entry.
 *
 * read_enable=0 ==> loads to this virtual page will fault
 * write_enable=0 ==> stores to this virtual page will fault
 * ppage refers to the physical page for this virtual page (unused if
 * both read_enable and write_enable are 0)
 */
typedef struct {
    unsigned long ppage : 51;		/* bit 0-50 */
    unsigned int read_enable : 1;	/* bit 51 */
    unsigned int write_enable : 1;	/* bit 52 */
} page_table_entry_t;

/*
 * Format of page table.  Entries start at virtual page VM_ARENA_BASEPAGE,
 * i.e. ptes[0] is the page table entry for virtual page VM_ARENA_BASEPAGE.
 */
typedef struct {
    page_table_entry_t ptes[VM_ARENA_SIZE/VM_PAGESIZE];
} page_table_t;

/*
 * MMU's page table base register.  This variable is defined by the
 * infrastructure, but it is controlled completely by the student's pager code.
 */
extern page_table_t *page_table_base_register;

#endif /* _VM_PAGER_H_ */
