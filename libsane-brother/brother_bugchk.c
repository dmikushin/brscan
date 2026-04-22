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
typedef	unsigned long	BUGCHK_SIGN;

int nMallocCnt=0; // Malloc�ؿ� �ƽФ����
int nFreeCnt=0; // Free�ؿ� �ƽФ����

void *bugchk_malloc(size_t size, int line, const char *file)
{
    void *p = malloc(size);
    if (!p) {
	fprintf(stderr, "bugchk_malloc(size=%zd), can't allocate@%s(%d)\n",
		size, file, line);
	abort();
    }
    return p;
}

void bugchk_free(void *ptr , int line, const char *file)
{
    free(ptr);  /* free(NULL) is valid C */
}



#ifdef	SELF_TEST
int main(int argc, char **argv)
{
    char  *ptr ;

//	FREE((void *)0);
//	FREE((void *)99);

    ptr = MALLOC(1023);
    memset(ptr, 0, 1023);
    FREE(ptr);

    ptr = MALLOC(1024 * 1024 * 1024 * 1);
    FREE(ptr);

    ptr = MALLOC(1024);
//	*(ptr - 5) = 0;
    memset(ptr, 0, 1025);
    FREE(ptr);

    return 0;
}
#endif /*--  #ifdef	SELF_TEST  --*/
