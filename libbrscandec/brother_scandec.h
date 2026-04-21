/*******************************************************************************
 *
 *	Scan Decode �إå����ե�����
 *
 *	Copyright: 2000 brother Industries , Ltd.
 *
 *	ver 1.0.0 : 2000.04.13 : ���� : ��������
 ******************************************************************************/
#ifndef	__BROTHER_SCAN_DECODE_H
#define	__BROTHER_SCAN_DECODE_H

#include "../libsane-brother/brother_dtype.h"

#include <stdint.h>

typedef	struct {
	INT		nInResoX ;		/* ���ϲ����� */
	INT		nInResoY ;		/* ���ϲ����� */
	INT		nOutResoX ;		/* ���ϲ����� */
	INT		nOutResoY ;		/* ���ϲ����� */
	INT		nColorType ;	/* ���顼���� */
	DWORD	dwInLinePixCnt ;	/* 1�饤������Pixel�� */
	INT		nOutDataKind ;	/* 24BitColor����RGB���Ϸ��� */
#if 0	/* brother_deccom.h�˵��� */
#define	SCODK_PIXEL		1	/* RGB RGB RGB���ǽ� */
#define	SCODK_PLANE		2	/* RRR GGG BBB���ǽ� */
#endif
	BOOL	bLongBoundary ;	/* Long Boundary���� */

/* ScanDecOpen������ꤵ��� */
	DWORD	dwOutLinePixCnt ;	/* 1�饤�����Pixel�� */
	DWORD	dwOutLineByte ;		/* 1�饤�����Byte�� */
	DWORD	dwOutWriteMaxSize ;	/* ����񤭹��ߥХåե������� */

} SCANDEC_OPEN ;

typedef	struct {
	INT		nInDataComp ;		/* ���ϥǡ������̼��� */
#define	SCIDC_WHITE		1	/* ����饤�� */
#define	SCIDC_NONCOMP	2	/* �󰵽� */
#define	SCIDC_PACK		3	/* �ѥå��ӥåİ��� */

	INT		nInDataKind ;		/* ���ϥǡ�����Ǽ���� */
#if 0	/* brother_deccom.h�˵��� */
#define	SCIDK_MONO		1	/* ��Υ��� */
#define	SCIDK_R			2	/* R�ǡ��� */
#define	SCIDK_G			3	/* G�ǡ��� */
#define	SCIDK_B			4	/* B�ǡ��� */
#define	SCIDK_RGB		5	/* RGB���ǽ� */
#define	SCIDK_BGR		6	/* BGR���ǽ� */
#define	SCIDK_256		7	/* 256�����顼 */
#endif
	CHAR	*pLineData ;	/* ���ϣ��饤��ǡ�����Ǽ�� */
	DWORD	dwLineDataSize ;	/* ���ϣ��饤��ǡ��������� */
	CHAR	*pWriteBuff ;	/* ���ϥǡ�����Ǽ�� */
	DWORD	dwWriteBuffSize ;	/* ���ϥǡ����Хåե������� */
	BOOL	bReverWrite ;	/* ���ϥǡ�����Ǽ��ˡ */

} SCANDEC_WRITE ;

BOOL	ScanDecOpen( SCANDEC_OPEN * );
void	ScanDecSetTblHandle( HANDLE, HANDLE );
BOOL	ScanDecPageStart( void );
DWORD	ScanDecWrite( SCANDEC_WRITE *, INT * );
DWORD	ScanDecPageEnd( SCANDEC_WRITE *, INT * );
BOOL	ScanDecClose( void );

typedef BOOL  (*SCANDECOPEN) (SCANDEC_OPEN * );
typedef void  (*SCANDECSETTBL) ( HANDLE, HANDLE );
typedef BOOL  (*SCANDECPAGESTART) ( void );
typedef DWORD (*SCANDECWRITE) ( SCANDEC_WRITE *, INT * );
typedef DWORD (*SCANDECPAGEEND) ( SCANDEC_WRITE *, INT * );
typedef BOOL  (*SCANDECCLOSE) ( void );

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char undefined;
typedef unsigned short undefined2;
typedef uint32_t undefined4;
typedef uint64_t undefined8;

/*
 * Ghidra decompiler helper functions.
 * These represent x86-64 operations that Ghidra couldn't simplify:
 *
 * The pattern SUB168(ZEXT816(0xaaaaaaaaaaaaaaab) * ZEXT816(x) >> 0x41, 0)
 * is the compiler's idiom for unsigned division by 3.
 *
 * The movlpd/ZEXT416/CONCAT88 patterns implement rounding (value + 0.5).
 */
static inline __int128 ZEXT816(int64_t val) {
	return (__int128)(uint64_t)val;
}

static inline __int128 ZEXT416(int32_t val) {
	return (__int128)(uint32_t)val;
}

static inline int64_t SUB168(__int128 val, int64_t offset) {
	/* Extract low 64 bits (offset is always 0 in this codebase) */
	(void)offset;
	return (int64_t)val;
}

static inline __int128 CONCAT88(int64_t hi, int64_t lo) {
	return ((__int128)(uint64_t)hi << 64) | (uint64_t)lo;
}

static inline __int128 movlpd(__int128 val, double d) {
	/* MOVLPD: replace low 64 bits of XMM reg with a double */
	union { double d; uint64_t i; } u;
	u.d = d;
	return (val & (((__int128)0xFFFFFFFFFFFFFFFFULL) << 64)) | u.i;
}

#endif	/* ! __BROTHER_SCAN_DECODE_H */
