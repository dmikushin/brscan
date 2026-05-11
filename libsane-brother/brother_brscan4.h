#ifndef BROTHER_BRSCAN4_H
#define BROTHER_BRSCAN4_H

#ifdef __cplusplus
extern "C" {
#endif

#define BRSCAN4_READ_CACHE_SIZE 0x20400
#define BRSCAN4_MAX_REQUEST_SIZE 0x20200
#define BRSCAN4_MAX_USB_READ_SIZE 0x7FFF

typedef int (*brscan4_read_fn)(void *ctx, unsigned char *dst, int size);

typedef struct Brscan4ReadCache {
	unsigned char data[BRSCAN4_READ_CACHE_SIZE];
	unsigned int len;
} Brscan4ReadCache;

int brscan4_is_boundary_status(unsigned char header);
int brscan4_record_length(const unsigned char *buf,
                          unsigned int len,
                          unsigned int *record_len,
                          unsigned int *payload_offset,
                          unsigned int *payload_len);
int brscan4_status_at_frame_boundary(const unsigned char *buf, unsigned int len);
void brscan4_cache_reset(Brscan4ReadCache *cache);
int brscan4_cache_read(Brscan4ReadCache *cache,
                       brscan4_read_fn read_fn,
                       void *read_ctx,
                       unsigned char *dst,
                       int want);
int brscan4_read_next_record(Brscan4ReadCache *cache,
                             brscan4_read_fn read_fn,
                             void *read_ctx,
                             unsigned char *dst,
                             int maxlen);

#ifdef __cplusplus
}
#endif

#endif
