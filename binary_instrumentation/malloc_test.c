#include <stdio.h>
#include <stdlib.h>
//extern int malloc_caller(int);
int main(int argc , char *argv[])
{
	unsigned long long num;
	int i, tot_size=0;
	char *p;
	if (argc < 2) {
		printf("Pass as ./test <num>\n");
		exit(0);
	}
	num = atoi(argv[1]);
	for (i=0; i < num; i++) {
		p = malloc(0xff + 0x40*i);
		tot_size += 0xff + 0x40*i;
	}
	printf(">>TEST PROG PRINT: total calls %d tot_size %d\n",i, tot_size);
}
