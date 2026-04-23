/*
 * brother_color.c — 24-bit color JPEG decode for brscan4.
 * See brother_color.h for the module contract.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>

#include <jpeglib.h>
#include <jerror.h>

#include "brother_color.h"
#include "brother_log.h"

#define COLOR_TMPL "/tmp/brscan_jpeg_PAGE%d_XXXXXX"

/* Single-instance state (same ownership model as lpRxBuff etc.). */
static struct {
	ColorScanPhase phase;
	FILE          *fp;            /* temp file: W during COLLECTING, R during DECODING */
	char           path[64];
	int            fd;
	long           total_bytes;   /* payload bytes collected */

	/* libjpeg decoder state, valid in DECODING phase */
	struct jpeg_decompress_struct cinfo;
	struct {
		struct jpeg_error_mgr base;
		jmp_buf               env;
	} err;
	int            cinfo_created; /* jpeg_create_decompress was called */
	int            decompress_started;
} g;

static void jpeg_error_exit(j_common_ptr cinfo)
{
	char msg[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message)(cinfo, msg);
	WriteLog("  color: libjpeg error: %s", msg);
	longjmp(((struct { struct jpeg_error_mgr base; jmp_buf env; } *)cinfo->err)->env, 1);
}

static void jpeg_emit_message(j_common_ptr cinfo, int msg_level)
{
	(void)msg_level;
	char msg[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message)(cinfo, msg);
	WriteLog("  color: libjpeg: %s", msg);
}

ColorScanPhase brother_color_phase(void)
{
	return g.phase;
}

int brother_color_output_width(void)
{
	return (g.phase == COLOR_SCAN_DECODING || g.phase == COLOR_SCAN_DONE)
		? (int)g.cinfo.output_width : 0;
}

int brother_color_output_height(void)
{
	return (g.phase == COLOR_SCAN_DECODING || g.phase == COLOR_SCAN_DONE)
		? (int)g.cinfo.output_height : 0;
}

int brother_color_begin_page(int page_num)
{
	brother_color_cleanup();
	memset(&g, 0, sizeof(g));

	snprintf(g.path, sizeof(g.path), COLOR_TMPL, page_num);
	g.fd = mkstemp(g.path);
	if (g.fd < 0) {
		WriteLog("  color: mkstemp(%s) failed", g.path);
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}
	g.fp = fdopen(g.fd, "w+b");
	if (!g.fp) {
		WriteLog("  color: fdopen failed");
		close(g.fd);
		unlink(g.path);
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}
	g.phase = COLOR_SCAN_COLLECTING;
	g.total_bytes = 0;
	WriteLog("  color: page %d begin (%s)", page_num, g.path);
	return 0;
}

int brother_color_append_payload(const void *buf, size_t n)
{
	if (g.phase != COLOR_SCAN_COLLECTING) return -1;
	if (n == 0) return 0;
	size_t w = fwrite(buf, 1, n, g.fp);
	if (w != n) {
		WriteLog("  color: fwrite short (%zu/%zu)", w, n);
		return -1;
	}
	g.total_bytes += (long)n;
	return 0;
}

int brother_color_begin_decode(void)
{
	if (g.phase != COLOR_SCAN_COLLECTING) return -1;

	if (fflush(g.fp) != 0) {
		WriteLog("  color: fflush failed");
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}
	if (fseek(g.fp, 0, SEEK_SET) != 0) {
		WriteLog("  color: fseek failed");
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}

	WriteLog("  color: begin decode, %ld payload bytes", g.total_bytes);

	g.cinfo.err = jpeg_std_error(&g.err.base);
	g.err.base.error_exit = jpeg_error_exit;
	g.err.base.emit_message = jpeg_emit_message;

	if (setjmp(g.err.env)) {
		WriteLog("  color: jpeg decode setup failed");
		if (g.cinfo_created) jpeg_destroy_decompress(&g.cinfo);
		g.cinfo_created = 0;
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}

	jpeg_create_decompress(&g.cinfo);
	g.cinfo_created = 1;
	jpeg_stdio_src(&g.cinfo, g.fp);

	int rc = jpeg_read_header(&g.cinfo, TRUE);
	if (rc != JPEG_HEADER_OK) {
		WriteLog("  color: jpeg_read_header returned %d", rc);
		jpeg_destroy_decompress(&g.cinfo);
		g.cinfo_created = 0;
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}
	g.cinfo.out_color_space = JCS_RGB;
	g.cinfo.output_components = 3;
	if (!jpeg_start_decompress(&g.cinfo)) {
		WriteLog("  color: jpeg_start_decompress failed");
		jpeg_destroy_decompress(&g.cinfo);
		g.cinfo_created = 0;
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}
	g.decompress_started = 1;

	WriteLog("  color: JPEG %ux%u x%d components",
		g.cinfo.output_width, g.cinfo.output_height, g.cinfo.output_components);

	g.phase = COLOR_SCAN_DECODING;
	return 0;
}

int brother_color_read_scanlines(void *out_buf, int out_buf_size)
{
	if (g.phase != COLOR_SCAN_DECODING) return -1;

	if (setjmp(g.err.env)) {
		WriteLog("  color: jpeg_read_scanlines threw");
		g.phase = COLOR_SCAN_ERROR;
		return -1;
	}

	int row_stride = (int)g.cinfo.output_width * g.cinfo.output_components;
	int max_rows = out_buf_size / row_stride;
	if (max_rows <= 0) return 0;

	unsigned char *dst = (unsigned char *)out_buf;
	int written_rows = 0;

	while (written_rows < max_rows && g.cinfo.output_scanline < g.cinfo.output_height) {
		JSAMPROW rowp[1] = { dst + written_rows * row_stride };
		JDIMENSION got = jpeg_read_scanlines(&g.cinfo, rowp, 1);
		if (got != 1) break;
		written_rows++;
	}

	if (g.cinfo.output_scanline >= g.cinfo.output_height) {
		/* All scanlines consumed — finish decompress now so next call
		 * can cleanly transition to DONE. Leave cleanup to caller. */
		if (setjmp(g.err.env) == 0) {
			jpeg_finish_decompress(&g.cinfo);
		}
		g.phase = COLOR_SCAN_DONE;
		WriteLog("  color: decode complete (%u scanlines)", g.cinfo.output_height);
	}

	return written_rows * row_stride;
}

void brother_color_cleanup(void)
{
	if (g.decompress_started) {
		/* setjmp-guarded in case libjpeg was in error state */
		if (setjmp(g.err.env) == 0) {
			jpeg_abort_decompress(&g.cinfo);
		}
		g.decompress_started = 0;
	}
	if (g.cinfo_created) {
		jpeg_destroy_decompress(&g.cinfo);
		g.cinfo_created = 0;
	}
	if (g.fp) {
		fclose(g.fp);
		g.fp = NULL;
		g.fd = -1;
	}
	if (g.path[0]) {
		unlink(g.path);
		g.path[0] = 0;
	}
	g.phase = COLOR_SCAN_IDLE;
	g.total_bytes = 0;
}
