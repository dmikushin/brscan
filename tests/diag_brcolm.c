/*
 * diag_brcolm.c — Diagnostic for libbrcolm internals.
 * Loads our library and prints intermediate state after init + match.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef int BOOL;
typedef unsigned char BYTE;

#pragma pack(push, 1)
typedef struct { int nRgbLine; int nPaperType; int nMachineId; char *lpLutName; } CMATCH_INIT;
typedef struct { float x, y, z; } BRLUT_DATA;
typedef struct {
	short sLutnum, sGamnum, sGraynum;
	short xmin, xmax, ymin, ymax, zmin, zmax;
	short xgrid, ygrid, zgrid, xspace, yspace, zspace;
} HEAD;
#pragma pack(pop)

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "Usage: %s <lib.so> <lut_file>\n", argv[0]); return 1; }

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

	BOOL (*init)(CMATCH_INIT) = dlsym(h, "ColorMatchingInit");
	void (*end)(void) = dlsym(h, "ColorMatchingEnd");
	BOOL (*match)(BYTE*, long, long) = dlsym(h, "ColorMatching");

	/* Access globals */
	HEAD *head = (HEAD *)dlsym(h, "gLutDataHead");
	BRLUT_DATA **ppLut = (BRLUT_DATA **)dlsym(h, "gpfLutDataTable");
	BRLUT_DATA **ppGam = (BRLUT_DATA **)dlsym(h, "gpfLutGammaTable");
	BRLUT_DATA **ppGray = (BRLUT_DATA **)dlsym(h, "gpfLutGrayTable");
	char *openFile = (char *)dlsym(h, "szgOpenFile");

	if (!init || !end || !match || !head || !ppLut) {
		fprintf(stderr, "Missing symbols\n"); return 1;
	}

	CMATCH_INIT ci = { .nRgbLine = 0, .nPaperType = 0, .nMachineId = 0, .lpLutName = argv[2] };
	BOOL r = init(ci);
	printf("ColorMatchingInit returned: %d\n", r);
	printf("szgOpenFile: '%s'\n", openFile ? openFile : "(null)");
	printf("gLutDataHead: lut=%d gam=%d gray=%d\n", head->sLutnum, head->sGamnum, head->sGraynum);
	printf("  range: X[%d,%d] Y[%d,%d] Z[%d,%d]\n",
	       head->xmin, head->xmax, head->ymin, head->ymax, head->zmin, head->zmax);
	printf("  grid: %dx%dx%d  space: %d,%d,%d\n",
	       head->xgrid, head->ygrid, head->zgrid, head->xspace, head->yspace, head->zspace);
	printf("gpfLutDataTable: %p\n", ppLut ? *ppLut : NULL);
	printf("gpfLutGammaTable: %p\n", ppGam ? *ppGam : NULL);
	printf("gpfLutGrayTable: %p\n", ppGray ? *ppGray : NULL);

	if (r && *ppLut) {
		printf("\nFirst 3 LUT entries:\n");
		for (int i = 0; i < 3; i++)
			printf("  [%d] = (%.2f, %.2f, %.2f)\n", i,
			       (*ppLut)[i].x, (*ppLut)[i].y, (*ppLut)[i].z);

		int lutnum = head->sLutnum;
		printf("Last LUT entry [%d]:\n", lutnum-1);
		printf("  = (%.2f, %.2f, %.2f)\n",
		       (*ppLut)[lutnum-1].x, (*ppLut)[lutnum-1].y, (*ppLut)[lutnum-1].z);

		if (*ppGam) {
			printf("\nGamma entries [0],[85],[170],[255]:\n");
			int off = lutnum;
			int vals[] = {0, 85, 170, 255};
			for (int i = 0; i < 4; i++) {
				int v = vals[i];
				printf("  [%d] = (%.2f, %.2f, %.2f)\n", v,
				       (*ppLut)[off+v].x, (*ppLut)[off+v].y, (*ppLut)[off+v].z);
			}
		}

		/* Test ColorMatching with flat 85 */
		BYTE buf[300]; /* 100 pixels */
		memset(buf, 85, sizeof(buf));
		printf("\nBefore ColorMatching: first pixel = (%d, %d, %d)\n", buf[0], buf[1], buf[2]);
		BOOL mr = match(buf, 300, 1);
		printf("ColorMatching returned: %d\n", mr);
		printf("After ColorMatching: first pixel = (%d, %d, %d)\n", buf[0], buf[1], buf[2]);
		printf("After ColorMatching: second pixel = (%d, %d, %d)\n", buf[3], buf[4], buf[5]);
	}

	end();
	dlclose(h);
	return 0;
}
