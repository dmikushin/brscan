//#include "build.h"

// TWAIN ���顼�ޥå��󥰽��� �إå��ե����� 
// Create 97.1.8  Brother Systems T.YUGUCHI

#ifndef _H_BRCOLOR_
#define _H_BRCOLOR_

#define HWND void *

//---------------------------------------------------------------------- ������
// TWAIN�¹ԥ⡼��(���顼�ޥå��󥰽���������¹Ի��˻����
#define	TWAIN_SCAN_MODE				0	//TWAIN�¹�  �̾凉����ʡ��⡼��
#define TWAIN_COPY_MODE				1	//TWAIN�¹�  �У� �ãϣУ� �⡼��

// �ޥå����оݥǡ��� �ңǣ¥ǡ����¤�
#define CMATCH_DATALINE_RGB			0	// Red , Green , Blue
#define CMATCH_DATALINE_BGR			1	// Blue , Green, Red

// ���顼�ޥå��󥰼���(���顼�ޥå��󥰽����¹Ի��˻����
#define CMATCH_KIND_GAMMA			0	//�å���������
#define CMATCH_KIND_LUT				1	//LUT�ˤ�륫�顼�ޥå���
#define CMATCH_KIND_MONITOR			2	//��˥��������֥졼�����

// �ޥå����ѥơ��֥��ΰ襵����
#define	GAMMA_TABLE_NAME			"BrGamma.dat"	
													//�å���������ơ��֥�̾
#define GAMMA_TABLE_ID				"BSGT"			//�å���������ơ��֥�ID
#define TABLE_GAMMA_SIZE			768 			//�å���������ơ��֥�
													//�åǡ��������� 256*3(byte)	

#define MONITOR_GAMMA_TABLE_NAME	"BrMonCal.dat"
													//��˥��������֥졼�������ͥơ��֥�̾
#define MONITOR_GAMMA_ID			"BRMC"			//��˥��������֥졼�������ͥơ��֥�ID

#define LUT_FILE_NAME				"BrLutCm.dat"
													//LUT�ǡ�������ե�����̾
#define LUT_FILE_ID					"BLCM"			//LUT�ǡ�������ե�����

#define LUT_KIND_SCAN				"SLUT"			//�̾凉����ʡ������ѣ̣գ�
#define LUT_KIND_COPY				"CLUT"			//PC COPY�ѣ̣գ�

#ifdef	TABLE_00
	#define	LUT_RGB						0				//LUT �ǡ������̥����� RGB
//  not used
//	#define LUT_CMY						1				//LUT �ǡ������̥����� CMY
//	#define	LUT_LAB						2				//LUT �ǡ������̥����� LAB
#endif	//TABLE_00
#ifdef	TABLE_VER
	#define	LUT_00						0				//grid only
	#define LUT_01						1				//grid , gamma , gray
#endif	//TABLE_VER

#define LUT_DEFINE_FILE				"BrLutDef.Dat"
													//LUT����������ե�����
#define LUT_DEFINE_FILE_ID			"BRLD"			//LUT����������ե�����ID

//��ǥ������������
//���߻��Ѥ��Ƥ��ʤ��Ȥ���
#define MEDIA_PLAIN					0				//PLAIN PAPER
#define	MEDIA_COTED					1				//COTED PAPER 320
#define	MEDIA_COTED720				2				//COTED PAPER 720
#define	MEDIA_GROSSY				3				//GROSSY
#define	MEDIA_OHP					4				//OHP
#define MEDIA_OHPMIRROR				5				//OHP MIRROR
#define	MEDIA_PHOTO					6				//PHOTO
//�����餬���ߤλȤ���
#define	MEDIA_STD					0				//standard paper
//#define	MEDIA_PHOTO					6				//photo paper
#define	MEDIA_FB_STD				MEDIA_STD		//FB	standard paper
#define	MEDIA_FB_PHOTO				MEDIA_PHOTO		//FB	photo paper
#define	MEDIA_ADF_STD				7				//ADF	standard paper
#define	MEDIA_ADF_PHOTO				8				//ADF	photo paper

//�ɥ�����ȥ⡼�����
#define	DOCMODE_AUTO				0				//Document Mode AUTO
#define	DOCMODE_GRAPH				1				//Document Mode GRAPH
#define	DOCMODE_PHOTO				2				//Document Mode PHOTO
#define	DOCMODE_CUSTOM				3				//Document Mode Custom

//���顼�ޥåȥ󥰥⡼��			
#define	COLOR_VIVID					0				//VIVID
#define	COLOR_MATCHSCREEN			1				//MATCH_SCREEN

//ICM(Win95)������
//#define ICM_OFF					0				//ICM Off
#define ICM_THROUGH					1				//ICM On
#define	ICM_MANUAL					2				//ICM Manual				

//�ץ�󥿡��ɥ饤�С�̾
#define MFC_PRINTER_NAME			"Brother MFC-7000 Series"
#define MC_PRINTER_NAME				"Brother MC3000"

//INI�ե�����̾
#define MFC_INIFILE_NAME			"Brmfc97c.ini"
#define MC_INIFILE_NAME				"Brmc97c.ini"

//����ɣ�
#define	MFC7000						0				//MFC-7000���꡼��
#define MC3000						1				//MC3000 (3in1)

//ɸ��̣գ� ID
#define DEFAULT_LUT					"DLUT"
#define DEFAULT_PHOTO_LUT			"PHTO"
#define ADF_STD_LUT					"DADF"
#define ADF_PHOTO_LUT				"PADF"
//--------------------------------------------------------------------- ��¤�����
#pragma pack(1)

typedef struct{					//���顼�ޥå��󥰽�����⡼�ɻ��깽¤�Ρ��ե�����̾�դ�
	int		nRgbLine;			//RGB�ǡ����¤�	  (BGR  or RGB)
	int		nPaperType;			//���� �����
	int		nMachineId;			//����ID
	LPSTR	lpLutName;			//Lut Name
}CMATCH_INIT;	

typedef struct{										//��˥��������֥졼�������͹�¤��
	char	FileID[4];			//'BRMC'
	short	BlackPoint[4];		//index[0,1,2,3]=[r,g,b,reserved]
	float	Gamma[4];			//index[0,1,2,3]=[r,g,b,reserved]
	short	Flag;				//default Flag
}BRMONCALDAT; 

typedef struct{										//LUT�ǡ�����¤��
	short	sLutVer;			//lut version
}BRLUT_HEAD_DATA_VER;

typedef struct{										//LUT�ǡ�����¤��
//	short	sLutDataKind;		//LUT�ǡ������̥�����

	float	s3D_Xmin;			//3D DATA X�Ǿ���
	float	s3D_Xmax;			//3D DATA X������
	float	s3D_Ymin;			//3D DATA Y�Ǿ���
	float	s3D_Ymax;			//3D DATA Y������
	float	s3D_Zmin;			//3D DATA Z�Ǿ���
	float	s3D_Zmax;			//3D DATA Z������

	short	s3D_Xsplit;			//3D DATA X����ʬ���
	short	s3D_Ysplit;			//3D DATA Y����ʬ���
	short	s3D_Zsplit;			//3D DATA Z����ʬ���

//	float	f3D_Xspase;			//3D��Ω���ζ��� �������ֳ�
//	float	f3D_Yspase;			//3D��Ω���ζ��� �������ֳ�
//	float	f3D_Zspase;			//3D��Ω���ζ��� �������ֳ�

}BRLUT_HEAD_DATA_00;

#ifdef	TABLE_01
typedef struct{										//LUT�ǡ�����¤��
//	short	sLutDataKind;		//LUT�ǡ������̥�����

	short	sLutnum;			//LUT data ��
	short	sGamnum;			//Gamma data ��
	short	sGraynum;			//Gray data ��

	short	s3D_Xmin;			//3D DATA X�Ǿ���
	short	s3D_Xmax;			//3D DATA X������
	short	s3D_Ymin;			//3D DATA Y�Ǿ���
	short	s3D_Ymax;			//3D DATA Y������
	short	s3D_Zmin;			//3D DATA Z�Ǿ���
	short	s3D_Zmax;			//3D DATA Z������

	short	s3D_Xgrid;			//3D DATA X����grid��
	short	s3D_Ygrid;			//3D DATA Y����grid��
	short	s3D_Zgrid;			//3D DATA Z����grid��
	short	s3D_Xspace;			//3D DATA X����grid size
	short	s3D_Yspace;			//3D DATA Y����grid size
	short	s3D_Zspace;			//3D DATA Z����grid size
}BRLUT_HEAD_DATA_01;
#endif	//TABLE_01

typedef struct{										//LUT�ǡ�������ե�����ơ��֥�إå�����¤��
	char	TableID[4];			//�ơ��֥뼱�̥�����
	long	lTableOffset;		//LUT�ǡ������ϥ��ե��å���
}BRLUT_FILE_HEAD;

typedef struct{										//LUT���顼�ޥå����оݥǡ�������
#ifdef	TABLE_00
	BYTE	*pbIndex_X;			//���ϥǡ��� ��
	BYTE	*pbIndex_Y;			//���ϥǡ��� ��
	BYTE	*pbIndex_Z;			//���ϥǡ��� ��

	short	sColorType_X;		//���ϥǡ��� �إ��顼������  (Red = 0, Green = 1, Blue = 2) 
	short	sColorType_Y;		//���ϥǡ��� �٥��顼������
	short	sColorType_Z;		//���ϥǡ��� �ڥ��顼������
#endif	//TABLE_00
#ifdef	TABLE_01
	short	sData_X;			//���ϥǡ��� ��
	short	sData_Y;			//���ϥǡ��� ��
	short	sData_Z;			//���ϥǡ��� ��
#endif	//TABLE_01

	short	sIndex_X;			//LUT���ȥ���ǥå��� ��
	short	sIndex_Y;			//LUT���ȥ���ǥå��� ��
	short	sIndex_Z;			//LUT���ȥ���ǥå��� ��
}BRLUT_INPUT_DATA;

typedef struct{										//LUT��ɽ��(¬���ǡ����˳�Ǽ��¤��
	float	fExpPoint_X[8];		//��ɽ�� �� �أ����أ�
	float	fExpPoint_Y[8];		//��ɽ�� �� �٣����٣�
	float	fExpPoint_Z[8];		//��ɽ�� �� �ڣ����ڣ�
}BRLUT_EXP_POINT;

typedef struct{										//LUT��ɽ���������ǡ����˳�Ǽ��¤��
	float	fLogicPoint_X[8];
	float	fLogicPoint_Y[8];
	float	fLogicPoint_Z[8];
}BRLUT_LOGIC_POINT;

typedef struct{										//LUT��ɽ���ǡ�����Ǽ��¤��
	float	fLutXData;			//��ɽ�� �������ǡ���
	float	fLutYData;			//��ɽ�� �������ǡ���
	float	fLutZData;			//��ɽ�� �������ǡ���
}BRLUT_DATA;

#ifdef	GAMMA_ADJ
//typedef struct{										//LUT�ǡ���Adjust��Ǽ��¤��
//	float	fGammaXData;			//�������ǡ���
//	float	fGammaYData;			//�������ǡ���
//	float	fGammaZData;			//�������ǡ���
//}BRLUT_GAMMA;
#endif	//GAMMA_ADJ

#ifdef	GRAY_ADJ
//typedef struct{										//LUT�ǡ���Adjust��Ǽ��¤��
//	float	fAdjXData;			//�������ǡ���
//	float	fAdjYData;			//�������ǡ���
//	float	fAdjZData;			//�������ǡ���
//}BRLUT_GRAY;
#endif	//GRAY_ADJ

typedef struct{										//LUT�����ѥǡ�����¤��
	short	nScanMode;			//������ʡ��⡼�Ɏޡ�SCAN or COPY)
	short	nInputMedia;		//���ϥ�ǥ����ʻ楿���ס�
	short	nOutputMedia;		//���ϥ�ǥ���
	short	nDocMode;			//�ɥ�����ȥ⡼��
	short	nColorMatch;		//VIVID or MATCHSCREEN
	short	nIcmControl;		//ICM �⡼��
}BRLUT_SELECT;	

typedef struct{										//Hash Table Data�ѹ�¤��
	BYTE	yColorX;
	BYTE	yColorY;
	BYTE	yColorZ;
}COLOR_HASH_DAT;

typedef struct{										//Hash Table �����ѹ�¤��
	long			nHashKey;	//Hash Key
	COLOR_HASH_DAT	*pColorDat;	//Hash Data Pointer
}COLOR_HASH_KEY;
#pragma pack()

//--------------------------------------------------------------------- �ؿ����
#ifdef BRCOLOR

//���顼�ޥå��󥰽��������
BOOL ColorMatchingInit(CMATCH_INIT matchingInitDat);

//���顼�ޥå��󥰽�λ����
void ColorMatchingEnd(void);

//���顼�ޥå��󥰽���
BOOL ColorMatching(BYTE *pRgbData, long lRgbDataLength, long lLineCount);


//�å����������ѥơ��֥���������
BOOL InitGammaTable(void);

//��˥��������֥졼�������������
BOOL InitMonitorGamma(void);

//LUT���������
BOOL InitLUT(char* pcLutKind);

//�å�������������		
void GammaCurveMatching(BYTE *pRgbData, long lRgbDataLength, long lLineCount);

//LUT ���顼�ޥå��󥰽���
BOOL LutColorMatching(BYTE *pRgbData, long lRgbDataLength, long lLineCount);

//��˥��������֥졼��������
void MonitorCalibration(BYTE *pRgbData, long lRgbDataLength, long lLineCount);

//LUT���ȥ���ǥå�����������
BOOL LutIndexGet(float fInPutData, short nLutColor, short *psIndex);

//LUT��ɽ��¬����������(¬���͡�
BOOL LutExampleDataGet(BRLUT_INPUT_DATA	lutMatchInputData,
						BRLUT_EXP_POINT *plutExampleData);

//LUT��ɽ����������(�����͡�
BOOL LutLogicalDataGet(BRLUT_INPUT_DATA	lutMatchInputData,
						BRLUT_LOGIC_POINT *plutLogicData);

//�ޥå��󥰥ǡ��� �׻�����
BOOL CreateMatchData(BRLUT_INPUT_DATA lutMatchInputData, BRLUT_EXP_POINT lutExampleData,
					BRLUT_LOGIC_POINT lutLogicData);

//LUT�������
BOOL LutSelect(BRLUT_SELECT lutSelectData, char * lutTableID);

//�ץ�󥿡��⡼�ɼ�������
BOOL GetPrinterMode(BRLUT_SELECT* plutSelectData);

//���顼�ޥå��󥰥ϥå������ �ϥå���ơ��֥��������
BOOL StoreColorHash(void);

//���顼�ޥå��󥰥ϥå������ �ϥå��帡������
BOOL LookupColorHash(BRLUT_INPUT_DATA lutMatchInputData);

//���顼�ޥå��󥰥ϥå������ ��λ����
BOOL EndColorHash(void);  

//LUT��ɽ��(������ ���ݥ����)¬���ǡ�������
//LUT��ɽ��(������ ���ݥ����)�����ǡ����ͼ���
//�ޥå��󥰥ǡ��� �׻�����
BOOL MatchDataGet(BRLUT_INPUT_DATA	*lutMatchInputData);

//-------------------------------------------------------------------�ޥå����ѥơ��֥�
BRMONCALDAT		gMonitorGammaDat;		//��˥��������֥졼�������͹�¤��


HANDLE			ghGammaCurveTable;		//�å���������ơ��֥�
BYTE			*gpbGammaCurveTable;

BRLUT_HEAD_DATA_00	gLutDataHead00;		//LUT �ǡ����إå�����
#ifdef	TABLE_01
BRLUT_HEAD_DATA_01	gLutDataHead;		//LUT �ǡ����إå�����
#endif	//TABLE_01


HANDLE			ghLutDataTable;			//LUT �ơ��֥�
BRLUT_DATA		*gpfLutDataTable;		//LUT DATA
#ifdef	GAMMA_ADJ

HANDLE			ghLutGammaTable;		//Gamma table
BRLUT_DATA		*gpfLutGammaTable;		//Gamma data
#endif	//GAMMA_ADJ
#ifdef	GRAY_ADJ

HANDLE			ghLutGrayTable;			//Gray table
BRLUT_DATA		*gpfLutGrayTable;		//Gray data
#endif	//GRAY_ADJ

CMATCH_INIT		gMatchingInitDat;		//���顼�ޥå��󥰽��������¤��


BRLUT_SELECT 	glutSelectData;			//LUT�������

char			szgWinSystem[145];		//Windows System �ǥ��쥯�ȥ�
#ifdef	INIT_LUT_NAME
char			szgLutDir[145];			//Lut Data �ǥ��쥯�ȥ�
#endif	//INIT_LUT_NAME
char			szgOpenFile[160];


HANDLE			ghHashTable;			//���顼�ޥå��󥰹�®���ѥϥå���ơ��֥�
COLOR_HASH_DAT	*gpHashTable;
//COLOR_HASH_KEY	colorHashKey[1000];		//���顼�ޥå��󥰹�®���ѥϥå��奭���ơ��֥�
//COLOR_HASH_KEY	colorHashKey[3375];		//���顼�ޥå��󥰹�®���ѥϥå��奭���ơ��֥�
int				gnColorHashFlag;		//���顼�ޥå��󥰹�®���ѥϥå�������ե饰��(=0 :OFF, =1 :ON)				

#else	//BRCOLOR
//---------------------------------------------------------------�������� �ؿ����
//���顼�ޥå��󥰽��������
BOOL ColorMatchingInit(CMATCH_INIT matchingInitDat);
//���顼�ޥå��󥰽�λ����
void ColorMatchingEnd(void);
//���顼�ޥå��󥰽���
BOOL ColorMatching(BYTE *pRgbData, long lRgbDataLength, long lLineCount);

//���顼�ޥå��󥰽��������
typedef BOOL (*COLORINIT)(CMATCH_INIT);
//���顼�ޥå��󥰽�λ����
typedef void (*COLOREND)(void);
//���顼�ޥå��󥰽���
typedef BOOL (*COLORMATCHING)(BYTE *, long , long );

#endif	//BRCOLOR

#endif // _H_BRCOLOR_

