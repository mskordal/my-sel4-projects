static void D() { }
static void Y() { D(); }
static void X() { Y(); }

/**
 *	INDiCES
 *	===============
 *	0: Ctrl register
 *	3: Function ID
 *	5: BRAM address
 *
 *	ctrl_reg mask: 29F
 *
 */


static void C()
{ 
	int *hls_base = (int*)0x7000000;
	int curr_ctrl_sigs;

	while( !( ( curr_ctrl_sigs = hls_base[0] ) & 4 ) );
	
	hls_base[4] = 1;
	hls_base[6] = 0xa0010000;
	hls_base[0] = curr_ctrl_sigs | 1;

	D();

	while( !( ( curr_ctrl_sigs = hls_base[0] ) & 4 ) );
	
	hls_base[4] = 0;
	hls_base[6] = 0xa0010000;
	hls_base[0] = curr_ctrl_sigs | 1;

	X();

	while( !( ( curr_ctrl_sigs = hls_base[0] ) & 4 ) );
	
	hls_base[4] = 0;
	hls_base[6] = 0xa0010000;
	hls_base[0] = curr_ctrl_sigs | 1;
}

static void B() { 
		int *hls_base = (int*)0x7000000;
	int curr_ctrl_sigs;

	while( !( ( curr_ctrl_sigs = hls_base[0] ) & 4 ) );
	
	hls_base[4] = 1;
	hls_base[6] = 0xa0010000;
	hls_base[0] = curr_ctrl_sigs | 1;
C(); }
static void S() { D(); }
static void P() { S(); }
static void O() { P(); }
static void N() { O(); }
static void M() { N(); }
static void G() { M(); }
static void A() { B(); G(); }

int main() {
  A();
}
