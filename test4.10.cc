#include <iostream>
#include "vm_app.h"
   
using namespace std;

//tests writing over page limits   
int main() {
   
	//create 2 pages
      	char *p;
      	char *q;
	
 	p = (char*) vm_extend();
 	q = (char*) vm_extend();

	//go to last addresses in page, write message that exceeds page limit
	p =  p + 8190;
	p [0] = 'h';
	p [1] = 'i';
	p [2] = ' ';
	p [3] = 't';
	p [4] = 'h';
	p [5] = 'e';
	p [6] = 'r';
	p [7] = 'e';
	//this should store the first characters in the message in p and the last
	//characters in q


	//print out message using both p and q
	//(q prints out only second half of message)
	vm_syslog(p, 8);
	vm_syslog(q, 6);

	//rewrite over the part of the message stored in q
	q[0] = 'c';
	q[1] = 'l';
	q[2] = 'e';
	q[3] = 'a';
	q[4] = 'r';
	
	//check that rewrite was successful
	vm_syslog(q, 5);

	//p is now the second to last index of q
	p = q + 8190;

	//write message that goes over bounds of q onto next page
	//--> since next page is invalid, syslog should return -1
	p[0] = 't';
	p[1] = 'e';
	p[2] = 's';
	p[3] = 't';

	vm_syslog(p, 4);
}

