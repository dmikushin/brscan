/*
 * test_integration.c — Integration tests for the brscan driver pipeline.
 *
 * Verifies each stage of the scan data processing using reference data
 * captured from a real DCP-1510 scan of a Swiss math exam sheet.
 *
 * Tests:
 *   1. brscan4 line frame parsing (10-byte wrapper + 2-byte LE length)
 *   2. Packbits decompression (signed char correctness on ARM)
 *   3. ScanDecOpen parameter computation for True Gray mode
 *   4. Full pipeline: raw frame → ScanDecWrite → decoded pixels
 *   5. libbrcolm LUT loading and color matching
 *
 * Build: gcc -o test_integration tests/test_integration.c -ldl -lm
 * Run:   ./test_integration ./libbrscandec.so.1 ./libbrcolm.so.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

typedef int BOOL, INT;
typedef unsigned long DWORD;
typedef char CHAR;
typedef void *HANDLE;

/* --- Scan decoder types (matching brother_scandec.h layout) --- */
typedef struct {
	INT    nInResoX, nInResoY, nOutResoX, nOutResoY, nColorType;
	DWORD  dwInLinePixCnt;
	INT    nOutDataKind;
	BOOL   bLongBoundary;
	DWORD  dwOutLinePixCnt, dwOutLineByte, dwOutWriteMaxSize;
} SCANDEC_OPEN;

typedef struct {
	INT    nInDataComp, nInDataKind;
	CHAR  *pLineData;
	DWORD  dwLineDataSize;
	CHAR  *pWriteBuff;
	DWORD  dwWriteBuffSize;
	BOOL   bReverWrite;
} SCANDEC_WRITE;

/* --- Color matching types --- */
#pragma pack(push, 1)
typedef struct {
	int nRgbLine, nPaperType, nMachineId;
	char *lpLutName;
} CMATCH_INIT;
#pragma pack(pop)

#define SCCLR_TYPE_TG   0x208
#define SCIDC_PACK      3
#define SCIDK_MONO      1
#define SCODK_PIXEL_RGB 1

static int g_pass = 0, g_fail = 0;

#define ASSERT_EQ(name, got, expected) do { \
	if ((got) == (expected)) { g_pass++; } \
	else { g_fail++; printf("  FAIL %s: got %ld, expected %ld\n", \
		name, (long)(got), (long)(expected)); } \
} while(0)

#define ASSERT_TRUE(name, cond) do { \
	if (cond) { g_pass++; } \
	else { g_fail++; printf("  FAIL %s\n", name); } \
} while(0)

/* ================================================================
 * Reference data from DCP-1510 scan (Swiss math exam, 50x20mm, 200dpi)
 *
 * Captured line parser output:
 *   line[0]: clen=101, total=113, next=42 07 00
 *   line[1]: clen=71,  total=83,  next=42 07 00
 *   line[2]: clen=89,  total=101, next=42 07 00
 *
 * Each frame: [42 07 00 01 00 84 00 00 00 00][LL LL][packbits data]
 * Scan params: 400x156 pixels, True Gray, 200dpi, no scaling
 * ================================================================ */

/* 10-byte wrapper that starts every brscan4 scan line */
static const uint8_t BRSCAN4_WRAPPER[10] = {
	0x42, 0x07, 0x00, 0x01, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00
};

/* Reference packbits-compressed line: all-white (400 pixels of 0xFF)
 * Packbits: 3 runs of 128 + 1 run of 16 = 400 bytes output
 *   0x81 0xFF  → repeat 0xFF 128 times
 *   0x81 0xFF  → repeat 0xFF 128 times
 *   0x81 0xFF  → repeat 0xFF 128 times
 *   0xF1 0xFF  → repeat 0xFF 16 times
 */
static const uint8_t WHITE_LINE_PACKBITS[] = {
	0x81, 0xFF, 0x81, 0xFF, 0x81, 0xFF, 0xF1, 0xFF
};
#define WHITE_LINE_PACKBITS_LEN 8

/* Build a complete brscan4 frame: wrapper + 2-byte LE length + data */
static uint8_t *make_brscan4_frame(const uint8_t *data, uint16_t datalen,
                                    size_t *framelen)
{
	*framelen = 10 + 2 + datalen;
	uint8_t *frame = malloc(*framelen);
	memcpy(frame, BRSCAN4_WRAPPER, 10);
	frame[10] = datalen & 0xFF;        /* LE low byte */
	frame[11] = (datalen >> 8) & 0xFF; /* LE high byte */
	memcpy(frame + 12, data, datalen);
	return frame;
}

/* ================================================================
 * Test 1: brscan4 frame structure
 * ================================================================ */
static void test_brscan4_frame_structure(void)
{
	printf("\n--- Test 1: brscan4 frame structure ---\n");

	size_t flen;
	uint8_t *frame = make_brscan4_frame(WHITE_LINE_PACKBITS,
	                                     WHITE_LINE_PACKBITS_LEN, &flen);

	ASSERT_EQ("frame_length", flen, 10 + 2 + WHITE_LINE_PACKBITS_LEN);
	ASSERT_EQ("wrapper_byte0", frame[0], 0x42);
	ASSERT_EQ("wrapper_byte1", frame[1], 0x07);
	ASSERT_EQ("wrapper_byte2", frame[2], 0x00);
	ASSERT_EQ("length_lo", frame[10], WHITE_LINE_PACKBITS_LEN);
	ASSERT_EQ("length_hi", frame[11], 0);

	/* Verify LE length decode */
	uint16_t len_le = frame[10] | (frame[11] << 8);
	ASSERT_EQ("length_le", len_le, WHITE_LINE_PACKBITS_LEN);

	/* Verify next frame boundary */
	size_t lineTotal = 12 + len_le;
	ASSERT_EQ("lineTotal", lineTotal, flen);

	/* Build 3 consecutive frames and verify boundaries */
	uint8_t *buf = malloc(flen * 3);
	memcpy(buf, frame, flen);
	memcpy(buf + flen, frame, flen);
	memcpy(buf + flen * 2, frame, flen);

	for (int i = 0; i < 3; i++) {
		uint8_t *p = buf + i * flen;
		ASSERT_EQ("multi_wrapper", p[0], 0x42);
		uint16_t l = p[10] | (p[11] << 8);
		ASSERT_EQ("multi_length", l, WHITE_LINE_PACKBITS_LEN);
	}

	free(buf);
	free(frame);
}

/* ================================================================
 * Test 2: Packbits decompression (signed char correctness)
 * ================================================================ */
static void test_packbits_decode(void *scandec_lib)
{
	printf("\n--- Test 2: Packbits decompression ---\n");

	BOOL  (*Open)(SCANDEC_OPEN *) = dlsym(scandec_lib, "ScanDecOpen");
	void  (*SetTbl)(HANDLE, HANDLE) = dlsym(scandec_lib, "ScanDecSetTblHandle");
	BOOL  (*PageStart)(void) = dlsym(scandec_lib, "ScanDecPageStart");
	DWORD (*Write)(SCANDEC_WRITE *, INT *) = dlsym(scandec_lib, "ScanDecWrite");
	BOOL  (*Close)(void) = dlsym(scandec_lib, "ScanDecClose");

	SCANDEC_OPEN info = {200, 200, 200, 200, SCCLR_TYPE_TG,
	                     400, SCODK_PIXEL_RGB, 0};
	BOOL r = Open(&info);
	ASSERT_TRUE("scandec_open", r);
	if (!r) return;

	SetTbl(NULL, NULL);
	PageStart();

	/* Test: decompress white line packbits → 400 bytes of 0xFF */
	uint8_t outbuf[1024];
	memset(outbuf, 0x00, sizeof(outbuf));  /* fill with black first */

	SCANDEC_WRITE sw = {0};
	sw.nInDataComp = SCIDC_PACK;
	sw.nInDataKind = SCIDK_MONO;
	sw.pLineData = (CHAR *)WHITE_LINE_PACKBITS;
	sw.dwLineDataSize = WHITE_LINE_PACKBITS_LEN;
	sw.pWriteBuff = (CHAR *)outbuf;
	sw.dwWriteBuffSize = info.dwOutWriteMaxSize;
	sw.bReverWrite = 0;

	INT lineCount = 0;
	DWORD ws = Write(&sw, &lineCount);
	ASSERT_EQ("write_size", ws, 400);
	ASSERT_EQ("line_count", lineCount, 1);

	/* Verify all 400 output bytes are 0xFF (white) */
	int white_count = 0;
	for (int i = 0; i < 400; i++) {
		if (outbuf[i] == 0xFF) white_count++;
	}
	ASSERT_EQ("white_pixels", white_count, 400);

	/* Test: mixed packbits (run + literal) */
	/* 0xFE 0xAA = repeat 0xAA 3 times, then 0x01 0x55 0x66 = literal 2 bytes */
	uint8_t mixed_pb[] = {0xFE, 0xAA, 0x01, 0x55, 0x66};
	memset(outbuf, 0x00, sizeof(outbuf));
	sw.pLineData = (CHAR *)mixed_pb;
	sw.dwLineDataSize = sizeof(mixed_pb);
	ws = Write(&sw, &lineCount);
	ASSERT_EQ("mixed_write_size", ws, 400);
	/* First 3 bytes should be 0xAA, next 2 should be 0x55, 0x66 */
	ASSERT_EQ("mixed_byte0", outbuf[0], 0xAA);
	ASSERT_EQ("mixed_byte1", outbuf[1], 0xAA);
	ASSERT_EQ("mixed_byte2", outbuf[2], 0xAA);
	ASSERT_EQ("mixed_byte3", outbuf[3], 0x55);
	ASSERT_EQ("mixed_byte4", outbuf[4], 0x66);
	/* Remaining should be white (0xFF from clear) */
	ASSERT_EQ("mixed_byte5", outbuf[5], 0xFF);

	/* Test: large run (128 repetitions = control byte 0x81) */
	/* This is the KEY test for the ARM signed char bug */
	uint8_t large_run[] = {0x81, 0xBB};  /* repeat 0xBB 128 times */
	memset(outbuf, 0x00, sizeof(outbuf));
	sw.pLineData = (CHAR *)large_run;
	sw.dwLineDataSize = sizeof(large_run);
	ws = Write(&sw, &lineCount);
	/* First 128 bytes should be 0xBB */
	int bb_count = 0;
	for (int i = 0; i < 128; i++) {
		if (outbuf[i] == 0xBB) bb_count++;
	}
	ASSERT_EQ("large_run_128xBB", bb_count, 128);
	/* Byte 128 should be 0xFF (white fill, not 0xBB) */
	ASSERT_EQ("after_run_is_white", outbuf[128], 0xFF);

	Close();
}

/* ================================================================
 * Test 3: ScanDecOpen parameter computation
 * ================================================================ */
static void test_scandec_open_params(void *scandec_lib)
{
	printf("\n--- Test 3: ScanDecOpen parameters ---\n");

	BOOL  (*Open)(SCANDEC_OPEN *) = dlsym(scandec_lib, "ScanDecOpen");
	BOOL  (*Close)(void) = dlsym(scandec_lib, "ScanDecClose");

	/* True Gray, 400 pixels, same resolution */
	{
		SCANDEC_OPEN s = {200, 200, 200, 200, SCCLR_TYPE_TG,
		                  400, SCODK_PIXEL_RGB, 0};
		BOOL r = Open(&s);
		ASSERT_TRUE("tg400_open", r);
		ASSERT_EQ("tg400_pixcnt", s.dwOutLinePixCnt, 400);
		ASSERT_EQ("tg400_bytecnt", s.dwOutLineByte, 400);
		ASSERT_EQ("tg400_maxsize", s.dwOutWriteMaxSize, 400);
		if (r) Close();
	}

	/* True Gray, 1648 pixels (A4 @ 200dpi) */
	{
		SCANDEC_OPEN s = {200, 200, 200, 200, SCCLR_TYPE_TG,
		                  1648, SCODK_PIXEL_RGB, 0};
		BOOL r = Open(&s);
		ASSERT_TRUE("tg1648_open", r);
		ASSERT_EQ("tg1648_pixcnt", s.dwOutLinePixCnt, 1648);
		ASSERT_EQ("tg1648_bytecnt", s.dwOutLineByte, 1648);
		ASSERT_EQ("tg1648_maxsize", s.dwOutWriteMaxSize, 1648);
		if (r) Close();
	}

	/* B&W, 128 pixels */
	{
		SCANDEC_OPEN s = {200, 200, 200, 200, 0x101, /* BW */
		                  128, SCODK_PIXEL_RGB, 0};
		BOOL r = Open(&s);
		ASSERT_TRUE("bw128_open", r);
		ASSERT_EQ("bw128_pixcnt", s.dwOutLinePixCnt, 128);
		ASSERT_EQ("bw128_bytecnt", s.dwOutLineByte, 16); /* 128/8 */
		if (r) Close();
	}

	/* Resolution upscale 200→300 */
	{
		SCANDEC_OPEN s = {200, 200, 300, 300, SCCLR_TYPE_TG,
		                  100, SCODK_PIXEL_RGB, 0};
		BOOL r = Open(&s);
		ASSERT_TRUE("upscale_open", r);
		ASSERT_EQ("upscale_pixcnt", s.dwOutLinePixCnt, 150);
		ASSERT_EQ("upscale_bytecnt", s.dwOutLineByte, 150);
		if (r) Close();
	}
}

/* ================================================================
 * Test 4: Full pipeline with brscan4 frames
 * ================================================================ */
static void test_full_pipeline(void *scandec_lib)
{
	printf("\n--- Test 4: Full pipeline (brscan4 frame → pixels) ---\n");

	BOOL  (*Open)(SCANDEC_OPEN *) = dlsym(scandec_lib, "ScanDecOpen");
	void  (*SetTbl)(HANDLE, HANDLE) = dlsym(scandec_lib, "ScanDecSetTblHandle");
	BOOL  (*PageStart)(void) = dlsym(scandec_lib, "ScanDecPageStart");
	DWORD (*Write)(SCANDEC_WRITE *, INT *) = dlsym(scandec_lib, "ScanDecWrite");
	BOOL  (*Close)(void) = dlsym(scandec_lib, "ScanDecClose");

	SCANDEC_OPEN info = {200, 200, 200, 200, SCCLR_TYPE_TG,
	                     400, SCODK_PIXEL_RGB, 0};
	Open(&info);
	SetTbl(NULL, NULL);
	PageStart();

	/* Feed 5 white lines through the pipeline */
	uint8_t outbuf[2048];
	int total_output = 0;

	for (int i = 0; i < 5; i++) {
		memset(outbuf, 0x00, sizeof(outbuf));
		SCANDEC_WRITE sw = {SCIDC_PACK, SCIDK_MONO,
		                    (CHAR *)WHITE_LINE_PACKBITS,
		                    WHITE_LINE_PACKBITS_LEN,
		                    (CHAR *)outbuf, info.dwOutWriteMaxSize, 0};
		INT lc = 0;
		DWORD ws = Write(&sw, &lc);
		total_output += ws;

		/* Each line should produce 400 white pixels */
		if (ws == 400) {
			int ok = 1;
			for (int j = 0; j < 400; j++) {
				if (outbuf[j] != 0xFF) { ok = 0; break; }
			}
			if (!ok) {
				g_fail++;
				printf("  FAIL pipeline_line%d: non-white pixel found\n", i);
			} else {
				g_pass++;
			}
		}
	}
	ASSERT_EQ("pipeline_total", total_output, 5 * 400);

	Close();
}

/* ================================================================
 * Test 5: libbrcolm color matching
 * ================================================================ */
static void test_color_matching(void *colm_lib, const char *lut_path)
{
	printf("\n--- Test 5: Color matching (libbrcolm) ---\n");

	if (!colm_lib) {
		printf("  SKIP: libbrcolm not loaded\n");
		return;
	}

	BOOL (*Init)(CMATCH_INIT) = dlsym(colm_lib, "ColorMatchingInit");
	void (*End)(void) = dlsym(colm_lib, "ColorMatchingEnd");
	BOOL (*Match)(uint8_t *, long, long) = dlsym(colm_lib, "ColorMatching");

	if (!Init || !End || !Match) {
		printf("  SKIP: missing symbols\n");
		return;
	}

	CMATCH_INIT ci = {0, 0, 0, (char *)lut_path};
	BOOL r = Init(ci);
	if (!r) {
		printf("  SKIP: LUT file not found (%s)\n", lut_path);
		return;
	}

	/* Test: black stays black */
	uint8_t black[6] = {0, 0, 0, 0, 0, 0};
	Match(black, 6, 1);
	ASSERT_EQ("black_R", black[0], 0);
	ASSERT_EQ("black_G", black[1], 0);
	ASSERT_EQ("black_B", black[2], 0);

	/* Test: white stays white */
	End();
	r = Init(ci);
	uint8_t white[6] = {255, 255, 255, 255, 255, 255};
	Match(white, 6, 1);
	ASSERT_EQ("white_R", white[0], 255);
	ASSERT_EQ("white_G", white[1], 255);
	ASSERT_EQ("white_B", white[2], 255);

	End();
}

/* ================================================================
 * Test 6: Packbits signed char edge cases
 * ================================================================ */
static void test_packbits_signedness(void *scandec_lib)
{
	printf("\n--- Test 6: Packbits signed char edge cases ---\n");

	BOOL  (*Open)(SCANDEC_OPEN *) = dlsym(scandec_lib, "ScanDecOpen");
	void  (*SetTbl)(HANDLE, HANDLE) = dlsym(scandec_lib, "ScanDecSetTblHandle");
	BOOL  (*PageStart)(void) = dlsym(scandec_lib, "ScanDecPageStart");
	DWORD (*Write)(SCANDEC_WRITE *, INT *) = dlsym(scandec_lib, "ScanDecWrite");
	BOOL  (*Close)(void) = dlsym(scandec_lib, "ScanDecClose");

	SCANDEC_OPEN info = {200, 200, 200, 200, SCCLR_TYPE_TG,
	                     400, SCODK_PIXEL_RGB, 0};
	Open(&info);
	SetTbl(NULL, NULL);
	PageStart();

	uint8_t outbuf[1024];

	/* Control byte 0xFF (-1 signed) → repeat 2 times */
	{
		uint8_t pb[] = {0xFF, 0x42};
		memset(outbuf, 0x00, sizeof(outbuf));
		SCANDEC_WRITE sw = {SCIDC_PACK, SCIDK_MONO, (CHAR *)pb, 2,
		                    (CHAR *)outbuf, 400, 0};
		INT lc = 0;
		Write(&sw, &lc);
		ASSERT_EQ("0xFF_repeat2_byte0", outbuf[0], 0x42);
		ASSERT_EQ("0xFF_repeat2_byte1", outbuf[1], 0x42);
		ASSERT_EQ("0xFF_repeat2_byte2", outbuf[2], 0xFF); /* white fill */
	}

	/* Control byte 0x80 (-128) → NOP, should skip */
	{
		uint8_t pb[] = {0x80, 0xFF, 0x42};  /* NOP, then repeat 0x42 x2 */
		memset(outbuf, 0x00, sizeof(outbuf));
		SCANDEC_WRITE sw = {SCIDC_PACK, SCIDK_MONO, (CHAR *)pb, 3,
		                    (CHAR *)outbuf, 400, 0};
		INT lc = 0;
		Write(&sw, &lc);
		ASSERT_EQ("0x80_nop_byte0", outbuf[0], 0x42);
		ASSERT_EQ("0x80_nop_byte1", outbuf[1], 0x42);
	}

	/* Control byte 0x82 (-126) → repeat 127 times */
	{
		uint8_t pb[] = {0x82, 0x33};
		memset(outbuf, 0x00, sizeof(outbuf));
		SCANDEC_WRITE sw = {SCIDC_PACK, SCIDK_MONO, (CHAR *)pb, 2,
		                    (CHAR *)outbuf, 400, 0};
		INT lc = 0;
		Write(&sw, &lc);
		int count = 0;
		for (int i = 0; i < 127; i++) {
			if (outbuf[i] == 0x33) count++;
		}
		ASSERT_EQ("0x82_repeat127", count, 127);
		ASSERT_EQ("0x82_after_is_white", outbuf[127], 0xFF);
	}

	/* Literal: 0x03 → copy 4 bytes */
	{
		uint8_t pb[] = {0x03, 0xAA, 0xBB, 0xCC, 0xDD};
		memset(outbuf, 0x00, sizeof(outbuf));
		SCANDEC_WRITE sw = {SCIDC_PACK, SCIDK_MONO, (CHAR *)pb, 5,
		                    (CHAR *)outbuf, 400, 0};
		INT lc = 0;
		Write(&sw, &lc);
		ASSERT_EQ("literal_byte0", outbuf[0], 0xAA);
		ASSERT_EQ("literal_byte1", outbuf[1], 0xBB);
		ASSERT_EQ("literal_byte2", outbuf[2], 0xCC);
		ASSERT_EQ("literal_byte3", outbuf[3], 0xDD);
		ASSERT_EQ("literal_byte4", outbuf[4], 0xFF);
	}

	Close();
}

/* ================================================================ */

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <libbrscandec.so> [libbrcolm.so] [lut_file]\n",
		        argv[0]);
		return 1;
	}

	void *scandec = dlopen(argv[1], RTLD_NOW);
	if (!scandec) { fprintf(stderr, "dlopen %s: %s\n", argv[1], dlerror()); return 1; }

	void *colm = (argc > 2) ? dlopen(argv[2], RTLD_NOW) : NULL;
	const char *lut = (argc > 3) ? argv[3] : NULL;

	test_brscan4_frame_structure();
	test_packbits_decode(scandec);
	test_scandec_open_params(scandec);
	test_full_pipeline(scandec);
	test_packbits_signedness(scandec);
	if (colm && lut) test_color_matching(colm, lut);

	printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

	if (colm) dlclose(colm);
	dlclose(scandec);
	return g_fail ? 1 : 0;
}
