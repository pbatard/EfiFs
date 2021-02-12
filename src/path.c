/* path.c - Path handling routines */
/*
 *  Copyright © 2014-2021 Pete Batard <pete@akeo.ie>
 *  Based on path sanitation code by Ludwig Nussel <ludwig.nussel@suse.de>
 *
 *  Note: This file has been relicensed from GPLv3+ to GPLv2+ by formal
 *  agreement of all of the contributors who applied changes on top of
 *  its original Public Domain source. The original source can be found at:
 *  https://github.com/saygili/pisilinux/blob/65c6b72d90aa282d8a3e79be209fa364c7180ffc/extra/util/archive/unarj/files/unarj-2.65-sanitation.patch#L4-L86
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "driver.h"

/* copy src into dest converting the path to a relative one inside the current
 * directory. dest must hold at least len bytes
 */

#ifndef PATH_CHAR
#define PATH_CHAR '/'
#endif

VOID CopyPathRelative(CHAR8 *dest, CHAR8 *src, INTN len)
{
	CHAR8* o = dest;
	CHAR8* p = src;

	*o = '\0';

	while(*p && *p == PATH_CHAR) ++p;
	for(; len && *p;)
	{
		src = p;
		while (*p && *p != PATH_CHAR) p++;
		if (!*p) p = src+strlena(src);

		/* . => skip */
		if(p-src == 1 && *src == '.' )
		{
			if(*p) src = ++p;
		}
		/* .. => pop one */
		else if(p-src == 2 && *src == '.' && src[1] == '.')
		{
			if(o != dest)
			{
				UINTN i;
				*o = '\0';
				for(i = strlena(dest)-1; i > 0 && dest[i] != PATH_CHAR; i--);
				len += o-&dest[i];
				o = &dest[i];
				if(*p) ++p;
			}
			else /* nothing to pop */
				if(*p) ++p;
		}
		else
		{
			INTN copy;
			if(o != dest)
			{
				--len;
				*o++ = PATH_CHAR;
			}
			copy = MIN(p-src,len);
			CopyMem(o, src, copy);
			len -= copy;
			src += copy;
			o += copy;
			if(*p) ++p;
		}
		while(*p && *p == PATH_CHAR) ++p;
	}
	o[len?0:-1] = '\0';
}
