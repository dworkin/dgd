# include <windows.h>
# include <time.h>
# include "dgd.h"

# define UNIXBIRTH	0x019db1ded53e8000

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970
 */
Uint P_time(void)
{
    FILETIME ft;
    SYSTEMTIME st;
    __int64 time;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    time = ((__int64) ft.dwHighDateTime << 32) + ft.dwLowDateTime - UNIXBIRTH;
    return (Uint) (time / 10000000);
}

/*
 * NAME:	P->mtime()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970 in milliseconds
 */
Uint P_mtime(unsigned short *milli)
{
    FILETIME ft;
    SYSTEMTIME st;
    __int64 time;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    time = ((__int64) ft.dwHighDateTime << 32) + ft.dwLowDateTime - UNIXBIRTH;
    *milli = (unsigned short) ((time % 10000000) / 10000);
    return (Uint) (time / 10000000);
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	return time as string
 */
char *P_ctime(char *buf, Uint time)
{
    int offset;
    time_t t;

    offset = 0;
    for (offset = 0; time > 2147397248; time -= 883612800, offset += 28) ;
    t = time;
    memcpy(buf, ctime(&t), 26);
    if (offset != 0) {
	long year;

	year = strtol(buf + 20, (char **) NULL, 10) + offset;
	if (year > 2100 ||
	    (year == 2100 && (buf[4] != 'J' || buf[5] != 'a') &&
	     (buf[4] != 'F' || (buf[8] == '2' && buf[9] == '9')))) {
	    /* 2100 is not a leap year */
	    t -= 378604800;
	    offset += 12;
	    memcpy(buf, ctime(&t), 26);
	    year = strtol(buf + 20, (char **) NULL, 10) + offset;
	}
	sprintf(buf + 20, "%ld\012", year);
    }
    if (buf[8] == '0') {
	buf[8] = ' ';	/* MSDEV ctime weirdness */
    }
    return buf;
}
