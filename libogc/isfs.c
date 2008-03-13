#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include <ipc.h>

#include "isfs.h"

#define ISFS_STRUCTSIZE				(sizeof(struct isfs_cb))
#define ISFS_HEAPSIZE				(ISFS_STRUCTSIZE<<4)

#define ISFS_FUNCNULL				0
#define ISFS_FUNCGETSTATS			1
#define ISFS_FUNCREADIR				2

#define ISFS_IOCTL_FORMAT			1
#define ISFS_IOCTL_GETSTATS			2
#define ISFS_IOCTL_CREATEDIR		3
#define ISFS_IOCTL_READDIR			4
#define ISFS_IOCTL_SETATTR			5
#define ISFS_IOCTL_GETATTR			6
#define ISFS_IOCTL_DELETE			7
#define ISFS_IOCTL_CREATEFILE		9
#define ISFS_IOCTL_GETFILESTATS		11

struct isfs_cb
{
	union {
		struct {
			char filepath[ISFS_MAXPATH];
		} open,delete;
		struct {
			char pad0[6];
			char filepath[ISFS_MAXPATH];
			u8 ownerperm;
			u8 groupperm;
			u8 otherperm;
			u8 attributes;
			u8 pad1[2];
		} fscreate;
		struct
		{
			ioctlv vector[4];
			char filepath[ISFS_MAXPATH];
			u32 no_entries;
		} fsreaddir;
		struct {
			u32	a;
			u32	b;
			u32	c;
			u32	d;
			u32	e;
			u32	f;
			u32	g;
		} fsstats;
	};

	isfscallback cb;
	void *usrdata;
	u32 functype;
	void *funcargp;
};


static s32 hId = -1;

static s32 _fs_fd = -1;
static u32 _fs_initialized = 0;
static char _dev_fs[] ATTRIBUTE_ALIGN(32) = "/dev/fs";

static s32 __isfsGetStatsCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	if(result==0) memcpy(param->funcargp,&param->fsstats,sizeof(param->fsstats));
	return result;
}

static s32 __isfsReadDirCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	if(result==0) *(u32*)param->funcargp = param->fsreaddir.no_entries;
	return result;
}

static s32 __isfsFunctionCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	
	if(result>=0) {
		if(param->functype==ISFS_FUNCGETSTATS) __isfsGetStatsCB(result,usrdata);
		else if(param->functype==ISFS_FUNCREADIR) __isfsReadDirCB(result,usrdata);
	}
	if(param->cb!=NULL) param->cb(result,param->usrdata);
	
	iosFree(hId,param);
	return result;
}

s32 ISFS_Initialize()
{
	s32 ret = IPC_OK;

	_fs_fd = IOS_Open(_dev_fs,0);
	if(_fs_fd<0) return _fs_fd;

	if(_fs_initialized==0) {
		hId = iosCreateHeap(ISFS_HEAPSIZE);
		if(hId<0) return IPC_ENOMEM;

		_fs_initialized = 1;
	}
	return ret;
}

s32 ISFS_Deinitialize()
{
	if(_fs_fd<0) return ISFS_EINVAL;

	IOS_Close(_fs_fd);
	_fs_fd = -1;

	iosDestroyHeap(hId);

	return 0;
}

s32 ISFS_ReadDir(const char *filepath,char *name_list,u32 *num)
{
	s32 ret;
	s32 len,cnt;
	s32 ilen,olen;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || num==NULL) return ISFS_EINVAL;
	if(name_list!=NULL && ((u32)name_list%32)!=0) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fsreaddir.filepath,filepath,(len+1));

	param->fsreaddir.vector[0].data = param->fsreaddir.filepath;
	param->fsreaddir.vector[0].len = ISFS_MAXPATH;
	param->fsreaddir.vector[1].data = &param->fsreaddir.no_entries;
	param->fsreaddir.vector[1].len = sizeof(u32);

	if(name_list!=NULL) {
		ilen = olen = 2;
		cnt = *num;
		param->fsreaddir.no_entries = cnt;
		param->fsreaddir.vector[2].data = name_list;
		param->fsreaddir.vector[2].len = (cnt*13);
		param->fsreaddir.vector[3].data = &param->fsreaddir.no_entries;
		param->fsreaddir.vector[3].len = sizeof(u32);
	} else
		ilen = olen = 1;

	ret = IOS_Ioctlv(_fs_fd,ISFS_IOCTL_READDIR,ilen,olen,param->fsreaddir.vector);
	if(ret==0) *num = param->fsreaddir.no_entries;

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_ReadDirAsync(const char *filepath,char *name_list,u32 *num,isfscallback cb,void *usrdata)
{
	s32 len,cnt;
	s32 ilen,olen;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || num==NULL) return ISFS_EINVAL;
	if(name_list!=NULL && ((u32)name_list%32)!=0) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->funcargp = num;
	param->functype = ISFS_FUNCREADIR;
	memcpy(param->fsreaddir.filepath,filepath,(len+1));

	param->fsreaddir.vector[0].data = param->fsreaddir.filepath;
	param->fsreaddir.vector[0].len = ISFS_MAXPATH;
	param->fsreaddir.vector[1].data = &param->fsreaddir.no_entries;
	param->fsreaddir.vector[1].len = sizeof(u32);

	if(name_list!=NULL) {
		ilen = olen = 2;
		cnt = *num;
		param->fsreaddir.no_entries = cnt;
		param->fsreaddir.vector[2].data = name_list;
		param->fsreaddir.vector[2].len = (cnt*13);
		param->fsreaddir.vector[3].data = &param->fsreaddir.no_entries;
		param->fsreaddir.vector[3].len = sizeof(u32);
	} else
		ilen = olen = 1;

	return IOS_IoctlvAsync(_fs_fd,ISFS_IOCTL_READDIR,ilen,olen,param->fsreaddir.vector,__isfsFunctionCB,param);
}

s32 ISFS_CreateDir(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fscreate.filepath,filepath,(len+1));
	
	param->fscreate.attributes = attributes;
	param->fscreate.ownerperm = owner_perm;
	param->fscreate.groupperm = group_perm;
	param->fscreate.otherperm = other_perm;
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_CREATEDIR,&param->fscreate,76,NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_CreateDirAsync(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->fscreate.filepath,filepath,(len+1));
	
	param->fscreate.attributes = attributes;
	param->fscreate.ownerperm = owner_perm;
	param->fscreate.groupperm = group_perm;
	param->fscreate.otherperm = other_perm;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_CREATEDIR,&param->fscreate,76,NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_Open(const char *filepath,u8 mode)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->open.filepath,filepath,(len+1));
	ret = IOS_Open(param->open.filepath,mode);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_OpenAsync(const char *filepath,u8 mode,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->open.filepath,filepath,(len+1));
	return IOS_OpenAsync(param->open.filepath,mode,__isfsFunctionCB,param);
}

s32 ISFS_Format()
{
	if(_fs_fd<0) return ISFS_EINVAL;

	return IOS_Ioctl(_fs_fd,ISFS_IOCTL_FORMAT,NULL,0,NULL,0);
}

s32 ISFS_FormatAsync(isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(_fs_fd<0) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;
	
	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_FORMAT,NULL,0,NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_GetStats(void *stats)
{
	s32 ret = ISFS_OK;
	struct isfs_cb *param;

	if(_fs_fd<0 || stats==NULL) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_GETSTATS,NULL,0,&param->fsstats,sizeof(param->fsstats));
	if(ret==IPC_OK) memcpy(stats,&param->fsstats,sizeof(param->fsstats));
	
	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_GetStatsAsync(void *stats,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(_fs_fd<0 || stats==NULL) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCGETSTATS;
	param->funcargp = stats;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_GETSTATS,NULL,0,&param->fsstats,sizeof(param->fsstats),__isfsFunctionCB,param);
}

s32 ISFS_Write(s32 fd,const void *buffer,u32 length)
{
	if(length<=0 || buffer==NULL || ((u32)buffer%32)!=0) return ISFS_EINVAL;
	
	return IOS_Write(fd,buffer,length);
}

s32 ISFS_WriteAsync(s32 fd,const void *buffer,u32 length,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(length<=0 || buffer==NULL || ((u32)buffer%32)!=0) return ISFS_EINVAL;
	
	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_WriteAsync(fd,buffer,length,__isfsFunctionCB,param);
}

s32 ISFS_Read(s32 fd,void *buffer,u32 length)
{
	if(length<=0 || buffer==NULL || ((u32)buffer%32)!=0) return ISFS_EINVAL;
	
	return IOS_Read(fd,buffer,length);
}

s32 ISFS_ReadAsync(s32 fd,void *buffer,u32 length,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(length<=0 || buffer==NULL || ((u32)buffer%32)!=0) return ISFS_EINVAL;
	
	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_ReadAsync(fd,buffer,length,__isfsFunctionCB,param);
}

s32 ISFS_Seek(s32 fd,s32 where,s32 whence)
{
	return IOS_Seek(fd,where,whence);
}

s32 ISFS_SeekAsync(s32 fd,s32 where,s32 whence,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;
	
	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_SeekAsync(fd,where,whence,__isfsFunctionCB,param);
}

s32 ISFS_CreateFile(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fscreate.filepath,filepath,(len+1));
	
	param->fscreate.attributes = attributes;
	param->fscreate.ownerperm = owner_perm;
	param->fscreate.groupperm = group_perm;
	param->fscreate.otherperm = other_perm;
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_CREATEFILE,&param->fscreate,sizeof(param->fscreate),NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_CreateFileAsync(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->fscreate.filepath,filepath,(len+1));
	
	param->fscreate.attributes = attributes;
	param->fscreate.ownerperm = owner_perm;
	param->fscreate.groupperm = group_perm;
	param->fscreate.otherperm = other_perm;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_CREATEFILE,&param->fscreate,sizeof(param->fscreate),NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_Delete(const char *filepath)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->delete.filepath,filepath,(len+1));
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_DELETE,param->delete.filepath,ISFS_MAXPATH,NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_DeleteAsync(const char *filepath,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->delete.filepath,filepath,(len+1));
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_DELETE,param->delete.filepath,ISFS_MAXPATH,NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_Close(s32 fd)
{
	if(fd<0) return 0;

	return IOS_Close(fd);
}

s32 ISFS_CloseAsync(s32 fd,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(fd<0) return 0;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_CloseAsync(fd,__isfsFunctionCB,param);
}

s32 ISFS_GetFileStats(s32 fd,fstats *status)
{
	if(status==NULL || ((u32)status%32)!=0) return ISFS_EINVAL;

	return IOS_Ioctl(fd,ISFS_IOCTL_GETFILESTATS,NULL,0,status,sizeof(fstats));
}

s32 ISFS_GetFileStatsAsync(s32 fd,fstats *status,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(status==NULL || ((u32)status%32)!=0) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_IoctlAsync(fd,ISFS_IOCTL_GETFILESTATS,NULL,0,status,sizeof(fstats),__isfsFunctionCB,param);
}