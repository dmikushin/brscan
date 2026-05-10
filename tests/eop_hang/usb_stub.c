/*
 * usb_stub.c — LD_PRELOAD shim for libusb-0.1, providing canned responses
 * sufficient to drive libsane-brother through sane_init / sane_open /
 * sane_start without a physical scanner attached.
 *
 * The bulk-read replay (which is the bit that actually controls the
 * parser-under-test) lives inside libsane-brother's brother_devaccs.c,
 * activated by BRSCAN_REPLAY_FILE. This stub returns zero from
 * usb_bulk_read so the shim's "no replay configured" path stays harmless;
 * with replay configured the shim never even calls usb_bulk_read.
 *
 * The fake device advertises Brother DCP-1510 (VID 0x04F9 / PID 0x02D0)
 * because that is what the captured stream was recorded from.
 *
 * usb_control_msg fakes the success response brscan's OpenDevice expects:
 *   data = 05 10 01 02 00   (length=5, descr=0x10, BREQ_GET_OPEN=0x01,
 *                            BCOMMAND_SCANNER=0x0002, no BCOMMAND_RETURN)
 *
 * Build:  gcc -shared -fPIC -o usb_stub.so usb_stub.c
 * Use:    LD_PRELOAD=/abs/path/usb_stub.so <scanimage|harness>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <usb.h>

/* ------------------------- fake topology ------------------------- */

static struct usb_bus     g_fake_bus;
static struct usb_device  g_fake_dev;

/* libusb-0.1 exports usb_busses as a global; brscan reads it directly
 * (not just via usb_get_busses), so we must override it too. With
 * LD_PRELOAD our copy of the symbol wins over the one in libusb.so. */
struct usb_bus *usb_busses = NULL;

static void ensure_topology(void)
{
    static int built = 0;
    if (built) return;
    built = 1;

    memset(&g_fake_bus, 0, sizeof(g_fake_bus));
    memset(&g_fake_dev, 0, sizeof(g_fake_dev));

    /* Just enough fields for libsane-brother's bus walk to recognise the
     * DCP-1510. brscan reads only the descriptor.idVendor/idProduct pair
     * and follows ->next pointers. */
    g_fake_dev.descriptor.idVendor  = 0x04F9;
    g_fake_dev.descriptor.idProduct = 0x02D0;
    g_fake_dev.bus                  = &g_fake_bus;
    g_fake_dev.next                 = NULL;
    g_fake_dev.prev                 = NULL;

    snprintf(g_fake_bus.dirname, sizeof(g_fake_bus.dirname), "006");
    g_fake_bus.devices = &g_fake_dev;
    g_fake_bus.next    = NULL;
    g_fake_bus.prev    = NULL;

    usb_busses = &g_fake_bus;
}

/* ------------------------- libusb-0.1 stubs ---------------------- */

static int g_trace = -1;
static void trace(const char *fmt, ...)
{
    if (g_trace < 0) g_trace = (getenv("BRSCAN_USBSTUB_TRACE") != NULL);
    if (!g_trace) return;
    va_list ap;
    va_start(ap, fmt);
    fputs("[usb_stub] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void usb_init(void)             { trace("usb_init"); ensure_topology(); }
int  usb_find_busses(void)      { trace("usb_find_busses"); ensure_topology(); return 1; }
int  usb_find_devices(void)     { trace("usb_find_devices"); ensure_topology(); return 1; }

struct usb_bus *usb_get_busses(void)
{
    trace("usb_get_busses → %p (idVendor=04F9 idProduct=02D0)", (void*)&g_fake_bus);
    ensure_topology();
    return &g_fake_bus;
}

/* The handle is opaque to brscan — only its address is compared / passed
 * back to libusb. We hand out one shared static block. */
static struct { int dummy; } g_fake_handle;

usb_dev_handle *usb_open(struct usb_device *dev)
{
    trace("usb_open(dev=%p) → %p", (void*)dev, (void*)&g_fake_handle);
    (void)dev;
    return (usb_dev_handle *)&g_fake_handle;
}
int usb_close(usb_dev_handle *dev)               { trace("usb_close"); (void)dev; return 0; }
int usb_set_configuration(usb_dev_handle *dev,int c){ trace("usb_set_configuration(c=%d)", c); (void)dev; return 0; }
int usb_claim_interface(usb_dev_handle *dev,int i)  { trace("usb_claim_interface(i=%d)", i); (void)dev; return 0; }
int usb_release_interface(usb_dev_handle *dev,int i){ trace("usb_release_interface(i=%d)", i); (void)dev; return 0; }
int usb_clear_halt(usb_dev_handle *dev,unsigned ep){ trace("usb_clear_halt(ep=0x%02x)", ep); (void)dev; return 0; }
int usb_set_altinterface(usb_dev_handle *dev,int a){ trace("usb_set_altinterface(a=%d)", a); (void)dev; return 0; }
int usb_detach_kernel_driver_np(usb_dev_handle *d,int i){trace("usb_detach_kernel_driver_np(i=%d)", i); (void)d; return 0; }
int usb_reset(usb_dev_handle *dev)                 { trace("usb_reset"); (void)dev; return 0; }

int usb_get_string_simple(usb_dev_handle *dev, int idx, char *buf, size_t len)
{
    (void)dev; (void)idx;
    snprintf(buf, len, "DCP-1510");
    return (int)strlen(buf);
}

/*
 * usb_control_msg: brscan calls this once during OpenDevice with
 * (BREQ_TYPE=0xC0, BREQ_GET_OPEN=0x01, BCOMMAND_SCANNER=0x02). It expects
 * a 5-byte response describing a successful open. CloseDevice does the
 * symmetric BREQ_GET_CLOSE=0x02. For anything else, return success with
 * the buffer left untouched.
 */
int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
                    int value, int index, char *bytes, int size, int timeout)
{
    trace("usb_control_msg(rt=0x%02x req=0x%02x val=0x%04x idx=%d size=%d)",
          requesttype & 0xff, request & 0xff, value & 0xffff, index, size);
    (void)dev; (void)timeout;

    if (bytes && size >= 5 && (request == 0x01 || request == 0x02)) {
        bytes[0] = 0x05;          /* BREQ_GET_LENGTH */
        bytes[1] = 0x10;          /* BDESC_TYPE      */
        bytes[2] = (char)request; /* echo request id */
        bytes[3] = 0x02;          /* BCOMMAND_SCANNER lo */
        bytes[4] = 0x00;          /* hi (no BCOMMAND_RETURN) */
        return 5;
    }
    return size;
}

int usb_bulk_write(usb_dev_handle *dev, int ep, const char *bytes,
                   int size, int timeout)
{
    trace("usb_bulk_write(ep=0x%02x size=%d)", ep, size);
    (void)dev; (void)bytes; (void)timeout;
    return size;
}

int usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes,
                  int size, int timeout)
{
    trace("usb_bulk_read(ep=0x%02x maxsize=%d)", ep, size);
    (void)dev; (void)ep; (void)bytes; (void)size; (void)timeout;
    return 0;
}

/* ---- additional libusb-0.1 entry points dlsym'd by Brother's blob ---- */

int usb_resetep(usb_dev_handle *dev, unsigned ep) { (void)dev; (void)ep; return 0; }
int usb_get_descriptor(usb_dev_handle *dev, unsigned char type,
                       unsigned char idx, void *buf, int size)
    { (void)dev; (void)type; (void)idx; if (buf && size > 0) memset(buf, 0, size); return size; }
int usb_get_descriptor_by_endpoint(usb_dev_handle *dev, int ep, unsigned char t,
                                   unsigned char i, void *buf, int s)
    { (void)dev; (void)ep; (void)t; (void)i;
      if (buf && s > 0) memset(buf, 0, s); return s; }
int usb_get_driver_np(usb_dev_handle *dev, int iface, char *name, unsigned int sz)
    { (void)dev; (void)iface; if (name && sz) name[0] = 0; return -1; /* no driver */ }
int usb_get_string(usb_dev_handle *dev, int idx, int lang, char *buf, size_t sz)
    { (void)dev; (void)idx; (void)lang;
      if (buf && sz) snprintf(buf, sz, "DCP-1510"); return (int)strlen(buf); }
int usb_interrupt_read(usb_dev_handle *dev, int ep, char *b, int s, int t)
    { (void)dev; (void)ep; (void)b; (void)s; (void)t; return 0; }
int usb_interrupt_write(usb_dev_handle *dev, int ep, const char *b, int s, int t)
    { (void)dev; (void)ep; (void)b; (void)t; return s; }
int usb_get_bus_and_port_number(usb_dev_handle *dev, unsigned char *bus,
                                unsigned char *port, unsigned char path_size)
    { (void)dev; (void)path_size; if (bus) *bus = 6; if (port) *port = 1; return 1; }

/* libusb-0.1 SONAME identifier — keeps the loader happy if Brother
 * inspects the version string. */
const char *usb_strerror_msg = "ok";
