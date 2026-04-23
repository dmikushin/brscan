/*
 * diag_brcolm2.c — Detailed interpolation diagnostic.
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
typedef struct {
	short sData_X, sData_Y, sData_Z;
	short sIndex_X, sIndex_Y, sIndex_Z;
} INPUT_DATA;
#pragma pack(pop)

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "Usage: %s <lib.so> <lut_file>\n", argv[0]); return 1; }

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

	BOOL (*init)(CMATCH_INIT) = dlsym(h, "ColorMatchingInit");
	void (*fini)(void) = dlsym(h, "ColorMatchingEnd");
	BOOL (*matchfn)(BYTE*, long, long) = dlsym(h, "ColorMatching");
	BOOL (*matchDataGet)(INPUT_DATA*) = dlsym(h, "MatchDataGet");

	HEAD *head = (HEAD *)dlsym(h, "gLutDataHead");
	BRLUT_DATA **ppLut = (BRLUT_DATA **)dlsym(h, "gpfLutDataTable");
	BRLUT_DATA **ppGam = (BRLUT_DATA **)dlsym(h, "gpfLutGammaTable");

	CMATCH_INIT ci = {0, 0, 0, argv[2]};
	BOOL r = init(ci);
	printf("init=%d, lut=%d gam=%d gray=%d, grid=%dx%dx%d space=%d,%d,%d\n",
	       r, head->sLutnum, head->sGamnum, head->sGraynum,
	       head->xgrid, head->ygrid, head->zgrid,
	       head->xspace, head->yspace, head->zspace);

	if (!r) return 1;

	int xg = head->xgrid, yg = head->ygrid;
	BRLUT_DATA *lut = *ppLut;

	/* Print LUT entries around the interpolation point for input (84,84,84) */
	/* Grid coord = 84/32 = 2.625, so cells at ix=2,3, iy=2,3, iz=2,3 */
	printf("\nLUT entries for grid cells around (2,2,2)-(3,3,3):\n");
	for (int iz = 2; iz <= 3; iz++)
		for (int iy = 2; iy <= 3; iy++)
			for (int ix = 2; ix <= 3; ix++) {
				int idx = iz * yg * xg + iy * xg + ix;
				printf("  [%d,%d,%d] idx=%d: (%.1f, %.1f, %.1f)\n",
				       ix, iy, iz, idx, lut[idx].x, lut[idx].y, lut[idx].z);
			}

	/* Also try reversed indexing: X varies slowest */
	printf("\nLUT entries with REVERSED order (X slowest):\n");
	for (int ix = 2; ix <= 3; ix++)
		for (int iy = 2; iy <= 3; iy++)
			for (int iz = 2; iz <= 3; iz++) {
				int idx = ix * yg * xg + iy * xg + iz;
				printf("  [%d,%d,%d] idx=%d: (%.1f, %.1f, %.1f)\n",
				       ix, iy, iz, idx, lut[idx].x, lut[idx].y, lut[idx].z);
			}

	/* Call MatchDataGet directly with (84,84,84) */
	if (matchDataGet) {
		INPUT_DATA id = {84, 84, 84, 0, 0, 0};
		printf("\nMatchDataGet input: (%d,%d,%d)\n", id.sData_X, id.sData_Y, id.sData_Z);
		BOOL mr = matchDataGet(&id);
		printf("MatchDataGet returned: %d, output: (%d,%d,%d)\n", mr, id.sData_X, id.sData_Y, id.sData_Z);
	}

	/* Also try with different orderings */
	if (matchDataGet) {
		/* What if the blob expects (R,G,B) = (X,Y,Z) order? */
		INPUT_DATA id2 = {84, 84, 84, 0, 0, 0};
		BOOL mr = matchDataGet(&id2);
		printf("MatchDataGet(84,84,84): (%d,%d,%d)\n", id2.sData_X, id2.sData_Y, id2.sData_Z);
	}

	/* Test ColorMatching directly with known pixels */
	printf("\nFull ColorMatching pipeline:\n");
	BYTE pixels[] = {85, 85, 85,  0, 0, 0,  255, 255, 255,  128, 64, 192};
	int npix = 4;
	printf("Before:\n");
	for (int i = 0; i < npix; i++)
		printf("  pixel[%d] = (%d, %d, %d)\n", i, pixels[i*3], pixels[i*3+1], pixels[i*3+2]);

	matchfn(pixels, npix * 3, 1);
	printf("After:\n");
	for (int i = 0; i < npix; i++)
		printf("  pixel[%d] = (%d, %d, %d)\n", i, pixels[i*3], pixels[i*3+1], pixels[i*3+2]);

	fini();
	dlclose(h);
	return 0;
}
