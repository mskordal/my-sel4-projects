#define REPEAT 10000000  //9999999999

#define VARS unsigned long i,j;

#define CALC(x) for(i=0; i<REPEAT/x; i++) {++j;}


int A() {VARS CALC(6) return j;}

int B() {return A();}

int main(void)
{
  int a;
  a = B();
  return 0;
}