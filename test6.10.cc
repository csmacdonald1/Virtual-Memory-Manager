#include <iostream>
#include <unistd.h>
#include "vm_app.h"

using namespace std;

//tests system (infrastructure) generated read fault
int main() {

		char x;

		char *p;
		p = (char *) vm_extend();
		//generate read fault
		x = p[0];

		p = (char *) vm_extend();

		x = p[6000];


}
