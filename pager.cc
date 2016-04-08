/* 
 *==================================================
 * File 	:	pager.cc
 * Authors	:	Nikhil Dasgupta and Chris MacDonald
 *==================================================
 */
#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <utility>
#include <cstring>
#include "vm_pager.h"

using namespace std;

/*
 *=================================================================================================
 *										GLOBAL VARIABLES
 *=================================================================================================
 */


//This vector keeps track of physical pages that have no virtual page assigned to them
vector<unsigned int> free_pages;


//The vpage struct represents a virtual page. The block_num entry_num and owner variabled help
//identify the page, representing the disk block the page is associated with, its entry number
//in its page table, and the id of the process that owns it. The reference and modify bits help
//determine which page to evict in the second chance algorithm. The resident bit determines whether
//or not the page is in memory, and the zero_fill bit determines whether or not anything has been
//written to the page.
typedef struct {
	unsigned int reference: 1;
	unsigned int modify: 1;
	unsigned int resident: 1;
	unsigned int zero_fill: 1;
	int block_num;
	int entry_num;
	pid_t owner;
} vpage;

//This struct represents a process. In holds a page table object to keep track of its map from
//virtual pages to physical pages. The vpage_vect holds all the process' virtual pages, and 
//num_vpages keeps track of how many pages the process has.
typedef struct {
		page_table_t page_table;
		vector<vpage *> vpage_vect;
		int num_vpages;
} process;

//These variables keep track of the currently running process and its id.
static pid_t curr_pid;
static process *curr_process;

//The clock_queue is a queue of physical pages that currently have virtual pages written to them.
//The unsigned int represents the ppage number, and vpage pointer represents its associated
//virtual page.
queue< pair<unsigned int, vpage *> > clock_queue;

//This array keeps track of all of the disk blocks. each block's number corresponds to its index 
//in the array. The boolean value indicates whether or not the block is free.
static bool *blocks_array;


//ints to keep track of how many disk blocks are being used and how many there are in total
int free_blocks;
int total_blocks;

//This map associates processes and their ids, so we can find old processes when yielding.
map<pid_t, process *> process_map;



/*
 *=================================================================================================
 *	SECOND_CHANCE -- An algorithm that determines which physical page to evict from the queue if
 *                   the program needs a free one and all are in use. The algorithm searches 
 *					 through the queue for a page that has not been recently referenced (i.e. 
 *					 whose reference bit bit is 0) and writes out that page. If a page has
 *                	 reference bit one, set it to 0 and push it onto the back of the queue. method
 *					 returns the number of the physical page that was evicted. 
 *=================================================================================================
 */
int second_chance() {
	
	//variables to keep track of eviction state, evicted page, and current page
	bool evicted = false;
	unsigned int evicted_page;
	pair<unsigned int, vpage *> next_page;

	//continue this loop until a page to evict has been selected
	while (!evicted) {

		//retrieve page at the front of the clock_queue
		next_page = clock_queue.front();
		clock_queue.pop();

		//if reference bit is 1, reset it to 0 and reset the read and write protections. Then push
		//it onto the back of the queue
		if(next_page.second->reference == 1) {

			next_page.second->reference = 0;
			process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].read_enable = 0;
			process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].write_enable = 0;

			clock_queue.push(next_page);

		}

		//if reference bit is 0, evict this page
		else {

			evicted_page = next_page.first;
			evicted = true;
			next_page.second->resident = 0;

			//if page has been modified since it was read into memory, write it out to disk
			if (next_page.second->modify == 1) {

				next_page.second->modify = 0;
				disk_write(next_page.second->block_num, next_page.first);
			}

		}

	}
	

	//reset read and write protections for the evicted page.
 	process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].read_enable = 0;
	process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].write_enable = 0;


	return evicted_page;

}


/*
 *=================================================================================================
 *	VM_INIT -- initialize all of the global variables when the pager starts running
 *=================================================================================================
 */
void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {

	//initialize vector of free pages by pushing back appropriate number of unsigned ints
	for (unsigned int i = 0; i < memory_pages; i++) {
		free_pages.push_back(i);
	}	

	//allocate the right amount of memory to the array of disk blocks
	blocks_array = new bool [disk_blocks];

	//initialze all blocks_array booleans to false, indicating that they are all free
	for (int i = 0; i < (int) disk_blocks; i++) {
		blocks_array[i] = false;
	}

	//initialize ints to keep track of total blocks and free blocks
	free_blocks = disk_blocks;
	total_blocks = disk_blocks;

}

/*
 *=================================================================================================
 *	VM_CREATE -- adjusts appropriate global variables when a new process is created. allocate
 *				 memory for a new process struct and insert it along with its pid into the global
 *				 process map.
 *=================================================================================================
 */
void vm_create(pid_t pid) {

	process *my_process = new process;
	process_map.insert(pair<pid_t, process*> (pid, my_process));


}

/*
 *=================================================================================================
 *	VM_SWITCH -- called by infrastructure when switching processes. adjusts global variables to
 *				 keep track of current process and pid. Also adjusts page_table_base_register
 *				 (a vairiable provided by the infrastructure) to point ot the current page table
 *=================================================================================================
 */
void vm_switch(pid_t pid){

	curr_pid = pid;
	curr_process = process_map[curr_pid];
	page_table_base_register = &process_map[curr_pid]->page_table;	


}


/*
 *=================================================================================================
 *	VM_FAULT -- called whenever a read or write fault occurs in the currently running process.
 *				This occurs if the process attempts to read or write to the page but the relevant
 *				bit is set to zero.
 *=================================================================================================
 */
int vm_fault(void *addr, bool write_flag){

	//make sure page_table_base_register is pointing to the correct object
	page_table_base_register = &curr_process->page_table;	

	//check that argument address is within the process' virtual arena. return failure if not
	if ((intptr_t) addr < (intptr_t) VM_ARENA_BASEADDR || (intptr_t) addr >= 
		(intptr_t) VM_ARENA_BASEADDR + (int) (VM_PAGESIZE * curr_process->num_vpages)) {

		return -1;
	}

	//find the virtual page number of the page that was faulted on
	int page_num = ((intptr_t) addr - (intptr_t) VM_ARENA_BASEADDR)/VM_PAGESIZE;


	//execute this code if the faulted page is not resident
	if (curr_process->vpage_vect[page_num]->resident == 0) {

		//this is the ppage number that will be assigned to the faulting page
		int phys_num;

		//if there are free physical pages, no need to do second chance clock algorithm
		if(!free_pages.empty()) {
			phys_num = free_pages[0];
			free_pages.erase(free_pages.begin());
		}

		//if there are no free pages, must use second chance clock algorithm to determine
		//which page to evict.
		else {
			phys_num = second_chance();
		}
	
		//if the zero_fill bit is 1 (i.e. if the virtual page has never been written to) then
		//clear the contents of the physical page
		if (curr_process->vpage_vect[page_num]->zero_fill == 1) {

			memset((char *)pm_physmem + (phys_num * VM_PAGESIZE), 0, VM_PAGESIZE);

		}

		//otherwise, read in the required contents from the disk block associate with the
		//faulting page
		else {
			
			disk_read(curr_process->vpage_vect[page_num]->block_num, phys_num);	
		}


		//assign the physical page to the faulting virtual page                                		
		page_table_base_register->ptes[page_num].ppage = phys_num;	

		//reset the resident bit appropriately
		curr_process->vpage_vect[page_num]->resident = 1;

		//add new physical page to clock queue
		clock_queue.push(pair<unsigned int, vpage *> (phys_num, 
			curr_process->vpage_vect[page_num])); 

	}


	//if this is a write fault, or if the page has already been modified, reset the
	//write_enable, modify, and zero_fill bits appropriately. Setting modify and zero_fill
	//bits is only relevent in the write fault case.
	if(write_flag || curr_process->vpage_vect[page_num]->modify == 1) {

		page_table_base_register->ptes[page_num].write_enable = 1;
		curr_process->vpage_vect[page_num]->modify = 1;
		curr_process->vpage_vect[page_num]->zero_fill = 0;

	}


	//for any fault, grant read privilege and mark as referenced
	page_table_base_register->ptes[page_num].read_enable = 1;
	curr_process->vpage_vect[page_num]->reference = 1;
	


	return 0;
}


/*
 *=================================================================================================
 *	VM_DESTROY -- called by infrastructure when a process is finished running. Clears all global
 *				  variables and structs associated with the process.
 *=================================================================================================
 */
void vm_destroy(){

	//initialize variables to used to iterate through clock_queue
	pair<unsigned int, vpage*> my_page;
	int queue_size = clock_queue.size();
	
	//iterate through queue, free any ppages that belong to process being destroyed
	for (int i = 0; i < queue_size; i++) {

		my_page = clock_queue.front();
		clock_queue.pop();
		
		//if ppage has a virtual page belonging to this process, push it into the free_pages vector
		if (my_page.second->owner == curr_pid){

			free_pages.push_back(my_page.first);
			
		}

		//otherwise, put it back into the queue
		else {

			clock_queue.push(my_page);

		}

	}
	
	//free all of the blocks associated with this process' virtual pages
	for (int j = 0; j < curr_process->num_vpages; j++) {
		blocks_array[curr_process->vpage_vect[j]->block_num] = false;
		delete curr_process->vpage_vect[j];

		free_blocks += 1;

	}

	//clear the current process from the global process map
	delete process_map[curr_pid];
	process_map.erase(curr_pid);
	
	return;

}


/*
 *=================================================================================================
 *	VM_EXTEND -- extends the current process' virtual arena by one page, if possible. Returns the
 *				 virtual address of the beginning of the new page on success, NULL on failure.
 *=================================================================================================
 */
void * vm_extend() {
	
	//find the number of valid virtual pages in the arena
	int num_pages = curr_process->vpage_vect.size();

	//return failure if there are no free blocks to hold the virtual page, or if the arena is
	//at capacity.
	if (free_blocks == 0 || num_pages >= VM_ARENA_SIZE/VM_PAGESIZE) {
		return NULL;
	}


	//find the next free disk block, assign this new virtual page to it
	int my_block = -1;
	for (int j = 0; j < total_blocks; j++) {
		if (blocks_array[j] == false) {
			my_block = j;
			blocks_array[j] = true;
			free_blocks -= 1;
			break;
		}
	}

	//check to make sure a block has been assigned.
	if (my_block == -1) {
		return NULL;
	}

	//initialize all bit values and identifying variables for new page
	vpage *new_vpage = new vpage;
	new_vpage->block_num = my_block;
	new_vpage->entry_num = num_pages;
	new_vpage->reference = 0;
	new_vpage->modify = 0;
	new_vpage->resident = 0;
	new_vpage->zero_fill = 1;
	new_vpage->owner = curr_pid;
	
	//add this new virtual page to the process' list of vpages and adjust size appropriately
	curr_process->vpage_vect.push_back(new_vpage);
	curr_process->num_vpages = curr_process->vpage_vect.size();
	
	//return address at beginning of new virtual page
	return (void *) ((intptr_t) VM_ARENA_BASEADDR + VM_PAGESIZE * num_pages);

}



/*
 *=================================================================================================
 *	VM_SYSLOG -- extends the current process' virtual arena by one page, if possible. Returns the
 *				 virtual address of the beginning of the new page on success, NULL on failure.
 *=================================================================================================
 */
int vm_syslog(void *message, unsigned int len){


	//make sure page_table_base_register is pointing to the correct object	
	page_table_base_register = &curr_process->page_table;

	//find the virtual address' virtual page number and offset 
	int page_num = ((intptr_t) message - (intptr_t) VM_ARENA_BASEADDR)/VM_PAGESIZE;
	int offset = ((intptr_t) message - (intptr_t) VM_ARENA_BASEADDR)%VM_PAGESIZE;


	//check that the entire message is inside the arena
	if ( (intptr_t) message <  (intptr_t) VM_ARENA_BASEADDR || 
		(intptr_t) message + (int) len > (intptr_t) VM_ARENA_BASEADDR  
		+ (int)  (VM_PAGESIZE * curr_process->num_vpages) || 
		(int) len == 0) {
	
		return -1;
	}


	//translate the argument virtual address to a physical address
	int phys_address = page_table_base_register->ptes[page_num].ppage*VM_PAGESIZE + offset;

	//declare variables to use for message iteration
	string my_message;
	bool finished = false;
	int remaining_len = len;

	//repeat the code as long the message is not entirely entered into the string
	while(!finished) {
		
		//if the current page is not read enabled, fault
		if (page_table_base_register->ptes[page_num].read_enable == 0) {

				//if this fault fails, then syslog fails
				if (vm_fault(message, false) == -1){
					return -1;
				}
		}

		//reset physical address value
		phys_address = page_table_base_register->ptes[page_num].ppage*VM_PAGESIZE + offset;


		//add characters to string while they are less than len and on the current page
		int j = 0;
		while (j <  remaining_len && phys_address + j < (phys_address - offset) + VM_PAGESIZE) {
			my_message.push_back(((char *)pm_physmem)[phys_address + j]);

			j++;
		}
		
		//if the entire message has been entered into the string, we are finished
		if (j == remaining_len) {
			finished = true;
		}

		//otherwise, we have reached the end of the page and must move to the next one
		else {
			
			//iterate remaining length
 			remaining_len -= j;
			page_num++;

			//make sure next page is valid
			if (page_num >= curr_process->num_vpages) {
				return -1;
			}

		    //set offset to zero, iterate 'message'
			offset = 0;
			 message =(char *)message + j;
		}

	}

	//print out string
	cout << "syslog \t\t\t" << my_message << endl;


	return 0;
}



