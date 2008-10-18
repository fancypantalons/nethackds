#include <nds.h>

#include "hack.h"

#include "ds_io.h"
/*#undef fopen
#undef fclose
#undef fseek
#undef rewind
#undef fgetc
#undef fgets
#undef fread
*/
#undef open
#undef close
#undef creat
#undef lseek
#undef read
#undef sopen
#undef write

#include <unistd.h>
#include <reent.h>
#include <sys/iosupport.h>
#include <errno.h>
FILE* logfile;
char logbuf[BUFSZ];

/*
FILE* ds_fopen(const char* fn, const char* mode) {
	int i;
	char buf[BUFSZ];
	for (i=0;fn[i];i++) {
		if (fn[i] == '\\') buf[i] = '/';
		else buf[i] = fn[i];
	}
	buf[i] = '\0';
	FILE* f = fopen(buf,mode);
	nds_debug_print(" fo %s,%s=%d ",buf,mode,(int)f);

logfile = fopen("/file.log","a");
sprintf(logbuf,"%08X: OPENED FROM \"%s\" WITH MODE \"%s\"\n",(int)f,fn,mode);
fputs(logbuf,logfile);
fclose(logfile);
	
	return f;
}

int ds_fclose(FILE* f) {
	nds_debug_print(" cl %d ",(int)f);
logfile = fopen("/file.log","a");
sprintf(logbuf,"%08X: CLOSED\n",(int)f);
fputs(logbuf,logfile);
fclose(logfile);
	return fclose(f);
}

int ds_fseek(FILE* f, long off, int origin) {
	int ret= fseek(f,off,origin);
logfile = fopen("/file.log","a");
sprintf(logbuf," %08X: FSEEK off=%d origin=%d ret=%d\n",(int)f,off,origin,ret);
fputs(logbuf,logfile);
fclose(logfile);
	return ret;
}

void ds_rewind(FILE* f) {
logfile = fopen("/file.log","a");
sprintf(logbuf," %08X: REWIND\n",(int)f);
fputs(logbuf,logfile);
fclose(logfile);
	rewind( f );
}

int ds_fgetc(FILE* f) {
	int ret = fgetc(f);
logfile = fopen("/file.log","a");
sprintf(logbuf," %08X: FGETC ret=%02X\n",(int)f,ret);
fputs(logbuf,logfile);
fclose(logfile);
	return ret;
}

char* ds_fgets(char* buf, int n, FILE* f) {
	char* ret = fgets(buf,n,f);
logfile = fopen("/file.log","a");
sprintf(logbuf," %08X: FGETS len=%d str=\"%.40s\"\n",(int)f,strlen(buf),buf);
fputs(logbuf,logfile);
fclose(logfile);
	return ret;
}

int ds_fread(void* buf, size_t size, size_t count, FILE* f) {
	int ret= fread(buf, size, count, f);
logfile = fopen("/file.log","a");
int temp = sprintf(logbuf," %08X: FREAD size=%d count=%d ret=%d buf=",(int)f,size,count,ret);
int i,j=0;
for (i=temp,j=0;i<BUFSZ-5 && j<size*count;i+=2,j++) {
	sprintf(logbuf+i,"%02X",((u8*)buf)[j]);
}
sprintf(logbuf+i,"\n");
fputs(logbuf,logfile);
fclose(logfile);
	return ret;
}
*/



int ds_close(int handle) {
	return (fclose((FILE*)handle) ? 0 : -1);
}

int ds_creat(const char* fn, int mode) {
	int f = ds_open(fn,O_CREAT | O_TRUNC | O_WRONLY,mode);
	return f;	// if FAT_fopen fails, it returns -1, which is the correct
 					//  return value from creat() on failure
}

// I don't really like it, but it works...  FAT_fseek returns 0 on success and
//  -1 on failure; lseek returns the offset from beginning of file on success
//  instead.  Fortunately, NetHack (currently) only uses SEEK_SET so it's O.K.
//  to return offset for now.

off_t ds_lseek(int handle, off_t offset, int origin) {
	int ret = fseek((FILE*)handle, offset, origin);
	return (ret == 0 ? (int)offset : -1);
}

int ds_open(const char* name, int oflag, int pmode) {
	char mode[3];
	
	mode[0] = mode[1] = mode[2] = '\0';
	//if (pmode & O_RDONLY) mode[0] = 'r';
	mode[0] = 'r';
	if ((oflag & O_WRONLY) || ((oflag & O_RDWR) && (oflag & O_TRUNC))) mode[0] = 'w';
	if (oflag & O_APPEND) mode[0] = 'a';
	if ((oflag & O_RDWR)) mode[1] = '+';
	
	
	//if (access(name,4) && (pmode & O_CREAT) && (pmode & O_EXCL)) return -1;
	
//errno=0;
	int ret = (int)fopen(name, mode);

/*logfile = fopen("/file.log","a");
sprintf(logbuf,"%08X: OPEN FROM \"%s\" WITH oflag=%d pmode=%d errno=%d mode=%s\n",ret,name,oflag,pmode,errno,mode);
fputs(logbuf,logfile);
fclose(logfile);*/

	if (ret == (int)NULL) return -1;
	return ret;
}

int ds_read(int handle, void* buf, unsigned int count) {
//errno=0;
	int ret = fread(buf,1,count,(FILE*)handle);
/*logfile = fopen("/file.log","a");
int temp = sprintf(logbuf," %08X: READ count=%d ret=%d errno=%d buf=",handle,count,ret,errno);
int i,j=0;
for (i=temp,j=0;i<BUFSZ-10 && j<count;i+=2,j++) {
	sprintf(logbuf+i,"%02X",((u8*)buf)[j]);
}
sprintf(logbuf+i,"\n");
fputs(logbuf,logfile);
fclose(logfile);*/

	return ret;
}

int ds_sopen(const char* name, int oflag, int shflag, int pmode) {
	return ds_open(name,oflag,pmode);
}


int ds_write(int handle, const void* buf, unsigned int count) {
//errno=0;
	int ret = fwrite(buf,1,count,(FILE*)handle);
/*logfile = fopen("/file.log","a");
int temp = sprintf(logbuf," %08X: WRITE count=%d ret=%d errno=%d buf=",handle,count,ret,errno);
int i=temp,j=0;
for (i=temp,j=0;i<BUFSZ-10 && j<count;i+=2,j++) {
	sprintf(logbuf+i,"%02X",((u8*)buf)[j]);
}
sprintf(logbuf+i,"\n");
fputs(logbuf,logfile);
fclose(logfile);*/


	return ret;
}




int creat(const char* fn, mode_t mode) {
	int ret = open(fn,O_TRUNC | O_CREAT,mode);
	return ret;
}
/*
// UGLY HACK
int chdir(const char* dir) {
	struct _reent r;
	r._errno = 0;
nds_debug_print("made reent,");
	int ret = FindDevice("fat:");
nds_debug_print("found dev %d,",ret);
	ret = (*(devoptab_list[ret]->chdir_r))(&r,dir);
nds_debug_print("ret %d, en %d,",ret,r._errno);
	errno = r._errno;
nds_debug_print("done.");
	return ret;
}*/

uid_t getuid() {
	return 0;
}

gid_t getgid() {
	return 0;
}

pid_t getpid() {
	return 1;
}


void error VA_DECL(const char*, line)
	/*VA_START(line);
	VA_INIT(line, char *);
	vpline(line, VA_ARGS);
	VA_END();*/
	// DUMMY
//	pline
	//while (1) { }
	
}


void
regularize(s)	/* normalize file name */
register char *s;
{
	register char *lp;

	for (lp = s; *lp; lp++)
	    if ( *lp == '?' || *lp == '"' || *lp == '\\' ||
		 *lp == '/' || *lp == '>' || *lp == '<'  ||
		 *lp == '*' || *lp == '|' || *lp == ':'  || ((unsigned char)*lp > 127))
			*lp = '_';
}


/*
 * This is used in nhlan.c to implement some of the LAN_FEATURES.
 */
char *get_username(lan_username_size)
int *lan_username_size;
{
	static char username_buffer[BUFSZ] = "SporkHack\0";	// will get it from preferences at some point
	
	//unsigned int status;
	//int i = BUFSZ - 1;

	/* i gets updated with actual size */
	//status = GetUserName(username_buffer, &i);		
	//if (status) username_buffer[i] = '\0';
	//else Strcpy(username_buffer, "SporkHack");
	if (lan_username_size) *lan_username_size = strlen(username_buffer);
	return username_buffer;
}

char* ds_getenv(const char* vn) {
	static char* path = ".;/SporkHack";
	static char* hackdir = "/SporkHack";
	if (!stricmp(vn,"PATH")) return path;
	if (!stricmp(vn,"HACKDIR")) return hackdir;
	return 0;
}

// fortunately, the return value is always passed to localtime() or srand(), 
// and ds_localtime processes this format into a struct tm* appropriately
time_t ds_time(time_t* t) {
	u32 temp = IPC->time.rtc.seconds & 0x3F;		// second:	6 bits
	temp |= (IPC->time.rtc.minutes & 0x3F) << 6;	// minute:	6 bits
	temp |= (IPC->time.rtc.hours & 0x1F) << 12;	// hour:	5 bits
	int day;
	if (IPC->time.rtc.day > 40) day = IPC->time.rtc.day - 40;
	else day = IPC->time.rtc.day;
	temp |= (day & 0x1F) << 17;				// day:		5 bits
	temp |= (IPC->time.rtc.month & 0x0F) << 22;	// month:	4 bits
	temp |= (IPC->time.rtc.year & 0x3F) << 26;	// year:	6 bits
	if (t != NULL) *t = temp;
	return temp;
}

static struct tm the_time;

// this method was taken from:
// http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week
// on April 11, 2006
const int month_numbers[] = {0,3,3,6,1,4,6,2,5,0,3,5};
#define IS_LEAP_YEAR(y)		((y%4 == 0) && ((y%100 != 0) || (y%400 == 0)))
int get_weekday() {
	int magic_number = 6;	// the number to start with for 2000-2099
	magic_number += the_time.tm_year + (the_time.tm_year >> 2);
	if (IS_LEAP_YEAR(the_time.tm_year) && the_time.tm_mon < 2) 
 		magic_number += 6;	// adjust for jan & feb of a leap year
	magic_number += month_numbers[the_time.tm_mon];
	magic_number += the_time.tm_mday;
	return magic_number%7;
}	

struct tm* ds_localtime(time_t* t) {
	u32 temp = *t;
	the_time.tm_sec = (temp) & 0x3F;				// second:	6 bits
	the_time.tm_min = (temp>>6) & 0x3F;				// minute:	6 bits
	the_time.tm_hour = (temp>>12) & 0x1F;			// hour:	5 bits
	the_time.tm_mday = (temp>>17) & 0x1F;			// day:		5 bits
	the_time.tm_mon = ((temp>>22) & 0x0F)-1;		// month:	4 bits
	the_time.tm_year = ((temp>>26) & 0x3F) + 100;	// year:	6 bits
	the_time.tm_wday = get_weekday();
	return &the_time;
}
