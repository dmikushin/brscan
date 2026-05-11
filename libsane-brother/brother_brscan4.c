#include "brother_brscan4.h"

#include <string.h>

int brscan4_is_boundary_status(unsigned char header)
{
	/* libsane-brother4.so.1.0.7 make_cache_block first-stage branch:
	 *   0x25ce9 cmp al,0x80; je status
	 *   0x25cf8 cmp al,0xc1; ja status
	 * Bytes 0x81..0xC1 fall through here and are parsed as data-frame
	 * headers at this stage; later status/page state is handled after
	 * frame construction. */
	return header == 0x80 || header > 0xC1;
}

int brscan4_record_length(const unsigned char *buf,
                          unsigned int len,
                          unsigned int *record_len,
                          unsigned int *payload_offset,
                          unsigned int *payload_len)
{
	unsigned int wrapper_len;
	unsigned int data_offset;
	unsigned int data_len;
	unsigned int total;

	if (len < 1)
		return 0;

	if (brscan4_is_boundary_status(buf[0])) {
		if (record_len) *record_len = 1;
		if (payload_offset) *payload_offset = 0;
		if (payload_len) *payload_len = 0;
		return 1;
	}

	if (len < 3)
		return 0;

	wrapper_len = (unsigned int)buf[1] | ((unsigned int)buf[2] << 8);
	data_offset = 3 + wrapper_len;
	if (data_offset + 2 > len)
		return 0;

	data_len = (unsigned int)buf[data_offset] |
	           ((unsigned int)buf[data_offset + 1] << 8);
	total = data_offset + 2 + data_len;
	if (total < 5 || total > len)
		return 0;

	if (record_len) *record_len = total;
	if (payload_offset) *payload_offset = data_offset + 2;
	if (payload_len) *payload_len = data_len;
	return 1;
}

int brscan4_status_at_frame_boundary(const unsigned char *buf, unsigned int len)
{
	unsigned int pos = 0;

	while (pos < len) {
		unsigned int record_len = 0;

		if (brscan4_is_boundary_status(buf[pos]))
			return 1;

		if (!brscan4_record_length(buf + pos, len - pos, &record_len, 0, 0))
			return 0;

		pos += record_len;
	}

	return 0;
}

void brscan4_cache_reset(Brscan4ReadCache *cache)
{
	cache->len = 0;
}

int brscan4_cache_read(Brscan4ReadCache *cache,
                       brscan4_read_fn read_fn,
                       void *read_ctx,
                       unsigned char *dst,
                       int want)
{
	int last_rc = 0;

	if (want > BRSCAN4_MAX_REQUEST_SIZE)
		return -1;

	while ((int)cache->len < want) {
		int free_size = BRSCAN4_READ_CACHE_SIZE - (int)cache->len;
		if (free_size > BRSCAN4_MAX_USB_READ_SIZE)
			free_size = BRSCAN4_MAX_USB_READ_SIZE;

		last_rc = read_fn(read_ctx, cache->data + cache->len, free_size);
		if (last_rc > 0)
			cache->len += (unsigned int)last_rc;
		else
			break;
	}

	if ((int)cache->len >= want) {
		memmove(dst, cache->data, (size_t)want);
		if (cache->len > (unsigned int)want)
			memmove(cache->data,
			        cache->data + want,
			        (size_t)(cache->len - (unsigned int)want));
		cache->len -= (unsigned int)want;
		return want;
	}

	if (cache->len > 0) {
		int partial = (int)cache->len;
		memmove(dst, cache->data, (size_t)partial);
		cache->len = 0;
		return partial;
	}

	return 0;
}

int brscan4_read_next_record(Brscan4ReadCache *cache,
                             brscan4_read_fn read_fn,
                             void *read_ctx,
                             unsigned char *dst,
                             int maxlen)
{
	int got;
	unsigned int record_len;
	unsigned int payload_offset;
	unsigned int payload_len;

	if (maxlen < 1)
		return 0;

	got = brscan4_cache_read(cache, read_fn, read_ctx, dst, 1);
	if (got != 1)
		return got;

	if (brscan4_is_boundary_status(dst[0]))
		return 1;

	if (maxlen < 3)
		return 0;
	got = brscan4_cache_read(cache, read_fn, read_ctx, dst + 1, 2);
	if (got != 2)
		return 0;

	unsigned int wrapper_len = (unsigned int)dst[1] | ((unsigned int)dst[2] << 8);
	unsigned int prefix_len = 3 + wrapper_len + 2;
	if (prefix_len > (unsigned int)maxlen)
		return 0;

	got = brscan4_cache_read(cache, read_fn, read_ctx, dst + 3, (int)(wrapper_len + 2));
	if (got != (int)wrapper_len + 2)
		return 0;

	if (!brscan4_record_length(dst, prefix_len, &record_len, &payload_offset, &payload_len)) {
		payload_offset = prefix_len;
		payload_len = (unsigned int)dst[3 + wrapper_len] |
		              ((unsigned int)dst[3 + wrapper_len + 1] << 8);
		record_len = payload_offset + payload_len;
	}

	if (record_len > (unsigned int)maxlen)
		return 0;

	got = brscan4_cache_read(cache, read_fn, read_ctx, dst + payload_offset, (int)payload_len);
	if (got != (int)payload_len)
		return 0;

	return (int)record_len;
}
