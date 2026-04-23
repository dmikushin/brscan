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
//	Source filename: brother_scanner.c
//
//	Copyright(c) 1995-2000 Brother Industries, Ltd.  All Rights Reserved.
//
//
//	Abstract:
//			the module for scan process
//
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include <dlfcn.h>
#include <string.h>
#include <usb.h>
#include <sys/time.h>

#include <sane/sane.h>

#include "brother_cmatch.h"
#include "brother_mfccmd.h"
#include "brother_devaccs.h"
#include "brother_devinfo.h"
#include "brother_log.h"
#include "brother_modelinf.h"
#include "brother_scandec.h"
#include "brother_deccom.h"
#include "brother_advini.h"
#include "brother_bugchk.h"

#include "brother_scanner.h"
#include "brother_color.h"

#ifdef NO39_DEBUG
#include <sys/time.h>
#endif

//$$$$$$$$
LPSTR   lpRxBuff = NULL;
LPBYTE  lpRxTempBuff = NULL;
DWORD   dwRxTempBuffLength = 0;
LPSTR   lpFwTempBuff = NULL;
DWORD	dwFwTempBuffMaxSize= 0;
int     FwTempBuffLength = 0;
BOOL    bTxScanCmd = FALSE;
DWORD	dwRxBuffMaxSize= 0;

LONG lRealY = 0;

HANDLE	hGray;			//Gray table from brgray.bin

//$$$$$$$$

//
// the valiables used for the expansion of the scaned data and exchange the resolution
//
SCANDEC_OPEN   ImageProcInfo;
SCANDEC_WRITE  ImgLineProcInfo;
DWORD  dwImageBuffSize;
DWORD  dwWriteImageSize;
int    nWriteLineCount;


//
// the name of ScenDec dinamic link library
//
#if       BRSANESUFFIX == 2
static char  szScanDecDl[] = "libbrscandec2.so";
#elif  BRSANESUFFIX == 1
static char  szScanDecDl[] = "libbrscandec.so.1";
#else
Not support (force causing compile error)
#endif   //BRSANESUFFIX


BOOL	bReceiveEndFlg;


// Static valiable for debug process
int nPageScanCnt;
int nReadCnt;


DWORD dwFWImageSize;
DWORD dwFWImageLine;
int nFwLenTotal;

//-----------------------------------------------------------------------------
//
//	Function name:	LoadScanDecDll
//
//
//	Abstract:
//		Load the ScanDec library and get the procedure addresses
//
//
//	Parameters:
//		None
//
//
//	Return values:
//		TRUE  = SUCCESS
//		FALSE = cannot find the ScenDec library (error occured)
//
//-----------------------------------------------------------------------------
//	LoadColorMatchDll (The parts of DllMain in the Windows version)
BOOL
LoadScanDecDll( Brother_Scanner *this )
{
	BOOL  bResult = TRUE;


	this->scanDec.hScanDec = dlopen ( szScanDecDl, RTLD_LAZY );

	if( this->scanDec.hScanDec != NULL ){
		//
		// get the procedure addresses  of scanDec
		//
		this->scanDec.lpfnScanDecOpen      = dlsym ( this->scanDec.hScanDec, "ScanDecOpen" );
		this->scanDec.lpfnScanDecSetTbl    = dlsym ( this->scanDec.hScanDec, "ScanDecSetTblHandle" );
		this->scanDec.lpfnScanDecPageStart = dlsym ( this->scanDec.hScanDec, "ScanDecPageStart" );
		this->scanDec.lpfnScanDecWrite     = dlsym ( this->scanDec.hScanDec, "ScanDecWrite" );
		this->scanDec.lpfnScanDecPageEnd   = dlsym ( this->scanDec.hScanDec, "ScanDecPageEnd" );
		this->scanDec.lpfnScanDecClose     = dlsym ( this->scanDec.hScanDec, "ScanDecClose" );

		if(  this->scanDec.lpfnScanDecOpen == NULL ||
			 this->scanDec.lpfnScanDecSetTbl  == NULL ||
			 this->scanDec.lpfnScanDecPageStart  == NULL ||
			 this->scanDec.lpfnScanDecWrite  == NULL ||
			 this->scanDec.lpfnScanDecPageEnd  == NULL ||
			 this->scanDec.lpfnScanDecClose  == NULL )
		{
			// ERROR: library exists but cannot get the procedure address 
			dlclose ( this->scanDec.hScanDec );
			this->scanDec.hScanDec = NULL;
			bResult = FALSE;
		}
	}else{
		this->scanDec.lpfnScanDecOpen      = NULL;
		this->scanDec.lpfnScanDecSetTbl    = NULL;
		this->scanDec.lpfnScanDecPageStart = NULL;
		this->scanDec.lpfnScanDecWrite     = NULL;
		this->scanDec.lpfnScanDecPageEnd   = NULL;
		this->scanDec.lpfnScanDecClose     = NULL;
		bResult = FALSE;
	}
	return bResult;
}


//-----------------------------------------------------------------------------
//
//	Function name:	FreeScanDecDll
//
//
//	Abstract:
//		Free the ScanDec DLL
//
//
//	Parameters:
//		None
//
//
//	Return values:
//		None
//
//-----------------------------------------------------------------------------
//
void
FreeScanDecDll( Brother_Scanner *this )
{
	if( this->scanDec.hScanDec != NULL ){
		//
		// Free ColorMatch DLL
		//
		dlclose ( this->scanDec.hScanDec );
		this->scanDec.hScanDec = NULL;
	}
}


/******************************************************************************
 *									      *
 *	FUNCTION	ScanStart					      *
 *									      *
 *	PURPOSE		Start process of the scan			      *
 *				start the resource manager and open the BI-DI *
 *									      *
 *	IN		Brother_Scanner *this				      *
 *	OUT		None						      *
 *									      *
 ******************************************************************************/
BOOL
ScanStart( Brother_Scanner *this )
{
	BOOL  bResult;

	int   rc;

	WriteLog( "" );
	WriteLog( ">>>>> Start Scanning >>>>>" );
	WriteLog( "nPageCnt = %d bEOF = %d  iProcessEnd = %d\n",
		this->scanState.nPageCnt, this->scanState.bEOF, this->scanState.iProcessEnd);

#if 1
	// debug for MASU
	dwFWImageSize = 0;
	dwFWImageLine = 0;
	nFwLenTotal = 0;
#endif
	this->scanState.nPageCnt++;
	this->scanState.bReadbufEnd=FALSE;
	this->scanState.bEOF=FALSE;

	if( this->scanState.nPageCnt == 1){
		int nResoLine;

		bTxScanCmd = FALSE;
		lRealY = 0;

		this->devScanInfo.wResoType  = this->uiSetting.wResoType;
		this->devScanInfo.wColorType = this->uiSetting.wColorType;

#ifndef DEBUG_No39

		{
		  //if(this->hScanner ==NULL){
		  //  this->hScanner = calloc(sizeof(dev_handle),1);
		  //}
		  this ->hScanner->usb_w_ep
		    = get_p_model_info_by_index(this->modelInf.index)->w_endpoint;
		  this ->hScanner->usb_r_ep
		    = get_p_model_info_by_index(this->modelInf.index)->r_endpoint;

		  if (IFTYPE_USB == this->hScanner->device){
		    this->hScanner->usb=usb_open(g_pdev->pdev);
		    if (!this->hScanner->usb)
		      return SANE_STATUS_IO_ERROR;

		    //05/11/11 Fix to escape I/O error
		    //if (usb_set_configuration(this->hScanner->usb, 1))
		    //  return SANE_STATUS_IO_ERROR;

		    // degrade :  2006/04/05 M-LNX-23 kado
		    //(M-LNX-24 2006/04/11 kado for Fedora5 USB2.0)
		    //errornum = usb_set_configuration(this->hScanner->usb, 1);
		    usb_set_configuration_or_reset_toggle(this, 1);
		    if (usb_claim_interface(this->hScanner->usb, 1))
		      return SANE_STATUS_IO_ERROR;

		    //(05/11/11 Fix to escape I/O error)   if (usb_set_configuration(this->hScanner->usb, 1))
		    //(05/11/11 Fix to escape I/O error)     return SANE_STATUS_IO_ERROR;
		  }
		}

		// Open Device
		rc= OpenDevice(this->hScanner, this->modelInf.seriesNo);
		if (!rc)
			return SANE_STATUS_IO_ERROR;
#endif

		CnvResoNoToUserResoValue( &this->scanInfo.UserSelect, this->devScanInfo.wResoType );

		bResult = QueryScannerInfo( this );
		if (!bResult)
			return SANE_STATUS_INVAL;

		GetScanAreaParam( this );


		// treat as error when the start position of the scan area is out of the MAX scan area 
		if ( (this->devScanInfo.wScanSource == MFCSCANSRC_FB) &&
			(this->scanInfo.ScanAreaMm.top >= (LONG)(this->devScanInfo.dwMaxScanHeight - 80)) )
			return SANE_STATUS_INVAL;


		bResult = StartDecodeStretchProc( this );
		if (!bResult)
			return SANE_STATUS_INVAL;

		//
		// Initialize the ColorMatch
		//
		InitColorMatchingFunc( this, this->devScanInfo.wColorType, CMATCH_DATALINE_RGB );

		//
		// start the page process of Decode/Stretch
		//
		if( this->devScanInfo.wColorType == COLOR_DTH || this->devScanInfo.wColorType == COLOR_TG ){
			//
			// Preparation of adjusting the Brightness/Contrast
			//
			if( this->uiSetting.nBrightness == 0 && this->uiSetting.nContrast == 0 ){
				//
				// Use the original gray table when both of Brightness and Contrast are the middle position 
				//
				hGray = this->cmatch.hGrayTbl;
			}else{
				//
				// Create the GrayTable for adjustiong the Brightness/Contrast
				//
				hGray = SetupGrayAdjust( this );
			}
		}else{
			hGray = NULL;
		}

		//
		// Allocate the buffer to store the received data
		//

		// Allocate the tranmission-preserve buffer , the buffer has enough area to keep more than 3 lines  
		//
		if (this->devScanInfo.wColorType == COLOR_FUL || this->devScanInfo.wColorType == COLOR_FUL_NOCM)
			dwRxBuffMaxSize = (this->devScanInfo.ScanAreaByte.lWidth + 3) * 3; 
		else
			dwRxBuffMaxSize = (this->devScanInfo.ScanAreaByte.lWidth + 3);

		dwRxBuffMaxSize *= (3 + 1); // Allocate the 4 line size (to keep more than 3 line )

		if (dwRxBuffMaxSize < (DWORD)gwInBuffSize)
			dwRxBuffMaxSize = (DWORD)gwInBuffSize;

		lpRxBuff = AllocReceiveBuffer( dwRxBuffMaxSize  );
		lpRxTempBuff = (LPBYTE)MALLOC( dwRxBuffMaxSize  );

		if( lpRxBuff == NULL || lpRxTempBuff == NULL ){
			//
			//  Because the buffer cannot be allocated ,return with error code
			//
			return SANE_STATUS_NO_MEM;
		}

		dwRxTempBuffLength = 0;
		FwTempBuffLength = 0;

		// Allocate the Transmission-preserve buffer

		// Allocate the buffer that contains more than 6 lins
		dwFwTempBuffMaxSize = this->scanInfo.ScanAreaByte.lWidth * 6;
		dwFwTempBuffMaxSize *=2;

		// If exchangeing the resolution is required , allocate the additional area to store the 
		//      expanded data
		nResoLine = this->scanInfo.UserSelect.wResoY / this->devScanInfo.DeviceScan.wResoY;
		if (nResoLine > 1) // Check whether exchanging the resolution is required or not
			dwFwTempBuffMaxSize *= nResoLine;

		// if the value is less than gwInBuffSize, expand to the same size .
		if (dwFwTempBuffMaxSize < (DWORD)gwInBuffSize)
			dwFwTempBuffMaxSize = (DWORD)gwInBuffSize;

		lpFwTempBuff = MALLOC( dwFwTempBuffMaxSize );
		WriteLog( " dwRxBuffMaxSize = %d, dwFwTempBuffMaxSize = %d, ", dwRxBuffMaxSize, dwFwTempBuffMaxSize );
		if( lpFwTempBuff == NULL ){
			//
			// if buffer allocation failed , return with No-Memory-error status
			//
			return SANE_STATUS_NO_MEM;
		}

		// Start page scanning
		if (!PageScanStart( this )) {
			ScanEnd( this);
			return SANE_STATUS_INVAL;
		}

		this->scanState.bScanning=TRUE;
		this->scanState.bCanceled=FALSE;
		this->scanState.iProcessEnd=0;
	}
	else {
		WriteLog( " PageStart scanState.iProcessEnd = %d, ", this->scanState.iProcessEnd );

		lRealY = 0;
		dwRxTempBuffLength = 0;
		FwTempBuffLength = 0;

		//06/02/28
		if ((this->devScanInfo.wScanSource == MFCSCANSRC_ADF || this->devScanInfo.wScanSource == MFCSCANSRC_ADF_DUP) 
			&& this->scanState.iProcessEnd == SCAN_MPS) {
			// The case  next page exists
			//    start the scanning
			this->scanState.iProcessEnd=0;
			if (!PageScanStart( this )) {
				return SANE_STATUS_INVAL;
			}
			bResult = SANE_STATUS_GOOD;
		}
		else if (this->scanState.iProcessEnd == SCAN_EOF){
			// The case  next page doesn&t exist
			bResult = SANE_STATUS_NO_DOCS;
			return bResult;
		}
		else {
			// The case  the scan-button is pushed during scanning
			bResult = SANE_STATUS_DEVICE_BUSY;
			return bResult;
		}
	}

	// Start the process of expanding the compressed data
	if (this->scanDec.lpfnScanDecSetTbl != NULL && this->scanDec.lpfnScanDecPageStart != NULL) {
		this->scanDec.lpfnScanDecSetTbl( hGray, NULL );
		bResult = this->scanDec.lpfnScanDecPageStart();
		if (!bResult)
			return SANE_STATUS_INVAL;
		else
			bResult = SANE_STATUS_GOOD;
	}
	else {
		return SANE_STATUS_INVAL;
	}

	nPageScanCnt = 0;	// clear the debug-counter

	nReadCnt = 0;	// clear the debug-counter

	return bResult;
}


//-----------------------------------------------------------------------------
//
//	Function name:	PageScanStart
//
//
//	Abstract:
//		Start the page-scanning
//
//
//	Parameters:
//		hWndDlg
//			Handle for windows
//
//
//	Return values:
//		None
//
//-----------------------------------------------------------------------------
//	PageScanStart��A part of PageScan of Windows version��
BOOL
PageScanStart( Brother_Scanner *this )
{
	BOOL rc=FALSE;

	if( this->scanState.nPageCnt == 1 ){
		//send command only 1st page
		if( !bTxScanCmd ){
			//
			// Start-scan-command hasn't be sended yet
			//
			char  szCmdStr[ MFCMAXCMDLENGTH ];
			int   nCmdStrLen;

			//
			// Initialise the command-sended-flag
			//
			bTxScanCmd = TRUE;		// Start-scan-command has already been sended 
			bTxCancelCmd = FALSE;	// Cancel command hasn't been sended yet 

			//
			// send the start-scan-command
			//
			nCmdStrLen = MakeupScanStartCmd( this, szCmdStr );
			if (WriteDeviceCommand( this->hScanner, szCmdStr, nCmdStrLen, this->modelInf.seriesNo ))
				rc=TRUE;

			sleep(3);
			WriteLogScanCmd( "Write",  szCmdStr );
		}
	}else if( this->scanState.nPageCnt > 1 ){

		//
		// The command for starting to scan the 2nd page or following page 
		//
		if (WriteDeviceCommand( this->hScanner, MFCMD_SCANNEXTPAGE, strlen( MFCMD_SCANNEXTPAGE ), this->modelInf.seriesNo))
			rc = TRUE;

		sleep(3);
		WriteLogScanCmd( "Write",  MFCMD_SCANNEXTPAGE );
	}

	return rc;
}


/**************************************************************************
 *									  *
 *	FUNCTION	StatusChk(2006/02/28 FIX)			  *
 *									  *
 *	PURPOSE		Check whether the status code is available or not *
 *									  *
 *	IN		char *lpBuff	buffer to be checked		  *
 *			int nDataSize	size of buffer			  *
 *									  *
 *	OUT		None						  *
 *                                                                        *
 *      Return value	0	Status code is not availabel		  *
 *			1	Status code is available		  *
 *                      2       Status code is DUPLEX_NORMAL              *
 *                      3       Status code is DUPLEX_REVERSE             *
 *									  *
 **************************************************************************/
BOOL
StatusChk(char *lpBuff, int nDataSize)
{
	BOOL	rc=FALSE;
	LPSTR	pt = lpBuff;

	while( 1 ) {
		BYTE headch;
		WORD length;

		if( nDataSize <= 0 )	break;	// All data can be processed (receive all of cluster)

		headch = (BYTE)*pt;

		if ((signed char)headch < 0) {
			// code of STATUS,CTRL family
			// refer the following header information
			//06/02/28
			rc=1;

			if(headch == 0x84){
			  rc=2;
			}
			else if(headch == 0x85){
			  rc=3;
			}

			WriteLog( ">>> StatusChk header=%2x <<<", headch);
			break;
		}else{
			if (headch == 0) {
				length = 1;
				pt += length;
			}
			else {
				// Image data

				if( nDataSize < 3 )
					length = 0;		// clear
				else
					// Acquire the information of raster data size
					length = ((unsigned char)pt[1] << 8) | (unsigned char)pt[2]; // big-endian for brscan4 models

				if( nDataSize < ( length + 3) ){	// length+3 = head(1B)+length(2B)+data(length)
					break;
				}
				else{
					// whole of 1 line data exists
					nDataSize -= length + 3;	// Consume the Image data size (length +3)
					pt += length + 3;			// get the next header information
				}
			}
			WriteLog( "Header=%2x  Count=%4d nDataSize=%d", (BYTE)headch, length, nDataSize );
		}
	}
	return rc;
}

#define READ_TIMEOUT 5*60*1000 // 5 minute

#define NO39_DEBUG


#ifdef NO39_DEBUG
static struct timeval save_tv;		// Valiable for time-interval information(sec,msec)
static struct timezone save_tz;		// Valiable for time-interval information(min)

#endif


#if       BRSANESUFFIX == 2

/******************************************************************************
 *  PageScanColor — 24-bit color (JPEG) scan path for brscan4 models.
 *
 *  Unlike mono packbits, the scanner sends a single contiguous JPEG stream
 *  framed in per-block wrappers. We collect the full JPEG into a temp file,
 *  then libjpeg-decode scanlines into the SANE output buffer.
 *
 *  Block wire format (empirical, per reference make_cache_block RE):
 *    [hdr_byte < 0x80][len_lo][len_hi][len bytes of JPEG payload]
 *  Status bytes (>= 0x80) terminate the JPEG stream:
 *    0x80 Page End, 0x81 NextPage, 0x82/0x86 page-boundary,
 *    0x83/0xE3 Cancel, 0x84/0x85 Duplex sync (skip 2 bytes and continue).
 *
 *  On first entry for a page the temp file is created; subsequent calls
 *  continue collecting until a status byte is seen, then switch to
 *  decoding and stream scanlines until output_height is reached.
 ******************************************************************************/
#define COLOR_RXBUF_SIZE 131072
static char g_colorResidue[COLOR_RXBUF_SIZE];
static int  g_colorResidueLen = 0;
static int  g_colorBlocksSeen = 0;

static int
PageScanColor( Brother_Scanner *this, char *lpFwBuf, int nMaxLen, int *lpFwLen )
{
	int rc;
	*lpFwLen = 0;

	if (this->scanState.bCanceled) {
		brother_color_cleanup();
		g_colorResidueLen = 0;
		g_colorBlocksSeen = 0;
		this->scanState.bScanning = FALSE;
		this->scanState.bCanceled = FALSE;
		return SANE_STATUS_CANCELLED;
	}

	ColorScanPhase phase = brother_color_phase();

	if (phase == COLOR_SCAN_IDLE) {
		if (brother_color_begin_page(this->scanState.nPageCnt + 1) != 0)
			return SANE_STATUS_NO_MEM;
		g_colorResidueLen = 0;
		g_colorBlocksSeen = 0;
		phase = COLOR_SCAN_COLLECTING;
	}

	/* -- COLLECTING: read USB, parse blocks, append payload -- */
	while (phase == COLOR_SCAN_COLLECTING) {
		int space = COLOR_RXBUF_SIZE - g_colorResidueLen;
		if (space <= 0) {
			WriteLog("  color: residue buffer full, forcing decode");
			brother_color_begin_decode();
			phase = brother_color_phase();
			break;
		}
		int nread = ReadNonFixedData(this->hScanner,
			g_colorResidue + g_colorResidueLen, space,
			READ_TIMEOUT, this->modelInf.seriesNo);
		if (nread < 0) {
			WriteLog("  color: USB read error rc=%d — force decode", nread);
			brother_color_begin_decode();
			phase = brother_color_phase();
			break;
		}
		g_colorResidueLen += nread;

		int i = 0;
		int saw_terminator = 0;
		while (i < g_colorResidueLen) {
			unsigned char h = (unsigned char)g_colorResidue[i];
			if (h >= 0x80) {
				/* Duplex sync: 2-byte marker, skip */
				if (h == 0x84 || h == 0x85) {
					if (g_colorResidueLen - i < 2) break;
					WriteLog("  color: duplex sync 0x%02x", h);
					i += 2;
					continue;
				}
				/* Any other status byte = end of JPEG stream */
				WriteLog("  color: terminator 0x%02x after %d blocks (%d residue)",
					h, g_colorBlocksSeen, g_colorResidueLen - i - 1);
				i++;
				saw_terminator = 1;
				break;
			}
			/* Data block [hdr][len_lo][len_hi][payload] */
			if (g_colorResidueLen - i < 3) break;
			int clen = (unsigned char)g_colorResidue[i+1]
				| ((unsigned char)g_colorResidue[i+2] << 8);
			int block_total = 3 + clen;
			if (g_colorResidueLen - i < block_total) break;
			if (g_colorBlocksSeen < 3) {
				unsigned char *p = (unsigned char *)&g_colorResidue[i+3];
				WriteLog("  color block[%d] hdr=0x%02x clen=%d payload[0..7]=%02x %02x %02x %02x %02x %02x %02x %02x",
					g_colorBlocksSeen, h, clen,
					clen>0?p[0]:0, clen>1?p[1]:0, clen>2?p[2]:0, clen>3?p[3]:0,
					clen>4?p[4]:0, clen>5?p[5]:0, clen>6?p[6]:0, clen>7?p[7]:0);
			}
			if (brother_color_append_payload(&g_colorResidue[i+3], clen) != 0) {
				brother_color_cleanup();
				g_colorResidueLen = 0;
				this->scanState.bScanning = FALSE;
				return SANE_STATUS_IO_ERROR;
			}
			g_colorBlocksSeen++;
			i += block_total;
		}
		/* Shift unparsed residue */
		if (i > 0 && i < g_colorResidueLen) {
			memmove(g_colorResidue, g_colorResidue + i, g_colorResidueLen - i);
		}
		g_colorResidueLen -= i;

		if (saw_terminator) {
			if (brother_color_begin_decode() != 0) {
				brother_color_cleanup();
				g_colorResidueLen = 0;
				this->scanState.bScanning = FALSE;
				return SANE_STATUS_IO_ERROR;
			}
			phase = brother_color_phase();
			break;
		}
		/* else: need more USB data; loop back to ReadNonFixedData */
	}

	/* -- DECODING: stream scanlines -- */
	if (phase == COLOR_SCAN_DECODING) {
		rc = brother_color_read_scanlines(lpFwBuf, nMaxLen);
		if (rc < 0) {
			brother_color_cleanup();
			g_colorResidueLen = 0;
			this->scanState.bScanning = FALSE;
			return SANE_STATUS_IO_ERROR;
		}
		*lpFwLen = rc;
		phase = brother_color_phase();
		if (phase == COLOR_SCAN_DONE) {
			this->scanState.bEOF = TRUE;
			brother_color_cleanup();
			g_colorResidueLen = 0;
			g_colorBlocksSeen = 0;
			if (rc > 0) {
				/* Data this call; next call gets EOF with lpFwLen=0. */
				return SANE_STATUS_GOOD;
			}
			this->scanState.bScanning = FALSE;
			return SANE_STATUS_EOF;
		}
		return SANE_STATUS_GOOD;
	}

	if (phase == COLOR_SCAN_DONE) {
		brother_color_cleanup();
		g_colorResidueLen = 0;
		this->scanState.bEOF = TRUE;
		this->scanState.bScanning = FALSE;
		return SANE_STATUS_EOF;
	}

	/* ERROR */
	brother_color_cleanup();
	g_colorResidueLen = 0;
	this->scanState.bScanning = FALSE;
	return SANE_STATUS_IO_ERROR;
}

/******************************************************************************
 *									      *
 *	FUNCTION	PageScan					      *
 *									      *
 *	PURPOSE		Scan 1 page					      *
 *									      *
 *      ARGUMENT	Brother_Scanner *this	�� Brother_Scanner structure  *
 *			char *lpFwBuf		�� transmission buffer	      *
 *			int nMaxLen		�� size of transmission buffer*
 *			int *lpFwLen		�� size of data to sent	      *
 *									      *
 *									      *
 *									      *
 ******************************************************************************/
int
PageScan( Brother_Scanner *this, char *lpFwBuf, int nMaxLen, int *lpFwLen )
{
	WORD	wData=0;	// received data size (byte)
	WORD	wDataLineCnt=0;	// number of received lines
	int	nAnswer=0;
	int	rc;
	LPBYTE  lpRxTop;
	WORD	wProcessSize;

	int	nReadSize;
	LPBYTE  lpReadBuf;
	int	nMinReadSize; // minimum read size

	//05/07/31 For select scan are
	int     nHeadOnly;
	WORD    nLength;

#ifdef NO39_DEBUG
	struct timeval start_tv, tv;
	struct timezone tz;
	long   nSec, nUsec;
#endif
	if (!this->scanState.bScanning) {
		rc = SANE_STATUS_IO_ERROR;
		return rc;
	}
	if (this->scanState.bCanceled) { //cancel-process
		WriteLog( "Page Canceled" );

		rc = SANE_STATUS_CANCELLED;
		this->scanState.bScanning=FALSE;
		this->scanState.bCanceled=FALSE;
		this->scanState.nPageCnt = 0;

		return rc;
	}

	/* brscan4 + 24-bit color: scanner sends a JPEG stream, not packbits.
	 * Route to the dedicated libjpeg-based path that bypasses ProcessMain. */
	if (this->modelInf.seriesNo >= MUST_CONVERT_MODEL &&
	    (this->devScanInfo.wColorType == COLOR_FUL ||
	     this->devScanInfo.wColorType == COLOR_FUL_NOCM)) {
		return PageScanColor(this, lpFwBuf, nMaxLen, lpFwLen);
	}

	nPageScanCnt++;
	WriteLog( ">>> PageScan Start <<< cnt=%d nMaxLen=%d\n", nPageScanCnt, nMaxLen);

#ifdef NO39_DEBUG
	if (gettimeofday(&tv, &tz) == 0) {

		if (tv.tv_usec < save_tv.tv_usec) {
			tv.tv_usec += 1000 * 1000 ;
			tv.tv_sec-- ;
		}
		nUsec = tv.tv_usec - save_tv.tv_usec;
		nSec = tv.tv_sec - save_tv.tv_sec;

		WriteLog( " No39 nSec = %d Usec = %d\n", nSec, nUsec ) ;
	}
#endif


	WriteLog( "scanInfo.ScanAreaSize.lWidth = [%d]", this->scanInfo.ScanAreaSize.lWidth );
	WriteLog( "scanInfo.ScanAreaSize.lHeight = [%d]", this->scanInfo.ScanAreaSize.lHeight );
	WriteLog( "scanInfo.ScanAreaByte.lWidth = [%d]", this->scanInfo.ScanAreaByte.lWidth );
	WriteLog( "scanInfo.ScanAreaByte.lHeight = [%d]", this->scanInfo.ScanAreaByte.lHeight );

	WriteLog( "devScanInfo.ScanAreaSize.lWidth = [%d]", this->devScanInfo.ScanAreaSize.lWidth );
	WriteLog( "devScanInfo.ScanAreaSize.lHeight = [%d]", this->devScanInfo.ScanAreaSize.lHeight );
	WriteLog( "devScanInfo.ScanAreaByte.lWidth = [%d]", this->devScanInfo.ScanAreaByte.lWidth );
	WriteLog( "devScanInfo.ScanAreaByte.lHeight = [%d]", this->devScanInfo.ScanAreaByte.lHeight );
	WriteLog( "ReadbufEnd is %d",this->scanState.bReadbufEnd); //050428


	memset(lpFwBuf, 0x00, nMaxLen);	//  clear the transmission buffer to zero
	*lpFwLen = 0;

	if ( (!this->scanState.iProcessEnd) && ( FwTempBuffLength < nMaxLen) ) { 
	// The case that the status code hasn't be received yet and
	//      the size of transmission-buffer is less than the size of data in a transmission-stored data  

	// if the data exists in a data storing buffer , copy the data to the receive-buffer
	memmove( lpRxBuff, lpRxTempBuff, dwRxTempBuffLength );	// restore the keeped data
	wData += dwRxTempBuffLength;	// revise the lenght of the stored data

	lpRxTop = lpRxBuff;

	// get the data from the device to expand the more than 3 line data
	if (this->devScanInfo.wColorType == COLOR_FUL || this->devScanInfo.wColorType == COLOR_FUL_NOCM)
		nMinReadSize = (this->devScanInfo.ScanAreaByte.lWidth + 3) * 3;
	else
		nMinReadSize = (this->devScanInfo.ScanAreaByte.lWidth + 3);

	nMinReadSize *= 3; // Get more than 3 line
	if ( !this->scanState.bReadbufEnd ) {
		for (rc=0 ; wData < nMinReadSize;)
		{

			nReadSize = dwRxBuffMaxSize - (dwRxTempBuffLength + wData);
			lpReadBuf = lpRxTop+wData;

			nReadCnt++;
			WriteLog( "Read request size is %d, (dwRxTempBuffLength = %d)", gwInBuffSize - dwRxTempBuffLength, dwRxTempBuffLength );
			WriteLog( "PageScan ReadNonFixedData Cnt = %d", nReadCnt );

			rc = ReadNonFixedData( this->hScanner, lpReadBuf, nReadSize, READ_TIMEOUT, this->modelInf.seriesNo );
			if (rc < 0) {
				this->scanState.bReadbufEnd = TRUE;
				WriteLog( "  bReadbufEnd =TRUE" );
				break;
			}
			else if (rc > 0){
				//06/02/28
				int sc;
				wData += rc;

				/* StatusChk uses the standard line format to scan for
				 * status bytes. For brscan4 models, the data format is
				 * different and StatusChk would false-positive on
				 * compressed data bytes >= 0x80. Skip it. */
				if (this->modelInf.seriesNo >= MUST_CONVERT_MODEL)
					sc = 0;
				else
					sc = StatusChk(lpRxBuff, wData);
				if (sc == 1) { // check whether the status code has been received or not
					this->scanState.bReadbufEnd = TRUE;
					WriteLog( "bReadbufEnd =TRUE" );
					break;
				}
				else if(sc == 2){
				  break;
				}
			}
		 }
	}

	WriteLog( "Read data size is %d, (dwRxTempBuffLength = %d)", wData, dwRxTempBuffLength );

	WriteLog( "Adjusted wData = %d, (dwRxTempBuffLength = %d)", wData, dwRxTempBuffLength );

	if (wData != 0)
	// spilit the data to each lines
	{
	LPBYTE  pt = lpRxBuff;
	int nFwTempBuffMaxLine;
	int nResoLine;

	// the width (dot) of image data to sent
	if (this->devScanInfo.wColorType == COLOR_FUL || this->devScanInfo.wColorType == COLOR_FUL_NOCM ) {
		nFwTempBuffMaxLine = (dwFwTempBuffMaxSize / 2 - FwTempBuffLength) / this->scanInfo.ScanAreaByte.lWidth;
		nFwTempBuffMaxLine *= 3;
	}
	else {
		nFwTempBuffMaxLine = (dwFwTempBuffMaxSize / 2 - FwTempBuffLength) / this->scanInfo.ScanAreaByte.lWidth;
	}
	nResoLine= this->scanInfo.UserSelect.wResoY / this->devScanInfo.DeviceScan.wResoY;
	if (nResoLine > 1)
		nFwTempBuffMaxLine /= nResoLine;

	//05/07/31
	nHeadOnly=0;
	nLength=0;

	dwRxTempBuffLength = wData;

#if BRSANESUFFIX == 2
	/*
	 * DCP-1510 and similar brscan4 models (seriesNo >= MUST_CONVERT_MODEL)
	 * use a different line format than the standard Brother protocol:
	 *
	 *   [42 07 00 01 00 84 00 00 00 00]  10-byte per-line wrapper
	 *   [NN]                              1-byte compressed data length
	 *   [00]                              1-byte padding/separator
	 *   [NN bytes of packbits data]       compressed raster data
	 *
	 * Total per line: 12 + NN bytes.
	 * Verified by hex dump analysis of raw USB data from DCP-1510.
	 */
	if (this->modelInf.seriesNo >= MUST_CONVERT_MODEL) {
		for (wDataLineCnt=0; wDataLineCnt < nFwTempBuffMaxLine;) {
			if (dwRxTempBuffLength == 0) break;

			BYTE headch = (BYTE)*pt;

			/* brscan4 sends bare 1-byte status codes between frames
			 * (0x80 = Page End, 0x81 = NextPage, 0x83/0xE3 = Cancel,
			 *  0x84/0x85 = Duplex sync). Handle BEFORE the 12-byte
			 * data-frame guard so a trailing status byte isn't stuck
			 * in the residue buffer forever.
			 *
			 * We leave the status byte in place (count it as one line)
			 * and break pre-parsing — ProcessMain's GetStatusCode() will
			 * see it on iteration `wDataLineCnt-1` and return SCAN_EOF /
			 * SCAN_MPS / SCAN_CANCEL, which triggers the ScanDecPageEnd
			 * flush path in the caller. */
			if ((signed char)headch < 0) {
				WriteLog("  brscan4: status byte 0x%02x at line %d, stopping pre-parse",
					headch, wDataLineCnt);
				dwRxTempBuffLength--;
				pt++;
				wDataLineCnt++;
				break;
			}

			/* Need at least 12 bytes for a data frame: 10 wrapper + 2 length */
			if (dwRxTempBuffLength < 12) break;

			if (headch == 0) {
				/* White line */
				dwRxTempBuffLength -= 1;
				pt += 1;
				wDataLineCnt++;
				nHeadOnly++;
				continue;
			}
			if (headch != 0x42) {
				/* Unknown header — stop parsing */
				WriteLog("  brscan4 parser: unexpected header 0x%02x at remain=%lu", headch, (unsigned long)dwRxTempBuffLength);
				break;
			}

			/* Skip 10-byte wrapper, read 2-byte LE length */
			WORD clen = (unsigned char)pt[10] | ((unsigned char)pt[11] << 8);
			DWORD lineTotal = 12 + (DWORD)clen;

			if (dwRxTempBuffLength < lineTotal) break;

			if (wDataLineCnt < 3) {
				WriteLog("  brscan4 line[%d]: clen=%u total=%lu next=%02x %02x %02x",
					wDataLineCnt, clen, (unsigned long)lineTotal,
					(unsigned char)pt[lineTotal],
					(unsigned char)pt[lineTotal+1],
					(unsigned char)pt[lineTotal+2]);
			}

			nLength = lineTotal;
			dwRxTempBuffLength -= lineTotal;
			pt += lineTotal;
			wDataLineCnt++;
		}
	} else
#endif
	{
	/* Standard Brother line format: [header][2-byte LE length][data] */
	for (wDataLineCnt=0; wDataLineCnt < nFwTempBuffMaxLine;){
		BYTE headch;

		if( dwRxTempBuffLength <= 0 )	break;

		headch = (BYTE)*pt;
		if ((signed char)headch < 0) {
			// STATUS,CTRL code family
			dwRxTempBuffLength --;
			pt++;
			wDataLineCnt+=3;
		}else{
			if (headch == 0) {
				dwRxTempBuffLength -= 1;
				pt += 1;
				wDataLineCnt++;
				nHeadOnly++;    //05/07/31
			}
			else {
				// Image data
				WORD length;

				if( dwRxTempBuffLength < 3 )
					length = 0;		// clear
				else{
					length = *(WORD *)( pt + 1 );
					nLength = length+3;
				}
				if (wDataLineCnt < 5) {
					WriteLog("  LP2[%d] h=0x%02x len=%u rem=%lu max=%d",
						wDataLineCnt, headch, (unsigned)length,
						(unsigned long)dwRxTempBuffLength, nFwTempBuffMaxLine);
				}
				if( dwRxTempBuffLength < (DWORD)( length + 3) ){	// length+3 = head(1B)+length(2B)+data(length)
					if (wDataLineCnt < 3) WriteLog("  LP2 BREAK: need %u have %lu", (unsigned)(length+3), (unsigned long)dwRxTempBuffLength);
					break;
				}
				else{
					// 1 line data exists
					dwRxTempBuffLength -= length + 3;	// spend length+3 byte for image data
					pt += length + 3;			// refer the next header information
					wDataLineCnt++;
				}
			}
		}
	} // end of for(;;)
	} // end of else (standard parser)
	wData -= dwRxTempBuffLength;	// take off the odd data (less than 1 line data)

	//transfer the data to the RGB data for the specified models
	if( (this->modelInf.seriesNo >=  MUST_CONVERT_MODEL && this->devScanInfo.wColorType == COLOR_FUL) ||
		(this->modelInf.seriesNo >=  MUST_CONVERT_MODEL && this->devScanInfo.wColorType == COLOR_FUL_NOCM) ) {
		//Analize the buffer data (though the second process)
		LPBYTE lpOrg;
		BYTE headch;
		WORD wDataLineCntTemp;

		//05/07/30 Adjust number of lines to multiple number of three
		while(( (wDataLineCnt - nHeadOnly) % 3) != 0){
		  WriteLog( "wDataLineCnt - nHeadOnly = %d", wDataLineCnt - nHeadOnly );
		  wDataLineCnt--;
		  wData -= nLength;
		  dwRxTempBuffLength += nLength;
		}

		wDataLineCntTemp = wDataLineCnt;

		lpOrg = lpRxBuff;

		for (wDataLineCntTemp;  wDataLineCntTemp > 0;){

			headch = (BYTE)*lpOrg;

			//06/02/28 for Duplex
			if(headch == 0x84 || headch == 0x85 || (char)headch == 0 ){
			  lpOrg++;
			  wDataLineCntTemp--;
			}else if((signed char)headch < 0){
			  break;
			}else{
				// Image data
				WORD length,i;
				unsigned char Y, Cb, Cr;
				int R, G, B;
				unsigned char* pData;

				// get the lenght-information of the raster data
				length = ((unsigned char)lpOrg[1] << 8) | (unsigned char)lpOrg[2]; // big-endian for brscan4 models
				length += 3;
				pData = (unsigned char *)lpOrg;

				//rerew the header to RGB header type
				*pData = (*pData & 0xE3) | 0x04;
				*(pData + length) = (*(pData + length) & 0xE3) | 0x08;
				*(pData + length*2) = (*(pData + length*2) & 0xE3) | 0x0C;

				pData +=3;

				//calculate the RGB data from YCbCr data
				for(i=0;i < length-3; i++){
				  Cr = *(pData + i);
				  Y  = *(pData + length + i);
				  Cb = *(pData + length*2 + i);

				  R =  Y + 1.402 * (Cr - 128);
				  G =  Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128);
				  B =  Y + 1.772 * (Cb -128);

				  if(R > 255) R=255;
				  if(R < 0) R=0;
				  if(G > 255) G=255;
				  if(G < 0) G=0;
				  if(G > 255) B=255;
				  if(G < 0) B=0;

				  *(pData + i) = (unsigned char)R;
				  *(pData + length + i) = (unsigned char)G;
				  *(pData + length*2 + i) = (unsigned char)B;

				}

				wDataLineCntTemp -= 3;
				//renew the pointer
				if(wDataLineCntTemp > 2){
				  lpOrg += length*3;
				}
			}
		}
	}//the end of RGB-exchanging process
	// the end of raster data expanding operation
#ifdef NO39_DEBUG
	if (gettimeofday(&start_tv, &tz) == -1)
		return FALSE;
#endif

	nAnswer = ProcessMain( this, wData, wDataLineCnt, lpFwTempBuff+FwTempBuffLength, &FwTempBuffLength, &wProcessSize );

#ifdef NO39_DEBUG
	if (gettimeofday(&tv, &tz) == 0) {
		if (tv.tv_usec < start_tv.tv_usec) {
			tv.tv_usec += 1000 * 1000 ;
			tv.tv_sec-- ;
		}
		nSec = tv.tv_sec - start_tv.tv_sec;
		nUsec = tv.tv_usec - start_tv.tv_usec;

		WriteLog( " PageScan ProcessMain Time %d sec %d Us", nSec, nUsec );

	}

#endif

	if ((dwRxTempBuffLength > 0) || (wProcessSize < wData)) {
		dwRxTempBuffLength += (wData - wProcessSize);
		memmove( lpRxTempBuff, lpRxBuff+wProcessSize, dwRxTempBuffLength );	// Keep the rest data
	}

	/* Fallback EOF detection: if the scanner completed the scan but the
	 * 0x80 Page-End status byte was lost in transit (or for models that
	 * do not send one), force SCAN_EOF once all expected lines arrived.
	 * The proper EOF path is via ProcessMain seeing a 0x80 status byte
	 * in the data stream (see brscan4 pre-parser above). */
	if (nAnswer == SCAN_GOOD && lRealY >= this->scanInfo.ScanAreaSize.lHeight) {
		WriteLog("  brscan4: all %ld lines received, forcing EOF (fallback)", (long)lRealY);
		this->scanState.bReadbufEnd = TRUE;
		nAnswer = SCAN_EOF;
	}

	if ( nAnswer == SCAN_EOF || nAnswer == SCAN_MPS )  {
		// The case of last page
		if( lRealY > 0 ){

			ImgLineProcInfo.pWriteBuff = lpFwTempBuff+FwTempBuffLength;
			ImgLineProcInfo.dwWriteBuffSize = dwImageBuffSize;

			dwWriteImageSize = this->scanDec.lpfnScanDecPageEnd( &ImgLineProcInfo, &nWriteLineCount );
			if( nWriteLineCount > 0 ){
				FwTempBuffLength += dwWriteImageSize;
				lRealY += nWriteLineCount;
			}

#if 1	// DEBUG for MASU
			dwFWImageSize += dwWriteImageSize;
			dwFWImageLine += nWriteLineCount;
			WriteLog( "DEBUG for MASU (PageScan) dwFWImageSize  = %d dwFWImageLine = %d", dwFWImageSize, dwFWImageLine );
			WriteLog( "  PageScan End1 nWriteLineCount = %d", nWriteLineCount );
#endif
		}
		// keep the sitiation  because  the status code is stored to the transmission-keep buffer
		this->scanState.iProcessEnd = nAnswer;
		WriteLog( " PageScan scanState.iProcessEnd = %d, ", this->scanState.iProcessEnd );
	}

	}
	else { // wData == 0
		if (FwTempBuffLength == 0 && dwRxTempBuffLength == 0) {
			nAnswer = SCAN_EOF;
			WriteLog( "<<<<< PageScan [Read Error End]  <<<<<" );
		}
	}
	WriteLog( "ProcessMain End dwRxTempBuffLength = %d", dwRxTempBuffLength );

	} // if ( (!this->scanState.iProcessEnd) || ( FwTempBuffLength > nMaxLen) ) 

	if (this->scanState.iProcessEnd) { // The case the status code is stored in the transmission-keep buffer
		WriteLog( "<<<<< PageScan Status Code Read!!!" );
		nAnswer = this->scanState.iProcessEnd;
	}

	/* Copy the image data stored in the transmission-keep buffer */
	WriteLog( "<<<<< PageScan FwTempBuffLength = %d", FwTempBuffLength );

	if ( FwTempBuffLength > nMaxLen )
		*lpFwLen = nMaxLen;
	else
		*lpFwLen = FwTempBuffLength;

	FwTempBuffLength -= *lpFwLen ;

	memmove( lpFwBuf, lpFwTempBuff, *lpFwLen);	// copy the data from the transmission-keep buffer to transmission buffer
	memmove( lpFwTempBuff, lpFwTempBuff+*lpFwLen, FwTempBuffLength ); // move the rest data to the buffer top

	rc = SANE_STATUS_GOOD;

#ifdef NO39_DEBUG
	gettimeofday(&save_tv, &save_tz);
#endif
	if ( nAnswer == SCAN_EOF || nAnswer == SCAN_MPS )  {

		if (FwTempBuffLength != 0 ) {
			return rc;
		}
		else {
			// clear the rest area to white ,if the size of received data is less than the specified size 
			if( lRealY < this->scanInfo.ScanAreaSize.lHeight ){
				// the case that it becomes page-end status before the received data fills  the area of specified size.
				int nHeightLen = this->scanInfo.ScanAreaSize.lHeight - lRealY;
				int nSize = this->scanInfo.ScanAreaByte.lWidth * nHeightLen; 
				int nMaxSize = nMaxLen - *lpFwLen;
				int nMaxLine;
				int nVal;

				if (this->devScanInfo.wColorType < COLOR_TG)
					nVal = 0x00;
				else
					nVal = 0xFF;

				if ( nSize < nMaxSize ) {
					memset(lpFwBuf+*lpFwLen, nVal, nSize);
					*lpFwLen += nSize;
					lRealY += nHeightLen;
					WriteLog( "PageScan AddSpace End lRealY = %d, nHeightLen = %d nSize = %d nMaxSize = %d *lpFwLen = %d",
					lRealY, nHeightLen, nSize, nMaxSize, *lpFwLen );
				}
				else {
					memset(lpFwBuf+*lpFwLen, nVal, nMaxSize);
					nMaxLine = nMaxSize / this->scanInfo.ScanAreaByte.lWidth;
					*lpFwLen += this->scanInfo.ScanAreaByte.lWidth * nMaxLine;
					lRealY += nMaxLine;
					WriteLog( "PageScan AddSpace lRealY = %d, nHeightLen = %d nSize = %d nMaxSize = %d *lpFwLen = %d",
					lRealY, nHeightLen, nSize, nMaxSize, *lpFwLen );
				}
			}
		}
	}

	switch( nAnswer ){
		case SCAN_CANCEL:
			WriteLog( "Page Canceled" );

			this->scanState.nPageCnt = 0;
			rc = SANE_STATUS_CANCELLED;
			this->scanState.bScanning=FALSE;
			this->scanState.bCanceled=FALSE;
			break;

		case SCAN_EOF:
			WriteLog( "Page End" );
			WriteLog( "  nAnswer = %d lRealY = %d", nAnswer, lRealY );

			if( lRealY != 0 ) {
				// return the SANE_STATUS_GOOD if  the data exists in transmission-keep buffer
				if (*lpFwLen == 0) {
					this->scanState.bEOF=TRUE;
					this->scanState.bScanning=FALSE;
					rc = SANE_STATUS_EOF
;
				}
			}
			else {
				// return the error status code if EOF is received without receiving any data
				rc = SANE_STATUS_IO_ERROR;
			}
			break;
		case SCAN_MPS:
			// return the SANE_STATUS_GOOD if  the data exists in transmission-keep buffer
			if (*lpFwLen == 0) {
				this->scanState.bEOF=TRUE;
				rc = SANE_STATUS_EOF;
			}
			break;
		case SCAN_NODOC:
			rc = SANE_STATUS_NO_DOCS;
			break;
		case SCAN_DOCJAM:
			rc = SANE_STATUS_JAMMED;
			break;
		case SCAN_COVER_OPEN:
			rc = SANE_STATUS_COVER_OPEN;
			break;
		case SCAN_SERVICE_ERR:
			rc = SANE_STATUS_IO_ERROR;
			break;
	}

	//2006/03/03 for sane_read
	if(nAnswer == SCAN_DUPLEX_NORMAL && *lpFwLen == 0){
	  *lpFwLen = 1;
	  rc = SANE_STATUS_DUPLEX_ADVERSE;
	}

	if(nAnswer != SCAN_DUPLEX_NORMAL)
	  nFwLenTotal += *lpFwLen;

	WriteLog( "<<<<< PageScan End <<<<< nFwLenTotal = %d lpFwLen = %d ",nFwLenTotal, *lpFwLen);

	return rc;
}
#elif  BRSANESUFFIX == 1

/********************************************************************************
 *										*
 *	FUNCTION	PageScan						*
 *										*
 *	PURPOSE		���ڡ���ʬ�򥹥���󤹤롣					*
 *										*
 *	����		Brother_Scanner *this	�� Brother_Scanner��¤��		*
 *			char *lpFwBuf		�� �����Хåե�			*
 *			int nMaxLen		�� �����Хåե�Ĺ			*
 *			int *lpFwLen		�� �����ǡ���Ĺ			*
 *										*
 *										*
 *										*
 ********************************************************************************/
int
PageScan( Brother_Scanner *this, char *lpFwBuf, int nMaxLen, int *lpFwLen )
{
	WORD	wData=0;	// �����ǡ����������ʥХ��ȿ���
	WORD	wDataLineCnt=0;	// �����ǡ����Υ饤���
	int	nAnswer=0;
	int	rc;
	LPSTR   lpRxTop;
	WORD	wProcessSize;

	int	nReadSize;
	LPSTR   lpReadBuf;
	int	nMinReadSize; // �Ǿ��꡼�ɥ�����

#ifdef NO39_DEBUG
	struct timeval start_tv, tv;
	struct timezone tz;
	long   nSec, nUsec;
#endif
	if (!this->scanState.bScanning) {
		rc = SANE_STATUS_IO_ERROR;
		return rc;
	}
	if (this->scanState.bCanceled) { //����󥻥����
		WriteLog( "Page Canceled" );

		rc = SANE_STATUS_CANCELLED;
		this->scanState.bScanning=FALSE;
		this->scanState.bCanceled=FALSE;
		this->scanState.nPageCnt = 0;

		return rc;
	}

	nPageScanCnt++;
	WriteLog( ">>> PageScan Start <<< cnt=%d nMaxLen=%d\n", nPageScanCnt, nMaxLen);

#ifdef NO39_DEBUG
	if (gettimeofday(&tv, &tz) == 0) {

	    if (tv.tv_usec < save_tv.tv_usec) {
			tv.tv_usec += 1000 * 1000 ;
			tv.tv_sec-- ;
		}
		nUsec = tv.tv_usec - save_tv.tv_usec;
		nSec = tv.tv_sec - save_tv.tv_sec;

		WriteLog( " No39 nSec = %d Usec = %d\n", nSec, nUsec ) ;
	}
#endif


	WriteLog( "scanInfo.ScanAreaSize.lWidth = [%d]", this->scanInfo.ScanAreaSize.lWidth );
	WriteLog( "scanInfo.ScanAreaSize.lHeight = [%d]", this->scanInfo.ScanAreaSize.lHeight );
	WriteLog( "scanInfo.ScanAreaByte.lWidth = [%d]", this->scanInfo.ScanAreaByte.lWidth );
	WriteLog( "scanInfo.ScanAreaByte.lHeight = [%d]", this->scanInfo.ScanAreaByte.lHeight );

	WriteLog( "devScanInfo.ScanAreaSize.lWidth = [%d]", this->devScanInfo.ScanAreaSize.lWidth );
	WriteLog( "devScanInfo.ScanAreaSize.lHeight = [%d]", this->devScanInfo.ScanAreaSize.lHeight );
	WriteLog( "devScanInfo.ScanAreaByte.lWidth = [%d]", this->devScanInfo.ScanAreaByte.lWidth );
	WriteLog( "devScanInfo.ScanAreaByte.lHeight = [%d]", this->devScanInfo.ScanAreaByte.lHeight );


	memset(lpFwBuf, 0x00, nMaxLen);	//  �����Хåե��򥼥����ꥢ���Ƥ�����
	*lpFwLen = 0;

	if ( (!this->scanState.iProcessEnd) && ( FwTempBuffLength < nMaxLen) ) { 
	// ���ơ����������ɤ�������Ƥ��ʤ����Ǥ��������Хåե����������������¸�Хåե��Υǡ���Ĺ�����������

	// ��¸�ǡ����Хåե��˥ǡ�����¸�ߤ�����ϡ������ǡ����Хåե��˥��ԡ����롣
	memmove( lpRxBuff, lpRxTempBuff, dwRxTempBuffLength );	// ��Ƭ������ǡ�������
	wData += dwRxTempBuffLength;	// ��Ǽ�ǡ���length������

	lpRxTop = lpRxBuff;

	// ������¸�Хåե��˺���3�饤��ʬ��Ÿ���Ǥ���褦�˥�����ʤ���ǡ����ɤ߹��ࡣ
	if (this->devScanInfo.wColorType == COLOR_FUL || this->devScanInfo.wColorType == COLOR_FUL_NOCM )
		nMinReadSize = (this->devScanInfo.ScanAreaByte.lWidth + 3) * 3;
	else
		nMinReadSize = (this->devScanInfo.ScanAreaByte.lWidth + 3);

	nMinReadSize *= 3; // ����3�饤��ʬ�ϥ꡼�ɤ��롣
	if ( !this->scanState.bReadbufEnd ) {
		for (rc=0 ; wData < nMinReadSize;)
		{
			nReadSize = dwRxBuffMaxSize - (dwRxTempBuffLength + wData);
			lpReadBuf = lpRxTop+wData;

			nReadCnt++;
			WriteLog( "Read request size is %d, (dwRxTempBuffLength = %d)", gwInBuffSize - dwRxTempBuffLength, dwRxTempBuffLength );
			WriteLog( "PageScan ReadNonFixedData Cnt = %d", nReadCnt );

			rc = ReadNonFixedData( this->hScanner, lpReadBuf, nReadSize, READ_TIMEOUT , this->modelInf.seriesNo );
			if (rc < 0) {
				this->scanState.bReadbufEnd = TRUE;
				WriteLog( "  bReadbufEnd =TRUE" );
				break;
			}
			else if (rc > 0){
				wData += rc;

				if (StatusChk(lpRxBuff, wData)) { // ���ơ����������ɤ���������������å����롣
					this->scanState.bReadbufEnd = TRUE;
					WriteLog( "bReadbufEnd =TRUE" );
					break;
				}
			}
		}
	}

	WriteLog( "Read data size is %d, (dwRxTempBuffLength = %d)", wData, dwRxTempBuffLength );

	WriteLog( "Adjusted wData = %d, (dwRxTempBuffLength = %d)", wData, dwRxTempBuffLength );

	if (wData != 0)
	// �ǡ�����饤��ñ�̤ޤǤ˶��ڤ�
	{
	LPSTR  pt = lpRxBuff;
	int nFwTempBuffMaxLine;
	int nResoLine;

	// �������륤�᡼���ǡ�������(�ɥå�)
	if (this->devScanInfo.wColorType == COLOR_FUL || this->devScanInfo.wColorType == COLOR_FUL_NOCM ) {
		nFwTempBuffMaxLine = (dwFwTempBuffMaxSize / 2 - FwTempBuffLength) / this->scanInfo.ScanAreaByte.lWidth;
		nFwTempBuffMaxLine *= 3;
	}
	else {
		nFwTempBuffMaxLine = (dwFwTempBuffMaxSize / 2 - FwTempBuffLength) / this->scanInfo.ScanAreaByte.lWidth;
	}
	nResoLine= this->scanInfo.UserSelect.wResoY / this->devScanInfo.DeviceScan.wResoY;
	if (nResoLine > 1)
		nFwTempBuffMaxLine /= nResoLine;

	dwRxTempBuffLength = wData;
	for (wDataLineCnt=0; wDataLineCnt < nFwTempBuffMaxLine;){
		BYTE headch;

		if( dwRxTempBuffLength <= 0 )	break;	// ���ƤΥǡ����Ͻ�����ǽ(���ڤ��ɤ��������줿)

		headch = (BYTE)*pt;
		if ((signed char)headch < 0) {
			// STATUS,CTRL�ϥ�����
			dwRxTempBuffLength --;			// CTRL�ϥ����ɤ�1byte����
			pt++;					// ����header����򻲾�

			wDataLineCnt+=3;
		}else{
			if (headch == 0) {
				dwRxTempBuffLength -= 1;
				pt += 1;
				wDataLineCnt++;
			}
			else {
				// �����ǡ���
				WORD length;

				if( dwRxTempBuffLength < 3 )
					length = 0;		// �����
				else
					// �饹���ǡ���Ĺ�μ���
					length = ((unsigned char)pt[1] << 8) | (unsigned char)pt[2]; // big-endian for brscan4 models

				if (wDataLineCnt < 3) {
					WriteLog("  LineParser[%d]: head=0x%02x len=%u remain=%lu pt=%02x %02x %02x %02x %02x %02x",
						wDataLineCnt, headch, length, (unsigned long)dwRxTempBuffLength,
						(unsigned char)pt[0], (unsigned char)pt[1], (unsigned char)pt[2],
						(unsigned char)pt[3], (unsigned char)pt[4], (unsigned char)pt[5]);
				}

				if( dwRxTempBuffLength < (DWORD)( length + 3) ){	// length+3 = head(1B)+length(2B)+data(length)
					if (wDataLineCnt == 0) {
						WriteLog("  LineParser BREAK: need %u have %lu",
							(unsigned)(length+3), (unsigned long)dwRxTempBuffLength);
					}
					break;
				}
				else{
					// 1lineʬ�Υǡ�������
					dwRxTempBuffLength -= length + 3;	// �����ǡ����� length+3 byte����
					pt += length + 3;			// ����header����򻲾�
					wDataLineCnt++;
				}
			}
		}
	} // end of for(;;)
	wData -= dwRxTempBuffLength;	// Ÿ�������˲󤹥ǡ������飱�饤��̤���Υǡ��������

	// �饹���ǡ�����Ÿ������
#ifdef NO39_DEBUG
	if (gettimeofday(&start_tv, &tz) == -1)
		return FALSE;
#endif

	nAnswer = ProcessMain( this, wData, wDataLineCnt, lpFwTempBuff+FwTempBuffLength, &FwTempBuffLength, &wProcessSize );

#ifdef NO39_DEBUG
	if (gettimeofday(&tv, &tz) == 0) {
		if (tv.tv_usec < start_tv.tv_usec) {
			tv.tv_usec += 1000 * 1000 ;
			tv.tv_sec-- ;
		}
		nSec = tv.tv_sec - start_tv.tv_sec;
		nUsec = tv.tv_usec - start_tv.tv_usec;

		WriteLog( " PageScan ProcessMain Time %d sec %d Us", nSec, nUsec );

	}

#endif


	if ((dwRxTempBuffLength > 0) || (wProcessSize < wData)) {
		dwRxTempBuffLength += (wData - wProcessSize);
		memmove( lpRxTempBuff, lpRxBuff+wProcessSize, dwRxTempBuffLength );	// �Ĥ�ǡ�������¸
	}

	if ( nAnswer == SCAN_EOF || nAnswer == SCAN_MPS )  {
		// �Ǹ�Υڡ����ǡ����ξ��
		if( lRealY > 0 ){

			ImgLineProcInfo.pWriteBuff = lpFwTempBuff+FwTempBuffLength;
			ImgLineProcInfo.dwWriteBuffSize = dwImageBuffSize;

			dwWriteImageSize = this->scanDec.lpfnScanDecPageEnd( &ImgLineProcInfo, &nWriteLineCount );
			if( nWriteLineCount > 0 ){
				FwTempBuffLength += dwWriteImageSize;
				lRealY += nWriteLineCount;
			}

#if 1	// DEBUG for MASU
			dwFWImageSize += dwWriteImageSize;
			dwFWImageLine += nWriteLineCount;
			WriteLog( "DEBUG for MASU (PageScan) dwFWImageSize  = %d dwFWImageLine = %d", dwFWImageSize, dwFWImageLine );
			WriteLog( "  PageScan End1 nWriteLineCount = %d", nWriteLineCount );
#endif
		}
		// ���ơ����������ɤ�������¸�Хåե������ä����ᡢ���֤�Ф��Ƥ���
		this->scanState.iProcessEnd = nAnswer;
		WriteLog( " PageScan scanState.iProcessEnd = %d, ", this->scanState.iProcessEnd );
	}

	}
	else { // wData == 0
		if (FwTempBuffLength == 0 && dwRxTempBuffLength == 0) {
			nAnswer = SCAN_EOF;
			WriteLog( "<<<<< PageScan [Read Error End]  <<<<<" );
		}
	}
	WriteLog( "ProcessMain End dwRxTempBuffLength = %d", dwRxTempBuffLength );

	} // if ( (!this->scanState.iProcessEnd) || ( FwTempBuffLength > nMaxLen) ) 

	if (this->scanState.iProcessEnd) { // ������¸�Хåե��˥��ơ����������ɤ�������Ƥ�����
		WriteLog( "<<<<< PageScan Status Code Read!!!" );
		nAnswer = this->scanState.iProcessEnd;
	}

	/* �����Хåե���������¸�Хåե��ˤ��륤�᡼���ǡ����򥳥ԡ����롣*/
	WriteLog( "<<<<< PageScan FwTempBuffLength = %d", FwTempBuffLength );

	if ( FwTempBuffLength > nMaxLen )
		*lpFwLen = nMaxLen;
	else
		*lpFwLen = FwTempBuffLength;

	FwTempBuffLength -= *lpFwLen ;

	memmove( lpFwBuf, lpFwTempBuff, *lpFwLen);	// ������¸�Хåե����������Хåե��إ��ԡ����롣
	memmove( lpFwTempBuff, lpFwTempBuff+*lpFwLen, FwTempBuffLength ); // �Ĥ����¸�ǡ�������Ƭ�˰�ư���롣	

	rc = SANE_STATUS_GOOD;

#ifdef NO39_DEBUG
	gettimeofday(&save_tv, &save_tz);
#endif
	if ( nAnswer == SCAN_EOF || nAnswer == SCAN_MPS )  {

		if (FwTempBuffLength != 0 ) {
			return rc;
		}
		else {
			// ���ꤷ���ǡ���Ĺ�����������ǡ���Ĺ�����ʤ���硢�Ĥ�Υǡ���Ĺ�����Ȥ��ƥ��åȤ��롣		
			if( lRealY < this->scanInfo.ScanAreaSize.lHeight ){
				// ���ꤷ��Ĺ����꾯�ʤ��ͤξ��֤ǡ��ڡ�������ɥ��ơ������Ȥʤä����
				int nHeightLen = this->scanInfo.ScanAreaSize.lHeight - lRealY;
				int nSize = this->scanInfo.ScanAreaByte.lWidth * nHeightLen; 
				int nMaxSize = nMaxLen - *lpFwLen;
				int nMaxLine;
				int nVal;

				if (this->devScanInfo.wColorType < COLOR_TG)
					nVal = 0x00;
				else
					nVal = 0xFF;

				if ( nSize < nMaxSize ) {
					memset(lpFwBuf+*lpFwLen, nVal, nSize);
					*lpFwLen += nSize;
					lRealY += nHeightLen;
					WriteLog( "PageScan AddSpace End lRealY = %d, nHeightLen = %d nSize = %d nMaxSize = %d *lpFwLen = %d",
					lRealY, nHeightLen, nSize, nMaxSize, *lpFwLen );
				}
				else {
					memset(lpFwBuf+*lpFwLen, nVal, nMaxSize);
					nMaxLine = nMaxSize / this->scanInfo.ScanAreaByte.lWidth;
					*lpFwLen += this->scanInfo.ScanAreaByte.lWidth * nMaxLine;
					lRealY += nMaxLine;
					WriteLog( "PageScan AddSpace lRealY = %d, nHeightLen = %d nSize = %d nMaxSize = %d *lpFwLen = %d",
					lRealY, nHeightLen, nSize, nMaxSize, *lpFwLen );
				}
			}
		}
	}

	switch( nAnswer ){
		case SCAN_CANCEL:
			WriteLog( "Page Canceled" );

			this->scanState.nPageCnt = 0;
			rc = SANE_STATUS_CANCELLED;
			this->scanState.bScanning=FALSE;
			this->scanState.bCanceled=FALSE;
			break;

		case SCAN_EOF:
			WriteLog( "Page End" );
			WriteLog( "  nAnswer = %d lRealY = %d", nAnswer, lRealY );

			if( lRealY != 0 ) {
				// ������¸�Хåե��˥ǡ���������֤ϡ�SANE_STATUS_GOOD���֤���
				if (*lpFwLen == 0) {
					this->scanState.bEOF=TRUE;
					this->scanState.bScanning=FALSE;
					rc = SANE_STATUS_EOF;
				}
			}
			else {
				// �ǡ�������̵����EOF�ξ�硢���顼�Ȥ��롣
				rc = SANE_STATUS_IO_ERROR;
			}
			break;
		case SCAN_MPS:
			// ������¸�Хåե��˥ǡ���������֤ϡ�SANE_STATUS_GOOD���֤���
			if (*lpFwLen == 0) {
				this->scanState.bEOF=TRUE;
				rc = SANE_STATUS_EOF;
			}
			break;
		case SCAN_NODOC:
			rc = SANE_STATUS_NO_DOCS;
			break;
		case SCAN_DOCJAM:
			rc = SANE_STATUS_JAMMED;
			break;
		case SCAN_COVER_OPEN:
			rc = SANE_STATUS_COVER_OPEN;
			break;
		case SCAN_SERVICE_ERR:
			rc = SANE_STATUS_IO_ERROR;
			break;
	}

	nFwLenTotal += *lpFwLen;
	WriteLog( "<<<<< PageScan End <<<<< nFwLenTotal = %d lpFwLen = %d ",nFwLenTotal, *lpFwLen);

	return rc;
}

#else    //BRSANESUFFIX
  force causing compile error
#endif   //BRSANESUFFIX



void
ReadTrash( Brother_Scanner *this )
{
	// discard the received data
	//    regard NO-DATA if ZERO datas are recieved continuously for  more than 1 sec

	CHAR *lpBrBuff;
	int nResultSize;
	int nEndPoint,i;

	struct timeval start_tv, tv;
	struct timezone tz;
	long   nSec, nUsec;
	long   nTimeOutSec, nTimeOutUsec;

	//2005/11/11 Add SeriesNnumber information for L4CFB and later
	nEndPoint = 0x84;

	for(i=0; i < ANOTHERENDPOINT; i++){
	  if( this->modelInf.seriesNo == ChangeEndpoint[i]){
	    nEndPoint = 0x85;
	    break;
	  }
	}

	//if(this->modelInf.seriesNo == L4CFB)
	//  nEndPoint=0x85;
	//else
	//  nEndPoint=0x84;

	if (gettimeofday(&start_tv, &tz) == -1)
		return ;

	lpBrBuff = MALLOC( 32000 );
	if (!lpBrBuff)
		return;

	if (gettimeofday(&start_tv, &tz) == -1) {
		FREE(lpBrBuff);
		return ;
	}
	// calculate the second-order of the timeout value
	nTimeOutSec = 1;
	// calculate the micro-second-order of the timeout value
	nTimeOutUsec = 0;

	nResultSize = 1;
	while (1) {
		if (gettimeofday(&tv, &tz) == 0) {
			if (tv.tv_usec < start_tv.tv_usec) {
				tv.tv_usec += 1000 * 1000 ;
				tv.tv_sec-- ;
			}
			nSec = tv.tv_sec - start_tv.tv_sec;
			nUsec = tv.tv_usec - start_tv.tv_usec;

			WriteLog( "AbortPageScan Read nSec = %d Usec = %d\n", nSec, nUsec ) ;

			if (nSec > nTimeOutSec) { // break if timeout occures
				break;
			}
			else if( nSec == nTimeOutSec) { // the case the read time  is the same as the timeout value in second-order
				if (nUsec >= nTimeOutUsec) { // break if read time is larger than timeout value 
					break;
				}
			}
		}
		else {
			break;
		}

		usleep(30 * 1000); // wait for 30ms

		// discard the data
		if (IFTYPE_NET != this->hScanner->device){
		  nResultSize = usb_bulk_read(this->hScanner->usb,
					      nEndPoint,
					      lpBrBuff,
					      32000,
					      2000
					      );
		}
		else{
		  struct timeval net_timeout = NETTIMEOUTST;
		  nResultSize = 0;
		  read_device_net(this->hScanner->net,
				  lpBrBuff,
				  32000,
				  &nResultSize,
				  &net_timeout);

		}

		WriteLog( "AbortPageScan Read end nResultSize = %d %d", nResultSize,nEndPoint) ;

		// reset the timer until the data can be read.
		if (nResultSize > 0) { // the case data exists.

			if (gettimeofday(&start_tv, &tz) == -1)
				break;
		}
	}
	FREE(lpBrBuff);

}
//-----------------------------------------------------------------------------
//
//	Function name:	AbortPageScan
//
//
//	Abstract:
//		Abort the Page-scan process
//
//
//	Parameters:
//
//	Return values:
//
//-----------------------------------------------------------------------------
//	AbortPageScan��The part of PageScan i the windows version��
void
AbortPageScan( Brother_Scanner *this )
{
	WriteLog( ">>>>> AbortPageScan Start >>>>>" );

	//
	// Send the cancel command
	//
	SendCancelCommand(this->hScanner, this->modelInf.seriesNo);
	this->scanState.bCanceled=TRUE;
	ReadTrash( this );

	WriteLog( "<<<<< AbortPageScan End <<<<<" );

	return;
}

/************************************************************************
 *									*
 *	FUNCTION	ScanEnd						*
 *									*
 *	PURPOSE		Process of termination of the scan session	*
 *				request the source-closeing process	*
 *									*
 *	IN		HWND	hdlg	handle to the dialog		*
 *									*
 *	OUT		None						*
 *									*
 ************************************************************************/
void
ScanEnd( Brother_Scanner *this )
{
    this->scanState.nPageCnt = 0;
    bTxScanCmd = FALSE;		// Clear the begining-of-scan flag

    /* Release color-scan temp file / libjpeg state if a color scan was
     * in progress. Safe no-op if not. */
    brother_color_cleanup();

#ifndef DEBUG_No39
    if ( this->hScanner != NULL ) {

	if (IFTYPE_USB == this->hScanner->device){
	    if (this->hScanner->usb){
		/* Reference ScanEnd (libsane-brother4.so @0x15578) sequence:
		 *   CloseDevice() → internally does control-msg + release_interface
		 *   usb_close()
		 * No usb_set_altinterface(0) — that extra SET_INTERFACE was
		 * causing BCOMMAND_RETURN=0x80 to stick across sessions. */
		CloseDevice(this->hScanner);
		usb_close(this->hScanner->usb);
		this->hScanner->usb = NULL;
	    }
	} else {
	    if (this->hScanner->net){
		CloseDevice(this->hScanner);
		this->hScanner->net = NULL;
	    }
	}
    }
#endif
    if( hGray && ( hGray != this->cmatch.hGrayTbl ) ){
	FREE( hGray );
	hGray = NULL;
	WriteLog( "free hGray" );
    }

    FreeReceiveBuffer();
    if (lpRxTempBuff) {
	FREE(lpRxTempBuff);
	lpRxTempBuff = NULL;
    }
    if (lpFwTempBuff) {
	FREE(lpFwTempBuff);
	lpFwTempBuff = NULL;
    }
    //
    // end of the decode/stretch process
    //
    this->scanDec.lpfnScanDecClose();

    WriteLog( "<<<<< Terminate Scanning <<<<<" );
}


//-----------------------------------------------------------------------------
//
//	Function name:	GetScanAreaParam
//
//
//	Abstract:
//		Acquire the scan-area informations for the scan parameters
//		     from the scan-area information (0.1mm-order)   ??
//
//
//	Parameters:
//		None
//
//
//	Return values:
//		None
//
//-----------------------------------------------------------------------------
//	GetScanAreaParam(GetScanDot of the windows version��
void
GetScanAreaParam( Brother_Scanner *this )
{
	LPAREARECT  lpScanAreaDot;
	LONG    lUserResoX,   lUserResoY;
	LONG    lDeviceResoX, lDeviceResoY;


	//
	// read the information from UI setting and write the information to the scanInfo structure  
	//
	this->scanInfo.ScanAreaMm = this->uiSetting.ScanAreaMm;

	//
	// exchange the scan-area-coordinates form  0.1mm unit to pixel unit.
	//
	lUserResoX   = this->scanInfo.UserSelect.wResoX;
	lUserResoY   = this->scanInfo.UserSelect.wResoY;
	lDeviceResoX = this->devScanInfo.DeviceScan.wResoX;
	lDeviceResoY = this->devScanInfo.DeviceScan.wResoY;

	// user-specified scan-area-coordinates (dot unit)
	lpScanAreaDot = &this->scanInfo.ScanAreaDot;
	lpScanAreaDot->left   = this->scanInfo.ScanAreaMm.left   * lUserResoX / 254L;
	lpScanAreaDot->right  = this->scanInfo.ScanAreaMm.right  * lUserResoX / 254L;
	lpScanAreaDot->top    = this->scanInfo.ScanAreaMm.top    * lUserResoY / 254L;
	lpScanAreaDot->bottom = this->scanInfo.ScanAreaMm.bottom * lUserResoY / 254L;

	// The scan-area-coordinates for scan-parameters (dot unit)
	lpScanAreaDot = &this->devScanInfo.ScanAreaDot;
	lpScanAreaDot->left   = this->scanInfo.ScanAreaMm.left   * lDeviceResoX / 254L;
	lpScanAreaDot->right  = this->scanInfo.ScanAreaMm.right  * lDeviceResoX / 254L;
	lpScanAreaDot->top    = this->scanInfo.ScanAreaMm.top    * lDeviceResoY / 254L;
	lpScanAreaDot->bottom = this->scanInfo.ScanAreaMm.bottom * lDeviceResoY / 254L;

	//
	// Calculate the scan-area-coordinates for scan-parameters
	//
	GetDeviceScanArea( this, lpScanAreaDot );

	//
	// Calculate the scan size (dot unit)
	//
	this->devScanInfo.ScanAreaSize.lWidth  = lpScanAreaDot->right  - lpScanAreaDot->left;
	this->devScanInfo.ScanAreaSize.lHeight = lpScanAreaDot->bottom - lpScanAreaDot->top;

	//
	// Calculate the size of scan-area (byte unit)
	//
	if( this->devScanInfo.wColorType == COLOR_BW || this->devScanInfo.wColorType == COLOR_ED ){
		this->devScanInfo.ScanAreaByte.lWidth = ( this->devScanInfo.ScanAreaSize.lWidth + 7 ) / 8;
	}else{
		this->devScanInfo.ScanAreaByte.lWidth = this->devScanInfo.ScanAreaSize.lWidth;
	}
	this->devScanInfo.ScanAreaByte.lHeight = this->devScanInfo.ScanAreaSize.lHeight;

	WriteLog(" brother_scanner.c GetScanAreaParam LOG START !!");

	WriteLog( "scanInfo.ScanAreaMm.left = [%d]", this->scanInfo.ScanAreaMm.left );
	WriteLog( "scanInfo.ScanAreaMm.right = [%d]", this->scanInfo.ScanAreaMm.right );
	WriteLog( "scanInfo.ScanAreaMm.top = [%d]", this->scanInfo.ScanAreaMm.top );
	WriteLog( "scanInfo.ScanAreaMm.bottom = [%d]", this->scanInfo.ScanAreaMm.bottom );

	WriteLog( "lUserResoX = [%d]", lUserResoX );
	WriteLog( "lUserResoY = [%d]", lUserResoY );

	WriteLog( "scanInfo.ScanAreaSize.lWidth = [%d]", this->scanInfo.ScanAreaSize.lWidth );
	WriteLog( "scanInfo.ScanAreaSize.lHeight = [%d]", this->scanInfo.ScanAreaSize.lHeight );
	WriteLog( "scanInfo.ScanAreaByte.lWidth = [%d]", this->scanInfo.ScanAreaByte.lWidth );
	WriteLog( "scanInfo.ScanAreaByte.lHeight = [%d]", this->scanInfo.ScanAreaByte.lHeight );

	WriteLog( "lDeviceResoX = [%d]", lDeviceResoX );
	WriteLog( "lDeviceResoY = [%d]", lDeviceResoY );

	WriteLog( "devScanInfo.ScanAreaSize.lWidth = [%d]", this->devScanInfo.ScanAreaSize.lWidth );
	WriteLog( "devScanInfo.ScanAreaSize.lHeight = [%d]", this->devScanInfo.ScanAreaSize.lHeight );
	WriteLog( "devScanInfo.ScanAreaByte.lWidth = [%d]", this->devScanInfo.ScanAreaByte.lWidth );
	WriteLog( "devScanInfo.ScanAreaByte.lHeight = [%d]", this->devScanInfo.ScanAreaByte.lHeight );

	WriteLog(" brother_scanner.c GetScanAreaParam LOG END !!");
}


//-----------------------------------------------------------------------------
//
//	Function name:	StartDecodeStretchProc
//
//
//	Abstract:
//		Initialize the scanned-data-expanding-modules/resolution-exchange-module
//
//
//	Parameters:
//		None
//
//
//	Return values:
//		TRUE  = terminate successfully
//		FALSE = initializing was failed
//
//-----------------------------------------------------------------------------
//
BOOL
StartDecodeStretchProc( Brother_Scanner *this )
{
	BOOL  bResult = TRUE;

	//
	// Set the raster information to the ImageProcInfo structure
	//
	ImageProcInfo.nInResoX  = this->devScanInfo.DeviceScan.wResoX;
	ImageProcInfo.nInResoY  = this->devScanInfo.DeviceScan.wResoY;
	ImageProcInfo.nOutResoX = this->scanInfo.UserSelect.wResoX;
	ImageProcInfo.nOutResoY = this->scanInfo.UserSelect.wResoY;
	WriteLog("nInResoX=%d nInResoY=%d nOutResoX=%d nOutResoY=%d",ImageProcInfo.nInResoX,ImageProcInfo.nInResoY,ImageProcInfo.nOutResoX,ImageProcInfo.nOutResoY);
	ImageProcInfo.dwInLinePixCnt = this->devScanInfo.ScanAreaSize.lWidth;

	ImageProcInfo.nOutDataKind = SCODK_PIXEL_RGB;

#if 1 // modify for the bug that scanning is failed  at Black^White mode
	ImageProcInfo.bLongBoundary = FALSE;	// never do 4 byte alignment
#else
	ImageProcInfo.bLongBoundary = TRUE;
#endif
	//
	// set color type
	//
	switch( this->devScanInfo.wColorType ){
		case COLOR_BW:			// Black & White
			ImageProcInfo.nColorType = SCCLR_TYPE_BW;
			break;

		case COLOR_ED:			// Error Diffusion Gray
			ImageProcInfo.nColorType = SCCLR_TYPE_ED;
			break;

		case COLOR_DTH:			// Dithered Gray
			ImageProcInfo.nColorType = SCCLR_TYPE_DTH;
			break;

		case COLOR_TG:			// True Gray
			ImageProcInfo.nColorType = SCCLR_TYPE_TG;
			break;

		case COLOR_256:			// 256 Color
			ImageProcInfo.nColorType = SCCLR_TYPE_256;
			break;

		case COLOR_FUL:			// 24bit Full Color
			ImageProcInfo.nColorType = SCCLR_TYPE_FUL;
			break;

		case COLOR_FUL_NOCM:	// 24bit Full Color(do not colormatch)
			ImageProcInfo.nColorType = SCCLR_TYPE_FULNOCM;
			break;
	}

	//
	// Initialize the scanned-data-expanding-modules/resolution-exchange-module
	//
	if (this->scanDec.lpfnScanDecOpen) {
		bResult = this->scanDec.lpfnScanDecOpen( &ImageProcInfo );
		WriteLog( "Result from ScanDecOpen is %d", bResult );
	}

	dwImageBuffSize = ImageProcInfo.dwOutWriteMaxSize;
	WriteLog( "ScanDec Func needs maximum size is %d", dwImageBuffSize );

	//
	//Set the expaned data width/ resolution-exchanged data width
	//
	this->scanInfo.ScanAreaSize.lWidth = ImageProcInfo.dwOutLinePixCnt;
	this->scanInfo.ScanAreaByte.lWidth = ImageProcInfo.dwOutLineByte;

	this->scanInfo.ScanAreaSize.lHeight = this->devScanInfo.ScanAreaSize.lHeight;

	if( this->devScanInfo.DeviceScan.wResoY != this->scanInfo.UserSelect.wResoY ){
		//
		// revise the scaled/magnified scan-length
		//
		this->scanInfo.ScanAreaSize.lHeight *= this->scanInfo.UserSelect.wResoY;
		this->scanInfo.ScanAreaSize.lHeight /= this->devScanInfo.DeviceScan.wResoY;
	}
	this->scanInfo.ScanAreaByte.lHeight = this->scanInfo.ScanAreaSize.lHeight;

	return bResult;
}


//-----------------------------------------------------------------------------
//
//	Function name:	GetDeviceScanArea
//
//
//	Abstract:
//		Acquire the scan-area informations for the scan parameters
//
//
//	Parameters:
//		lpScanAreaDot
//			pointer to the scan-area coordinates
//
//
//	Return values:
//		None
//
//-----------------------------------------------------------------------------
//	GetDeviceScanArea��Parts of GetScanDot in  the Windows version��
void
GetDeviceScanArea( Brother_Scanner *this, LPAREARECT lpScanAreaDot )
{
	LONG    lMaxScanPixels;
	LONG    lMaxScanRaster;
	LONG    lTempWidth;


	lMaxScanPixels = this->devScanInfo.dwMaxScanPixels;

	if( this->devScanInfo.wScanSource == MFCSCANSRC_FB ){
		//
		// When the scan-source is FB
		//    restrict to the max claster number (it is acquired from the device) 
		//
		lMaxScanRaster = this->devScanInfo.dwMaxScanRaster;
	}else{
		//
		// When the scan-source is ADF restrict to 14 inch.
		//
		lMaxScanRaster = 14 * this->devScanInfo.DeviceScan.wResoY;
	}
	//
	// revise the right position of the scan-area
	//
	if( lpScanAreaDot->right > lMaxScanPixels ){
		lpScanAreaDot->right = lMaxScanPixels;
	}
	lTempWidth = lpScanAreaDot->right - lpScanAreaDot->left;
	if( lTempWidth < 16 ){
	    //
	    // The case the scaned width is less than 16 dots
	    //
	    lpScanAreaDot->right = lpScanAreaDot->left + 16;
	    if( lpScanAreaDot->right > lMaxScanPixels ){
			//
			// if the specified right position of the scan area is over the scannable limit 
			//       calculate the left position of the scan-area based on the right position
			//
			lpScanAreaDot->right = lMaxScanPixels;
			lpScanAreaDot->left  = lMaxScanPixels - 16;
		}
	}
	//
	// revise the bottom position of the scan-area
	//
	if( lpScanAreaDot->bottom > lMaxScanRaster ){
		lpScanAreaDot->bottom = lMaxScanRaster;
	}

	//
	// if the normal-scan is selected round on the 16dot unit
	//   0-7 -> 0, 8-15 -> 16
	//
	lpScanAreaDot->left  = ( lpScanAreaDot->left  + 0x8 ) & 0xFFF0;
	lpScanAreaDot->right = ( lpScanAreaDot->right + 0x8 ) & 0xFFF0;

}


/*************************************************************************
 *									 *
 *	FUNCTION	ProcessMain					 *
 *									 *
 *	IN		pointer to the Brother_Scanner	structure	 *
 *			WORD		number of data			 *
 *			char *		output buffer			 *
 *			int *		pointer to the output data number*
 *									 *
 *	OUT		None						 *
 *									 *
 *	COMMENT		process the data acquired from the scanner	 *
 *			Regard the stored data as the line unit data	 *
 *									 *
 *************************************************************************/
int
ProcessMain(Brother_Scanner *this, WORD wByte, WORD wDataLineCnt, char * lpFwBuf, int *lpFwBufcnt, WORD *lpProcessSize)
{
	LPSTR	lpScn = lpRxBuff;
	LPSTR	lpScnEnd;
	int	answer = SCAN_GOOD;
	char	Header;
	DWORD	Dcount;
	LONG	count;
	LPSTR	lpSrc;
	WORD	wLineCnt;
#ifdef NO39_DEBUG
	struct timeval start_tv, tv;
	struct timezone tz;
	long   nSec, nUsec;
#endif

	WriteLog( ">>>>> ProcessMain Start >>>>> wDataLineCnt=%d", wDataLineCnt);

	lpScn = lpRxBuff;

	lpScnEnd = lpScn + wByte;
	WriteLog( "lpFwBuf = %X, lpScn = %X, wByte = %d, lpScnEnd = %X", lpFwBuf, lpScn, wByte, lpScnEnd );

	if (this->devScanInfo.wColorType == COLOR_FUL || this->devScanInfo.wColorType == COLOR_FUL_NOCM) {
		if (wDataLineCnt)
		wDataLineCnt = (wDataLineCnt / 3 * 3);
	}
	for( wLineCnt=0; wLineCnt < wDataLineCnt; wLineCnt++ ){
		//
		//	Header <- line header (byte)
		//
		Header = *lpScn++;
		//06/02/28
		if((BYTE)Header == 0x84){
		  WriteLog( "Header = 84" );
		  wLineCnt += 2;
		  answer = SCAN_DUPLEX_NORMAL;
		}
		else if( (BYTE)Header == 0x85){
		  WriteLog( "Header = 85" );
		  wLineCnt += 2;
		  answer = SCAN_DUPLEX_REVERSE;
		}
		else if( (signed char)Header < 0 ){
			//
			//	Status code
			//
			WriteLog( "Header=%2x  ", (BYTE)Header );

			answer = GetStatusCode( Header );
			break;	//break for
		}else{
			//
			//	Scanned Data
			//
			if( Header == 0 ){
				//
				//	White line
				//
				WriteLog( "Header=%2x  while line", (BYTE)Header );
				WriteLog( "\tlpFwBufp = %X, lpScn = %X", lpFwBuf, lpScn );

				if( lpFwBuf ){
					lRealY++;
					lpFwBuf += this->scanInfo.ScanAreaByte.lWidth;
				}
#if BRSANESUFFIX == 2
			}else if( Header == 0x42 && this->modelInf.seriesNo >= MUST_CONVERT_MODEL ){
				/*
				 * brscan4 line format: [42 07 00 XX XX XX XX XX XX XX][NN][00][data]
				 * Header 0x42 already consumed. lpScn points to byte 1 (0x07).
				 * Skip remaining 9 wrapper bytes, read 1-byte length, skip padding.
				 */
				lpScn += 9;  /* skip to length field (bytes 10-11 of original frame) */
				count = (WORD)((unsigned char)lpScn[0] | ((unsigned char)lpScn[1] << 8)); /* 2-byte LE length */
				lpScn += 2;
				lpSrc = lpScn;
				Dcount = count;

				WriteLog( "Header=42(brscan4) Count=%4d", count );

				if( lpFwBuf ){
					SetupImgLineProc( 0x42 );  /* MONO PACKBITS */
					ImgLineProcInfo.pLineData      = lpSrc;
					ImgLineProcInfo.dwLineDataSize = count;
					ImgLineProcInfo.pWriteBuff     = lpFwBuf;
					/* Clear output line to white before decompression,
					 * so any unfilled pixels are white, not stale data */
					memset(lpFwBuf, 0xFF, this->scanInfo.ScanAreaByte.lWidth);

					dwWriteImageSize = this->scanDec.lpfnScanDecWrite( &ImgLineProcInfo, &nWriteLineCount );
					WriteLog( "\tlpFwBuf = %X, WriteSize = %d, LineCount = %d, RealY = %d", lpFwBuf, dwWriteImageSize, nWriteLineCount, lRealY );

					if( nWriteLineCount > 0 ){
						*lpFwBufcnt += dwWriteImageSize;
						lpFwBuf += dwWriteImageSize;
						lRealY += nWriteLineCount;
					}
				}
				lpScn += Dcount;
#endif
			}else{
				//
				//	Scanner data (standard format)
				//
				Dcount = (DWORD)*( (WORD *)lpScn );	// data length (LE)
				count  = (WORD)Dcount;
				lpScn += 2;
				lpSrc = lpScn;

				WriteLog( "Header=%2x  Count=%4d", (BYTE)Header, count );

				if( lpFwBuf ){
					SetupImgLineProc( Header );
					ImgLineProcInfo.pLineData      = lpSrc;
					ImgLineProcInfo.dwLineDataSize = count;
					ImgLineProcInfo.pWriteBuff     = lpFwBuf;
					//
					// raster data expansion/ resolution exchanging
					//
#ifdef NO39_DEBUG
	if (gettimeofday(&start_tv, &tz) == -1)
		return FALSE;
#endif

					dwWriteImageSize = this->scanDec.lpfnScanDecWrite( &ImgLineProcInfo, &nWriteLineCount );
					WriteLog( "\tlpFwBuf = %X, WriteSize = %d, LineCount = %d, RealY = %d", lpFwBuf, dwWriteImageSize, nWriteLineCount, lRealY );

#ifdef NO39_DEBUG
	if (gettimeofday(&tv, &tz) == 0) {
		if (tv.tv_usec < start_tv.tv_usec) {
			tv.tv_usec += 1000 * 1000 ;
			tv.tv_sec-- ;
		}
		nSec = tv.tv_sec - start_tv.tv_sec;
		nUsec = tv.tv_usec - start_tv.tv_usec;

		WriteLog( " ProcessMain ScanDecWrite Time %d sec %d Us", nSec, nUsec );

	}
#endif

					if( nWriteLineCount > 0 ){
						*lpFwBufcnt += dwWriteImageSize;
#if 1	// DEBUG for MASU
						dwFWImageSize += dwWriteImageSize;
						dwFWImageLine += nWriteLineCount;
						WriteLog( "DEBUG for MASU (ProcessMain) dwFWImageSize  = %d dwFWImageLine = %d", dwFWImageSize, dwFWImageLine );
#endif

						if( this->mfcModelInfo.bColorModel && ! this->modelConfig.bNoUseColorMatch && this->devScanInfo.wColorType == COLOR_FUL ){
							int  i;
							for( i = 0; i < nWriteLineCount; i++ ){
							    ExecColorMatchingFunc( this, (LPBYTE) lpFwBuf, this->scanInfo.ScanAreaByte.lWidth, 1 );
								lpFwBuf += dwWriteImageSize / nWriteLineCount;
							}
						}else{
							lpFwBuf += dwWriteImageSize;
						}
						lRealY += nWriteLineCount;
					}
				}
				lpScn += Dcount;
			}

		}//end of if *lpScn<0
	}//end of for
	*lpProcessSize = (WORD)(lpScn - lpRxBuff);

	WriteLog( "<<<<< ProcessMain End <<<<<" );

	return answer;
}


//-----------------------------------------------------------------------------
//
//	Function name:	SetupImgLineProc
//
//
//	Abstract:
//		set the valiables for raster-data-expansion/ resolution-exchanging
//
//
//	Parameters:
//		chLineHeader
//			line header of the acquired raster data
//
//
//	Return values:
//		None
//
//-----------------------------------------------------------------------------
//
void
SetupImgLineProc( BYTE chLineHeader )
{
	BYTE  chColorMode;
	BYTE  chCompression;


	//
	// Set the color type of the line data
	//
	chColorMode = chLineHeader & 0x1C;
	switch( chColorMode ){
		case 0x00:	// Mono-data
			ImgLineProcInfo.nInDataKind = SCIDK_MONO;
			break;

		case 0x04:	// Red data
			ImgLineProcInfo.nInDataKind = SCIDK_R;
			break;

		case 0x08:	// Green data
			ImgLineProcInfo.nInDataKind = SCIDK_G;
			break;

		case 0x0C:	// Blue data
			ImgLineProcInfo.nInDataKind = SCIDK_B;
			break;

		case 0x10:	// Dot-orderd RGB data (gaso-jyunji-RGB)
			ImgLineProcInfo.nInDataKind = SCIDK_RGB;
			break;

		case 0x14:	// Dot-orderd BGR data (gaso-jyunji-BGR)
			ImgLineProcInfo.nInDataKind = SCIDK_BGR;
			break;

		case 0x1C:	// 256 colors data
			ImgLineProcInfo.nInDataKind = SCIDK_256;
			break;
	}

	//
	// Set the compression mode for the line data
	//
	chCompression = chLineHeader & 0x03;
	if( chCompression == 2 ){
		// Packbits compression
		ImgLineProcInfo.nInDataComp = SCIDC_PACK;
	}else{
		ImgLineProcInfo.nInDataComp = SCIDC_NONCOMP;
	}

	//
	// set other parameters
	//
#if 1
	ImgLineProcInfo.bReverWrite     = FALSE;
#else
	ImgLineProcInfo.bReverWrite     = TRUE;
#endif
	ImgLineProcInfo.dwWriteBuffSize = dwImageBuffSize;
}


//-----------------------------------------------------------------------------
//
//	Function name:	GetStatusCode
//
//
//	Abstract:
//		proccess for the line status/ error code
//
//
//	Parameters:
//		nLineHeader
//			line header
//
//
//
//	Return values:
//		status information of scan
//
//-----------------------------------------------------------------------------
//
int
GetStatusCode( BYTE nLineHeader )
{
	int   answer;

	switch( nLineHeader ){
		case 0x80:	//terminate
			WriteLog( "\tPage end Detect" );
			answer = SCAN_EOF;
			break;

		case 0x81:	//Page end
			WriteLog( "\tNextPage Detect" );
			answer = SCAN_MPS;
			break;

		case 0xE3:	//MF14	MFC cancel scan because of timeout
		case 0x83:	//cancel acknowledge
			WriteLog( "\tCancel acknowledge" );
			answer = SCAN_CANCEL;
			break;
		//06/02/28
		case 0x84:
			WriteLog( "\tDuplex Normal" );
			answer = SCAN_MPS;
			break;
		case 0x85:
			WriteLog( "\tDuplex Reverse" );
			answer = SCAN_MPS;
			break;
		case 0xC2:	//no document
			WriteLog( "\tNo document" );
			answer = SCAN_NODOC;	// if no document, don't send picture
			break;

		case 0xC3:	//document jam
			WriteLog( "\tDocument JAM" );
			answer = SCAN_DOCJAM;
			break;

		case 0xC4:	//Cover Open
			WriteLog( "\tCover open" );
			answer = SCAN_COVER_OPEN;
			break;

		case 0xE5:	//
		case 0xE6:	//
		case 0xE7:	//
		default:	//service error
			WriteLog( "\tService Error\n" );
			answer = SCAN_SERVICE_ERR;
			break;
	}

	return answer;
}


//-----------------------------------------------------------------------------
//
//	Function name:	CnvResoNoToUserResoValue
//
//
//	Abstract:
//		Obtain the resolution value from the resolution type
//
//
//	Parameters:
//		nResoNo
//			the number of the resolution-type
//		pScanInfo
//			information of  scanning
//
//	Return values:
//		None
//
//-----------------------------------------------------------------------------
//
void
CnvResoNoToUserResoValue( LPRESOLUTION pUserSelect, WORD nResoNo )
{
	switch( nResoNo ){
		case RES100X100:	// 100 x 100 dpi
			pUserSelect->wResoX = 100;
			pUserSelect->wResoY = 100;
			break;

		case RES150X150:	// 150 x 150 dpi
			pUserSelect->wResoX = 150;
			pUserSelect->wResoY = 150;
			break;

		case RES200X100:	// 200 x 100 dpi
			pUserSelect->wResoX = 200;
			pUserSelect->wResoY = 100;
			break;

		case RES200X200:	// 200 x 200 dpi
			pUserSelect->wResoX = 200;
			pUserSelect->wResoY = 200;
			break;

		case RES200X400:	// 200 x 400 dpi
			pUserSelect->wResoX = 200;
			pUserSelect->wResoY = 400;
			break;

		case RES300X300:	// 300 x 300 dpi
			pUserSelect->wResoX = 300;
			pUserSelect->wResoY = 300;
			break;

		case RES400X400:	// 400 x 400 dpi
			pUserSelect->wResoX = 400;
			pUserSelect->wResoY = 400;
			break;

		case RES600X600:	// 600 x 600 dpi
			pUserSelect->wResoX = 600;
			pUserSelect->wResoY = 600;
			break;

		case RES800X800:	// 800 x 800 dpi
			pUserSelect->wResoX = 800;
			pUserSelect->wResoY = 800;
			break;

		case RES1200X1200:	// 1200 x 1200 dpi
			pUserSelect->wResoX = 1200;
			pUserSelect->wResoY = 1200;
			break;

		case RES2400X2400:	// 2400 x 2400 dpi
			pUserSelect->wResoX = 2400;
			pUserSelect->wResoY = 2400;
			break;

		case RES4800X4800:	// 4800 x 4800 dpi
			pUserSelect->wResoX = 4800;
			pUserSelect->wResoY = 4800;
			break;

		case RES9600X9600:	// 9600 x 9600 dpi
			pUserSelect->wResoX = 9600;
			pUserSelect->wResoY = 9600;
			break;
	}
}

//////// end of brother_scanner.c ////////
