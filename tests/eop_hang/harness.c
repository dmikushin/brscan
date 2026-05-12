/*
 * harness.c — drives libsane-brother.so directly via SANE-API dlsym, with
 * no /etc/sane.d, no dll backend, no scanimage.  Combined with the
 * usb_stub.so LD_PRELOAD and BRSCAN_REPLAY_FILE pointing at a captured
 * USB transcript, this lets us replay an exact page-scan session without
 * the scanner attached.
 *
 * Behaviour:
 *   * Configures a 200 dpi True Gray A4 scan (matches the captured
 *     transcript at tests/data/dcp1510_a4_truegray_200dpi.bin).
 *   * Loops over sane_read with a wall-clock budget, printing total
 *     bytes received and bytes-per-second every second.
 *   * Detects the end-of-page hang by either:
 *       - sane_read returning a non-EOF terminal status while bytes
 *         received are below the expected page total (truncated scan),
 *       - sane_read freezing in tight GOOD/len=0 returns for a while
 *         (the original "lines_done=248/2291" symptom),
 *       - hitting the hard wall-clock ceiling.
 *
 * Exit codes:
 *     0  scan reached SANE_STATUS_EOF cleanly (no hang)
 *     2  bug reproduced: any of (terminal status before EOF, GOOD/len=0
 *        spin past HANG_DETECT_S, hard wall-clock timeout)
 *     other  setup error
 *
 * Usage:
 *     LD_PRELOAD=/abs/path/usb_stub.so \
 *     BRSCAN_REPLAY_FILE=tests/data/dcp1510_a4_truegray_200dpi.bin \
 *     ./harness ./libsane-brother.so.1
 *
 * Build:  gcc -O0 -g -o harness harness.c -ldl
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sane/sane.h>
#include <sane/saneopts.h>

#define WALL_CLOCK_S         60   /* hard ceiling */
#define HANG_DETECT_S         5   /* no-progress window that means "hung" */
#define HANG_ZERO_READS    2000   /* must also see this many GOOD/len=0 reads */

/* Expected output for a 200dpi True Gray A4 scan, computed from
 * the SANE_Parameters returned by sane_get_parameters. */
static uint64_t expected_bytes_total = 0;

/* Resolved SANE entry points. */
struct sane_api {
    SANE_Status (*init)(SANE_Int *, SANE_Auth_Callback);
    SANE_Status (*get_devices)(const SANE_Device ***, SANE_Bool);
    SANE_Status (*open)(SANE_String_Const, SANE_Handle *);
    void        (*close)(SANE_Handle);
    void        (*exit)(void);
    const SANE_Option_Descriptor *(*get_option_descriptor)(SANE_Handle, SANE_Int);
    SANE_Status (*control_option)(SANE_Handle, SANE_Int, SANE_Action, void *, SANE_Int *);
    SANE_Status (*get_parameters)(SANE_Handle, SANE_Parameters *);
    SANE_Status (*start)(SANE_Handle);
    SANE_Status (*read)(SANE_Handle, SANE_Byte *, SANE_Int, SANE_Int *);
    void        (*cancel)(SANE_Handle);
};

#define BIND(api, h, name) \
    do { (api)->name = dlsym((h), full_sym(#name)); \
         if (!(api)->name) { fprintf(stderr, "missing sane_%s in %s: %s\n", \
                                     #name, argv[1], dlerror()); return 4; } } while (0)

/* SANE backends export either bare `sane_*` symbols (when linked via the
 * SANE dll backend, which renames them) or `sane_<backend>_*` (proprietary
 * Brother libs do this — `sane_brother4_init` etc.). Accept both: caller
 * sets SANE_SYM_PREFIX to override (default "sane_"). */
static char g_sym_prefix[64] = "sane_";

static const char *full_sym(const char *name)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s%s", g_sym_prefix, name);
    return buf;
}

/* Find the option index by name. brscan exposes the standard SANE option
 * names (SANE_NAME_SCAN_MODE etc.) but not at fixed indices. Walk the
 * descriptor array. */
static SANE_Int find_option(const struct sane_api *s, SANE_Handle h,
                            const char *name)
{
    /* opt 0 is mandated to be the "number of options" int. Read it. */
    const SANE_Option_Descriptor *d0 = s->get_option_descriptor(h, 0);
    if (!d0) return -1;
    SANE_Int n_options = 0;
    if (s->control_option(h, 0, SANE_ACTION_GET_VALUE, &n_options, NULL)
        != SANE_STATUS_GOOD)
        return -1;

    for (SANE_Int i = 1; i < n_options; i++) {
        const SANE_Option_Descriptor *d = s->get_option_descriptor(h, i);
        if (d && d->name && strcmp(d->name, name) == 0)
            return i;
    }
    return -1;
}

static int set_string_opt(const struct sane_api *s, SANE_Handle h,
                          const char *name, const char *val)
{
    SANE_Int idx = find_option(s, h, name);
    if (idx < 0) { fprintf(stderr, "option '%s' not found\n", name); return -1; }
    /* SANE wants a writable buffer of at least desc->size bytes. */
    const SANE_Option_Descriptor *d = s->get_option_descriptor(h, idx);
    char *buf = calloc(1, (size_t)d->size + 1);
    snprintf(buf, d->size, "%s", val);
    SANE_Int info = 0;
    SANE_Status st = s->control_option(h, idx, SANE_ACTION_SET_VALUE, buf, &info);
    free(buf);
    if (st != SANE_STATUS_GOOD) {
        fprintf(stderr, "set '%s'='%s' failed: %d\n", name, val, st);
        return -1;
    }
    return 0;
}

static int set_int_opt(const struct sane_api *s, SANE_Handle h,
                       const char *name, SANE_Int val)
{
    SANE_Int idx = find_option(s, h, name);
    if (idx < 0) { fprintf(stderr, "option '%s' not found\n", name); return -1; }
    SANE_Int info = 0;
    SANE_Status st = s->control_option(h, idx, SANE_ACTION_SET_VALUE, &val, &info);
    if (st != SANE_STATUS_GOOD) {
        fprintf(stderr, "set '%s'=%d failed: %d\n", name, val, st);
        return -1;
    }
    return 0;
}

static int set_fixed_opt_mm(const struct sane_api *s, SANE_Handle h,
                            const char *name, double mm)
{
    SANE_Int idx = find_option(s, h, name);
    if (idx < 0) { fprintf(stderr, "option '%s' not found\n", name); return -1; }
    SANE_Word fixed = SANE_FIX(mm);
    SANE_Int info = 0;
    SANE_Status st = s->control_option(h, idx, SANE_ACTION_SET_VALUE, &fixed, &info);
    if (st != SANE_STATUS_GOOD) {
        fprintf(stderr, "set '%s'=%.2f mm failed: %d\n", name, mm, st);
        return -1;
    }
    return 0;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <path/libsane-brother.so.1> [device-name]\n",
                argv[0]);
        return 1;
    }
    const char *libpath = argv[1];
    const char *devname = (argc >= 3) ? argv[2] : "bus1;dev1";

    void *h = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen %s: %s\n", libpath, dlerror()); return 4; }

    /* Pick the symbol prefix: env override, else autodetect by probing
     * `sane_init` then `sane_brother4_init`. */
    const char *envp = getenv("SANE_SYM_PREFIX");
    if (envp && envp[0]) {
        snprintf(g_sym_prefix, sizeof(g_sym_prefix), "%s", envp);
    } else if (dlsym(h, "sane_init")) {
        snprintf(g_sym_prefix, sizeof(g_sym_prefix), "sane_");
    } else if (dlsym(h, "sane_brother4_init")) {
        snprintf(g_sym_prefix, sizeof(g_sym_prefix), "sane_brother4_");
    } else {
        fprintf(stderr, "no sane_init or sane_brother4_init in %s\n", libpath);
        return 4;
    }
    fprintf(stderr, "[harness] using symbol prefix '%s'\n", g_sym_prefix);

    struct sane_api S;
    BIND(&S, h, init);
    BIND(&S, h, get_devices);
    BIND(&S, h, open);
    BIND(&S, h, close);
    BIND(&S, h, exit);
    BIND(&S, h, get_option_descriptor);
    BIND(&S, h, control_option);
    BIND(&S, h, get_parameters);
    BIND(&S, h, start);
    BIND(&S, h, read);
    BIND(&S, h, cancel);

    SANE_Int version = 0;
    if (S.init(&version, NULL) != SANE_STATUS_GOOD) {
        fprintf(stderr, "sane_init failed\n");
        return 5;
    }
    fprintf(stderr, "[harness] sane_init OK, version=0x%x\n", version);

    /* Some backends (notably Brother's proprietary libsane-brother4) only
     * populate their internal device list after sane_get_devices. Call it
     * once and print everything we found before opening. */
    const SANE_Device **devs = NULL;
    if (S.get_devices(&devs, SANE_TRUE) == SANE_STATUS_GOOD && devs) {
        for (int di = 0; devs[di]; di++) {
            fprintf(stderr, "[harness] device[%d] name='%s' vendor='%s' model='%s' type='%s'\n",
                    di, devs[di]->name ? devs[di]->name : "(null)",
                    devs[di]->vendor ? devs[di]->vendor : "(null)",
                    devs[di]->model  ? devs[di]->model  : "(null)",
                    devs[di]->type   ? devs[di]->type   : "(null)");
        }
        if (devs[0] && devs[0]->name && (!argv[2] || !argv[2][0])) {
            /* If caller didn't override the device name, take whatever the
             * backend reported as device 0. */
            devname = devs[0]->name;
        }
    }

    SANE_Handle dev = NULL;
    SANE_Status st = S.open(devname, &dev);
    if (st != SANE_STATUS_GOOD) {
        fprintf(stderr, "sane_open('%s') failed: %d\n", devname, st);
        return 6;
    }
    fprintf(stderr, "[harness] sane_open OK\n");

    const char *mode = getenv("BRSCAN_HARNESS_MODE");
    const char *resolution_s = getenv("BRSCAN_HARNESS_RESOLUTION");
    int resolution = resolution_s && resolution_s[0] ? atoi(resolution_s) : 200;
    if (!mode || !mode[0]) mode = "True Gray";

    /* Configure an A4 scan matching the captured transcript so the parser sees
     * exactly the bytes it saw on hardware. */
    if (set_string_opt(&S, dev, SANE_NAME_SCAN_MODE,       mode)       < 0 ||
        set_int_opt   (&S, dev, SANE_NAME_SCAN_RESOLUTION, resolution) < 0 ||
        set_fixed_opt_mm(&S, dev, SANE_NAME_SCAN_TL_X, 0.0)              < 0 ||
        set_fixed_opt_mm(&S, dev, SANE_NAME_SCAN_TL_Y, 0.0)              < 0 ||
        set_fixed_opt_mm(&S, dev, SANE_NAME_SCAN_BR_X, 210.0)            < 0 ||
        set_fixed_opt_mm(&S, dev, SANE_NAME_SCAN_BR_Y, 297.0)            < 0)
    {
        return 7;
    }

    SANE_Parameters p;
    if (S.get_parameters(dev, &p) == SANE_STATUS_GOOD) {
        expected_bytes_total = (uint64_t)p.lines * (uint64_t)p.bytes_per_line;
        fprintf(stderr, "[harness] params: %dx%d, %d bpp, format=%d, lines=%d, "
                        "bytes/line=%d, expected_total=%llu\n",
                p.pixels_per_line, p.lines, p.depth, p.format,
                p.lines, p.bytes_per_line,
                (unsigned long long)expected_bytes_total);
    }

    if ((st = S.start(dev)) != SANE_STATUS_GOOD) {
        fprintf(stderr, "sane_start failed: %d\n", st);
        return 8;
    }

    /* Drive the read loop with two budgets:
     *   - WALL_CLOCK_S     hard timeout (avoid an infinite test)
     *   - HANG_DETECT_S    after this many seconds without bytes growing,
     *                      and HANG_ZERO_READS GOOD/len=0 returns, declare
     *                      the bug "reproduced".
     */
    SANE_Byte buf[32 * 1024];
    uint64_t total = 0;
    uint64_t prev_total = 0;
    int collect_rgb_stats = getenv("BRSCAN_DEBUG_COLOR") || getenv("BRSCAN_ASSERT_NOT_YELLOW");
    uint64_t rgb_sum[3] = {0, 0, 0};
    uint64_t rgb_pixels = 0;
    unsigned char rgb_min[3] = {255, 255, 255};
    unsigned char rgb_max[3] = {0, 0, 0};
    int      zero_reads_since_progress = 0;
    double   t0 = now_s();
    double   t_last_progress = t0;
    double   t_last_print = t0;

    while (1) {
        double now = now_s();
        if (now - t0 > WALL_CLOCK_S) {
            fprintf(stderr, "[harness] HARD TIMEOUT after %.1fs, total=%llu/%llu "
                            "— end-of-page hang reproduced (timeout)\n",
                    now - t0,
                    (unsigned long long)total,
                    (unsigned long long)expected_bytes_total);
            S.cancel(dev);
            S.close(dev);
            S.exit();
            dlclose(h);
            return 2;
        }

        SANE_Int got = 0;
        st = S.read(dev, buf, sizeof(buf), &got);

        if (st == SANE_STATUS_EOF) {
            double elapsed = now_s() - t0;
            /* Brother rounds the requested area to the nearest scanner-
             * native step (mm rounding + DPI quantisation), so the actual
             * byte count is always a few % below the upper bound returned
             * by sane_get_parameters. Any clean EOF means the parser
             * found the Page-End status byte — no end-of-page hang. */
            fprintf(stderr, "[harness] sane_read=EOF after %.1fs, total=%llu "
                            "(sane_get_parameters upper bound: %llu) — clean EOF\n",
                    elapsed,
                    (unsigned long long)total,
                    (unsigned long long)expected_bytes_total);
            break;
        }

        if (st != SANE_STATUS_GOOD) {
            double elapsed = now_s() - t0;
            fprintf(stderr, "[harness] sane_read returned status=%d after "
                            "%.1fs, total=%llu/%llu — end-of-page hang "
                            "reproduced (terminal status before EOF)\n",
                    st, elapsed,
                    (unsigned long long)total,
                    (unsigned long long)expected_bytes_total);
            S.cancel(dev);
            S.close(dev);
            S.exit();
            dlclose(h);
            return 2;
        }

        if (got > 0) {
            if (collect_rgb_stats && p.format == SANE_FRAME_RGB && p.depth == 8) {
                for (SANE_Int i = 0; i + 2 < got; i += 3) {
                    for (int c = 0; c < 3; c++) {
                        unsigned char v = buf[i + c];
                        rgb_sum[c] += v;
                        if (v < rgb_min[c]) rgb_min[c] = v;
                        if (v > rgb_max[c]) rgb_max[c] = v;
                    }
                    rgb_pixels++;
                }
            }
            total += (uint64_t)got;
            t_last_progress = now;
            zero_reads_since_progress = 0;
        } else {
            zero_reads_since_progress++;
        }

        if (now - t_last_print >= 1.0) {
            fprintf(stderr, "[harness] t=%.1fs total=%llu bytes  "
                            "(+%llu in last 1s, %d empty reads)\n",
                    now - t0,
                    (unsigned long long)total,
                    (unsigned long long)(total - prev_total),
                    zero_reads_since_progress);
            prev_total = total;
            t_last_print = now;
        }

        if ((now - t_last_progress) >= HANG_DETECT_S
            && zero_reads_since_progress >= HANG_ZERO_READS)
        {
            fprintf(stderr, "[harness] HANG REPRODUCED: %llu bytes received, "
                            "no progress for %.1fs across %d empty reads\n",
                    (unsigned long long)total,
                    now - t_last_progress,
                    zero_reads_since_progress);
            S.cancel(dev);
            S.close(dev);
            S.exit();
            dlclose(h);
            return 2;
        }
    }

    fprintf(stderr, "[harness] DONE: total=%llu bytes, expected=%llu — full scan\n",
            (unsigned long long)total, (unsigned long long)expected_bytes_total);
    if (rgb_pixels > 0) {
        double r_mean = (double)rgb_sum[0] / (double)rgb_pixels;
        double g_mean = (double)rgb_sum[1] / (double)rgb_pixels;
        double b_mean = (double)rgb_sum[2] / (double)rgb_pixels;
        fprintf(stderr, "[harness] RGB stats: mean=%.3f/%.3f/%.3f min=%u/%u/%u max=%u/%u/%u\n",
                r_mean, g_mean, b_mean,
                rgb_min[0], rgb_min[1], rgb_min[2],
                rgb_max[0], rgb_max[1], rgb_max[2]);
        if (getenv("BRSCAN_ASSERT_NOT_YELLOW") &&
            rgb_min[0] == 255 && rgb_min[1] == 255 && b_mean < 200.0) {
            fprintf(stderr, "[harness] FAIL: RGB output is yellow-only (R/G saturated)\n");
            S.cancel(dev);
            S.close(dev);
            S.exit();
            dlclose(h);
            return 3;
        }
    }
    S.cancel(dev);
    S.close(dev);
    S.exit();
    dlclose(h);
    return 0;
}
