#include <stdio.h>
#include <sel4utils/process.h>


/* constants */
#define MSG_DATA 0x6161 //  arbitrary data to send

void func1(void);
void func2(void);
void func3(void);

int main(int argc, char **argv)
{
	int x = 0;

	// printf("process_2: hey hey hey\n");

	// func1();
	x = x + 1;

	// func2();
	x = x + 2;

	// func3();
	x = x + 3;

	// printf("x: %d\n", x);
	/*while(1);*/

	return 0;
}

void func1(void)
{
	printf("This is func1\n");
}

void func2(void)
{
	printf("This is func2\n");
}

void func3(void)
{
	printf("This is func3\n");
}

