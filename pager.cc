//version 1
//clear pages when reassigning

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

vector<unsigned int> free_pages;

static pid_t curr_pid;

typedef struct {
	unsigned int reference: 1;
	unsigned int modify: 1;
	unsigned int resident: 1;
	unsigned int zero_fill: 1;
	int block_num;
	int entry_num;
	pid_t owner;
} vpage;

//Process struct to create an object that holds all variables needed for a 
//single process
typedef struct {
		page_table_t page_table;
		vector<vpage *> vpage_vect;
		int num_vpages;
} process;

//pointer to keep track of the current process
static process *curr_process;

//queue of pairs, each pair is a ppage and the corresponding vpage it currently holds 
queue< pair<unsigned int, vpage *> > clock_queue;


//array of pairs, each block's number corresponds to its index in the array. First bool 
//indicates whether block is free, second bool represents if block should be zero-filled 
static bool *blocks_array;


//ints to keep track of how many are being used and how many there are in total
int free_blocks;
int total_blocks;
int pages_per_process = VM_ARENA_SIZE/VM_PAGESIZE;

map<pid_t, process *> process_map;


int second_chance() {
	
	//cerr << "CLOCK\n";

	bool evicted = false;
	unsigned int evicted_page;
	pair<unsigned int, vpage *> next_page;
	while (!evicted) {
		next_page = clock_queue.front();
		clock_queue.pop();


		if(next_page.second->reference == 1) {

			next_page.second->reference = 0;
			process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].read_enable = 0;
			process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].write_enable = 0;

			clock_queue.push(next_page);

		}

		else {

			evicted_page = next_page.first;
			evicted = true;
			next_page.second->resident = 0;

			if (next_page.second->modify == 1) {

				next_page.second->modify = 0;
				disk_write(next_page.second->block_num, next_page.first);
			}

		}

	}
	

	//TODO: want to go to the page table entry associated with this ppage,
	//set the read and write bits to 0
	//this gets the process that was using the ppage that was just evicted
 	process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].read_enable = 0;
	process_map[next_page.second->owner]->page_table.ptes[next_page.second->entry_num].write_enable = 0;


	return evicted_page;

}


void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {

	for (unsigned int i = 0; i < memory_pages; i++) {
		free_pages.push_back(i);
	}	

	//make the blocks_array global, each index represents a block, each bool value
	//represents whether that block is used
	blocks_array = new bool [disk_blocks];

	for (int i = 0; i < (int) disk_blocks; i++) {
		blocks_array[i] = false;
	}


	free_blocks = disk_blocks;
	total_blocks = disk_blocks;

}

void vm_create(pid_t pid) {

	process *my_process = new process;

	process_map.insert(pair<pid_t, process*> (pid, my_process));


}

void vm_switch(pid_t pid){

	curr_pid = pid;

	page_table_base_register = &process_map[curr_pid]->page_table;	

	curr_process = process_map[curr_pid];

}

int vm_fault(void *addr, bool write_flag){

	page_table_base_register = &curr_process->page_table;	

	if ((intptr_t) addr < (intptr_t) VM_ARENA_BASEADDR || (intptr_t) addr >= 
		(intptr_t) VM_ARENA_BASEADDR + (int) (VM_PAGESIZE * curr_process->num_vpages)) {

		return -1;
	}


	//find the virtual page number of this virtual address
	int page_num = ((intptr_t) addr - (intptr_t) VM_ARENA_BASEADDR)/VM_PAGESIZE;



	if (curr_process->vpage_vect[page_num]->resident == 0) {
		//determine physical page to associate with vpage being written to
		int phys_num;

		//if there are free physical pages, no need to do second chance clock algorithm
		if(!free_pages.empty()) {
			phys_num = free_pages[0];
			free_pages.erase(free_pages.begin());
		}
		//if there are no free pages, must use second chance clock algorithm
		else {
			phys_num = second_chance();
		}
	
		//FIXME: doing memset is unecessary here if page is being used for first time,
		//NEED TO FIX THIS
		if (curr_process->vpage_vect[page_num]->zero_fill == 1) {

			memset((char *)pm_physmem + (phys_num * VM_PAGESIZE), 0, VM_PAGESIZE);

		}

		else {
			//read the vpage from its block on the disk into a physical memory page
			disk_read(curr_process->vpage_vect[page_num]->block_num, phys_num);	
		}


		//assign the physical page to the virtual page                                		
		page_table_base_register->ptes[page_num].ppage = phys_num;	

		curr_process->vpage_vect[page_num]->resident = 1;

		//add new physical page to clock queue
		clock_queue.push(pair<unsigned int, vpage *> (phys_num, 
			curr_process->vpage_vect[page_num])); 

	}


	//write fault, or if page has been modified
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

void vm_destroy(){


	pair<unsigned int, vpage*> my_page;
	int queue_size = clock_queue.size();
	
	//go through queue, free ppages that are no longer being used
	for (int i = 0; i < queue_size; i++) {

		my_page = clock_queue.front();
		clock_queue.pop();
		
		//if the ppage is holding a vpage of the process being destroyed, free the ppage
		if (my_page.second->owner == curr_pid){

			//free the ppage
			free_pages.push_back(my_page.first);
			
		}
		else {

			clock_queue.push(my_page);

		}

	}
	
	//go through vpages, free block holding them
	for (int j = 0; j < curr_process->num_vpages; j++) {
		blocks_array[curr_process->vpage_vect[j]->block_num] = false;
		delete curr_process->vpage_vect[j];

		free_blocks += 1;

	}

	delete process_map[curr_pid];
	process_map.erase(curr_pid);
	
	return;

}


void * vm_extend() {
	
	//find the number of valid virtual pages in the arena
	int num_pages = curr_process->vpage_vect.size();
	int new_page_num;

	//make sure there is a block available to hold new virtual page or if the
	//arena is at capacity.
	if (free_blocks == 0 || num_pages >= pages_per_process) {
		return NULL;
	}
	else {
		new_page_num = num_pages;
	}


	//find the next free disk block, assign this virtual page to it
	int my_block = -1;
	for (int j = 0; j < total_blocks; j++) {
		if (blocks_array[j] == false) {
			my_block = j;
			blocks_array[j] = true;
			free_blocks -= 1;
			break;
		}
	}

	if (my_block == -1) {
		return NULL;
	}

	vpage *new_vpage = new vpage;
	new_vpage->block_num = my_block;
	new_vpage->entry_num = new_page_num;
	new_vpage->reference = 0;
	new_vpage->modify = 0;
	new_vpage->resident = 0;
	new_vpage->zero_fill = 1;
	new_vpage->owner = curr_pid;
	
	//add this new virtual page to the process' list of vpages
	curr_process->vpage_vect.push_back(new_vpage);
	//size problem?
	curr_process->num_vpages = curr_process->vpage_vect.size();
	

	long byte_val = (intptr_t) VM_ARENA_BASEADDR + VM_PAGESIZE * new_page_num;


	//page_table_base_register->ptes[new_page_num].read_enable = 1;

	return (void *) byte_val;

}




int vm_syslog(void *message, unsigned int len){

	//FIXME: if syslog message carries over multiple frames
	
	page_table_base_register = &curr_process->page_table;

	//find the virtual address' virtual page number and offset 
	int page_num = ((intptr_t) message - (intptr_t) VM_ARENA_BASEADDR)/VM_PAGESIZE;
	int offset = ((intptr_t) message - (intptr_t) VM_ARENA_BASEADDR)%VM_PAGESIZE;


	//check if page number is valid
	if ( (intptr_t) message <  (intptr_t) VM_ARENA_BASEADDR || 
		(intptr_t) message + (int) len > (intptr_t) VM_ARENA_BASEADDR  
		+ (int)  (VM_PAGESIZE * curr_process->num_vpages) || 
		(int) len == 0) {
	
		return -1;
	}



	int phys_address = page_table_base_register->ptes[page_num].ppage*VM_PAGESIZE + offset;

	string my_message;
	bool finished = false;
	int remaining_len = len;

	while(!finished) {
		
		//cerr << "offset : " << offset << endl;
		//FIXME: check that fault is successful
		if (page_table_base_register->ptes[page_num].read_enable == 0) {
				if (vm_fault(message, false) == -1){
					return -1;
				}
				//cerr << "Successful fault! -------------------------------------------------\n";

		}
		//good
		phys_address = page_table_base_register->ptes[page_num].ppage*VM_PAGESIZE + offset;
		//cerr << "NEW PHYS ADDR: " << phys_address << " of vpage " << page_num << " at ppage " << page_table_base_register->ptes[page_num].ppage << endl;


		//add characters to string while they are less than len or on the current page
		int j = 0;
		//first condition is good, second condition is good
		while (j <  remaining_len && phys_address + j < (phys_address - offset) + VM_PAGESIZE) {
			my_message.push_back(((char *)pm_physmem)[phys_address + j]);
		
			//cerr << " message -----> " << ((char *)pm_physmem)[phys_address + j] << " at " << phys_address + j << endl;
			j++;
		}
		


		//good
		if (j == remaining_len) {
			finished = true;
		}

		else {
			
			//good
 			remaining_len -= j;
			page_num++;
			//make sure next page is valid, good
			if (page_num >= curr_process->num_vpages) {
				return -1;
			}
		//--> ppage could be outdated, wait for fault//	phys_address = (page_table_base_register->ptes[page_num].ppage)*VM_PAGESIZE + (intptr_t) VM_ARENA_BASEADDR;
			offset = 0;
			 message =(char *)message + j;
		}

	}

	cout << "syslog \t\t\t" << my_message << endl;


	return 0;
}



