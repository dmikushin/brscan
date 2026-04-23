/*
 * test_changeresinit.c — Call ChangeResoInit directly with uint64_t buffer
 * to check if it produces correct output for Gray mode.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "Usage: %s <libbrscandec.so>\n", argv[0]); return 1; }

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

	unsigned long (*ChangeResoInit)(uint64_t *) = dlsym(h, "ChangeResoInit");
	void (*ChangeResoClose)(void) = dlsym(h, "ChangeResoClose");
	if (!ChangeResoInit) { fprintf(stderr, "no ChangeResoInit\n"); return 1; }

	/* Build the x86-64 layout buffer:
	 *   [0] = nInResoX(4) + nInResoY(4)   = 200 | 200
	 *   [1] = nOutResoX(4) + nOutResoY(4)  = 200 | 200
	 *   [2] = nColorType(4) + padding(4)    = 0x208 | 0
	 *   [3] = dwInLinePixCnt(8)             = 80
	 *   [4] = nOutDataKind(4) + bLong(4)    = 1 | 0
	 *   [5] = dwOutLinePixCnt(8)            (output)
	 *   [6] = dwOutLineByte(8)              (output)
	 *   [7] = dwOutWriteMaxSize(8)          (output)
	 */
	uint64_t buf[8] = {0};

	/* Test with True Gray (0x208) and 80 pixels */
	((int32_t*)&buf[0])[0] = 200;    /* nInResoX */
	((int32_t*)&buf[0])[1] = 200;    /* nInResoY */
	((int32_t*)&buf[1])[0] = 200;    /* nOutResoX */
	((int32_t*)&buf[1])[1] = 200;    /* nOutResoY */
	((int32_t*)&buf[2])[0] = 0x208;  /* nColorType = SCCLR_TYPE_TG */
	((int32_t*)&buf[2])[1] = 0;      /* padding */
	buf[3] = 80;                      /* dwInLinePixCnt */
	((int32_t*)&buf[4])[0] = 1;      /* nOutDataKind */
	((int32_t*)&buf[4])[1] = 0;      /* bLongBoundary */

	printf("Before ChangeResoInit:\n");
	for (int i = 0; i < 8; i++)
		printf("  buf[%d] = 0x%016lx\n", i, (unsigned long)buf[i]);

	/* Verify what ChangeResoInit will read as nColorType */
	uint32_t *p = (uint32_t *)buf;
	printf("\nAs uint32 array:\n");
	for (int i = 0; i < 16; i++)
		printf("  u32[%d] (byte %2d) = 0x%08x = %u\n", i, i*4, p[i], p[i]);

	printf("\nChecking nColorType at *(uint*)(buf+2) = *(uint*)(byte16):\n");
	uint32_t colorType = *(uint32_t *)((char*)buf + 16);
	printf("  colorType = 0x%x\n", colorType);
	printf("  bit 10 = %d (should be 0 for non-RGB)\n", (colorType >> 10) & 1);
	printf("  bit 9  = %d (should be 1 for 8-bit)\n", (colorType >> 9) & 1);
	printf("  bit 8  = %d (should be 0)\n", (colorType >> 8) & 1);

	printf("\nCalling ChangeResoInit...\n");
	unsigned long rc = ChangeResoInit(buf);
	printf("ChangeResoInit returned: %lu\n", rc);

	printf("\nAfter ChangeResoInit:\n");
	for (int i = 0; i < 8; i++)
		printf("  buf[%d] = 0x%016lx = %lu\n", i, (unsigned long)buf[i], (unsigned long)buf[i]);

	printf("\nOutput fields:\n");
	printf("  dwOutLinePixCnt  = buf[5] = %lu\n", (unsigned long)buf[5]);
	printf("  dwOutLineByte    = buf[6] = %lu\n", (unsigned long)buf[6]);
	printf("  dwOutWriteMaxSize = buf[7] = %lu\n", (unsigned long)buf[7]);

	if (buf[5] == 80 && buf[6] == 80)
		printf("\nPASS: Gray mode output correct (pixCnt=byteCnt=80)\n");
	else if (buf[5] == 80 && buf[6] == 10)
		printf("\nFAIL: B&W mode output (pixCnt=80, byteCnt=10=80/8)\n");
	else
		printf("\nFAIL: unexpected output\n");

	if (rc) ChangeResoClose();
	dlclose(h);
	return 0;
}
