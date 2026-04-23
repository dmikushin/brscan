/*
 * test_scandec_arm_mock.c — Reproduce the ARM scan loop hang locally.
 *
 * Simulates the scan flow on x86-64 but using the ARM-portable
 * ScanDecOpen (uint64_t buffer layout) to expose the same bug that
 * causes scanInfo.ScanAreaByte.lWidth=10 instead of 80 on ARM.
 *
 * We feed the same parameters that the DCP-1510 driver uses:
 *   nInReso = 200x200, nOutReso = 200x200
 *   nColorType = 0x208 (SCCLR_TYPE_TG = True Gray, 8-bit)
 *   dwInLinePixCnt = 80 (10mm at 200dpi)
 *
 * Then call ScanDecWrite with a mock scan line to check if the
 * output parameters and line processing are correct.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

typedef int BOOL, INT;
typedef unsigned long DWORD;
typedef char CHAR;
typedef void *HANDLE;

/* Use the real struct layout (matches whatever platform we're on) */
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
} SCANDEC_OPEN;

typedef struct {
	INT    nInDataComp;
	INT    nInDataKind;
	CHAR  *pLineData;
	DWORD  dwLineDataSize;
	CHAR  *pWriteBuff;
	DWORD  dwWriteBuffSize;
	BOOL   bReverWrite;
} SCANDEC_WRITE;

#define SCCLR_TYPE_TG   0x208
#define SCIDC_NONCOMP   2
#define SCIDK_MONO      1
#define SCODK_PIXEL_RGB 1

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <libbrscandec.so>\n", argv[0]);
		return 1;
	}

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

	BOOL  (*Open)(SCANDEC_OPEN *) = dlsym(h, "ScanDecOpen");
	void  (*SetTbl)(HANDLE, HANDLE) = dlsym(h, "ScanDecSetTblHandle");
	BOOL  (*PageStart)(void) = dlsym(h, "ScanDecPageStart");
	DWORD (*Write)(SCANDEC_WRITE *, INT *) = dlsym(h, "ScanDecWrite");
	DWORD (*PageEnd)(SCANDEC_WRITE *, INT *) = dlsym(h, "ScanDecPageEnd");
	BOOL  (*Close)(void) = dlsym(h, "ScanDecClose");

	printf("=== Platform info ===\n");
	printf("sizeof(DWORD)=%zu sizeof(void*)=%zu sizeof(SCANDEC_OPEN)=%zu\n",
	       sizeof(DWORD), sizeof(void *), sizeof(SCANDEC_OPEN));

	SCANDEC_OPEN info = {0};
	info.nInResoX = 200;
	info.nInResoY = 200;
	info.nOutResoX = 200;
	info.nOutResoY = 200;
	info.nColorType = SCCLR_TYPE_TG;  /* 0x208 = True Gray 8-bit */
	info.dwInLinePixCnt = 80;         /* 10mm at 200dpi ≈ 79 pixels, rounded to 80 */
	info.nOutDataKind = SCODK_PIXEL_RGB;
	info.bLongBoundary = 0;

	printf("\n=== ScanDecOpen ===\n");
	printf("Input: inReso=%dx%d outReso=%dx%d color=0x%x pixCnt=%lu\n",
	       info.nInResoX, info.nInResoY, info.nOutResoX, info.nOutResoY,
	       info.nColorType, (unsigned long)info.dwInLinePixCnt);

	BOOL r = Open(&info);
	printf("Open returned: %d\n", r);
	printf("Output: dwOutLinePixCnt=%lu dwOutLineByte=%lu dwOutWriteMaxSize=%lu\n",
	       (unsigned long)info.dwOutLinePixCnt,
	       (unsigned long)info.dwOutLineByte,
	       (unsigned long)info.dwOutWriteMaxSize);

	/* Check: for TrueGray, dwOutLineByte should equal dwOutLinePixCnt (1 byte/pixel) */
	if (info.dwOutLineByte != info.dwOutLinePixCnt) {
		printf("BUG: dwOutLineByte (%lu) != dwOutLinePixCnt (%lu) for Gray mode!\n",
		       (unsigned long)info.dwOutLineByte,
		       (unsigned long)info.dwOutLinePixCnt);
		printf("     Expected: both should be %lu (1 byte per pixel for 8-bit gray)\n",
		       (unsigned long)info.dwOutLinePixCnt);

		/* Check if it looks like B&W calculation */
		if (info.dwOutLineByte * 8 == info.dwOutLinePixCnt ||
		    info.dwOutLinePixCnt / 8 == info.dwOutLineByte) {
			printf("     Looks like B&W (1-bit) calculation instead of Gray (8-bit)\n");
		}
	} else {
		printf("OK: Gray mode output dimensions are consistent\n");
	}

	if (!r) {
		printf("ScanDecOpen failed, cannot continue\n");
		Close();
		dlclose(h);
		return 1;
	}

	/* Try writing a mock scan line */
	printf("\n=== ScanDecWrite test ===\n");
	SetTbl(NULL, NULL);
	PageStart();

	/* Create uncompressed gray line data: 80 bytes of gradient */
	unsigned char lineData[80];
	for (int i = 0; i < 80; i++) lineData[i] = i * 3;

	DWORD outMax = info.dwOutWriteMaxSize;
	if (outMax == 0) outMax = 4096;
	char *outBuf = calloc(1, outMax);

	/* Write 3 lines and see what happens */
	for (int i = 0; i < 3; i++) {
		SCANDEC_WRITE sw = {0};
		sw.nInDataComp = SCIDC_NONCOMP;
		sw.nInDataKind = SCIDK_MONO;
		sw.pLineData = (CHAR *)lineData;
		sw.dwLineDataSize = 80;
		sw.pWriteBuff = outBuf;
		sw.dwWriteBuffSize = outMax;
		sw.bReverWrite = 0;

		INT lineCount = 0;
		DWORD ws = Write(&sw, &lineCount);
		printf("Write[%d]: size=%lu lines=%d\n", i, (unsigned long)ws, lineCount);
	}

	Close();
	free(outBuf);
	dlclose(h);
	printf("\nDone.\n");
	return 0;
}
