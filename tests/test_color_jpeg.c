/*
 * test_color_jpeg.c — unit test for brother_color module.
 *
 * Uses libjpeg to synthesize a small 64x32 RGB gradient JPEG into memory,
 * streams it through brother_color in arbitrary chunks, then verifies the
 * module returns the correct number of scanlines with plausible pixel data.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <jpeglib.h>
#include "brother_color.h"

#define W 64
#define H 32

static unsigned char *synthesize_jpeg(size_t *out_size)
{
	unsigned char *jpeg_buf = NULL;
	unsigned long  jpeg_size = 0;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_mem_dest(&cinfo, &jpeg_buf, &jpeg_size);
	cinfo.image_width = W;
	cinfo.image_height = H;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 85, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	unsigned char row[W*3];
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			row[x*3 + 0] = (unsigned char)(x * 255 / W);   // R
			row[x*3 + 1] = (unsigned char)(y * 255 / H);   // G
			row[x*3 + 2] = 128;                             // B
		}
		JSAMPROW rp = row;
		jpeg_write_scanlines(&cinfo, &rp, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	*out_size = jpeg_size;
	return jpeg_buf;
}

int main(void)
{
	size_t jpeg_size;
	unsigned char *jpeg = synthesize_jpeg(&jpeg_size);
	printf("synthesized JPEG: %zu bytes\n", jpeg_size);
	assert(jpeg && jpeg_size > 100);
	assert(jpeg[0] == 0xFF && jpeg[1] == 0xD8);

	if (brother_color_begin_page(1) != 0) {
		fprintf(stderr, "begin_page failed\n");
		return 1;
	}

	/* Stream the JPEG in arbitrary small chunks */
	size_t off = 0;
	while (off < jpeg_size) {
		size_t chunk = (off + 37 > jpeg_size) ? jpeg_size - off : 37;
		int rc = brother_color_append_payload(jpeg + off, chunk);
		assert(rc == 0);
		off += chunk;
	}

	if (brother_color_begin_decode() != 0) {
		fprintf(stderr, "begin_decode failed\n");
		return 1;
	}
	assert(brother_color_output_width() == W);
	assert(brother_color_output_height() == H);

	/* Read scanlines in small batches */
	unsigned char out[W*H*3];
	memset(out, 0, sizeof(out));
	size_t total = 0;
	while (1) {
		size_t remaining = sizeof(out) - total;
		if (remaining == 0) break;
		/* Limit to 5 rows per call */
		size_t max = remaining > 5*W*3 ? 5*W*3 : remaining;
		int n = brother_color_read_scanlines(out + total, (int)max);
		if (n <= 0) break;
		total += (size_t)n;
	}
	printf("decoded %zu bytes (%zu scanlines)\n", total, total / (W*3));
	assert(total == W*H*3);

	/* Spot-check gradient: top-left should be dark, top-right bright red-ish */
	unsigned char tl_r = out[0];
	unsigned char tr_r = out[(W-1)*3];
	printf("TL R=%u, TR R=%u (expect TR much > TL)\n", tl_r, tr_r);
	assert(tr_r > tl_r + 100);

	/* Bottom-left should have higher G than top-left (y gradient) */
	unsigned char tl_g = out[1];
	unsigned char bl_g = out[(H-1)*W*3 + 1];
	printf("TL G=%u, BL G=%u (expect BL > TL)\n", tl_g, bl_g);
	assert(bl_g > tl_g + 100);

	brother_color_cleanup();
	free(jpeg);
	printf("OK\n");
	return 0;
}
