/*
 * SANE API stubs: plain sane_* wrappers that call sane_brother_*.
 *
 * This file is compiled WITHOUT BACKEND_NAME / sanei_backend.h
 * to avoid the macro redefinition that would make sane_open
 * collide with sane_brother_open.
 */

#include <sane/sane.h>

/* Declare the backend functions (defined in brother.c via macro rename) */
extern SANE_Status sane_brother_init(SANE_Int *, SANE_Auth_Callback);
extern SANE_Status sane_brother_exit(void);
extern SANE_Status sane_brother_get_devices(const SANE_Device ***, SANE_Bool);
extern SANE_Status sane_brother_open(SANE_String_Const, SANE_Handle *);
extern void        sane_brother_close(SANE_Handle);
extern const SANE_Option_Descriptor *sane_brother_get_option_descriptor(SANE_Handle, SANE_Int);
extern SANE_Status sane_brother_control_option(SANE_Handle, SANE_Int, SANE_Action, void *, SANE_Word *);
extern SANE_Status sane_brother_get_parameters(SANE_Handle, SANE_Parameters *);
extern SANE_Status sane_brother_start(SANE_Handle);
extern SANE_Status sane_brother_read(SANE_Handle, SANE_Byte *, SANE_Int, SANE_Int *);
extern void        sane_brother_cancel(SANE_Handle);
extern SANE_Status sane_brother_set_io_mode(SANE_Handle, SANE_Bool);
extern SANE_Status sane_brother_get_select_fd(SANE_Handle, SANE_Int *);

SANE_Status sane_init(SANE_Int *vc, SANE_Auth_Callback cb)
{ return sane_brother_init(vc, cb); }

void sane_exit(void)
{ sane_brother_exit(); }

SANE_Status sane_get_devices(const SANE_Device ***dl, SANE_Bool local)
{ return sane_brother_get_devices(dl, local); }

SANE_Status sane_open(SANE_String_Const name, SANE_Handle *h)
{ return sane_brother_open(name, h); }

void sane_close(SANE_Handle h)
{ sane_brother_close(h); }

const SANE_Option_Descriptor *sane_get_option_descriptor(SANE_Handle h, SANE_Int opt)
{ return sane_brother_get_option_descriptor(h, opt); }

SANE_Status sane_control_option(SANE_Handle h, SANE_Int opt, SANE_Action act, void *val, SANE_Word *info)
{ return sane_brother_control_option(h, opt, act, val, info); }

SANE_Status sane_get_parameters(SANE_Handle h, SANE_Parameters *parms)
{ return sane_brother_get_parameters(h, parms); }

SANE_Status sane_start(SANE_Handle h)
{ return sane_brother_start(h); }

SANE_Status sane_read(SANE_Handle h, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *lenp)
{ return sane_brother_read(h, buf, maxlen, lenp); }

void sane_cancel(SANE_Handle h)
{ sane_brother_cancel(h); }

SANE_Status sane_set_io_mode(SANE_Handle h, SANE_Bool non_blocking)
{ return sane_brother_set_io_mode(h, non_blocking); }

SANE_Status sane_get_select_fd(SANE_Handle h, SANE_Int *fdp)
{ return sane_brother_get_select_fd(h, fdp); }
