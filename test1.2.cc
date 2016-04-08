#include <iostream> 
#include "vm_app.h"
   
using namespace std;

//tests read fault on resident page with modify bit equal to 1 (should enable
//read AND write privileges)    
int main() {


	char *a;
	char *b;
	char *c;

	a = (char *) vm_extend();
	b = (char *) vm_extend();
	c = (char *) vm_extend();

	//write to all 3 pages
	a [0] = 'a';
	b [0] = 'b';
	c [0] = 'c';
			
	//since there are 2 ppages, page a should have been evicted by clock algorithm
	// --> page b should be first in clock queue with mod bit equal to 1
	// --> read fault on page b should cause all its bits to be set to 1, including
	//     write bit
	vm_syslog(b, 1);
}
