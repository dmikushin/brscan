/*

 This file is part of the Brother MFC/DCP backend for SANE.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place, Suite 330, Boston, MA  02111-1307  USA

*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//	Source filename: brother_mfcinfo.h
//
//		Copyright(c) 1997-2000 Brother Industries, Ltd.  All Rights Reserved.
//
//
//	Abstract:
//			�ǥХ�������˴ؤ��빽¤�Ρ�����ܥ����
//
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifndef _BROTHER_MFCINFO_H_
#define _BROTHER_MFCINFO_H_

#include "brother_dtype.h"

//
// MFC��ǥ롦������
//
#define MODEL_YL1_1      0		// 4550
#define MODEL_YL1_2      1		// 6550,7550
#define MODEL_BY1_US     2		// 7000,7200
#define MODEL_BY1_EUR    3		// BY Eurlope
#define MODEL_BY1_3IN1   4		// MC3000
#define MODEL_YL2_US    10		// YL2 USA model
#define MODEL_YL2_EUR   11		// YL2 Eurlope model
#define MODEL_BY2_US    12		// BY2 USA Color model
#define MODEL_BY2_EUR   13		// BY2 Eurlope Color model
#define MODEL_YL3       14		// YL3 series
#define MODEL_BY4       15		// BY4 series
#define MODEL_YL4       16		// YL4 series
#define MODEL_ZLe       17		// ZLe series
#define MODEL_BHL       18		// BH-low series

#define BIT_YL		0x100		// YL bit
#define BIT_BY		0x200		// BY bit
#define BIT_YL2		0x400		// YL2 bit
#define BIT_BY2		0x800		// BY2 bit
#define BIT_3IN1	0x1000		// 3in1 bit
#define BIT_EUR		0x2000		// Eurlope

typedef struct tagMFCMODELINFO {
	WORD  wModelType;		// MFC��ǥ롦������
	WORD  wDialogType;		// Dialog(UI)������
	BOOL  bColorModel;		// ���顼������ʡ���ǥ�
	BOOL  b3in1Type;		// 3in1��ǥ�(MC3000)
	BOOL  bDither;			// Dither���ݡ��ȡ���ǥ�
	BOOL  bVideoCap;		// VideoCapture���ݡ��ȡ���ǥ�
	BOOL  bQcmdEnable;		// Q-command���ݡ��ȡ���ǥ�
} MFCMODELINFO, *LPMFCMODELINFO;

#define TWDSUI_YL    0
#define TWDSUI_3IN1  1
#define TWDSUI_BY    2
#define TWDSUI_NOVC  3


#pragma pack(1)
//
// �ǥХ��������ݡ��Ȥ��륫�顼������
//
typedef union tagMFCCOLORTYPE {
	struct {
		BYTE  bBlackWhite:1;	// ���͡������
		BYTE  bErrorDiffusion:1;// ���Ȼ�
		BYTE  bTrueGray:1;	// ���졼��������
		BYTE  b256Color:1;	// 256�����顼
		BYTE  b24BitColor:1;	// 24bit���顼
		BYTE  b256IndexColor:1;	// 256�����顼��MFC����ѥ�åȥơ��֥������
	} bit;
	BYTE val;
} MFCCOLORTYPE, *LPMFCCOLORTYPE;

#define MFCDEVINFMONO   0x07		// �����ǥ�Υǥե���ȥ��顼������
					//   (B&W, ErrorDiff, TrueGray)
#define MFCDEVINFCOLOR  0x1F		// ���顼��ǥ�Υǥե���ȥ��顼������
					//   (B&W, ErrorDiff, TrueGray, 256Color, 24bit)


//
// �ǥХ����Υ�����ʡ��ӥǥ�ǽ�Ͼ���إå���
//
typedef struct tagMFCDEVICEHEAD {
	WORD  wDeviceInfoID;		// �ǥХ��������ID
	BYTE  nInfoSize;		// �ǥХ�������Υ�������ID�ϥ������˴ޤޤʤ���
	BYTE  nProtcolType; 		// DS<->MFC�֤Υץ�ȥ������
					// 00h=��1999ǯ��ǥ�, 01h=2000ǯ��ǥ�
} MFCDEVICEHEAD, *LPMFCDEVICEHEAD;

#define MFCDEVICEINFOID  0x00C1		// �ǥХ��������ID��
#define MFCPROTCOL1998   0		// ��1999ǯ��ǥ롦�ץ�ȥ���
#define MFCPROTCOL2000   1		// 2000ǯ��ǥ롦�ץ�ȥ���


//
// �ǥХ����Υ�����ʡ��ӥǥ�ǽ�Ͼ���
//
typedef struct tagMFCDEVICEINFO {
	BYTE          nVideoSignalType; // �ӥǥ��ο������
					//   00h=Reserve, FFh=�ӥǥ�̵��, 01h=NTSC, 02h=PAL
	MFCCOLORTYPE  nColorType;	// ������ʤ��б����顼������
					//   00h=Reserve, MSB|0:0:256ix:24c:256c:TG:ED:BW|LSB
	WORD          nVideoNtscSignal; // NTSC���浬��
					//   0=Reserve 1:B,2:G,3:H,4:I,5:D,6:K,7:K1,8:L,9:M,10:N
	WORD          nVideoPalSignal;	// PAL���浬��
					//   0=Reserve 1:B,2:G,3:H,4:I,5:D,6:K,7:K1,8:L,9:M,10:N
	WORD          nVideoSecamSignal;// SECAM���浬��
					//   0=Reserve 1:B,2:G,3:H,4:I,5:D,6:K,7:K1,8:L,9:M,10:N
	BYTE          nHardwareType;	// ���Ĥμ���
					//   00h=Reserve, 01h=NTSC���� 02h=NTSC/Lexmark���� 81h=PAL����
	BYTE          nHardwareVersion; // ���ĤΥС������
					//   00h=Reserve, 01h��=�С�������ֹ�
	BYTE          nMainScanDpi;	// ������ʼ����������٤�ǽ��
					//   00h=Reserve, 01h=200dpi, 02h=200,300dpi, 03h=100,200,300dpi
	BYTE          nPaperSizeMax;	// ��������б��ѻ極����
					//   00h=Reserve, 01h=A4, 02h=B4
} MFCDEVICEINFO, *LPMFCDEVICEINFO;

#define MFCVIDEONONE     0xFF		// �ӥǥ�̵��
#define MFCVIDEONTSC     1		// �ӥǥ��ο�����̤�NTSC
#define MFCVIDEOPAL      2		// �ӥǥ��ο�����̤�PAL
#define MFCVIDEOSECAM    3		// �ӥǥ��ο�����̤�Secam

#define MFCHARDNTSC      1		// ���Ĥμ����NTSC����
#define MFCHARDLEX       2		// ���Ĥμ����NTSC/Lexmark����
#define MFCHARDPAL       0x81		// ���Ĥμ����PAL����

#define MFCMAINSCAN200   1		// �����������٤�ǽ�Ϥ�200dpi�Τ�
#define MFCMAINSCAN300   2		// �����������٤�ǽ�Ϥ�200,300dpi
#define MFCMAINSCAN100   3		// �����������٤�ǽ�Ϥ�100,200,300dpi

#pragma pack()
//
// �ǥХ����Υ�����ʡ��ӥǥ�ǽ�Ͼ���Υ������ʥǥХ��������ID�ϥ������˴ޤޤʤ���
//
#define MFCDEVINFOFULLSIZE  sizeof( BYTE ) * 2 + sizeof( MFCDEVICEINFO )


//
// �ǥХ����Υ�����ʾ��󡿥������ѥ�᡼��
//
typedef struct tagDEVSCANINFO {
	WORD        wResoType;		// �����������٥������ֹ�
	WORD        wColorType;		// ������󤹤륫�顼������
	RESOLUTION  DeviceScan; 	// ������󤹤�²�����
	WORD        wScanSource;	// ������󥽡���
	DWORD       dwMaxScanWidth; 	// �ɤ߼���������0.1mmñ�̡�
	DWORD       dwMaxScanPixels;	// �ɤ߼���������Pixel����
	DWORD       dwMaxScanHeight;	// �ɤ߼�����Ĺ��0.1mmñ�̡�
	DWORD       dwMaxScanRaster;	// �ɤ߼�����Ĺ�ʥ饹������
	AREARECT    ScanAreaDot;	// ��������ϰϻ����dotñ�̡�
	AREASIZE    ScanAreaSize;	// �ɤ߼���ϰϡʥɥåȿ���
	AREASIZE    ScanAreaByte;	// �ɤ߼���ϰϡʥХ��ȿ���
} DEVSCANINFO, *LPDEVSCANINFO;

#define MFCSCANSRC_ADF  1		// ������󥽡�����ADF
#define MFCSCANSRC_FB   2		// ������󥽡�����FB
#define MFCSCANSRC_ADF_DUP  3           //06/02/27 ������󥽡�����ADF��ξ��

#define MFCSCANMAXWIDTH      2080	// �����ɤ߼������0.1mmñ�̡�
#define MFCSCAN200MAXPIXEL   1664	// �����ɤ߼��ɥåȿ���200dpi��
#define MFCSCAN300MAXPIXEL   2464	// �����ɤ߼��ɥåȿ���300dpi��
#define MFCSCANMAXHEIGHT     3476	// ADF�����ɤ߼��Ĺ��0.1mmñ�̡�
#define MFCSCAN400MAXRASTER  5474	// ADF�����ɤ߼��饹������400dpi��
#define MFCSCAN600MAXRASTER  8210	// ADF�����ɤ߼��饹������600dpi��
#define MFCSCANFBHEIGHT      2910	// FB�����ɤ߼��Ĺ��0.1mmñ�̡�
#define MFCSCAN400FBRASTER   4582	// FB�����ɤ߼��饹������400dpi��
#define MFCSCAN600FBRASTER   6874	// FB�����ɤ߼��饹������600dpi��

#endif //_BROTHER_MFCINFO_H_

//////// end of brother_mfcinfo.h ////////
