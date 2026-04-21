/*
 * verify_brcolm.c — Verification test for libbrcolm replacement.
 *
 * Loads both the original blob and our replacement via dlopen,
 * feeds identical input to both, and compares outputs byte-by-byte.
 *
 * Usage: ./verify_brcolm <path_to_original.so> <path_to_replacement.so> <lut_file>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <time.h>

typedef int BOOL;
typedef unsigned char BYTE;

#pragma pack(push, 1)
typedef struct {
	int   nRgbLine;
	int   nPaperType;
	int   nMachineId;
	char *lpLutName;
} CMATCH_INIT;
#pragma pack(pop)

typedef BOOL (*ColorMatchingInit_t)(CMATCH_INIT);
typedef void (*ColorMatchingEnd_t)(void);
typedef BOOL (*ColorMatching_t)(BYTE *, long, long);

struct colmatch_lib {
	void *handle;
	ColorMatchingInit_t init;
	ColorMatchingEnd_t  end;
	ColorMatching_t     match;
	const char         *name;
};

static int load_lib(struct colmatch_lib *lib, const char *path, const char *name)
{
	lib->name = name;
	lib->handle = dlopen(path, RTLD_NOW);
	if (!lib->handle) {
		fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
		return -1;
	}
	lib->init  = (ColorMatchingInit_t)dlsym(lib->handle, "ColorMatchingInit");
	lib->end   = (ColorMatchingEnd_t)dlsym(lib->handle, "ColorMatchingEnd");
	lib->match = (ColorMatching_t)dlsym(lib->handle, "ColorMatching");

	if (!lib->init || !lib->end || !lib->match) {
		fprintf(stderr, "%s: missing symbols (init=%p end=%p match=%p)\n",
			name, (void*)lib->init, (void*)lib->end, (void*)lib->match);
		dlclose(lib->handle);
		return -1;
	}
	return 0;
}

static void close_lib(struct colmatch_lib *lib)
{
	if (lib->handle) {
		lib->end();
		dlclose(lib->handle);
		lib->handle = NULL;
	}
}

/*
 * Run a test: initialize both libs with the same LUT file,
 * process identical pixel data, compare results.
 */
static int run_test(struct colmatch_lib *orig, struct colmatch_lib *repl,
		    const char *lutFile, int nRgbLine, int nPaperType,
		    BYTE *pixels, long lineBytes, long lineCount,
		    const char *testName)
{
	/* Make two copies of the pixel data */
	long totalBytes = lineBytes * lineCount;
	BYTE *buf_orig = malloc(totalBytes);
	BYTE *buf_repl = malloc(totalBytes);
	if (!buf_orig || !buf_repl) {
		fprintf(stderr, "malloc failed\n");
		free(buf_orig);
		free(buf_repl);
		return -1;
	}
	memcpy(buf_orig, pixels, totalBytes);
	memcpy(buf_repl, pixels, totalBytes);

	/* Initialize both */
	CMATCH_INIT ci;
	ci.nRgbLine    = nRgbLine;
	ci.nPaperType  = nPaperType;
	ci.nMachineId  = 0;
	ci.lpLutName   = (char *)lutFile;

	BOOL r1 = orig->init(ci);
	BOOL r2 = repl->init(ci);

	if (r1 != r2) {
		printf("FAIL [%s]: init return mismatch: orig=%d repl=%d\n",
		       testName, r1, r2);
		free(buf_orig);
		free(buf_repl);
		return 1;
	}

	if (!r1) {
		printf("SKIP [%s]: both libs failed to init (no LUT data?) — OK\n",
		       testName);
		free(buf_orig);
		free(buf_repl);
		return 0;
	}

	/* Process through both */
	BOOL m1 = orig->match(buf_orig, lineBytes, lineCount);
	BOOL m2 = repl->match(buf_repl, lineBytes, lineCount);

	if (m1 != m2) {
		printf("FAIL [%s]: match return mismatch: orig=%d repl=%d\n",
		       testName, m1, m2);
		orig->end();
		repl->end();
		free(buf_orig);
		free(buf_repl);
		return 1;
	}

	/* Compare outputs */
	int mismatches = 0;
	int max_diff = 0;
	long first_mismatch = -1;

	for (long i = 0; i < totalBytes; i++) {
		int diff = abs((int)buf_orig[i] - (int)buf_repl[i]);
		if (diff > 0) {
			if (first_mismatch < 0) first_mismatch = i;
			mismatches++;
			if (diff > max_diff) max_diff = diff;
		}
	}

	orig->end();
	repl->end();

	if (mismatches == 0) {
		printf("PASS [%s]: %ld bytes, perfect match\n", testName, totalBytes);
	} else {
		printf("FAIL [%s]: %d/%ld bytes differ, max_diff=%d, first@%ld "
		       "(orig=0x%02x repl=0x%02x)\n",
		       testName, mismatches, totalBytes, max_diff, first_mismatch,
		       buf_orig[first_mismatch], buf_repl[first_mismatch]);
	}

	free(buf_orig);
	free(buf_repl);
	return (mismatches > 0) ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "Usage: %s <original.so> <replacement.so> <lut_file> [lut_file2 ...]\n", argv[0]);
		return 1;
	}

	const char *origPath = argv[1];
	const char *replPath = argv[2];

	int total_tests = 0;
	int total_fail = 0;

	/* Test each provided LUT file */
	for (int f = 3; f < argc; f++) {
		const char *lutFile = argv[f];
		printf("\n=== Testing with LUT: %s ===\n", lutFile);

		/* We must load/unload for each LUT file because both libs
		 * use global state */
		struct colmatch_lib orig, repl;
		if (load_lib(&orig, origPath, "original") < 0) return 1;
		if (load_lib(&repl, replPath, "replacement") < 0) {
			dlclose(orig.handle);
			return 1;
		}

		/* Test 1: Gradient — all possible RGB values in a systematic pattern */
		{
			long width = 256 * 3; /* 256 pixels per line */
			long lines = 1;
			BYTE *grad = malloc(width * lines);
			for (int i = 0; i < 256; i++) {
				grad[i * 3 + 0] = i;         /* R */
				grad[i * 3 + 1] = 255 - i;   /* G */
				grad[i * 3 + 2] = i / 2;     /* B */
			}
			char name[256];
			snprintf(name, sizeof(name), "gradient_RGB_%s", lutFile);
			int r = run_test(&orig, &repl, lutFile, 0, 0,
					 grad, width, lines, name);
			total_tests++;
			total_fail += (r > 0);
			free(grad);
		}

		close_lib(&orig);
		close_lib(&repl);

		/* Reload for next test */
		if (load_lib(&orig, origPath, "original") < 0) return 1;
		if (load_lib(&repl, replPath, "replacement") < 0) {
			dlclose(orig.handle);
			return 1;
		}

		/* Test 2: Random pixels */
		{
			srand(42);
			long width = 300 * 3; /* 300 pixels */
			long lines = 10;
			BYTE *rnd = malloc(width * lines);
			for (long i = 0; i < width * lines; i++)
				rnd[i] = rand() & 0xFF;

			char name[256];
			snprintf(name, sizeof(name), "random_RGB_%s", lutFile);
			int r = run_test(&orig, &repl, lutFile, 0, 0,
					 rnd, width, lines, name);
			total_tests++;
			total_fail += (r > 0);
			free(rnd);
		}

		close_lib(&orig);
		close_lib(&repl);

		/* Reload for BGR test */
		if (load_lib(&orig, origPath, "original") < 0) return 1;
		if (load_lib(&repl, replPath, "replacement") < 0) {
			dlclose(orig.handle);
			return 1;
		}

		/* Test 3: BGR mode */
		{
			srand(42);
			long width = 300 * 3;
			long lines = 5;
			BYTE *rnd = malloc(width * lines);
			for (long i = 0; i < width * lines; i++)
				rnd[i] = rand() & 0xFF;

			char name[256];
			snprintf(name, sizeof(name), "random_BGR_%s", lutFile);
			int r = run_test(&orig, &repl, lutFile, 1, 0,
					 rnd, width, lines, name);
			total_tests++;
			total_fail += (r > 0);
			free(rnd);
		}

		close_lib(&orig);
		close_lib(&repl);

		/* Test 4: Edge cases — all black, all white, all same value */
		for (int val = 0; val <= 255; val += 85) {
			if (load_lib(&orig, origPath, "original") < 0) return 1;
			if (load_lib(&repl, replPath, "replacement") < 0) {
				dlclose(orig.handle);
				return 1;
			}

			long width = 100 * 3;
			BYTE *flat = malloc(width);
			memset(flat, val, width);

			char name[256];
			snprintf(name, sizeof(name), "flat_%d_%s", val, lutFile);
			int r = run_test(&orig, &repl, lutFile, 0, 0,
					 flat, width, 1, name);
			total_tests++;
			total_fail += (r > 0);
			free(flat);

			close_lib(&orig);
			close_lib(&repl);
		}
	}

	printf("\n=== Summary: %d/%d tests passed ===\n",
	       total_tests - total_fail, total_tests);

	return total_fail ? 1 : 0;
}
