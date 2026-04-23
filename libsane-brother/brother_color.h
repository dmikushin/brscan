/*
 * brother_color.h — 24-bit color (JPEG over USB) decode path for brscan4 models.
 *
 * On DCP-1510 (and other brscan4 scanners), 24-bit Color mode sends a
 * JPEG stream framed into per-block wrappers. This module:
 *   1. Collects the JPEG payload bytes from each block into a temp file.
 *   2. On end-of-page status byte, closes the temp file and opens a
 *      libjpeg decoder on it.
 *   3. Streams decoded RGB scanlines to the SANE output buffer across
 *      multiple PageScan calls.
 *
 * Single-instance (mirrors the rest of this module's global state).
 */
#ifndef _BROTHER_COLOR_H_
#define _BROTHER_COLOR_H_

#include <stddef.h>

typedef enum {
	COLOR_SCAN_IDLE = 0,
	COLOR_SCAN_COLLECTING,
	COLOR_SCAN_DECODING,
	COLOR_SCAN_DONE,
	COLOR_SCAN_ERROR
} ColorScanPhase;

/* Begin a new page: open temp file, set phase=COLLECTING.
 * Returns 0 on success, -1 on error. */
int  brother_color_begin_page(int page_num);

/* Append raw JPEG payload bytes (stripped of block wrappers) to temp file. */
int  brother_color_append_payload(const void *buf, size_t n);

/* Transition COLLECTING → DECODING: close temp, init libjpeg,
 * read header, start decompress. Returns 0 on success. */
int  brother_color_begin_decode(void);

/* Read scanlines into out_buf. Returns bytes written (>=0), or -1 on error.
 * When all scanlines decoded, transitions to DONE.
 * Max bytes written <= out_buf_size (rounded down to whole scanlines). */
int  brother_color_read_scanlines(void *out_buf, int out_buf_size);

/* Query current phase. */
ColorScanPhase brother_color_phase(void);

/* Destroy libjpeg state and delete temp file. Idempotent. */
void brother_color_cleanup(void);

/* Query decoded image geometry (valid after begin_decode). */
int  brother_color_output_width(void);
int  brother_color_output_height(void);

#endif /* _BROTHER_COLOR_H_ */
