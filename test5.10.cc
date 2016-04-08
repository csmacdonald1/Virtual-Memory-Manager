#include <iostream> 
#include "vm_app.h"
   
using namespace std;
//tests combination of syslog and extensive use of clock algorithm   
int main() {

	char *a;
	char *b;

	//make first page in arena valid
	a = (char *) vm_extend();
	
	//call syslog on empty valid page 
	vm_syslog(a, 10);

	
	//fill the page 
	int i = 0;
	while (i < 8192) {
		a [i] = 'a';
		i ++;
	}

	//print out last thing written on page
	vm_syslog(a + 8191, 1);

	//make more pages so that there are more vpages than ppages
	for (int j = 0; j < 10; j++) {
		b = (char *) vm_extend();
		b [0] = 'b';
	}	

	//read part of a again (should cause a read fault)
	vm_syslog(a, 1);

	//write more to a
	a [i] = 'x';

	//print out what was just added to a
	vm_syslog(a+i, 1);

	//cause clock algorithm to run, a should not be evicted
	vm_extend();

	//read a, should cause a to have read and write privileges (because a's modify bit is 1)
	vm_syslog(a, 1);

	
}
