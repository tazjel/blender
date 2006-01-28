/* util.c
 *
 * various string, file, list operations.
 *
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * 
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "DNA_listBase.h"
#include "BLI_storage.h"
#include "BLI_storage_types.h"

#include "BLI_util.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif


#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef __APPLE__
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#endif


// copies from BKE_utildefines
#ifndef FILE_MAXDIR
#define FILE_MAXDIR  160
#endif

#ifndef FILE_MAXFILE
#define FILE_MAXFILE 80
#endif

/* local */

static int add_win32_extension(char *name);

/* implementation */

/* Ripped this from blender.c
 */
void addlisttolist(ListBase *list1, ListBase *list2)
{

	if(list2->first==0) return;

	if(list1->first==0) {
		list1->first= list2->first;
		list1->last= list2->last;
	}
	else {
		((struct Link *)list1->last)->next= list2->first;
		((struct Link *)list2->first)->prev= list1->last;
		list1->last= list2->last;
	}
	list2->first= list2->last= 0;
}

int BLI_stringdec(char *string, char *kop, char *staart, unsigned short *numlen)
{
	unsigned short len, len2, nums = 0, nume = 0;
	short i, found = 0;

	len2 = len =  strlen( string);
	
	if (len > 6) {
		if (BLI_strncasecmp(string + len - 6, ".blend", 6) == 0) len -= 6;
		else if (BLI_strncasecmp(string + len - 6, ".trace", 6) == 0) len -= 6;
	}
	
	if (len > 9) {
		if (BLI_strncasecmp(string + len - 9, ".blend.gz", 9) == 0) len -= 9;
	}
		
	
	if (len == len2) {
		if (len > 4) {
			/* handle .jf0 en .jf1 for jstreams */
			if (BLI_strncasecmp(string + len - 4, ".jf", 3) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".tga", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".jpg", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".png", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".txt", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".cyc", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".enh", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".rgb", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".psx", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".ble", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".exr", 4) == 0) len -= 4;
		}
	}
	
	for (i = len - 1; i >= 0; i--){
		if (string[i] == '/') break;
		if (isdigit(string[i])) {
			if (found){
				nums = i;
			}
			else{
				nume = i;
				nums = i;
				found = 1;
			}
		}
		else{
			if (found) break;
		}
	}
	if (found){
		if (staart) strcpy(staart,&string[nume+1]);
		if (kop) {
			strcpy(kop,string);
			kop[nums]=0;
		}
		if (numlen) *numlen = nume-nums+1;
		return ((int)atoi(&(string[nums])));
	}
	if (staart) strcpy(staart, string + len);
	if (kop) {
		strncpy(kop, string, len);
		kop[len] = 0;
	}
	if (numlen) *numlen=0;
	return 0;
}


void BLI_stringenc(char *string, char *kop, char *staart, unsigned short numlen, int pic)
{
	char numstr[10]="";
	unsigned short len,i;

	strcpy(string,kop);
	
	if (pic>0 || numlen==4) {
		len= sprintf(numstr,"%d",pic);

		for(i=len;i<numlen;i++){
			strcat(string,"0");
		}
		strcat(string,numstr);
	}
	strcat(string,staart);
}


void BLI_newname(char * name, int add)
{
	char head[128], tail[128];
	int pic;
	unsigned short digits;
	
	pic = BLI_stringdec(name, head, tail, &digits);
	
	/* are we going from 100 -> 99 or from 10 -> 9 */
	if (add < 0 && digits < 4 && digits > 0) {
		int i, exp;
		exp = 1;
		for (i = digits; i > 1; i--) exp *= 10;
		if (pic >= exp && (pic + add) < exp) digits--;
	}
	
	pic += add;
	
	if(digits==4 && pic<0) pic= 0;
	BLI_stringenc(name, head, tail, digits, pic);
}


void BLI_addhead(ListBase *listbase, void *vlink)
{
	struct Link *link= vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	link->next = listbase->first;
	link->prev = 0;

	if (listbase->first) ((struct Link *)listbase->first)->prev = link;
	if (listbase->last == 0) listbase->last = link;
	listbase->first = link;
}


void BLI_addtail(ListBase *listbase, void *vlink)
{
	struct Link *link= vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	link->next = 0;
	link->prev = listbase->last;

	if (listbase->last) ((struct Link *)listbase->last)->next = link;
	if (listbase->first == 0) listbase->first = link;
	listbase->last = link;
}


void BLI_remlink(ListBase *listbase, void *vlink)
{
	struct Link *link= vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	if (link->next) link->next->prev = link->prev;
	if (link->prev) link->prev->next = link->next;

	if (listbase->last == link) listbase->last = link->prev;
	if (listbase->first == link) listbase->first = link->next;
}


void BLI_freelinkN(ListBase *listbase, void *vlink)
{
	struct Link *link= vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	BLI_remlink(listbase,link);
	MEM_freeN(link);
}


void BLI_insertlink(ListBase *listbase, void *vprevlink, void *vnewlink)
{
	struct Link *prevlink= vprevlink, *newlink= vnewlink;

	/* newlink comes after prevlink */

	if (newlink == 0) return;
	if (listbase == 0) return;

	if(listbase->first==0) { /* empty list */
		listbase->first= newlink;
		listbase->last= newlink;
		return;
	}
	if (prevlink== 0) {	/* insert before first element */
		newlink->next= listbase->first;
		newlink->prev= 0;
		newlink->next->prev= newlink;
		listbase->first= newlink;
		return;
	}

	if (listbase->last== prevlink) /* at end of list */
		listbase->last = newlink;

	newlink->next= prevlink->next;
	prevlink->next= newlink;
	if(newlink->next) newlink->next->prev= newlink;
	newlink->prev= prevlink;
}

void BLI_insertlinkbefore(ListBase *listbase, void *vnextlink, void *vnewlink)
{
	struct Link *nextlink= vnextlink, *newlink= vnewlink;

	/* newlink before nextlink */

	if (newlink == 0) return;
	if (listbase == 0) return;

	if(listbase->first==0) { /* empty list */
		listbase->first= newlink;
		listbase->last= newlink;
		return;
	}
	if (nextlink== 0) {	/* insert at end of list */
		newlink->prev= listbase->last;
		newlink->next= 0;
		((struct Link *)listbase->last)->next= newlink;
		listbase->last= newlink;
		return;
	}

	if (listbase->first== nextlink) /* at beginning of list */
		listbase->first = newlink;

	newlink->next= nextlink;
	newlink->prev= nextlink->prev;
	nextlink->prev= newlink;
	if(newlink->prev) newlink->prev->next= newlink;
}


void BLI_freelist(ListBase *listbase)
{
	struct Link *link,*next;

	if (listbase == 0) return;
	link= listbase->first;
	while(link) {
		next= link->next;
		free(link);
		link= next;
	}
	listbase->first=0;
	listbase->last=0;
}

void BLI_freelistN(ListBase *listbase)
{
	struct Link *link,*next;

	if (listbase == 0) return;
	link= listbase->first;
	while(link) {
		next= link->next;
		MEM_freeN(link);
		link= next;
	}
	listbase->first=0;
	listbase->last=0;
}


int BLI_countlist(ListBase *listbase)
{
	Link * link;
	int count = 0;
	
	if (listbase){
		link = listbase->first;
		while(link) {
			count++;
			link= link->next;
		}
	}
	return(count);
}

void * BLI_findlink(ListBase *listbase, int number)
{
	Link * link = NULL;

	if (number >= 0) {
		link = listbase->first;
		while (link != NULL && number != 0) {
			number--;
			link = link->next;
		}
	}

	return (link);
}


char *BLI_strdupn(char *str, int len) {
	char *n= MEM_mallocN(len+1, "strdup");
	memcpy(n, str, len);
	n[len]= '\0';
	
	return n;
}
char *BLI_strdup(char *str) {
	return BLI_strdupn(str, strlen(str));
}

char *BLI_strncpy(char *dst, char *src, int maxncpy) {
	int srclen= strlen(src);
	int cpylen= (srclen>(maxncpy-1))?(maxncpy-1):srclen;
	
	memcpy(dst, src, cpylen);
	dst[cpylen]= '\0';
	
	return dst;
}

int BLI_streq(char *a, char *b) {
	return (strcmp(a, b)==0);
}
int BLI_strcaseeq(char *a, char *b) {
	return (BLI_strcasecmp(a, b)==0);
}

/* ******************** string encoding ***************** */

/* This is quite an ugly function... its purpose is to
 * take the dir name, make it absolute, and clean it up, replacing
 * excess file entry stuff (like /tmp/../tmp/../)
 * note that dir isn't protected for max string names... 
 */

void BLI_cleanup_dir(const char *relabase, char *dir)
{
	short a;
	char *start, *eind;
	
	BLI_convertstringcode(dir, relabase, 0);
	
#ifdef WIN32
	if(dir[0]=='.') {	/* happens for example in FILE_MAIN */
	   dir[0]= '\\';
	   dir[1]= 0;
	   return;
	}	

	while ( (start = strstr(dir, "\\..\\")) ) {
		eind = start + strlen("\\..\\") - 1;
		a = start-dir-1;
		while (a>0) {
			if (dir[a] == '\\') break;
			a--;
		}
		strcpy(dir+a,eind);
	}

	while ( (start = strstr(dir,"\\.\\")) ){
		eind = start + strlen("\\.\\") - 1;
		strcpy(start,eind);
	}

	while ( (start = strstr(dir,"\\\\" )) ){
		eind = start + strlen("\\\\") - 1;
		strcpy(start,eind);
	}

	if((a = strlen(dir))){				/* remove the '\\' at the end */
		while(a>0 && dir[a-1] == '\\'){
			a--;
			dir[a] = 0;
		}
	}

	strcat(dir, "\\");
#else	
	if(dir[0]=='.') {	/* happens, for example in FILE_MAIN */
	   dir[0]= '/';
	   dir[1]= 0;
	   return;
	}	

	while ( (start = strstr(dir, "/../")) ) {
		eind = start + strlen("/../") - 1;
		a = start-dir-1;
		while (a>0) {
			if (dir[a] == '/') break;
			a--;
		}
		strcpy(dir+a,eind);
	}

	while ( (start = strstr(dir,"/./")) ){
		eind = start + strlen("/./") - 1;
		strcpy(start,eind);
	}

	while ( (start = strstr(dir,"//" )) ){
		eind = start + strlen("//") - 1;
		strcpy(start,eind);
	}

	if( (a = strlen(dir)) ){				/* remove all '/' at the end */
		while(dir[a-1] == '/'){
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}

	strcat(dir, "/");
#endif
}


void BLI_makestringcode(const char *relfile, char *file)
{
	char * p;
	char * q;
	char * lslash;
	int len=0;
	char temp[FILE_MAXDIR+FILE_MAXFILE];
	char res[FILE_MAXDIR+FILE_MAXFILE];
	
	/* if file is already relative, bail out */
	if(file[0]=='/' && file[1]=='/') return;
	
	strcpy(temp, relfile);
	
#ifdef WIN32
	if (strlen(file) > 2) {
		if ( temp[1] == ':' && file[1] == ':' && temp[0] != file[0] )
			return;
	}
#endif

	BLI_char_switch(temp, '\\', '/');
	BLI_char_switch(file, '\\', '/');

	/* the last slash in the file indicates where the path part ends */
	lslash = BLI_last_slash(temp);

	if (lslash) 
	{	
		/* find the prefix of the filename that is equal for both filenames.
		   This is replaced by the two slashes at the beginning */
		p = temp;
		q = file;
		while (*p == *q) {
			++p; ++q;
		}
		/* we might have passed the slash when the beginning of a dir matches 
		   so we rewind. Only check on the actual filename
		*/
		if (*q != '/') {
			while ( (q >= file) && (*q != '/') ) { --q; --p; }
		} 
		else if (*p != '/') {
			while ( (p >= temp) && (*p != '/') ) { --p; --q; }
		}
		
		strcpy(res,	"//");

		/* p now points to the slash that is at the beginning of the part
		   where the path is different from the relative path. 
		   We count the number of directories we need to go up in the
		   hierarchy to arrive at the common 'prefix' of the path
		*/			
		while (p && p < lslash)	{
			if (*p == '/') 
				strcat(res,	"../");
			++p;
		}

		strcat(res, q+1); /* don't copy the slash at the beginning */
		
#ifdef	WIN32
		BLI_char_switch(res+2, '/', '\\');
#endif
		strcpy(file, res);
	}
}

int BLI_convertstringcode(char *path, const char *basepath, int framenum)
{
	int len, wasrelative;
	char tmp[FILE_MAXDIR+FILE_MAXFILE];
	char base[FILE_MAXDIR];
	
	strcpy(tmp, path);
	strcpy(base, basepath);
	
	/* push slashes into unix mode - strings entering this part are
	   potentially messed up: having both back- and forward slashes.
	   Here we push into one conform direction, and at the end we
	   push them into the system specific dir. This ensures uniformity
	   of paths and solving some problems (and prevent potential future
	   ones) -jesterKing. */
	BLI_char_switch(tmp, '\\', '/');
	BLI_char_switch(base, '\\', '/');
	
	wasrelative= (strncmp(tmp, "//", 2)==0);

	if (tmp[0] == '/' && tmp[1] == '/') {
		char *filepart= BLI_strdup(tmp+2); /* skip code */
		char *lslash= BLI_last_slash(base);

		if (lslash) {
			int baselen= (int) (lslash-base) + 1;

			memcpy(tmp, base, baselen);
			strcpy(tmp+baselen, filepart);
		} else {
			strcpy(tmp, filepart);
		}
		
		MEM_freeN(filepart);
	}

	len= strlen(tmp);
	if(len && tmp[len-1]=='#') {
		sprintf(tmp+len-1, "%04d", framenum);
	}
	
	strcpy(path, tmp);
#ifdef WIN32
	/* skip first two chars, which in case of
	   absolute path will be drive:/blabla and
	   in case of relpath //blabla/. So relpath
	   // will be retained, rest will be nice and
	   shiny win32 backward slashes :) -jesterKing
	*/
	BLI_char_switch(path+2, '/', '\\');
#endif

	return wasrelative;
}

void BLI_splitdirstring(char *di,char *fi)
{
	char *lslash= BLI_last_slash(di);

	if (lslash) {
		strcpy(fi, lslash+1);
		*(lslash+1)=0;
	} else {
		strcpy(fi, di);
		di[0]= 0;
	}
}

char *BLI_gethome(void) {
	#ifdef __BeOS
		return "/boot/home/";		/* BeOS 4.5: doubleclick at icon doesnt give home env */

	#elif !defined(WIN32)
		return getenv("HOME");

	#else /* Windows */
		char * ret;
		static char dir[512];

		/* Check for %HOME% env var */

		ret = getenv("HOME");
		if(ret) {
			sprintf(dir, "%s\\.blender", ret);
			if (BLI_exists(dir)) return dir;
		}

		/* else, check install dir (path containing blender.exe) */

		BLI_getInstallationDir(dir);

		if (BLI_exists(dir))
		{
			strcat(dir,"\\.blender");
			if (BLI_exists(dir)) return(dir);
		}

				
		/* add user profile support for WIN 2K / NT */
		ret = getenv("USERPROFILE");
		if (ret) {
			if (BLI_exists(ret)) { /* from fop, also below... */
				sprintf(dir, "%s\\Application Data\\Blender Foundation\\Blender", ret);
				BLI_recurdir_fileops(dir);
				if (BLI_exists(dir)) {
					strcat(dir,"\\.blender");
					if(BLI_exists(dir)) return(dir);
				}
			}
		}

		/* 
		   Saving in the Windows dir is less than desirable. 
		   Use as a last resort ONLY! (aphex)
		*/
		
		ret = getenv("WINDOWS");		
		if (ret) {
			if(BLI_exists(ret)) return ret;
		}

		ret = getenv("WINDIR");	
		if (ret) {
			if(BLI_exists(ret)) return ret;
		}
		
		return "C:\\Temp";	/* sheesh! bad, bad, bad! (aphex) */
	#endif
}

void BLI_clean(char *path)
{
	if(path==0) return;
#ifdef WIN32
	if(path && strlen(path)>2) {
		BLI_char_switch(path+2, '/', '\\');
	}
#else
	BLI_char_switch(path, '\\', '/');
#endif
}

void BLI_char_switch(char *string, char from, char to) 
{
	if(string==0) return;
	while (*string != 0) {
		if (*string == from) *string = to;
		string++;
	}
}

void BLI_make_exist(char *dir) {
	int a;

	#ifdef WIN32
		BLI_char_switch(dir, '/', '\\');
	#else
		BLI_char_switch(dir, '\\', '/');
	#endif	
	
	a = strlen(dir);
	
#ifdef WIN32	
	while(BLI_exists(dir) == 0){
		a --;
		while(dir[a] != '\\'){
			a--;
			if (a <= 0) break;
		}
		if (a >= 0) dir[a+1] = 0;
		else {
			strcpy(dir,"\\");
			break;
		}
	}
#else
	while(BLI_exist(dir) == 0){
		a --;
		while(dir[a] != '/'){
			a--;
			if (a <= 0) break;
		}
		if (a >= 0) dir[a+1] = 0;
		else {
			strcpy(dir,"/");
			break;
		}
	}
#endif
}

void BLI_make_existing_file(char *name)
{
	char di[FILE_MAXDIR], fi[FILE_MAXFILE];
	
	strcpy(di, name);
	BLI_splitdirstring(di, fi);
	
	/* test exist */
	if (BLI_exists(di) == 0) {
		BLI_recurdir_fileops(di);
	}
}


void BLI_make_file_string(const char *relabase, char *string,  const char *dir, const char *file)
{

	if (!string || !dir || !file) return; /* We don't want any NULLs */
	
	string[0]= 0; /* ton */

	/* we first push all slashes into unix mode, just to make sure we don't get
	   any mess with slashes later on. -jesterKing */
	/* constant strings can be passed for those parameters - don't change them - elubie */
	/*
	BLI_char_switch(relabase, '\\', '/');
	BLI_char_switch(dir, '\\', '/');
	BLI_char_switch(file, '\\', '/');
	*/

	/* Resolve relative references */	
	if (relabase && dir[0] == '/' && dir[1] == '/') {
		char *lslash;
		
		/* Get the file name, chop everything past the last slash (ie. the filename) */
		strcpy(string, relabase);
		
		lslash= (strrchr(string, '/')>strrchr(string, '\\'))?strrchr(string, '/'):strrchr(string, '\\');
		
		if(lslash) *(lslash+1)= 0;

		dir+=2; /* Skip over the relative reference */
	}
	
	strcat(string, dir);

	/* Make sure string ends in one (and only one) slash */
	if (string[strlen(string)-1] != '/' && string[strlen(string)-1] != '\\')
		strcat(string, "/");
	
	while (*file && (*file == '/' || *file == '\\')) /* Trim slashes from the front of file */
		file++;
		
	strcat (string, file);
	
	/* Push all slashes to the system preferred direction */
	BLI_clean(string);
}

int BLI_testextensie(char *str, char *ext)
{
	short a, b;
	int retval;

	a= strlen(str);
	b= strlen(ext);

	if(a==0 || b==0 || b>=a) {
		retval = 0;
	} else if (BLI_strcasecmp(ext, str + a - b)) {
		retval = 0;	
	} else {
		retval = 1;
	}

	return (retval);
}



void BLI_split_dirfile(char *string, char *dir, char *file)
{
	int a;
	
	dir[0]= 0;
	file[0]= 0;

#ifdef WIN32
	BLI_char_switch(string, '/', '\\'); /* make sure we have a valid path format */

	if (strlen(string)) {
		int len;
		if (string[0] == '/' || string[0] == '\\') { 
			BLI_strncpy(dir, string, FILE_MAXDIR);
		} else if (string[1] == ':' && string[2] == '\\') {
            BLI_strncpy(dir, string, FILE_MAXDIR);
        } else {
            BLI_getwdN(dir);
            strcat(dir,"/");
            strcat(dir,string);
            BLI_strncpy(string,dir,FILE_MAXDIR+FILE_MAXFILE);
        }

        BLI_make_exist(dir);

        // BLI_exist doesn't recognize a slashed dirname as a dir
        //  check if a trailing slash exists, and remove it. Do not do this
        //  when we are already at root. -jesterKing
        a = strlen(dir);
        if(a>=4 && dir[a-1]=='\\') dir[a-1] = 0;

        if (S_ISDIR(BLI_exist(dir))) {

	    /* copy from end of string into file, to ensure filename itself isn't truncated 
	       if string is too long. (aphex) */

	    len = FILE_MAXFILE - strlen(string);

	    if (len < 0)
		BLI_strncpy(file,string + abs(len),FILE_MAXFILE);
	    else
		BLI_strncpy(file,string,FILE_MAXFILE);
	    
            if (strrchr(string,'\\')){
		BLI_strncpy(file,strrchr(string,'\\')+1,FILE_MAXFILE);
	    }

            if (a = strlen(dir)) {
                if (dir[a-1] != '\\') strcat(dir,"\\");
            }
		}
		else {
			a = strlen(dir) - 1;
			while(a>0 && dir[a] != '\\') a--;
			dir[a + 1] = 0;
			BLI_strncpy(file, string + strlen(dir),FILE_MAXFILE);
		}
		
	}
	else {
		strcpy(dir, "\\");
		file[0]=0;
	}
#else
	if (strlen(string)) {
		if (string[0] == '/') { 
			strcpy(dir, string);
		} else if (string[1] == ':' && string[2] == '\\') {
			string+=2;
			strcpy(dir, string);
		} else {
			BLI_getwdN(dir);
			strcat(dir,"/");
			strcat(dir,string);
			strcpy(string,dir);
		}

		BLI_make_exist(dir);
			
		if (S_ISDIR(BLI_exist(dir))) {
			strcpy(file,string + strlen(dir));

			if (strrchr(file,'/')) strcpy(file,strrchr(file,'/')+1);
		
			if ( (a = strlen(dir)) ) {
				if (dir[a-1] != '/') strcat(dir,"/");
			}
		}
		else {
			a = strlen(dir) - 1;
			while(dir[a] != '/') a--;
			dir[a + 1] = 0;
			strcpy(file, string + strlen(dir));
		}
	}
	else {
		BLI_getwdN(dir);
		strcat(dir, "/");
		file[0] = 0;
	}
#endif
}
 
static int add_win32_extension(char *name)
{
	int retval = 0;
	int type;

	type = BLI_exist(name);
	if ((type == 0) || S_ISDIR(type)) {
#ifdef _WIN32
		char filename[FILE_MAXDIR+FILE_MAXFILE];
		char ext[FILE_MAXDIR+FILE_MAXFILE];
		char *extensions = getenv("PATHEXT");
		if (extensions) {
			char *temp;
			do {
				strcpy(filename, name);
				temp = strstr(extensions, ";");
				if (temp) {
					strncpy(ext, extensions, temp - extensions);
					ext[temp - extensions] = 0;
					extensions = temp + 1;
					strcat(filename, ext);
				} else {
					strcat(filename, extensions);
				}

				type = BLI_exist(filename);
				if (type && (! S_ISDIR(type))) {
					retval = 1;
					strcpy(name, filename);
					break;
				}
			} while (temp);
		}
#endif
	} else {
		retval = 1;
	}

	return (retval);
}

void BLI_where_am_i(char *fullname, char *name)
{
	char filename[FILE_MAXDIR+FILE_MAXFILE];
	char *path, *temp;
	int len;
#ifdef _WIN32
	char *seperator = ";";
	char *slash = "\\";
#else
	char *seperator = ":";
	char *slash = "/";
#endif

	if (name && fullname && strlen(name)) {
		strcpy(fullname, name);
		if (name[0] == '.') {
			// relative path, prepend cwd
			BLI_getwdN(fullname);
			len = strlen(fullname);
			if (len && fullname[len -1] != slash[0]) {
				strcat(fullname, slash);
			}
			strcat(fullname, name);
			add_win32_extension(fullname);
		} else if (BLI_last_slash(name)) {
			// full path
			strcpy(fullname, name);
			add_win32_extension(fullname);
		} else {
			// search for binary in $PATH
			path = getenv("PATH");
			if (path) {
				do {
					temp = strstr(path, seperator);
					if (temp) {
						strncpy(filename, path, temp - path);
						filename[temp - path] = 0;
						path = temp + 1;
					} else {
						strncpy(filename, path, sizeof(filename));
					}
					len = strlen(filename);
					if (len && filename[len - 1] != slash[0]) {
						strcat(filename, slash);
					}
					strcat(filename, name);
					if (add_win32_extension(filename)) {
						strcpy(fullname, filename);
						break;
					}
				} while (temp);
			}
		}
#ifndef NDEBUG
		if (strcmp(name, fullname)) {
			printf("guessing '%s' == '%s'\n", name, fullname);
		}
#endif

#ifdef _WIN32
		// in windows change long filename to short filename because
		// win2k doesn't know how to parse a commandline with lots of
		// spaces and double-quotes. There's another solution to this
		// with spawnv(P_WAIT, bprogname, argv) instead of system() but
		// that's even uglier
		GetShortPathName(fullname, fullname, FILE_MAXDIR+FILE_MAXFILE);
#ifndef NDEBUG
		printf("Shortname = '%s'\n", fullname);
#endif
#endif
	}
}

/* 
 * returns absolute path to the app bundle
 * only useful on OS X 
 */
#ifdef __APPLE__
char* BLI_getbundle(void) {
	CFURLRef bundleURL;
	CFStringRef pathStr;
	static char path[MAXPATHLEN];
	CFBundleRef mainBundle = CFBundleGetMainBundle();

	bundleURL = CFBundleCopyBundleURL(mainBundle);
	pathStr = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle);
	CFStringGetCString(pathStr, path, MAXPATHLEN, kCFStringEncodingASCII);
	return path;
}
#endif

int BLI_strcasecmp(const char *s1, const char *s2) {
	int i;

	for (i=0; ; i++) {
		char c1 = tolower(s1[i]);
		char c2 = tolower(s2[i]);

		if (c1<c2) {
			return -1;
		} else if (c1>c2) {
			return 1;
		} else if (c1==0) {
			break;
		}
	}

	return 0;
}

int BLI_strncasecmp(const char *s1, const char *s2, int n) {
	int i;

	for (i=0; i<n; i++) {
		char c1 = tolower(s1[i]);
		char c2 = tolower(s2[i]);

		if (c1<c2) {
			return -1;
		} else if (c1>c2) {
			return 1;
		} else if (c1==0) {
			break;
		}
	}

	return 0;
}

void BLI_timestr(double time, char *str)
{
	/* format 00:00:00.00 (hr:min:sec) string has to be 12 long */
	int  hr= (int)      time/(60*60);
	int min= (int) fmod(time/60, 60.0);
	int sec= (int) fmod(time, 60.0);
	int hun= (int) fmod(time*100.0, 100.0);
	
	if (hr) {
		sprintf(str, "%.2d:%.2d:%.2d.%.2d",hr,min,sec,hun);
	} else {
		sprintf(str, "%.2d:%.2d.%.2d",min,sec,hun);
	}
	
	str[11]=0;
}


