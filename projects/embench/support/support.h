/* Support header for BEEBS.

	Copyright (C) 2014 Embecosm Limited and the University of Bristol
	Copyright (C) 2019 Embecosm Limited

	Contributor James Pallister <james.pallister@bristol.ac.uk>

	Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

	This file is part of Embench and was formerly part of the Bristol/Embecosm
	Embedded Benchmark Suite.

	SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SUPPORT_H
#define SUPPORT_H

#define WARMUP_HEAT 1
#define CPU_MHZ 1

int mont64_verify (int r);
void mont64_init (void);
void mont64_warm (int heat);
int mont64_run (void);

int crc_32_verify (int r);
void crc_32_init (void);
void crc_32_warm (int heat);
int crc_32_run (void);

int basicmath_small_verify (int res __attribute ((unused)) );
void basicmath_small_init (void);
void basicmath_small_warm (int heat);
int basicmath_small_run (void);

int libedn_verify (int unused);
void libedn_init (void);
void libedn_warm (int heat);
int libedn_run (void);

int libhuffbench_verify (int res __attribute ((unused)));
void libhuffbench_init (void);
void libhuffbench_warm (int heat);
int libhuffbench_run (void);

int matmult_int_verify (int unused);
void matmult_int_init (void);
void matmult_int_warm (int heat);
int matmult_int_run (void);

int md5_verify (int r);
void md5_init (void);
void md5_warm (int heat);
int md5_run (void);

int libminver_verify (int res __attribute ((unused)));
void libminver_init (void);
void libminver_warm (int heat);
int libminver_run (void);

int nbody_verify (int tot_e_ok);
void nbody_init (void);
void nbody_warm (int heat);
int nbody_run (void);

int nettle_aes_verify (int res __attribute ((unused)));
void nettle_aes_init (void);
void nettle_aes_warm (int heat);
int nettle_aes_run (void);

int nettle_sha256_verify (int res __attribute ((unused)));
void nettle_sha256_init (void);
void nettle_sha256_warm (int heat);
int nettle_sha256_run (void);

int libsichneu_verify (int unused);
void libsichneu_init (void);
void libsichneu_warm (int heat);
int libsichneu_run (void);

int libpicojpeg_test_verify (int res __attribute ((unused)));
void libpicojpeg_test_init (void);
void libpicojpeg_test_warm (int heat);
int libpicojpeg_test_run (void);

int primecount_verify (int result);
void primecount_init (void);
void primecount_warm (int heat);
int primecount_run (void);

int qrtest_verify (int unused);
void qrtest_init (void);
void qrtest_warm (int heat);
int qrtest_run (void);

int combined_verify (int unused);
void combined_init (void);
void combined_warm (int heat);
int combined_run (void);

int libslre_verify (int unused);
void libslre_init (void);
void libslre_warm (int heat);
int libslre_run (void);

int libst_verify (int unused);
void libst_init (void);
void libst_warm (int heat);
int libst_run (void);

int libstatemate_verify (int unused);
void libstatemate_init (void);
void libstatemate_warm (int heat);
int libstatemate_run (void);

int libud_verify (int res);
void libud_init (void);
void libud_warm (int heat);
int libud_run (void);

int libwikisort_verify (int res __attribute ((unused)));
void libwikisort_init (void);
void libwikisort_warm (int heat);
int libwikisort_run (void);

int tarfind_verify (int r);
void tarfind_init (void);
void tarfind_warm (int heat);
int tarfind_run (void);

enum benchmarks
{
	mont64_i,
	crc_32_i,
	basicmath_small_i,
	libedn_i,
	libhuffbench_i,
	matmult_int_i,
	md5_i,
	libminver_i,
	nbody_i,
	nettle_aes_i,
	nettle_sha256_i,
	libnsichneu_i,
	libpicojpeg_test_i,
	primecount_i,
	qrtest_i,
	combined_i,
	libslre_i,
	libst_i,
	libstatemate_i,
	libud_i,
	libwikisort_i,
	tarfind_i,
	benchmarksNum
};
// extern const char* benchName[benchmarksNum];

typedef int (*verify_f)(int);
typedef void (*init_f)(void);
typedef void (*warm_f)(int);
typedef int (*run_f)(void);

// extern verify_f benchVerify[benchmarksNum];

// extern init_f benchInit[benchmarksNum];

// extern warm_f benchWarm[benchmarksNum];

// extern run_f benchRun[benchmarksNum];


/* Local simplified versions of library functions */

#include "beebsc.h"

#endif /* SUPPORT_H */

/*
	Local Variables:
	mode: C
	c-file-style: "gnu"
	End:
*/
