#include <stdio.h>


/* constants */
#define MSG_DATA 0x6161 //  arbitrary data to send

int func1(int x);
int func2(int x);
int func3(int x);

int main(int argc, char **argv)
{
	unsigned long* ptr = (unsigned long *)0x7000000;
	int x = 0;

	printf("process_2: hey hey hey\n");

	x += func1(3);

	x += func2(5);

	x += func3(6);

	printf("x: %d\n", x);
	/*while(1);*/

	return 0;
}

int func1(int x)
{
	int y = x + x;
	return y;
}

int func2(int x)
{
	int y = x + x + x;
	return y;
}

int func3(int x)
{
	int y = x * x;
	return y;
}

