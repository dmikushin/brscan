/*
 * brcolm.c — Open-source replacement for Brother libbrcolm color matching library
 *
 * Reverse-engineered from the x86-64 binary blob libbrcolm.so.1.0.1
 * (compiled with GCC 3.4.2, circa 2004, Brother Industries).
 *
 * The library loads 3D LUT (Look-Up Table) color correction data from
 * Brother scanner calibration files (.dat/.cm), and applies trilinear
 * interpolation to transform scanned RGB pixel data.
 *
 * File format (BLCM):
 *   - 4-byte file ID: "BLCM"
 *   - Array of 8-byte entries: { char ID[4]; int32_t offset; }
 *     terminated by "END " entry
 *   - At the offset (SEEK_CUR from entry position):
 *     - 2-byte version (0 or 1)
 *     - Version 0: BRLUT_HEAD_DATA_00 (floats + shorts, 30 bytes)
 *     - Version 1: BRLUT_HEAD_DATA_01 (all shorts, 30 bytes)
 *     - LUT data: sLutnum × BRLUT_DATA (12 bytes each)
 *     - Gamma data: sGamnum × BRLUT_DATA (optional)
 *     - Gray data: sGraynum × BRLUT_DATA (optional)
 *
 * Copyright 2024 Dmitry Mikushin. Licensed under GPLv2+.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * Type definitions matching Brother's Windows-origin codebase.
 */
typedef int             BOOL;
typedef unsigned char   BYTE;
typedef void           *HANDLE;
typedef char           *LPSTR;

#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

/* Color data order */
#define CMATCH_DATALINE_RGB 0
#define CMATCH_DATALINE_BGR 1

/* Paper types */
#define MEDIA_PLAIN     0
#define MEDIA_PHOTO     6
#define MEDIA_ADF_STD   7
#define MEDIA_ADF_PHOTO 8

/* LUT table IDs */
#define LUT_FILE_ID        "BLCM"
#define DEFAULT_LUT        "DLUT"
#define DEFAULT_PHOTO_LUT  "PHTO"
#define ADF_STD_LUT        "DADF"
#define ADF_PHOTO_LUT      "PADF"

/*
 * Packed structures — must match the on-disk file format and the
 * original blob's in-memory layout (compiled with #pragma pack(1)).
 */
#pragma pack(push, 1)

typedef struct {
	int   nRgbLine;
	int   nPaperType;
	int   nMachineId;
	LPSTR lpLutName;
} CMATCH_INIT;

typedef struct {
	float fLutXData;
	float fLutYData;
	float fLutZData;
} BRLUT_DATA;

/* Version 0 header — uses floats for min/max, shorts for split counts */
typedef struct {
	float s3D_Xmin;
	float s3D_Xmax;
	float s3D_Ymin;
	float s3D_Ymax;
	float s3D_Zmin;
	float s3D_Zmax;
	short s3D_Xsplit;
	short s3D_Ysplit;
	short s3D_Zsplit;
} BRLUT_HEAD_DATA_00;

/* Version 1 header (also used as the normalized internal format) */
typedef struct {
	short sLutnum;
	short sGamnum;
	short sGraynum;
	short s3D_Xmin;
	short s3D_Xmax;
	short s3D_Ymin;
	short s3D_Ymax;
	short s3D_Zmin;
	short s3D_Zmax;
	short s3D_Xgrid;
	short s3D_Ygrid;
	short s3D_Zgrid;
	short s3D_Xspace;
	short s3D_Yspace;
	short s3D_Zspace;
} BRLUT_HEAD_DATA_01;

/* Input/output data for MatchDataGet */
typedef struct {
	short sData_X;
	short sData_Y;
	short sData_Z;
	short sIndex_X;
	short sIndex_Y;
	short sIndex_Z;
} BRLUT_INPUT_DATA;

/* File header entry */
typedef struct {
	char    TableID[4];
	int32_t lTableOffset;
} BRLUT_FILE_ENTRY;

#pragma pack(pop)

/*
 * Global state — exported symbols matching the original blob.
 */
CMATCH_INIT        gMatchingInitDat;
char                szgOpenFile[160];
BRLUT_HEAD_DATA_00  gLutDataHead00;
BRLUT_HEAD_DATA_01  gLutDataHead;

HANDLE     ghLutDataTable  = NULL;
BRLUT_DATA *gpfLutDataTable = NULL;

HANDLE     ghLutGammaTable  = NULL;
BRLUT_DATA *gpfLutGammaTable = NULL;

HANDLE     ghLutGrayTable  = NULL;
BRLUT_DATA *gpfLutGrayTable = NULL;

HANDLE ghGammaCurveTable    = NULL;
BYTE  *gpbGammaCurveTable   = NULL;
HANDLE ghHashTable          = NULL;
void  *gpHashTable          = NULL;
int    gnColorHashFlag      = 0;
char   szgWinSystem[145];
char   szgLutDir[145];

static int nMallocCnt = 0;
static int nFreeCnt   = 0;

/*
 * bugchk_malloc / bugchk_free — simplified versions without guard bytes.
 * The original blob had guard-byte checking; we just wrap malloc/free.
 */
void *bugchk_malloc(size_t size, const char *filename, int line)
{
	nMallocCnt++;
	void *p = malloc(size);
	if (!p) {
		fprintf(stderr, "bugchk_malloc(size=%zu), can't allocate@%s(%d)\n",
			size, filename, line);
		abort();
	}
	return p;
}

void bugchk_free(void *ptr, const char *filename, int line)
{
	nFreeCnt++;
	if (!ptr) {
		fprintf(stderr, "bugchk_free(ptr=%p)@%s(%d)\n", ptr, filename, line);
		abort();
	}
	free(ptr);
}

/*
 * InitLUT — Load a 3D color LUT from a BLCM-format file.
 *
 * The file path is in szgOpenFile (set by ColorMatchingInit).
 * pcLutKind selects which table to load ("DLUT", "PHTO", etc.).
 */
BOOL InitLUT(char *pcLutKind)
{
	FILE *fp = fopen(szgOpenFile, "rb");
	if (!fp)
		return FALSE;

	/* Verify file ID = "BLCM" */
	char fileId[5] = {0};
	if (fread(fileId, 1, 4, fp) != 4 || strcmp(fileId, LUT_FILE_ID) != 0) {
		fclose(fp);
		return FALSE;
	}

	/* Scan the table-of-contents entries for the requested LUT kind.
	 * Each entry is 8 bytes: 4-char ID + 4-byte offset.
	 * The list is terminated by an "END " entry. */
	BRLUT_FILE_ENTRY entry;
	int found = 0;

	while (fread(&entry, 1, sizeof(entry), fp) == sizeof(entry)) {
		char entryId[5] = {0};
		memcpy(entryId, entry.TableID, 4);

		if (strcmp(entryId, pcLutKind) == 0) {
			found = 1;
			break;
		}
		if (strcmp(entryId, "END ") == 0)
			break;
	}

	if (!found) {
		fclose(fp);
		return FALSE;
	}

	/* Seek forward by lTableOffset from current position (SEEK_CUR).
	 * This skips past any remaining TOC entries to the LUT data. */
	fseek(fp, entry.lTableOffset, SEEK_CUR);

	/* Read 2-byte version */
	short version = -1;
	if (fread(&version, 1, 2, fp) != 2) {
		fclose(fp);
		return FALSE;
	}

	if (version == 0) {
		/* Version 0: float-based header */
		if (fread(&gLutDataHead00, 1, sizeof(gLutDataHead00), fp)
		    != sizeof(gLutDataHead00)) {
			fclose(fp);
			return FALSE;
		}

		/* Convert to the normalized internal format (all shorts) */
		gLutDataHead.sLutnum = (gLutDataHead00.s3D_Xsplit + 1)
				     * (gLutDataHead00.s3D_Ysplit + 1)
				     * (gLutDataHead00.s3D_Zsplit + 1);
		gLutDataHead.sGamnum  = 0;
		gLutDataHead.sGraynum = 0;

		gLutDataHead.s3D_Xmin = (short)gLutDataHead00.s3D_Xmin;
		gLutDataHead.s3D_Xmax = (short)gLutDataHead00.s3D_Xmax;
		gLutDataHead.s3D_Ymin = (short)gLutDataHead00.s3D_Ymin;
		gLutDataHead.s3D_Ymax = (short)gLutDataHead00.s3D_Ymax;
		gLutDataHead.s3D_Zmin = (short)gLutDataHead00.s3D_Zmin;
		gLutDataHead.s3D_Zmax = (short)gLutDataHead00.s3D_Zmax;

		gLutDataHead.s3D_Xgrid = gLutDataHead00.s3D_Xsplit + 1;
		gLutDataHead.s3D_Ygrid = gLutDataHead00.s3D_Ysplit + 1;
		gLutDataHead.s3D_Zgrid = gLutDataHead00.s3D_Zsplit + 1;

		/* Grid spacing: (max - min + 1) / splits  (integer division) */
		gLutDataHead.s3D_Xspace = (short)((int)(gLutDataHead00.s3D_Xmax
			- gLutDataHead00.s3D_Xmin + 1.0f) / gLutDataHead00.s3D_Xsplit);
		gLutDataHead.s3D_Yspace = (short)((int)(gLutDataHead00.s3D_Ymax
			- gLutDataHead00.s3D_Ymin + 1.0f) / gLutDataHead00.s3D_Ysplit);
		gLutDataHead.s3D_Zspace = (short)((int)(gLutDataHead00.s3D_Zmax
			- gLutDataHead00.s3D_Zmin + 1.0f) / gLutDataHead00.s3D_Zsplit);

	} else if (version == 1) {
		/* Version 1: short-based header — read directly */
		if (fread(&gLutDataHead, 1, sizeof(gLutDataHead), fp)
		    != sizeof(gLutDataHead)) {
			fclose(fp);
			return FALSE;
		}
	} else {
		fclose(fp);
		return FALSE;
	}

	/* Validate grid dimensions */
	if (gLutDataHead.s3D_Xspace == 0 || gLutDataHead.s3D_Yspace == 0
	    || gLutDataHead.s3D_Zspace == 0) {
		fclose(fp);
		return FALSE;
	}

	/* Allocate and read all LUT + gamma + gray data as one contiguous array */
	long totalEntries = (long)gLutDataHead.sLutnum
			  + (long)gLutDataHead.sGamnum
			  + (long)gLutDataHead.sGraynum;

	size_t dataSize = (size_t)totalEntries * sizeof(BRLUT_DATA);
	ghLutDataTable = malloc(dataSize);
	if (!ghLutDataTable) {
		fclose(fp);
		return FALSE;
	}
	gpfLutDataTable = (BRLUT_DATA *)ghLutDataTable;

	if (fread(gpfLutDataTable, sizeof(BRLUT_DATA), totalEntries, fp)
	    != (size_t)totalEntries) {
		free(ghLutDataTable);
		ghLutDataTable  = NULL;
		gpfLutDataTable = NULL;
		fclose(fp);
		return FALSE;
	}

	fclose(fp);

	/* Set up pointers into the contiguous array */
	gpfLutGammaTable = NULL;
	gpfLutGrayTable  = NULL;

	if (gLutDataHead.sGamnum > 0) {
		gpfLutGammaTable = gpfLutDataTable + gLutDataHead.sLutnum;
		ghLutGammaTable  = gpfLutGammaTable;
	}
	if (gLutDataHead.sGraynum > 0) {
		gpfLutGrayTable = gpfLutDataTable + gLutDataHead.sLutnum
				+ gLutDataHead.sGamnum;
		ghLutGrayTable  = gpfLutGrayTable;
	}

	return TRUE;
}

/*
 * MatchDataGet — 3D trilinear interpolation in the LUT grid.
 *
 * Input: lutInput->sData_X/Y/Z = pixel channel values (B, G, R)
 * Output: overwrites sData_X/Y/Z with the interpolated result.
 */
BOOL MatchDataGet(BRLUT_INPUT_DATA *lutInput)
{
	if (!gpfLutDataTable)
		return FALSE;

	int xgrid = gLutDataHead.s3D_Xgrid;
	int ygrid = gLutDataHead.s3D_Ygrid;
	int xspace = gLutDataHead.s3D_Xspace;
	int yspace = gLutDataHead.s3D_Yspace;
	int zspace = gLutDataHead.s3D_Zspace;

	/* Map input values to grid coordinates */
	float fx = (float)(lutInput->sData_X - gLutDataHead.s3D_Xmin) / (float)xspace;
	float fy = (float)(lutInput->sData_Y - gLutDataHead.s3D_Ymin) / (float)yspace;
	float fz = (float)(lutInput->sData_Z - gLutDataHead.s3D_Zmin) / (float)zspace;

	/* Clamp to valid grid range */
	int ix = (int)fx;
	int iy = (int)fy;
	int iz = (int)fz;

	if (ix < 0) ix = 0;
	if (iy < 0) iy = 0;
	if (iz < 0) iz = 0;
	if (ix >= xgrid - 1) ix = xgrid - 2;
	if (iy >= ygrid - 1) iy = ygrid - 2;
	if (iz >= gLutDataHead.s3D_Zgrid - 1) iz = gLutDataHead.s3D_Zgrid - 2;

	/* Fractional position within the cell */
	float tx = fx - (float)ix;
	float ty = fy - (float)iy;
	float tz = fz - (float)iz;

	if (tx < 0.0f) tx = 0.0f;
	if (ty < 0.0f) ty = 0.0f;
	if (tz < 0.0f) tz = 0.0f;
	if (tx > 1.0f) tx = 1.0f;
	if (ty > 1.0f) ty = 1.0f;
	if (tz > 1.0f) tz = 1.0f;

	/* Fetch the 8 corner values of the enclosing grid cell.
	 * Grid layout: index = iz * Ygrid * Xgrid + iy * Xgrid + ix */
	#define LUTIDX(x, y, z) ((z) * ygrid * xgrid + (y) * xgrid + (x))

	BRLUT_DATA *c000 = &gpfLutDataTable[LUTIDX(ix,   iy,   iz  )];
	BRLUT_DATA *c100 = &gpfLutDataTable[LUTIDX(ix+1, iy,   iz  )];
	BRLUT_DATA *c010 = &gpfLutDataTable[LUTIDX(ix,   iy+1, iz  )];
	BRLUT_DATA *c110 = &gpfLutDataTable[LUTIDX(ix+1, iy+1, iz  )];
	BRLUT_DATA *c001 = &gpfLutDataTable[LUTIDX(ix,   iy,   iz+1)];
	BRLUT_DATA *c101 = &gpfLutDataTable[LUTIDX(ix+1, iy,   iz+1)];
	BRLUT_DATA *c011 = &gpfLutDataTable[LUTIDX(ix,   iy+1, iz+1)];
	BRLUT_DATA *c111 = &gpfLutDataTable[LUTIDX(ix+1, iy+1, iz+1)];

	#undef LUTIDX

	/* Trilinear interpolation for each output channel */
	#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))

	float outX = LERP(LERP(LERP(c000->fLutXData, c100->fLutXData, tx),
				LERP(c010->fLutXData, c110->fLutXData, tx), ty),
			   LERP(LERP(c001->fLutXData, c101->fLutXData, tx),
				LERP(c011->fLutXData, c111->fLutXData, tx), ty), tz);

	float outY = LERP(LERP(LERP(c000->fLutYData, c100->fLutYData, tx),
				LERP(c010->fLutYData, c110->fLutYData, tx), ty),
			   LERP(LERP(c001->fLutYData, c101->fLutYData, tx),
				LERP(c011->fLutYData, c111->fLutYData, tx), ty), tz);

	float outZ = LERP(LERP(LERP(c000->fLutZData, c100->fLutZData, tx),
				LERP(c010->fLutZData, c110->fLutZData, tx), ty),
			   LERP(LERP(c001->fLutZData, c101->fLutZData, tx),
				LERP(c011->fLutZData, c111->fLutZData, tx), ty), tz);

	#undef LERP

	/* Truncate (not round) — matching the blob's cvttss2si behavior */
	lutInput->sData_X = (short)outX;
	lutInput->sData_Y = (short)outY;
	lutInput->sData_Z = (short)outZ;

	return TRUE;
}

/*
 * LutColorMatching — Apply 3D LUT color correction to a scanline.
 *
 * Verified against original blob disassembly:
 * 1. Read pixel bytes (RGB or BGR order)
 * 2. Map to internal (X=R, Y=G, Z=B)
 * 3. Optional gamma pre-correction from gpfLutGammaTable
 * 4. Compute grid indices: value / xspace, clamped to xspace-2
 * 5. Call MatchDataGet (modifies sData_X/Y/Z in place)
 * 6. Write output (X=R, Y=G, Z=B back to pixel order)
 * NOTE: No gray post-correction — the blob doesn't apply it here.
 */
BOOL LutColorMatching(BYTE *pRgbData, long lRgbDataLength, long lLineCount)
{
	if (!gpfLutDataTable)
		return FALSE;

	int xspace = gLutDataHead.s3D_Xspace;
	long numPixels = lRgbDataLength / 3;
	int isBGR = (gMatchingInitDat.nRgbLine == CMATCH_DATALINE_BGR);
	BYTE *inPtr = pRgbData;
	BYTE *outPtr = pRgbData;

	for (long p = 0; p < numPixels; p++) {
		short valR, valG, valB;

		/* Read pixel — internal mapping: X=R, Y=G, Z=B */
		if (isBGR) {
			valB = *inPtr++;  /* byte[0] = B */
			valG = *inPtr++;  /* byte[1] = G */
			valR = *inPtr++;  /* byte[2] = R */
		} else {
			valR = *inPtr++;  /* byte[0] = R */
			valG = *inPtr++;  /* byte[1] = G */
			valB = *inPtr++;  /* byte[2] = B */
		}

		/* Optional gamma pre-correction:
		 * R uses fLutXData, G uses fLutYData, B uses fLutZData */
		if (gLutDataHead.sGamnum > 0 && gpfLutGammaTable) {
			valR = (short)gpfLutGammaTable[valR].fLutXData;
			valG = (short)gpfLutGammaTable[valG].fLutYData;
			valB = (short)gpfLutGammaTable[valB].fLutZData;
		}

		/* Build BRLUT_INPUT_DATA: data values + pre-computed grid indices */
		BRLUT_INPUT_DATA lutInput;
		lutInput.sData_X = valR;
		lutInput.sData_Y = valG;
		lutInput.sData_Z = valB;

		/* Grid indices: value / xspace, clamped to xspace - 2 */
		short idxR = valR / xspace;
		short idxG = valG / xspace;
		short idxB = valB / xspace;
		if (idxR > xspace - 2) idxR = xspace - 2;
		if (idxG > xspace - 2) idxG = xspace - 2;
		if (idxB > xspace - 2) idxB = xspace - 2;
		lutInput.sIndex_X = idxR;
		lutInput.sIndex_Y = idxG;
		lutInput.sIndex_Z = idxB;

		if (!MatchDataGet(&lutInput)) {
			return FALSE;
		}

		/* Write back: X=R, Y=G, Z=B to pixel order */
		if (isBGR) {
			*outPtr++ = (BYTE)lutInput.sData_Z;  /* B */
			*outPtr++ = (BYTE)lutInput.sData_Y;  /* G */
			*outPtr++ = (BYTE)lutInput.sData_X;  /* R */
		} else {
			*outPtr++ = (BYTE)lutInput.sData_X;  /* R */
			*outPtr++ = (BYTE)lutInput.sData_Y;  /* G */
			*outPtr++ = (BYTE)lutInput.sData_Z;  /* B */
		}
	}

	return TRUE;
}

/*
 * ColorMatchingInit — Initialize color matching from a LUT calibration file.
 *
 * matchingInitDat.lpLutName is the full path to the .dat/.cm file.
 * matchingInitDat.nPaperType selects which LUT table to load.
 */
BOOL ColorMatchingInit(CMATCH_INIT matchingInitDat)
{
	gMatchingInitDat = matchingInitDat;
	strcpy(szgOpenFile, matchingInitDat.lpLutName);

	/* Select the LUT table ID based on paper type */
	char lutKind[8];
	switch (matchingInitDat.nPaperType) {
	case MEDIA_PHOTO:
		strcpy(lutKind, DEFAULT_PHOTO_LUT);
		break;
	case MEDIA_ADF_STD:
		strcpy(lutKind, ADF_STD_LUT);
		break;
	case MEDIA_ADF_PHOTO:
		strcpy(lutKind, ADF_PHOTO_LUT);
		break;
	default:
		strcpy(lutKind, DEFAULT_LUT);
		break;
	}

	return InitLUT(lutKind);
}

/*
 * ColorMatchingEnd — Terminate color matching.
 *
 * The original blob was a complete no-op (just push/pop rbp).
 * We clean up properly.
 */
void ColorMatchingEnd(void)
{
	if (ghLutDataTable) {
		free(ghLutDataTable);
		ghLutDataTable  = NULL;
		gpfLutDataTable = NULL;
	}
	gpfLutGammaTable = NULL;
	gpfLutGrayTable  = NULL;
	ghLutGammaTable  = NULL;
	ghLutGrayTable   = NULL;
}

/*
 * ColorMatching — Apply color correction to RGB scan data.
 *
 * pRgbData:        pointer to pixel data (3 bytes per pixel)
 * lRgbDataLength:  number of bytes per line
 * lLineCount:      number of lines to process
 */
BOOL ColorMatching(BYTE *pRgbData, long lRgbDataLength, long lLineCount)
{
	if (!gpfLutDataTable)
		return TRUE;  /* No LUT loaded — pass through (matches blob behavior) */

	return LutColorMatching(pRgbData, lRgbDataLength, lLineCount);
}
