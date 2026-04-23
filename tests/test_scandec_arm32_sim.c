/*
 * test_scandec_arm32_sim.c — Simulate ARM32 struct layout on x86-64.
 *
 * On ARM32: DWORD = unsigned long = 4 bytes, SCANDEC_OPEN = 44 bytes.
 * On x86-64: DWORD = unsigned long = 8 bytes, SCANDEC_OPEN = 64 bytes.
 *
 * Our ScanDecOpen builds a uint64_t[8] buffer from the SCANDEC_OPEN fields.
 * This test manually builds the 44-byte ARM layout and calls ScanDecOpen
 * through a wrapper that converts, to verify the conversion is correct.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

/* ARM32 struct layout: all fields are 4 bytes (int or unsigned long=4) */
#pragma pack(push, 1)
typedef struct {
	int32_t  nInResoX;         /* 0 */
	int32_t  nInResoY;         /* 4 */
	int32_t  nOutResoX;        /* 8 */
	int32_t  nOutResoY;        /* 12 */
	int32_t  nColorType;       /* 16 */
	uint32_t dwInLinePixCnt;   /* 20 */
	int32_t  nOutDataKind;     /* 24 */
	int32_t  bLongBoundary;    /* 28 */
	uint32_t dwOutLinePixCnt;  /* 32 (output) */
	uint32_t dwOutLineByte;    /* 36 (output) */
	uint32_t dwOutWriteMaxSize;/* 40 (output) */
} SCANDEC_OPEN_ARM32;  /* 44 bytes */
#pragma pack(pop)

/* Our ScanDecOpen builds uint64_t[8] from the struct:
 *   [0] = nInResoX(4) + nInResoY(4)
 *   [1] = nOutResoX(4) + nOutResoY(4)
 *   [2] = nColorType(4) + padding(4)
 *   [3] = dwInLinePixCnt(8)
 *   [4] = nOutDataKind(4) + bLongBoundary(4)
 *   [5] = dwOutLinePixCnt(8) (output)
 *   [6] = dwOutLineByte(8)   (output)
 *   [7] = dwOutWriteMaxSize(8) (output)
 *
 * Let's verify this conversion matches what ChangeResoInit expects.
 */

typedef unsigned long DWORD;
typedef int BOOL, INT;

/* Native x86-64 struct for calling the library */
typedef struct {
	INT    nInResoX;
	INT    nInResoY;
	INT    nOutResoX;
	INT    nOutResoY;
	INT    nColorType;
	DWORD  dwInLinePixCnt;
	INT    nOutDataKind;
	BOOL   bLongBoundary;
	DWORD  dwOutLinePixCnt;
	DWORD  dwOutLineByte;
	DWORD  dwOutWriteMaxSize;
} SCANDEC_OPEN_NATIVE;

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <libbrscandec.so>\n", argv[0]);
		return 1;
	}

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

	BOOL  (*Open)(SCANDEC_OPEN_NATIVE *) = dlsym(h, "ScanDecOpen");
	BOOL  (*Close)(void) = dlsym(h, "ScanDecClose");

	printf("sizeof(SCANDEC_OPEN_ARM32) = %zu (should be 44)\n", sizeof(SCANDEC_OPEN_ARM32));
	printf("sizeof(SCANDEC_OPEN_NATIVE) = %zu (should be 64 on x86-64)\n", sizeof(SCANDEC_OPEN_NATIVE));

	/* Test 1: Call with native struct (should work) */
	printf("\n=== Test 1: Native x86-64 struct ===\n");
	{
		SCANDEC_OPEN_NATIVE info = {0};
		info.nInResoX = 200; info.nInResoY = 200;
		info.nOutResoX = 200; info.nOutResoY = 200;
		info.nColorType = 0x208;
		info.dwInLinePixCnt = 80;
		info.nOutDataKind = 1;
		info.bLongBoundary = 0;

		BOOL r = Open(&info);
		printf("Open=%d pixCnt=%lu byteCnt=%lu maxSize=%lu\n",
		       r, (unsigned long)info.dwOutLinePixCnt,
		       (unsigned long)info.dwOutLineByte,
		       (unsigned long)info.dwOutWriteMaxSize);
		if (r) Close();
	}

	/* Test 2: Manually build the uint64_t[8] buffer that ScanDecOpen
	 * creates on ARM, and call ChangeResoInit directly to see the output */
	printf("\n=== Test 2: Simulate ARM32 → uint64_t[8] conversion ===\n");
	{
		SCANDEC_OPEN_ARM32 arm = {0};
		arm.nInResoX = 200; arm.nInResoY = 200;
		arm.nOutResoX = 200; arm.nOutResoY = 200;
		arm.nColorType = 0x208;
		arm.dwInLinePixCnt = 80;
		arm.nOutDataKind = 1;
		arm.bLongBoundary = 0;

		/* Build the x86-64 layout buffer exactly as our ScanDecOpen does */
		uint64_t buf[8] = {0};
		((int*)&buf[0])[0] = arm.nInResoX;      /* 200 */
		((int*)&buf[0])[1] = arm.nInResoY;      /* 200 */
		((int*)&buf[1])[0] = arm.nOutResoX;     /* 200 */
		((int*)&buf[1])[1] = arm.nOutResoY;     /* 200 */
		((int*)&buf[2])[0] = arm.nColorType;    /* 0x208 */
		buf[3] = (uint64_t)arm.dwInLinePixCnt;  /* 80 */
		((int*)&buf[4])[0] = arm.nOutDataKind;  /* 1 */
		((int*)&buf[4])[1] = arm.bLongBoundary; /* 0 */

		printf("buf[0] = 0x%016lx (expect: 200|200 = 0x000000c8000000c8)\n", buf[0]);
		printf("buf[1] = 0x%016lx (expect: 200|200 = 0x000000c8000000c8)\n", buf[1]);
		printf("buf[2] = 0x%016lx (expect: 0x208|0 = 0x0000000000000208)\n", buf[2]);
		printf("buf[3] = 0x%016lx (expect: 80      = 0x0000000000000050)\n", buf[3]);
		printf("buf[4] = 0x%016lx (expect: 1|0     = 0x0000000000000001)\n", buf[4]);

		/* Now call ChangeResoInit through ScanDecOpen-like flow */
		/* Actually, let's just call ScanDecOpen with a native struct
		 * that has the SAME field values, and compare */

		SCANDEC_OPEN_NATIVE native = {0};
		native.nInResoX = arm.nInResoX;
		native.nInResoY = arm.nInResoY;
		native.nOutResoX = arm.nOutResoX;
		native.nOutResoY = arm.nOutResoY;
		native.nColorType = arm.nColorType;
		native.dwInLinePixCnt = arm.dwInLinePixCnt;
		native.nOutDataKind = arm.nOutDataKind;
		native.bLongBoundary = arm.bLongBoundary;

		BOOL r = Open(&native);
		printf("\nScanDecOpen with same values:\n");
		printf("Open=%d pixCnt=%lu byteCnt=%lu maxSize=%lu\n",
		       r, (unsigned long)native.dwOutLinePixCnt,
		       (unsigned long)native.dwOutLineByte,
		       (unsigned long)native.dwOutWriteMaxSize);

		/* Compare with what ARM would get from buf[5..7] */
		printf("\nARM would read from buf:\n");
		printf("dwOutLinePixCnt = %u (from buf[5])\n", (uint32_t)buf[5]);
		printf("dwOutLineByte   = %u (from buf[6])\n", (uint32_t)buf[6]);
		printf("dwOutWriteMaxSize = %u (from buf[7])\n", (uint32_t)buf[7]);
		printf("(Note: buf[5..7] are still 0 because we didn't call ChangeResoInit)\n");

		if (r) Close();
	}

	/* Test 3: Call ScanDecOpen but intercept ChangeResoInit output
	 * by checking what gets written to the struct's output fields */
	printf("\n=== Test 3: Verify output field offsets ===\n");
	{
		/* Fill the struct with a sentinel pattern to detect which bytes change */
		SCANDEC_OPEN_NATIVE info;
		memset(&info, 0xAA, sizeof(info));
		info.nInResoX = 200; info.nInResoY = 200;
		info.nOutResoX = 200; info.nOutResoY = 200;
		info.nColorType = 0x208;
		info.dwInLinePixCnt = 80;
		info.nOutDataKind = 1;
		info.bLongBoundary = 0;

		printf("Before Open: dwOutLinePixCnt at offset %zu = 0x%lx\n",
		       (char*)&info.dwOutLinePixCnt - (char*)&info,
		       (unsigned long)info.dwOutLinePixCnt);
		printf("Before Open: dwOutLineByte at offset %zu = 0x%lx\n",
		       (char*)&info.dwOutLineByte - (char*)&info,
		       (unsigned long)info.dwOutLineByte);
		printf("Before Open: dwOutWriteMaxSize at offset %zu = 0x%lx\n",
		       (char*)&info.dwOutWriteMaxSize - (char*)&info,
		       (unsigned long)info.dwOutWriteMaxSize);

		BOOL r = Open(&info);
		printf("After Open (r=%d):\n", r);
		printf("  dwOutLinePixCnt = %lu\n", (unsigned long)info.dwOutLinePixCnt);
		printf("  dwOutLineByte   = %lu\n", (unsigned long)info.dwOutLineByte);
		printf("  dwOutWriteMaxSize = %lu\n", (unsigned long)info.dwOutWriteMaxSize);

		/* What ARM would see (4 bytes each) */
		printf("\nIf these were 4-byte fields (ARM32):\n");
		uint32_t *p = (uint32_t *)&info;
		printf("  offset 32 (ARM dwOutLinePixCnt): %u\n", p[8]);
		printf("  offset 36 (ARM dwOutLineByte):   %u\n", p[9]);
		printf("  offset 40 (ARM dwOutWriteMaxSize): %u\n", p[10]);

		if (r) Close();
	}

	dlclose(h);
	return 0;
}
