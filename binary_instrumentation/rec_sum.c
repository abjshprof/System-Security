#include <stdio.h>
#include <stdlib.h>
unsigned long long sum(unsigned long long num) {
	if (num < 0)
		return -1;
	if (num == 0)
		return 0;
	return num + sum(num-1);
}

int main(int argc , char *argv[])
{
	unsigned long long num;
	if (argc < 2) {
		printf("Pass as ./rec_sum <num>\n");
		exit(0);
	}
	num = atoi(argv[1]);
	printf("TEST_PROG_PRINTS: Sum upto  %llu is %llu\n", num, sum(num));

}
