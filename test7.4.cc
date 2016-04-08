#include <iostream> 
#include "vm_app.h"
   
using namespace std;

//tests syslog's ability to handle requests outside of arena/valid pages   
int main() {

	char *a;
	
	a = (char *) vm_extend();

	int i = 0;
	while ( i < 8192) {
		a[i] = 'a';
		i++;
	}

	a[4] = 'x';
	a[8191] = 'y';

	vm_syslog(a-1,2);
	vm_syslog(a+3, 3);
	vm_syslog(a+8190, 4);
}
