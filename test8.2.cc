#include <iostream> 
#include "vm_app.h"
   
using namespace std;
//tests syslog's ability to read messages that span two pages 
//where first page is resident and second page is not resident   
int main() {

	char *a;
	char *b;
	char *c;

	a = (char *)vm_extend();
	b = (char *)vm_extend();
	c = (char *)vm_extend();

	int i = 0;
	while (i < 8191 ) {
		a [i] = 'x';
		i++;
	}
	a [8191] = 'e';
	b [0] = 's';
	c [0] = 'c';

	//b is first in clock queue, kick b out
	a [8190] = 'y';
	
	//now read a (tests read fault in syslog)
	vm_syslog(a + 8190, 3);
	vm_syslog(b, 1);
}
