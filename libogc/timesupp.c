#include <config.h>
#include <_ansi.h>
#include <_syslist.h>

#include "asm.h"
#include "processor.h"
#include "lwp.h"
#include "lwp_threadq.h"
#include "timesupp.h"
#include "exi.h"
#include "system.h"
#include "conf.h"

#include <stdio.h>

/* time variables */
static u32 exi_wait_inited = 0;
static lwpq_t time_exi_wait;

extern u32 __SYS_GetRTC(u32 *gctime);
extern syssram* __SYS_LockSram();
extern u32 __SYS_UnlockSram(u32 write);

unsigned long _DEFUN(gettick,(),
						  _NOARGS)
{
	__asm__ __volatile__ (
		"1:	mftb	3\n\
		    blr"
			: : : "memory");
	return 0;
}

long long _DEFUN(gettime,(),
						  _NOARGS)
{
	__asm__ __volatile__ (
		"1:	mftbu	3\n\
		    mftb	4\n\
		    mftbu	5\n\
			cmpw	3,5\n\
			bne		1b\n\
			blr"
			: : : "memory");
	return 0;
}

void _DEFUN(settime,(t),
			long long t)
{
	__asm__ __volatile__ (
		"li		5,0\n\
		 mttbl  5\n\
		 mttbu  3\n\
		 mttbl  4\n\
	     blr"
		 : : : "memory");
}

u32 diff_sec(long long start,long long end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_secs(diff);
}

u32 diff_msec(long long start,long long end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_millisecs(diff);
}

u32 diff_usec(long long start,long long end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_microsecs(diff);
}

u32 diff_nsec(long long start,long long end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_nanosecs(diff);
}

void __timesystem_init()
{
	if(!exi_wait_inited) {
		exi_wait_inited = 1;
		LWP_InitQueue(&time_exi_wait);
	}
}

void timespec_substract(const struct timespec *tp_start,const struct timespec *tp_end,struct timespec *result)
{
	struct timespec start_st = *tp_start;
	struct timespec *start = &start_st;
	u32 nsecpersec = TB_NSPERSEC;

	if(tp_end->tv_nsec<start->tv_nsec) {
		int secs = (start->tv_nsec - tp_end->tv_nsec)/nsecpersec+1;
		start->tv_nsec -= nsecpersec * secs;
		start->tv_sec += secs;
	}
	if((tp_end->tv_nsec - start->tv_nsec)>nsecpersec) {
		int secs = (start->tv_nsec - tp_end->tv_nsec)/nsecpersec;
		start->tv_nsec += nsecpersec * secs;
		start->tv_sec -= secs;
	}

	result->tv_sec = (tp_end->tv_sec - start->tv_sec);
	result->tv_nsec = (tp_end->tv_nsec - start->tv_nsec);
}

unsigned long long timespec_to_ticks(const struct timespec *tp)
{
	return __lwp_wd_calc_ticks(tp);
}

int clock_gettime(struct timespec *tp)
{
	u32 gctime;
#if defined(HW_RVL)
	u32 wii_bias = 0;
#endif

	if(!tp) return -1;

	if(!__SYS_GetRTC(&gctime)) return -1;

#if defined(HW_DOL)
	syssram* sram = __SYS_LockSram();
	gctime += sram->counter_bias;
	__SYS_UnlockSram(0);
#else
	if(CONF_GetCounterBias(&wii_bias)>=0) gctime += wii_bias;
#endif
	gctime += 946684800;

	tp->tv_sec = gctime;
	tp->tv_nsec = ticks_to_nanosecs(gettick());

	return 0;
}

// this function spins till timeout is reached
void _DEFUN(udelay,(us),
			int us)
{
	long long start, end;
	start = gettime();
	while (1)
	{
		end = gettime();
		if (diff_usec(start,end) >= us)
			break;
	}
}

unsigned int _DEFUN(nanosleep,(tb),
           struct timespec *tb)
{
	u64 timeout;

	__lwp_thread_dispatchdisable();

	timeout = __lwp_wd_calc_ticks(tb);
	__lwp_thread_setstate(_thr_executing,LWP_STATES_DELAYING|LWP_STATES_INTERRUPTIBLE_BY_SIGNAL);
	__lwp_wd_initialize(&_thr_executing->timer,__lwp_thread_delayended,_thr_executing->object.id,_thr_executing);
	__lwp_wd_insert_ticks(&_thr_executing->timer,timeout);

	__lwp_thread_dispatchenable();
	return TB_SUCCESSFUL;
}

static u32 __getrtc(u32 *gctime)
{
	u32 ret;
	u32 cmd;
	u32 time;

	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		return 0;
	}

	ret = 0;
	time = 0;
	cmd = 0x20000000;
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_Imm(EXI_CHANNEL_0,&time,4,EXI_READ,NULL)==0) ret |= 0x04;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;

	*gctime = time;
	if(ret) return 0;

	return 1;
}

static s32 __time_exi_unlock(s32 chn,s32 dev)
{
	LWP_ThreadBroadcast(time_exi_wait);
	return 1;
}

static void __time_exi_wait()
{
	u32 ret;

	do {
		if((ret=EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,__time_exi_unlock))==1) break;
		LWP_ThreadSleep(time_exi_wait);
	}while(ret==0);
}

static u32 __getRTC(u32 *gctime)
{
	u32 cnt,ret;
	u32 time1,time2;

	__time_exi_wait();

	cnt = 0;
	ret = 0;
	while(cnt<16) {
		if(__getrtc(&time1)==0
			|| __getrtc(&time2)==0) {
			EXI_Unlock(EXI_CHANNEL_0);
			break;
		}
		if(time1==time2) {
			*gctime = time1;
			EXI_Unlock(EXI_CHANNEL_0);
			return 1;
		}
		cnt++;
	}
	return 0;
}

time_t _DEFUN(time,(timer),
			  time_t *timer)
{
	time_t gctime = 0;
#if defined(HW_RVL)
	u32 wii_bias = 0;
#endif

	if(__getRTC((u32*)&gctime)==0) return (time_t)0;

#if defined(HW_DOL)
	syssram* sram = __SYS_LockSram();
	gctime += sram->counter_bias;
	__SYS_UnlockSram(0);
#else
	if(CONF_GetCounterBias(&wii_bias)>=0) gctime += wii_bias;
#endif

	gctime += 946684800;

	if(timer) *timer = gctime;
	return gctime;
}

unsigned int _DEFUN(sleep,(s),
		   unsigned int s)
{
	struct timespec tb;

	tb.tv_sec = s;
	tb.tv_nsec = 0;
	return nanosleep(&tb);
}

unsigned int _DEFUN(usleep,(us),
           unsigned int us)
{
	struct timespec tb;

	tb.tv_sec = 0;
	tb.tv_nsec = us*TB_NSPERUS;
	return nanosleep(&tb);
}

clock_t clock(void) {
	return -1;
}

