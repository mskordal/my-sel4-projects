#include <stdio.h>

void prolog(long *x, long *y, long *z);

long l = 13;
long m = 14;
long n = 15;

int main(int argc, char **argv)
{
	long a, b, c;

	a = 3;
	b = 4;
	c = 5;
	prolog(&a, &b, &c);

	printf("%ld %ld %ld\n", a, b, c);

	return 0;
}

void prolog(long *x, long *y, long *z)
{
	*x = *x + l;
	*y = *y + m;
	*z = *z + n;
	return;
}


