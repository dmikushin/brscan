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
//	Source filename: brother_misc.c
//
//		Copyright(c) 1997-2000 Brother Industries, Ltd.  All Rights Reserved.
//
//
//	Abstract:
//			�Ƽ�ؿ���
//
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "brother_misc.h"


//-----------------------------------------------------------------------------
//
//	Function name:	GetPathFromFileName
//
//
//	Abstract:
//		�ե�����Υե�ѥ�̾����ѥ�̾���Ѵ�����
//
//
//	Parameters:
//		lpszLogStr
//			in:  �ե�����Υե�ѥ�̾�ؤΥݥ���
//			out: �ѥ�̾����Ǽ�����
//
//
//	Return values:
//		�ʤ�
//
//-----------------------------------------------------------------------------
//
void
GetPathFromFileName( LPSTR lpszFileName )
{
	int  nFileNameSize;
	LPSTR  lpszNameTop;


	lpszNameTop = lpszFileName;
	nFileNameSize = strlen( lpszFileName );
	lpszFileName += nFileNameSize;

	for( ; nFileNameSize > 0; --nFileNameSize ){
		if( *(--lpszFileName) == '\\')
			break;
	}
	if( nFileNameSize <= 0 ){
		*lpszNameTop = '\0';
	}else{
		*lpszFileName = '\0';
	}
}


//-----------------------------------------------------------------------------
//
//	Function name:	MakePathName
//
//
//	Abstract:
//		�ѥ�̾�˥ե����롿�ե����̾���ɲä���
//
//
//	Parameters:
//		lpszPathName
//			in:  �ѥ�̾�ؤΥݥ���
//			out: �ե����롿�ե�����Υե�ѥ�̾����Ǽ�����
//
//		lpszAddName
//			in:  �ե����롿�ե����̾�ؤΥݥ���
//
//
//	Return values:
//		�ʤ�
//
//-----------------------------------------------------------------------------
//
void
MakePathName( LPSTR lpszPathName, LPSTR lpszAddName )
{
	strcat( lpszPathName, "\\" );
	strcat( lpszPathName, lpszAddName );
}


//-----------------------------------------------------------------------------
//
//	Function name:	GetToken
//
//
//	Abstract:
//		����ʸ���󤫤�Token����Ф�
//
//
//	Parameters:
//		lppszData
//			in:  ʸ����ؤΥݥ��󥿤ؤΥݥ���
//			out: Token���Ф����ʸ����ؤΥݥ��󥿤���Ǽ�����
//
//
//	Return values:
//		���Ф���Token�ؤΥݥ���
//
//-----------------------------------------------------------------------------
//
LPSTR
GetToken( LPSTR *lppszData )
{
	LPSTR  lpszToken;
	LPSTR  lpszNextTop;


	lpszToken = lpszNextTop = *lppszData;

	if( lpszNextTop != NULL && *lpszNextTop ){
		while( *lpszNextTop ){
			if( *lpszNextTop == ',' ){
				*lpszNextTop++ = '\0';
				*lppszData = lpszNextTop;
				break;
			}else{
				lpszNextTop++;
			}
		}
	}else{
		lpszToken = NULL;
	}
	return lpszToken;
}


//-----------------------------------------------------------------------------
//
//	Function name:	StrToWord
//
//
//	Abstract:
//		ʸ��������(WORD)���Ѵ�
//
//
//	Parameters:
//		lpszText
//			ʸ����ؤΥݥ���
//
//
//	Return values:
//		�Ѵ����줿����
//
//-----------------------------------------------------------------------------
//
WORD
StrToWord( LPSTR lpszText )
{
	WORD   wResult = 0;
	char   chData = *lpszText;


	if( lpszText != NULL ){
		while( chData ){
			if( '0' <= chData && chData <= '9' ){
				wResult = wResult * 10 + ( chData - '0' );
			}else{
				wResult = 0;
				break;
			}
			lpszText++;
			chData = *lpszText;
		}
	}
	return wResult;
}


//-----------------------------------------------------------------------------
//
//	Function name:	WordToStr
//
//
//	Abstract:
//		����(WORD)��ʸ������Ѵ�
//
//
//	Parameters:
//		wValue
//			����(WORD)
//
//		lpszTextTop
//			ʸ������Ǽ����Хåե��ؤΥݥ���
//
//
//	Return values:
//		�Ѵ����줿ʸ����ؤΥݥ���
//
//-----------------------------------------------------------------------------
//
LPSTR
WordToStr( WORD wValue, LPSTR lpszTextTop )
{
	LPSTR  lpszText;
	char   szTemp[ 8 ];
	LPSTR  lpszTemp;
	int    nTextLen;

 
	lpszTemp = szTemp;
	do{
		*lpszTemp++ = '0' + ( wValue % 10 );
		wValue /= 10;
	}while( wValue > 0 );
	*lpszTemp = '\0';

	lpszText = lpszTextTop;
	nTextLen = strlen( szTemp );
	for( ; nTextLen > 0; nTextLen-- ){
		*lpszText++ = *(--lpszTemp);
	}
	*lpszText = '\0';

	return lpszTextTop;
}


//-----------------------------------------------------------------------------
//
//	Function name:	StrToShort
//
//
//	Abstract:
//		ʸ��������(short)���Ѵ�
//
//
//	Parameters:
//		lpszText
//			ʸ����ؤΥݥ���
//
//
//	Return values:
//		�Ѵ����줿����
//
//-----------------------------------------------------------------------------
//
short
StrToShort( LPSTR lpszText )
{
	short  nSign = 1;
	short  nResult = 0;
	char   chData = *lpszText;


	if( lpszText != NULL ){
		if( *lpszText == '-' ){
			nSign = -1;
			lpszText++;
		}
		while( chData ){
			if( '0' <= chData && chData <= '9' ){
				nResult = nResult * 10 + ( chData - '0' );
			}else{
				nResult = 0;
				break;
			}
			lpszText++;
			chData = *lpszText;
		}
		nResult *= nSign;
	}
	return nResult;
}


//-----------------------------------------------------------------------------
//
//	Function name:	ShortToStr
//
//
//	Abstract:
//		����(short)��ʸ������Ѵ�
//
//
//	Parameters:
//		nValue
//			����(short)
//
//		lpszTextTop
//			ʸ������Ǽ����Хåե��ؤΥݥ���
//
//
//	Return values:
//		�Ѵ����줿ʸ����ؤΥݥ���
//
//-----------------------------------------------------------------------------
//
LPSTR
ShortToStr( short nValue, LPSTR lpszTextTop )
{
	LPSTR  lpszText;
	char   szTemp[ 8 ];
	LPSTR  lpszTemp;
	int    nTextLen;
	BOOL   bSign = FALSE;


	if( nValue < 0 ){
		nValue *= -1;
		bSign = TRUE;
	}
	lpszTemp = szTemp;
	do{
		*lpszTemp++ = '0' + ( nValue % 10 );
		nValue /= 10;
	}while( nValue > 0 );
	*lpszTemp = '\0';

	lpszText = lpszTextTop;
	if( bSign ){
		*lpszText++ = '-';
	}
	nTextLen = strlen( szTemp );
	for( ; nTextLen > 0; nTextLen-- ){
		*lpszText++ = *(--lpszTemp);
	}
	*lpszText = '\0';

	return lpszTextTop;
}


//-----------------------------------------------------------------------------
//
//	Function name:	StrToDword
//
//
//	Abstract:
//		ʸ��������(DWORD)���Ѵ�
//
//
//	Parameters:
//		lpszText
//			ʸ����ؤΥݥ���
//
//
//	Return values:
//		�Ѵ����줿����
//
//-----------------------------------------------------------------------------
//
DWORD
StrToDword( LPSTR lpszText )
{
	DWORD  dwResult = 0;
	char  chData = *lpszText;


	if( lpszText != NULL ){
		while( chData ){
			if( '0' <= chData && chData <= '9' ){
				dwResult = dwResult * 10 + ( chData - '0' );
			}else{
				dwResult = 0;
				break;
			}
			chData = *lpszText++;
		}
	}
	return dwResult;
}


//-----------------------------------------------------------------------------
//
//	Function name:	DwordToStr
//
//
//	Abstract:
//		����(DWORD)��ʸ������Ѵ�
//
//
//	Parameters:
//		dwValue
//			����(DWORD)
//
//		lpszTextTop
//			ʸ������Ǽ����Хåե��ؤΥݥ���
//
//
//	Return values:
//		�Ѵ����줿ʸ����ؤΥݥ���
//
//-----------------------------------------------------------------------------
//
LPSTR
DwordToStr( DWORD dwValue, LPSTR lpszTextTop )
{
	LPSTR  lpszText;
	char   szTemp[ 16 ];
	LPSTR  lpszTemp;
	int    nTextLen;


	lpszTemp = szTemp;
	do{
		*lpszTemp++ = '0' + (BYTE)( dwValue % 10 );
		dwValue /= 10;
	}while( dwValue > 0 );
	*lpszTemp = '\0';

	lpszText = lpszTextTop;
	nTextLen = strlen( szTemp );
	for( ; nTextLen > 0; nTextLen-- ){
		*lpszText++ = *(--lpszTemp);
	}
	*lpszText = '\0';

	return lpszTextTop;
}


//////// end of brother_misc.c ////////
