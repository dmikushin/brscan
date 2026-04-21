#ifndef BROTHER_CHRESO_H
#define BROTHER_CHRESO_H

#include "brother_scandec.h"

#include <stdio.h>

static void FUN_00102149(int param_1,int param_2,undefined4 *param_3);
static size_t FUN_001016e4(int *param_1,undefined4 *param_2);
static size_t FUN_001017b1(int *param_1,undefined4 *param_2);
static size_t FUN_0010187e(int *param_1,undefined4 *param_2);
static long FUN_0010194b(long param_1,int *param_2);
static long FUN_001019e4(long param_1,int *param_2);
static long FUN_00101bd8(long param_1,int *param_2);
static long FUN_00101cba(long param_1,int *param_2);
static long FUN_00101eae(int *param_1,int *param_2);
static void FUN_001020b0(int param_1,int param_2,int *param_3,int *param_4);
static ulong FUN_0010227a(undefined4 *param_1);
static ulong FUN_0010260e(void);
static undefined8 FUN_00102944(float param_1,long param_2);
static undefined8 FUN_00102d59(float param_1,undefined *param_2);
static undefined8 FUN_00103059(float param_1,undefined *param_2);
static undefined8 FUN_001033a4(float param_1,undefined *param_2);
static ulong FUN_001036ef(float param_1,undefined *param_2);
static undefined8 FUN_00103a7b(float param_1,long param_2);
static undefined8 FUN_00103bca(float param_1,undefined *param_2);
static undefined8 FUN_00103ccf(float param_1,undefined *param_2);
static undefined8 FUN_00103de8(float param_1,undefined *param_2);
static ulong FUN_00103f01(float param_1,undefined *param_2);
static ulong FUN_0010404d(byte *param_1,ulong param_2,byte *param_3);
static ulong FUN_001040d6(byte *param_1,long param_2,undefined2 *param_3);
static ulong FUN_00104153(byte *param_1,undefined8 param_2,undefined *param_3);
static ulong FUN_00104296(byte *param_1,long param_2,undefined2 *param_3);
static ulong FUN_00104365(void *param_1,undefined8 param_2,void *param_3);
static ulong FUN_00104399(undefined *param_1,ulong param_2,undefined *param_3,int param_4);
static ulong FUN_00104412(byte *param_1,undefined8 param_2,void *param_3,int param_4);
static float FUN_00105413(float param_1);
static long FUN_0010222c(int param_1,long param_2,int param_3);
static ulong FUN_001044ca(void *param_1);
static ulong FUN_00104654(void *param_1);
static ulong FUN_00104a67(void *param_1,int param_2,void *param_3);
static ulong FUN_00104c9e(void *param_1,int param_2,void *param_3);
static ulong FUN_00104ed0(void *param_1,int param_2,void *param_3);
static ulong FUN_001050ff(ulong param_1,int param_2);
static void FUN_00105173(ulong param_1,int param_2,undefined *param_3,undefined *param_4,undefined *param_5);
static void FUN_0010561d(float param_1,float param_2,undefined *param_3);
static void FUN_001056c7(float param_1,float param_2,undefined *param_3);
static void FUN_00105771(float param_1,float param_2,undefined *param_3,undefined *param_4, undefined *param_5);
static ulong FUN_00105879(float param_1,float param_2,float param_3,float param_4,undefined *param_5);
static ulong FUN_00105aa6(float param_1,float param_2,float param_3,float param_4,undefined *param_5);
static ulong FUN_00105cd3(float param_1,float param_2,float param_3,float param_4,undefined *param_5,undefined *param_6,undefined *param_7);
static ulong FUN_00104754(void *param_1,int param_2);
static void FUN_00105279(ulong param_1,int param_2,float *param_3,float *param_4,float *param_5);
static float FUN_0010553d(long param_1);
static float FUN_001053a0(long param_1,long param_2);

ulong ChangeResoInit(undefined8 *param_1);
undefined8 ChangeResoClose(void);
long ChangeResoWrite(void *param_1,undefined4 *param_2);
ulong ChangeResoWriteStart(void);
long ChangeResoWriteEnd(void *param_1,undefined4 *param_2);

#endif // BROTHER_CHRESO_H
