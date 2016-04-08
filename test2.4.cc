#include <iostream>
#include "vm_app.h"

using namespace std;

//tests writing to 2 pages, then reading from them 
int main() {


	char *p;
	char *q;

	p = (char *) vm_extend();
	q = (char *) vm_extend();

	p[0] = 'c';
	p[1] = 'h';
	p[2] = 'r';
	p[3] = 'i';
	p[4] = 's';

	q[0] = 'n';
	q[1] = 'i';
	q[2] = 'k';
	q[3] = 'h';
	q[4] = 'i';
	q[5] = 'l';

	vm_syslog(p, 5);
	vm_syslog(q, 6);




}
