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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brother_bugchk.h"


#define	BUGCHK_SIGN_VALUE	0x53545244 /* STRD */
typedef	unsigned long	BUGCHK_SIGN ;

int nMallocCnt=0; // Malloc�ؿ� �ƽФ����
int nFreeCnt=0; // Free�ؿ� �ƽФ����

void *	bugchk_malloc(size_t size, int line, const char *file)
{
	char *pRet ;

	nMallocCnt++;

	pRet = malloc(size + sizeof (size_t) + sizeof (BUGCHK_SIGN) * 2) ;

	if (pRet == NULL)
	{
		fprintf(stderr, "bugchk_malloc(size=%d), can't allocate@%s(%d)\n", 
			size, file, line) ;
		abort() ;
	}
	/*	���꤬�Ȥ줿�顢�ޡ����ͤ�����	*/
	else {
		char	*pMark = pRet ;


		/*	�ǽ�	*/
		*((BUGCHK_SIGN *)pMark) = BUGCHK_SIGN_VALUE ;
		pMark += sizeof (BUGCHK_SIGN) ;

		/*	������	*/
		*((size_t *)pMark) = size ;
		pMark += sizeof (size_t) ;
		pRet = pMark ;	/* ����ͤ���¸	*/

		/*	�Ǹ�	*/
		pMark += size ;
		*((BUGCHK_SIGN *)pMark) = BUGCHK_SIGN_VALUE ;
	}

	return	pRet ;
}



void 	bugchk_free(void *ptr , int line, const char *file)
{
	int		ok = 0 ;

	nFreeCnt++;

	if (ptr == NULL || (unsigned int)ptr < 100UL)
	{
		fprintf(stderr, "bugchk_free(ptr=%p)@%s(%d)\n", ptr, file, line) ;
	}
	else {
		char	*pMark = (char *)ptr ;

		pMark -= (sizeof (size_t) + sizeof (BUGCHK_SIGN)) ;
		if ( BUGCHK_SIGN_VALUE != *((BUGCHK_SIGN *)pMark) )
		{
			fprintf(stderr, "bugchk_free(ptr=%p), invalid begin-mark=0x%lx@%s(%d)\n", 
				ptr, *((BUGCHK_SIGN *)pMark), file, line) ;
		}
		else {
			size_t	size ;

			pMark += sizeof (BUGCHK_SIGN) ;

			size = *((size_t *)pMark) ;
			pMark += sizeof (size_t) ;

			pMark += size ;
			if ( BUGCHK_SIGN_VALUE != *((BUGCHK_SIGN *)pMark) )
			{
				fprintf(stderr, "bugchk_free(ptr=%p), invalid end-mark=0x%lx, size=%d@%s(%d)\n", 
					ptr, *((BUGCHK_SIGN *)pMark), size, file, line) ;
			}
			else 
			{
				ok = 1 ;
			}
		}
	}

	if (!ok)
	{
		fflush(stderr) ;
		abort() ;
	}
}



#ifdef	SELF_TEST
int main(int argc, char **argv)
{
	char  *ptr  ;
		
//	FREE((void *)0) ;
//	FREE((void *)99) ;


	ptr = MALLOC(1023) ;
	memset(ptr, 0, 1023) ;
	FREE(ptr) ;
	
	ptr = MALLOC(1024 * 1024 * 1024 * 1) ;
	FREE(ptr) ;

	ptr = MALLOC(1024) ;
//	*(ptr - 5) = 0 ;
	memset(ptr, 0, 1025) ;
	FREE(ptr) ;
	

	return 0 ;
}
#endif /*--  #ifdef	SELF_TEST  --*/
