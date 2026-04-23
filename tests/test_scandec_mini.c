/* Minimal test for brscandec ScanDecOpen */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef int BOOL;
typedef unsigned long DWORD;

typedef struct {
	int nInResoX, nInResoY, nOutResoX, nOutResoY, nColorType;
	DWORD dwInLinePixCnt;
	int nOutDataKind;
	BOOL bLongBoundary;
	DWORD dwOutLinePixCnt, dwOutLineByte, dwOutWriteMaxSize;
} SCANDEC_OPEN;

int main(int argc, char **argv) {
	if (argc < 2) { fprintf(stderr, "Usage: %s lib.so\n", argv[0]); return 1; }

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }

	BOOL (*Open)(SCANDEC_OPEN*) = dlsym(h, "ScanDecOpen");
	void (*SetTbl)(void*, void*) = dlsym(h, "ScanDecSetTblHandle");
	BOOL (*PageStart)(void) = dlsym(h, "ScanDecPageStart");
	BOOL (*Close)(void) = dlsym(h, "ScanDecClose");

	printf("Symbols: Open=%p SetTbl=%p PageStart=%p Close=%p\n",
	       (void*)Open, (void*)SetTbl, (void*)PageStart, (void*)Close);

	SCANDEC_OPEN info = {0};
	info.nInResoX = 200; info.nInResoY = 200;
	info.nOutResoX = 200; info.nOutResoY = 200;
	info.nColorType = 0x208;  /* SCCLR_TYPE_TG */
	info.dwInLinePixCnt = 100;
	info.nOutDataKind = 1;  /* SCODK_PIXEL_RGB */
	info.bLongBoundary = 0;

	printf("Calling ScanDecOpen...\n");
	printf("  sizeof(SCANDEC_OPEN) = %zu\n", sizeof(SCANDEC_OPEN));
	printf("  &nColorType offset = %zu\n", (char*)&info.nColorType - (char*)&info);
	printf("  &dwInLinePixCnt offset = %zu\n", (char*)&info.dwInLinePixCnt - (char*)&info);
	printf("  &nOutDataKind offset = %zu\n", (char*)&info.nOutDataKind - (char*)&info);

	BOOL r = Open(&info);
	printf("ScanDecOpen returned: %d\n", r);

	if (r) {
		printf("  dwOutLinePixCnt = %lu\n", info.dwOutLinePixCnt);
		printf("  dwOutLineByte = %lu\n", info.dwOutLineByte);
		printf("  dwOutWriteMaxSize = %lu\n", info.dwOutWriteMaxSize);

		printf("Calling SetTblHandle(NULL, NULL)...\n");
		SetTbl(NULL, NULL);

		printf("Calling ScanDecPageStart...\n");
		BOOL ps = PageStart();
		printf("ScanDecPageStart returned: %d\n", ps);

		if (ps) {
			/* Try ScanDecWrite with uncompressed gray data */
			typedef struct {
				int nInDataComp, nInDataKind;
				char *pLineData;
				DWORD dwLineDataSize;
				char *pWriteBuff;
				DWORD dwWriteBuffSize;
				BOOL bReverWrite;
			} SCANDEC_WRITE;
			typedef DWORD (*Write_t)(SCANDEC_WRITE*, int*);
			Write_t Write = (Write_t)dlsym(h, "ScanDecWrite");

			DWORD outMax = info.dwOutWriteMaxSize;
			if (outMax == 0) outMax = 4096;
			char *outBuf = calloc(1, outMax);
			char lineData[100];
			for (int i = 0; i < 100; i++) lineData[i] = i * 2;

			SCANDEC_WRITE sw = {0};
			sw.nInDataComp = 2;  /* SCIDC_NONCOMP */
			sw.nInDataKind = 1;  /* SCIDK_MONO */
			sw.pLineData = lineData;
			sw.dwLineDataSize = 100;
			sw.pWriteBuff = outBuf;
			sw.dwWriteBuffSize = outMax;
			sw.bReverWrite = 0;

			printf("Calling ScanDecWrite...\n");
			printf("  sizeof(SCANDEC_WRITE) = %zu\n", sizeof(SCANDEC_WRITE));
			printf("  &pLineData offset = %zu\n", (char*)&sw.pLineData - (char*)&sw);
			printf("  &pWriteBuff offset = %zu\n", (char*)&sw.pWriteBuff - (char*)&sw);
			printf("  &bReverWrite offset = %zu\n", (char*)&sw.bReverWrite - (char*)&sw);
			int lineCount = 0;
			DWORD ws = Write(&sw, &lineCount);
			printf("ScanDecWrite returned: size=%lu lines=%d\n", ws, lineCount);
			free(outBuf);
		}

		printf("Calling ScanDecClose...\n");
		Close();
	}

	dlclose(h);
	printf("Done.\n");
	return 0;
}
