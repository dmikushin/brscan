/*
 * verify_brscandec.c — Verification test for libbrscandec replacement.
 *
 * Loads both the original blob and our replacement via dlopen,
 * feeds identical scan data through both, compares outputs byte-by-byte.
 *
 * Usage: ./verify_brscandec <path_to_original.so> <path_to_replacement.so>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef int             BOOL;
typedef int             INT;
typedef unsigned char   BYTE;
typedef unsigned long   DWORD;
typedef char            CHAR;
typedef void           *HANDLE;

/* Matches the structs in brother_scandec.h — NO pack(1), uses natural alignment.
 * DWORD = unsigned long = 8 bytes on x86-64 Linux.
 * Verified against blob disassembly:
 *   nColorType at 0x10, dwInLinePixCnt at 0x18, nOutDataKind at 0x20,
 *   bLongBoundary at 0x24, dwOutLinePixCnt at 0x28, dwOutLineByte at 0x30,
 *   dwOutWriteMaxSize at 0x38. */

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

/* Color type constants from brother_deccom.h */
#define SC_BW       0x01
#define SC_TG       0x08
#define SC_FUL      0x20
#define SC_2BIT     (0x01<<8)
#define SC_8BIT     (0x02<<8)
#define SC_24BIT    (0x04<<8)
#define SCCLR_TYPE_BW   (SC_BW | SC_2BIT)
#define SCCLR_TYPE_TG   (SC_TG | SC_8BIT)
#define SCCLR_TYPE_FUL  (SC_FUL | SC_24BIT)

#define SCIDC_NONCOMP  2
#define SCIDC_PACK     3
#define SCIDK_MONO     1
#define SCIDK_R        2
#define SCIDK_G        3
#define SCIDK_B        4
#define SCIDK_RGB      5
#define SCODK_PIXEL_RGB 1

typedef BOOL  (*ScanDecOpen_t)(SCANDEC_OPEN *);
typedef void  (*ScanDecSetTbl_t)(HANDLE, HANDLE);
typedef BOOL  (*ScanDecPageStart_t)(void);
typedef DWORD (*ScanDecWrite_t)(SCANDEC_WRITE *, INT *);
typedef DWORD (*ScanDecPageEnd_t)(SCANDEC_WRITE *, INT *);
typedef BOOL  (*ScanDecClose_t)(void);

struct scandec_lib {
	void *handle;
	ScanDecOpen_t      open;
	ScanDecSetTbl_t    setTbl;
	ScanDecPageStart_t pageStart;
	ScanDecWrite_t     write;
	ScanDecPageEnd_t   pageEnd;
	ScanDecClose_t     close;
	const char        *name;
};

static int load_lib(struct scandec_lib *lib, const char *path, const char *name)
{
	lib->name = name;
	lib->handle = dlopen(path, RTLD_NOW);
	if (!lib->handle) {
		fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
		return -1;
	}
	lib->open      = (ScanDecOpen_t)dlsym(lib->handle, "ScanDecOpen");
	lib->setTbl    = (ScanDecSetTbl_t)dlsym(lib->handle, "ScanDecSetTblHandle");
	lib->pageStart = (ScanDecPageStart_t)dlsym(lib->handle, "ScanDecPageStart");
	lib->write     = (ScanDecWrite_t)dlsym(lib->handle, "ScanDecWrite");
	lib->pageEnd   = (ScanDecPageEnd_t)dlsym(lib->handle, "ScanDecPageEnd");
	lib->close     = (ScanDecClose_t)dlsym(lib->handle, "ScanDecClose");

	if (!lib->open || !lib->setTbl || !lib->pageStart ||
	    !lib->write || !lib->pageEnd || !lib->close) {
		fprintf(stderr, "%s: missing symbols\n", name);
		dlclose(lib->handle);
		return -1;
	}
	return 0;
}

static void close_lib(struct scandec_lib *lib)
{
	if (lib->handle) {
		lib->close();
		dlclose(lib->handle);
		lib->handle = NULL;
	}
}

static int compare_buffers(const BYTE *a, const BYTE *b, long len,
			   const char *testName)
{
	int mismatches = 0;
	int max_diff = 0;
	long first_mismatch = -1;

	for (long i = 0; i < len; i++) {
		int diff = abs((int)a[i] - (int)b[i]);
		if (diff > 0) {
			if (first_mismatch < 0) first_mismatch = i;
			mismatches++;
			if (diff > max_diff) max_diff = diff;
		}
	}

	if (mismatches == 0) {
		printf("  PASS [%s]: %ld bytes, perfect match\n", testName, len);
	} else {
		printf("  FAIL [%s]: %d/%ld bytes differ, max_diff=%d, first@%ld "
		       "(orig=0x%02x repl=0x%02x)\n",
		       testName, mismatches, len, max_diff, first_mismatch,
		       a[first_mismatch], b[first_mismatch]);
	}
	return mismatches > 0 ? 1 : 0;
}

/*
 * Test a full decode cycle: Open → SetTbl → PageStart → Write lines → PageEnd → Close
 * with identical inputs on both libs, compare all outputs.
 */
static int run_scandec_test(struct scandec_lib *orig, struct scandec_lib *repl,
			    int inResoX, int inResoY,
			    int outResoX, int outResoY,
			    int colorType, int pixelWidth,
			    int dataComp, int dataKind,
			    BYTE *lineData, DWORD lineDataSize,
			    int numLines, const char *testName)
{
	int fails = 0;

	printf("\n--- Test: %s ---\n", testName);
	printf("  Reso: %dx%d -> %dx%d, color=0x%x, width=%d, lines=%d\n",
	       inResoX, inResoY, outResoX, outResoY, colorType, pixelWidth, numLines);

	/* ScanDecOpen on both */
	SCANDEC_OPEN info_o = {0}, info_r = {0};

	info_o.nInResoX = info_r.nInResoX = inResoX;
	info_o.nInResoY = info_r.nInResoY = inResoY;
	info_o.nOutResoX = info_r.nOutResoX = outResoX;
	info_o.nOutResoY = info_r.nOutResoY = outResoY;
	info_o.nColorType = info_r.nColorType = colorType;
	info_o.dwInLinePixCnt = info_r.dwInLinePixCnt = pixelWidth;
	info_o.nOutDataKind = info_r.nOutDataKind = SCODK_PIXEL_RGB;
	info_o.bLongBoundary = info_r.bLongBoundary = 0;

	BOOL ro = orig->open(&info_o);
	BOOL rr = repl->open(&info_r);

	if (ro != rr) {
		printf("  FAIL: ScanDecOpen return mismatch: orig=%d repl=%d\n", ro, rr);
		return 1;
	}
	if (!ro) {
		printf("  SKIP: both ScanDecOpen failed\n");
		return 0;
	}

	/* Compare the output parameters set by ScanDecOpen */
	if (info_o.dwOutLinePixCnt != info_r.dwOutLinePixCnt) {
		printf("  FAIL: dwOutLinePixCnt: orig=%lu repl=%lu\n",
		       info_o.dwOutLinePixCnt, info_r.dwOutLinePixCnt);
		fails++;
	}
	if (info_o.dwOutLineByte != info_r.dwOutLineByte) {
		printf("  FAIL: dwOutLineByte: orig=%lu repl=%lu\n",
		       info_o.dwOutLineByte, info_r.dwOutLineByte);
		fails++;
	}
	if (info_o.dwOutWriteMaxSize != info_r.dwOutWriteMaxSize) {
		printf("  FAIL: dwOutWriteMaxSize: orig=%lu repl=%lu\n",
		       info_o.dwOutWriteMaxSize, info_r.dwOutWriteMaxSize);
		fails++;
	}
	if (fails == 0) {
		printf("  PASS [ScanDecOpen]: output params match "
		       "(pixCnt=%lu lineByte=%lu maxSize=%lu)\n",
		       info_o.dwOutLinePixCnt, info_o.dwOutLineByte,
		       info_o.dwOutWriteMaxSize);
	}

	/* SetTblHandle — no gray table for this test */
	orig->setTbl(NULL, NULL);
	repl->setTbl(NULL, NULL);

	/* PageStart */
	ro = orig->pageStart();
	rr = repl->pageStart();
	if (ro != rr) {
		printf("  FAIL: ScanDecPageStart return mismatch: orig=%d repl=%d\n", ro, rr);
		fails++;
	}

	/* Write lines — use generous buffer (maxSize may be per-line,
	 * but with resolution upscaling multiple lines may be output per call) */
	DWORD outMaxSize = info_o.dwOutWriteMaxSize;
	if (outMaxSize == 0) outMaxSize = 65536;
	outMaxSize *= 8;  /* headroom for multi-line output */

	BYTE *outBuf_o = calloc(1, outMaxSize);
	BYTE *outBuf_r = calloc(1, outMaxSize);

	for (int line = 0; line < numLines; line++) {
		memset(outBuf_o, 0xAA, outMaxSize);
		memset(outBuf_r, 0xAA, outMaxSize);

		SCANDEC_WRITE sw_o = {0}, sw_r = {0};
		INT lineCount_o = 0, lineCount_r = 0;

		sw_o.nInDataComp = sw_r.nInDataComp = dataComp;
		sw_o.nInDataKind = sw_r.nInDataKind = dataKind;
		sw_o.pLineData = sw_r.pLineData = (CHAR *)lineData;
		sw_o.dwLineDataSize = sw_r.dwLineDataSize = lineDataSize;
		sw_o.pWriteBuff = (CHAR *)outBuf_o;
		sw_r.pWriteBuff = (CHAR *)outBuf_r;
		sw_o.dwWriteBuffSize = sw_r.dwWriteBuffSize = outMaxSize;
		sw_o.bReverWrite = sw_r.bReverWrite = 0;

		DWORD ws_o = orig->write(&sw_o, &lineCount_o);
		DWORD ws_r = repl->write(&sw_r, &lineCount_r);

		if (ws_o != ws_r || lineCount_o != lineCount_r) {
			printf("  FAIL: ScanDecWrite[%d] size: orig=%lu repl=%lu, "
			       "lines: orig=%d repl=%d\n",
			       line, ws_o, ws_r, lineCount_o, lineCount_r);
			fails++;
		}

		if (ws_o > 0 && ws_o == ws_r) {
			char name[128];
			snprintf(name, sizeof(name), "ScanDecWrite[%d]", line);
			fails += compare_buffers(outBuf_o, outBuf_r, ws_o, name);
		}
	}

	/* PageEnd */
	{
		memset(outBuf_o, 0xAA, outMaxSize);
		memset(outBuf_r, 0xAA, outMaxSize);

		SCANDEC_WRITE sw_o = {0}, sw_r = {0};
		INT lineCount_o = 0, lineCount_r = 0;

		sw_o.pWriteBuff = (CHAR *)outBuf_o;
		sw_r.pWriteBuff = (CHAR *)outBuf_r;
		sw_o.dwWriteBuffSize = sw_r.dwWriteBuffSize = outMaxSize;

		DWORD ws_o = orig->pageEnd(&sw_o, &lineCount_o);
		DWORD ws_r = repl->pageEnd(&sw_r, &lineCount_r);

		if (ws_o != ws_r || lineCount_o != lineCount_r) {
			printf("  FAIL: ScanDecPageEnd size: orig=%lu repl=%lu, "
			       "lines: orig=%d repl=%d\n",
			       ws_o, ws_r, lineCount_o, lineCount_r);
			fails++;
		}
		if (ws_o > 0 && ws_o == ws_r) {
			fails += compare_buffers(outBuf_o, outBuf_r, ws_o, "ScanDecPageEnd");
		}
	}

	free(outBuf_o);
	free(outBuf_r);

	/* Close */
	orig->close();
	repl->close();

	if (fails == 0)
		printf("  PASS [%s]: all outputs match\n", testName);
	else
		printf("  RESULT [%s]: %d failures\n", testName, fails);

	return fails;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <original.so> <replacement.so>\n", argv[0]);
		return 1;
	}

	int total_tests = 0;
	int total_fail = 0;

	/*
	 * Test 1: True Gray, same resolution (no scaling)
	 * 200x200 → 200x200, 8-bit gray, 100 pixels wide
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		/* Uncompressed gray line: 100 bytes of gradient data */
		BYTE lineData[100];
		for (int i = 0; i < 100; i++) lineData[i] = (i * 255) / 99;

		int r = run_scandec_test(&orig, &repl,
			200, 200, 200, 200,
			SCCLR_TYPE_TG, 100,
			SCIDC_NONCOMP, SCIDK_MONO,
			lineData, sizeof(lineData), 5,
			"TrueGray_200x200_noScale");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	/*
	 * Test 2: True Gray with resolution scaling (200→300)
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		BYTE lineData[100];
		for (int i = 0; i < 100; i++) lineData[i] = (i * 255) / 99;

		int r = run_scandec_test(&orig, &repl,
			200, 200, 300, 300,
			SCCLR_TYPE_TG, 100,
			SCIDC_NONCOMP, SCIDK_MONO,
			lineData, sizeof(lineData), 10,
			"TrueGray_200to300");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	/*
	 * Test 3: B&W, same resolution
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		/* B&W: 128 pixels = 16 bytes */
		BYTE lineData[16];
		for (int i = 0; i < 16; i++) lineData[i] = 0xAA; /* alternating pattern */

		int r = run_scandec_test(&orig, &repl,
			200, 200, 200, 200,
			SCCLR_TYPE_BW, 128,
			SCIDC_NONCOMP, SCIDK_MONO,
			lineData, sizeof(lineData), 5,
			"BW_200x200_noScale");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	/*
	 * Test 4: 24-bit color, RGB, same resolution
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		/* RGB: 100 pixels = 300 bytes per color plane */
		BYTE lineData[300];
		for (int i = 0; i < 300; i++) lineData[i] = (i * 17) & 0xFF;

		int r = run_scandec_test(&orig, &repl,
			200, 200, 200, 200,
			SCCLR_TYPE_FUL, 100,
			SCIDC_NONCOMP, SCIDK_R,
			lineData, sizeof(lineData), 3,
			"FullColor_R_200x200");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	/*
	 * Test 5: PackBits compressed gray
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		/* PackBits: run of 50 bytes of value 0x80, then 10 literal bytes */
		BYTE lineData[] = {
			0xCD, 0x80,        /* repeat 0x80 fifty times (0x01 - 0xCD = 50) */
			0x09,              /* 10 literal bytes follow */
			0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0
		};

		int r = run_scandec_test(&orig, &repl,
			200, 200, 200, 200,
			SCCLR_TYPE_TG, 60,
			SCIDC_PACK, SCIDK_MONO,
			lineData, sizeof(lineData), 3,
			"TrueGray_PackBits");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	/*
	 * Test 6: Gray with 2x upscale
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		BYTE lineData[80];
		for (int i = 0; i < 80; i++) lineData[i] = i * 3;

		int r = run_scandec_test(&orig, &repl,
			150, 150, 300, 300,
			SCCLR_TYPE_TG, 80,
			SCIDC_NONCOMP, SCIDK_MONO,
			lineData, sizeof(lineData), 8,
			"TrueGray_150to300_2x");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	/*
	 * Test 7: White line (compression type 1 = white)
	 */
	{
		struct scandec_lib orig, repl;
		if (load_lib(&orig, argv[1], "original") < 0) return 1;
		if (load_lib(&repl, argv[2], "replacement") < 0) return 1;

		BYTE lineData[1] = {0};

		int r = run_scandec_test(&orig, &repl,
			200, 200, 200, 200,
			SCCLR_TYPE_TG, 100,
			1 /* SCIDC_WHITE */, SCIDK_MONO,
			lineData, 0, 3,
			"TrueGray_WhiteLine");
		total_tests++; total_fail += (r > 0);

		close_lib(&orig);
		close_lib(&repl);
	}

	printf("\n=== Summary: %d/%d tests passed ===\n",
	       total_tests - total_fail, total_tests);

	return total_fail ? 1 : 0;
}
