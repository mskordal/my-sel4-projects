#define REPEAT 10000000  //9999999999

#define VARS unsigned long i,j;

#define CALC(x) for(i=0; i<REPEAT/x; i++) {++j;}

static void D() {VARS CALC(1)}
static void Y() {VARS CALC(1) D(); CALC(3)}
static void X() {VARS CALC(1) Y(); CALC(1)}

static void C() {VARS CALC(3) D(); CALC(3) X(); CALC(9)}
static void B() {VARS CALC(5) C(); CALC(7) }
static void S() {VARS CALC(2) D(); CALC(9) }
static void P() {VARS CALC(8) S(); CALC(3) }
static void O() {VARS CALC(3) P(); CALC(1) }
static void N() {VARS CALC(2) O(); CALC(6) }
static void M() {VARS CALC(7) N(); CALC(8) }
static void G() {VARS CALC(3) M(); CALC(4) }
static void A() {VARS CALC(7) B(); CALC(2) G(); CALC(6)}
int main(void)
{
  A();
}