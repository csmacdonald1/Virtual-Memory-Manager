#include <iostream>
#include "vm_app.h"

using namespace std;

//tests clock algorithm
int main()
{

    char *p;
    char *q;
    char *r;
    char *s;
    char *t;
	
    p = (char *) vm_extend();
    q = (char *) vm_extend();	
    r = (char *) vm_extend();
    s = (char *) vm_extend();
    t = (char *) vm_extend();
    
    //write to 5 different pages
    p[0] = 'P';
    q[0] = 'Q';
    r[0] = 'R';
    s[0] = 'S';
    t[0] = 'T';

    //read pages
    vm_syslog(t, 1);
    //reading subsequent pages should cause faults
    //because they are not resident, requires
    //clock algorithm to make space for them
    vm_syslog(p, 5);
    vm_syslog(q, 1);
    vm_syslog(r, 1);
    vm_syslog(s, 1);
    vm_syslog(t, 1);
    vm_syslog(t, 1);

}
