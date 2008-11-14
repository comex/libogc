
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asm.h"
#include "processor.h"
#include "bte.h"
#include "conf.h"
#include "ir.h"
#include "dynamics.h"
#include "guitar_hero_3.h"
#include "wiiuse_internal.h"
#include "wiiuse/wpad.h"
#include "lwp_threads.h"
#include "ogcsys.h"

#define EVENTQUEUE_LENGTH			16

#define DISCONNECT_BATTERY_DIED		0x14
#define DISCONNECT_POWER_OFF		0x15

struct _wpad_thresh{
	s32 btns;
	s32 ir;
	s32 js;
	s32 acc;
};

struct _wpad_cb {
	wiimote *wm;
	s32 data_fmt;
	s32 queue_head;
	s32 queue_tail;
	s32 queue_full;
	u32 queue_length;
	u32 dropped_events;
	s32 idle_time;
	struct _wpad_thresh thresh;

	WPADData lstate;
	WPADData *queue_ext;
	WPADData queue_int[EVENTQUEUE_LENGTH];
};

static syswd_t __wpad_timer;
static vu32 __wpads_inited = 0;
static vs32 __wpads_ponded = 0;
static u32 __wpad_idletimeout = 300;
static vu32 __wpads_active = 0;
static vs32 __wpads_registered = 0;
static wiimote **__wpads = NULL;
static WPADData wpaddata[WPAD_MAX_WIIMOTES];
static struct _wpad_cb __wpdcb[WPAD_MAX_WIIMOTES];
static conf_pad_device __wpad_devs[WPAD_MAX_WIIMOTES];
static struct linkkey_info __wpad_keys[WPAD_MAX_WIIMOTES];

static s32 __wpad_onreset(s32 final);
static s32 __wpad_disconnect(struct _wpad_cb *wpdcb);
static void __wpad_eventCB(struct wiimote_t *wm,s32 event);

static void __wpad_def_powcb(s32 chan);
static WPADShutdownCallback __wpad_batcb = NULL;
static WPADShutdownCallback __wpad_powcb = __wpad_def_powcb;

extern void __wiiuse_sensorbar_enable(int enable);
extern void __SYS_DoPowerCB(void);

static sys_resetinfo __wpad_resetinfo = {
	{},
	__wpad_onreset,
	127
};

static s32 __wpad_onreset(s32 final)
{
	//printf("__wpad_onreset(%d)\n",final);
	if(final==FALSE) {
		WPAD_Shutdown();
	}
	return 1;
}

static void __wpad_def_powcb(s32 chan)
{
	__SYS_DoPowerCB();
}

static void __wpad_timeouthandler(syswd_t alarm)
{
	s32 i;
	struct wiimote_t *wm = NULL;
	struct _wpad_cb *wpdcb = NULL;

	if(!__wpads_active) return;

	__lwp_thread_dispatchdisable();
	for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
		wpdcb = &__wpdcb[i];
		wm = wpdcb->wm;
		if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
			wpdcb->idle_time++;
			if(wpdcb->idle_time>=__wpad_idletimeout) {
				wpdcb->idle_time = 0;
				wiiuse_disconnect(wm);
			}
		}
	}
	__lwp_thread_dispatchunnest();
}

static void __wpad_setfmt(s32 chan)
{
	switch(__wpdcb[chan].data_fmt) {
		case WPAD_FMT_BTNS:
			wiiuse_set_flags(__wpads[chan], 0, WIIUSE_CONTINUOUS);
			wiiuse_motion_sensing(__wpads[chan],0);
			wiiuse_set_ir(__wpads[chan],0);
			break;
		case WPAD_FMT_BTNS_ACC:
			wiiuse_set_flags(__wpads[chan], WIIUSE_CONTINUOUS, 0);
			wiiuse_motion_sensing(__wpads[chan],1);
			wiiuse_set_ir(__wpads[chan],0);
			break;
		case WPAD_FMT_BTNS_ACC_IR:
			wiiuse_set_flags(__wpads[chan], WIIUSE_CONTINUOUS, 0);
			wiiuse_motion_sensing(__wpads[chan],1);
			wiiuse_set_ir(__wpads[chan],1);
			break;
		default:
			break;
	}
}

static s32 __wpad_init_finished(s32 result,void *usrdata)
{
	u32 i;
	struct bd_addr bdaddr;

	//printf("__wpad_init_finished(%d)\n",result);
	
	if(result==ERR_OK) {
		for(i=0;__wpads[i] && i<WPAD_MAX_WIIMOTES && i<__wpads_registered;i++) {
			BD_ADDR(&(bdaddr),__wpad_devs[i].bdaddr[5],__wpad_devs[i].bdaddr[4],__wpad_devs[i].bdaddr[3],__wpad_devs[i].bdaddr[2],__wpad_devs[i].bdaddr[1],__wpad_devs[i].bdaddr[0]);
			wiiuse_register(__wpads[i],&(bdaddr));
		}
		__wpads_inited = WPAD_STATE_ENABLED;
	}
	return ERR_OK;
}

static s32 __wpad_patch_finished(s32 result,void *usrdata)
{
	//printf("__wpad_patch_finished(%d)\n",result);
	BTE_InitSub(__wpad_init_finished);
	return ERR_OK;
}

static s32 __readlinkkey_finished(s32 result,void *usrdata)
{
	//printf("__readlinkkey_finished(%d)\n",result);

	__wpads_ponded = result;
	BTE_ApplyPatch(__wpad_patch_finished);

	return ERR_OK;
}

static s32 __initcore_finished(s32 result,void *usrdata)
{
	//printf("__initcore_finished(%d)\n",result);

	if(result==ERR_OK) {
		BTE_ReadStoredLinkKey(__wpad_keys,WPAD_MAX_WIIMOTES,__readlinkkey_finished);
	}
	return ERR_OK;
}

static s32 __wpad_disconnect(struct _wpad_cb *wpdcb)
{
	struct wiimote_t *wm;

	if(wpdcb==NULL) return 0;

	wm = wpdcb->wm;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		wiiuse_disconnect(wm);
	}

	return 0;
}

static void __wpad_calc_data(WPADData *data,WPADData *lstate,struct accel_t *accel_calib,u32 smoothed)
{
	if(data->err!=WPAD_ERR_NONE) return;

	data->orient = lstate->orient;

	data->ir.state = lstate->ir.state;
	data->ir.sensorbar = lstate->ir.sensorbar;
	data->ir.x = lstate->ir.x;
	data->ir.y = lstate->ir.y;
	data->ir.sx = lstate->ir.sx;
	data->ir.sy = lstate->ir.sy;
	data->ir.ax = lstate->ir.ax;
	data->ir.ay = lstate->ir.ay;
	data->ir.distance = lstate->ir.distance;
	data->ir.z = lstate->ir.z;
	data->ir.angle = lstate->ir.angle;
	data->ir.error_cnt = lstate->ir.error_cnt;
	data->ir.glitch_cnt = lstate->ir.glitch_cnt;

	data->btns_l = lstate->btns_h;
	if(data->data_present & WPAD_DATA_ACCEL) {
		calculate_orientation(accel_calib, &data->accel, &data->orient, smoothed);
		calculate_gforce(accel_calib, &data->accel, &data->gforce);
	}
	if(data->data_present & WPAD_DATA_IR) {
		interpret_ir_data(&data->ir,&data->orient);
	}
	if(data->data_present & WPAD_DATA_EXPANSION) {
		switch(data->exp.type) {
			case EXP_NUNCHUK:
			{
				struct nunchuk_t *nc = &data->exp.nunchuk;

				nc->orient = lstate->exp.nunchuk.orient;
				calc_joystick_state(&nc->js,nc->js.pos.x,nc->js.pos.y);
				calculate_orientation(&nc->accel_calib,&nc->accel,&nc->orient,smoothed);
				calculate_gforce(&nc->accel_calib,&nc->accel,&nc->gforce);
				data->btns_h |= (data->exp.nunchuk.btns<<16);
			}
			break;

			case EXP_CLASSIC:
			{
				struct classic_ctrl_t *cc = &data->exp.classic;

				cc->r_shoulder = ((f32)cc->rs_raw/0x1F);
				cc->l_shoulder = ((f32)cc->ls_raw/0x1F);
				calc_joystick_state(&cc->ljs, cc->ljs.pos.x, cc->ljs.pos.y);
				calc_joystick_state(&cc->rjs, cc->rjs.pos.x, cc->rjs.pos.y);
				data->btns_h |= (data->exp.classic.btns<<16);
			}
			break;

			case EXP_GUITAR_HERO_3:
			{
				struct guitar_hero_3_t *gh3 = &data->exp.gh3;

				gh3->whammy_bar = (gh3->wb_raw - GUITAR_HERO_3_WHAMMY_BAR_MIN) / (float)(GUITAR_HERO_3_WHAMMY_BAR_MAX - GUITAR_HERO_3_WHAMMY_BAR_MIN);
				calc_joystick_state(&gh3->js, gh3->js.pos.x, gh3->js.pos.y);
				data->btns_h |= (data->exp.gh3.btns<<16);
			}
			break;

			default:
				break;
		}
	}
	data->btns_d = data->btns_h & ~data->btns_l;
	data->btns_u = ~data->btns_h & data->btns_l;
	*lstate = *data;
}

static void __save_state(struct wiimote_t* wm) {
	/* wiimote */
	wm->lstate.btns = wm->btns;
	wm->lstate.accel = wm->accel;

	/* ir */
	wm->lstate.ir = wm->ir;

	/* expansion */
	switch (wm->exp.type) {
		case EXP_NUNCHUK:
			wm->lstate.exp.nunchuk = wm->exp.nunchuk;
			break;
		case EXP_CLASSIC:
			wm->lstate.exp.classic = wm->exp.classic;
			break;
		case EXP_GUITAR_HERO_3:
			wm->lstate.exp.gh3 = wm->exp.gh3;
			break;
	}
}

#define ABS(x)		((s32)(x)>0?(s32)(x):-((s32)(x)))

#define STATE_CHECK(thresh, a, b) \
	if(((thresh) > WPAD_THRESH_IGNORE) && (ABS((a)-(b)) > (thresh))) \
		state_changed = 1;

#define STATE_CHECK_SIMPLE(thresh, a, b) \
	if(((thresh) > WPAD_THRESH_IGNORE) && ((a) != (b))) \
		state_changed = 1;

static u32 __wpad_read_expansion(struct wiimote_t *wm,WPADData *data, struct _wpad_thresh *thresh)
{
	int state_changed = 0;
	switch(data->exp.type) {
		case EXP_NUNCHUK:
			data->exp.nunchuk = wm->exp.nunchuk;
			STATE_CHECK_SIMPLE(thresh->btns, wm->exp.nunchuk.btns, wm->lstate.exp.nunchuk.btns);
			STATE_CHECK(thresh->acc, wm->exp.nunchuk.accel.x, wm->lstate.exp.nunchuk.accel.x);
			STATE_CHECK(thresh->acc, wm->exp.nunchuk.accel.y, wm->lstate.exp.nunchuk.accel.y);
			STATE_CHECK(thresh->acc, wm->exp.nunchuk.accel.z, wm->lstate.exp.nunchuk.accel.z);
			STATE_CHECK(thresh->js, wm->exp.nunchuk.js.pos.x, wm->lstate.exp.nunchuk.js.pos.x);
			STATE_CHECK(thresh->js, wm->exp.nunchuk.js.pos.y, wm->lstate.exp.nunchuk.js.pos.y);
			break;
		case EXP_CLASSIC:
			data->exp.classic = wm->exp.classic;
			STATE_CHECK_SIMPLE(thresh->btns, wm->exp.classic.btns, wm->lstate.exp.classic.btns);
			STATE_CHECK(thresh->js, wm->exp.classic.rs_raw, wm->lstate.exp.classic.rs_raw);
			STATE_CHECK(thresh->js, wm->exp.classic.ls_raw, wm->lstate.exp.classic.ls_raw);
			STATE_CHECK(thresh->js, wm->exp.classic.ljs.pos.x, wm->lstate.exp.classic.ljs.pos.x);
			STATE_CHECK(thresh->js, wm->exp.classic.ljs.pos.y, wm->lstate.exp.classic.ljs.pos.y);
			STATE_CHECK(thresh->js, wm->exp.classic.rjs.pos.x, wm->lstate.exp.classic.rjs.pos.x);
			STATE_CHECK(thresh->js, wm->exp.classic.rjs.pos.y, wm->lstate.exp.classic.rjs.pos.y);
			break;
		case EXP_GUITAR_HERO_3:
			data->exp.gh3 = wm->exp.gh3;
			STATE_CHECK_SIMPLE(thresh->btns, wm->exp.gh3.btns, wm->lstate.exp.gh3.btns);
			STATE_CHECK(thresh->js, wm->exp.gh3.wb_raw, wm->lstate.exp.gh3.wb_raw);
			STATE_CHECK(thresh->js, wm->exp.gh3.js.pos.x, wm->lstate.exp.gh3.js.pos.x);
			STATE_CHECK(thresh->js, wm->exp.gh3.js.pos.y, wm->lstate.exp.gh3.js.pos.y);
			break;
	}
	return state_changed;
}

static void __wpad_read_wiimote(struct wiimote_t *wm, WPADData *data, s32 *idle_time, struct _wpad_thresh *thresh)
{
	int i;
	int state_changed = 0;
	data->err = WPAD_ERR_TRANSFER;
	data->data_present = 0;
	data->exp.type = wm->exp.type;
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			switch(wm->event_buf[0]) {
				case WM_RPT_BTN:
				case WM_RPT_BTN_ACC:
				case WM_RPT_BTN_ACC_IR:
				case WM_RPT_BTN_EXP:
				case WM_RPT_BTN_ACC_EXP:
				case WM_RPT_BTN_IR_EXP:
				case WM_RPT_BTN_ACC_IR_EXP:
					data->btns_h = (wm->btns&0xffff);
					data->data_present |= WPAD_DATA_BUTTONS;
					STATE_CHECK_SIMPLE(thresh->btns, wm->btns, wm->lstate.btns);
			}
			switch(wm->event_buf[0]) {
				case WM_RPT_BTN_ACC:
				case WM_RPT_BTN_ACC_IR:
				case WM_RPT_BTN_ACC_EXP:
				case WM_RPT_BTN_ACC_IR_EXP:
					data->accel = wm->accel;
					data->data_present |= WPAD_DATA_ACCEL;
					STATE_CHECK(thresh->acc, wm->accel.x, wm->lstate.accel.x);
					STATE_CHECK(thresh->acc, wm->accel.y, wm->lstate.accel.y);
					STATE_CHECK(thresh->acc, wm->accel.z, wm->lstate.accel.z);
			}
			switch(wm->event_buf[0]) {
				//IR requires acceleration
				//case WM_RPT_BTN_IR_EXP:
				case WM_RPT_BTN_ACC_IR:
				case WM_RPT_BTN_ACC_IR_EXP:
					data->ir = wm->ir;
					data->data_present |= WPAD_DATA_IR;
					for(i=0; i<WPAD_MAX_IR_DOTS; i++) {
						STATE_CHECK_SIMPLE(thresh->ir, wm->ir.dot[i].visible, wm->lstate.ir.dot[i].visible);
						STATE_CHECK(thresh->ir, wm->ir.dot[i].rx, wm->lstate.ir.dot[i].rx);
						STATE_CHECK(thresh->ir, wm->ir.dot[i].ry, wm->lstate.ir.dot[i].ry);
					}
			}
			switch(wm->event_buf[0]) {
				case WM_RPT_BTN_EXP:
				case WM_RPT_BTN_ACC_EXP:
				case WM_RPT_BTN_IR_EXP:
				case WM_RPT_BTN_ACC_IR_EXP:
					state_changed |= __wpad_read_expansion(wm,data,thresh);
					data->data_present |= WPAD_DATA_EXPANSION;
			}
			data->err = WPAD_ERR_NONE;
			if(state_changed) {
				*idle_time = 0;
				__save_state(wm);
			}
		} else
			data->err = WPAD_ERR_NOT_READY;
	} else
		data->err = WPAD_ERR_NO_CONTROLLER;
}


static void __wpad_eventCB(struct wiimote_t *wm,s32 event)
{
	s32 chan;
	u32 maxbufs;
	WPADData *wpadd = NULL;
	struct _wpad_cb *wpdcb = NULL;

	switch(event) {
		case WIIUSE_EVENT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			
			if(wpdcb->queue_ext!=NULL) {
				maxbufs = wpdcb->queue_length;
				wpadd = &(wpdcb->queue_ext[wpdcb->queue_tail]);
			} else {
				maxbufs = EVENTQUEUE_LENGTH;
				wpadd = &(wpdcb->queue_int[wpdcb->queue_tail]);
			}
			if(wpdcb->queue_full == maxbufs) {
				wpdcb->queue_head++;
				wpdcb->queue_head %= maxbufs;
				wpdcb->dropped_events++;
			} else {
				wpdcb->queue_full++;
			}

			__wpad_read_wiimote(wm, wpadd, &wpdcb->idle_time, &wpdcb->thresh);

			wpdcb->queue_tail++;
			wpdcb->queue_tail %= maxbufs;

			break;
		case WIIUSE_STATUS:
			break;
		case WIIUSE_CONNECT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			wpdcb->wm = wm;
			wpdcb->queue_head = 0;
			wpdcb->queue_tail = 0;
			wpdcb->queue_full = 0;
			wpdcb->idle_time = 0;
			memset(&wpdcb->lstate,0,sizeof(WPADData));
			memset(&wpaddata[chan],0,sizeof(WPADData));
			memset(wpdcb->queue_int,0,(sizeof(WPADData)*EVENTQUEUE_LENGTH));
			wiiuse_set_ir_position(wm,(CONF_GetSensorBarPosition()^1));
			wiiuse_set_ir_sensitivity(wm,CONF_GetIRSensitivity());
			wiiuse_set_leds(wm,(WIIMOTE_LED_1<<chan),NULL);
			__wpad_setfmt(chan);
			__wpads_active |= (0x01<<chan);
			break;
		case WIIUSE_DISCONNECT:
			chan = wm->unid;
			wpdcb = &__wpdcb[chan];
			wpdcb->wm = wm;
			wpdcb->queue_head = 0;
			wpdcb->queue_tail = 0;
			wpdcb->queue_full = 0;
			wpdcb->queue_length = 0;
			wpdcb->queue_ext = NULL;
			wpdcb->idle_time = -1;
			memset(&wpdcb->lstate,0,sizeof(WPADData));
			memset(&wpaddata[chan],0,sizeof(WPADData));
			memset(wpdcb->queue_int,0,(sizeof(WPADData)*EVENTQUEUE_LENGTH));
			__wpads_active &= ~(0x01<<chan);
			break;
		default:
			break;
	}
}

void __wpad_disconnectCB(struct bd_addr *offaddr, u8 reason)
{
	struct bd_addr bdaddr;
	int i;

	if(__wpads_inited == WPAD_STATE_ENABLED) {
		for(i=0;__wpads[i] && i<WPAD_MAX_WIIMOTES && i<__wpads_registered;i++) {
			BD_ADDR(&(bdaddr),__wpad_devs[i].bdaddr[5],__wpad_devs[i].bdaddr[4],__wpad_devs[i].bdaddr[3],__wpad_devs[i].bdaddr[2],__wpad_devs[i].bdaddr[1],__wpad_devs[i].bdaddr[0]);
			if(bd_addr_cmp(offaddr,&bdaddr)) {
				if(reason == DISCONNECT_BATTERY_DIED) {
					if(__wpad_batcb) __wpad_batcb(i);		//sanity check since this pointer can be NULL.
				} else if(reason == DISCONNECT_POWER_OFF)
					__wpad_powcb(i);						//no sanity check because there's a default callback iff not otherwise set.
				break;
			}
		}
	}
}

s32 WPAD_Init()
{
	u32 level;
	struct timespec tb;
	int i;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		__wpads_ponded = 0;
		__wpads_active = 0;
		__wpads_registered = 0;

		memset(__wpdcb,0,sizeof(struct _wpad_cb)*WPAD_MAX_WIIMOTES);
		memset(__wpad_devs,0,sizeof(conf_pad_device)*WPAD_MAX_WIIMOTES);
		memset(__wpad_keys,0,sizeof(struct linkkey_info)*WPAD_MAX_WIIMOTES);
		
		for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
			__wpdcb[i].thresh.btns = WPAD_THRESH_DEFAULT_BUTTONS;
			__wpdcb[i].thresh.ir = WPAD_THRESH_DEFAULT_IR;
			__wpdcb[i].thresh.acc = WPAD_THRESH_DEFAULT_ACCEL;
			__wpdcb[i].thresh.js = WPAD_THRESH_DEFAULT_JOYSTICK;
		}

		__wpads_registered = CONF_GetPadDevices(__wpad_devs,WPAD_MAX_WIIMOTES);
		if(__wpads_registered<=0) {
			_CPU_ISR_Restore(level);
			return WPAD_ERR_NONEREGISTERED;
		}

		__wpads = wiiuse_init(WPAD_MAX_WIIMOTES,__wpad_eventCB);
		if(__wpads==NULL) {
			_CPU_ISR_Restore(level);
			return WPAD_ERR_UNKNOWN;
		}

		__wiiuse_sensorbar_enable(1);

		BTE_Init();
		BTE_SetDisconnectCallback(__wpad_disconnectCB);
		BTE_InitCore(__initcore_finished);

		SYS_CreateAlarm(&__wpad_timer);
		SYS_RegisterResetFunc(&__wpad_resetinfo);
	
		tb.tv_sec = 1;
		tb.tv_nsec = 0;
		SYS_SetPeriodicAlarm(__wpad_timer,&tb,&tb,__wpad_timeouthandler);

		__wpads_inited = WPAD_STATE_ENABLING;
	}
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_ReadEvent(s32 chan, WPADData *data)
{
	u32 level;
	u32 maxbufs,smoothed = 0;
	struct accel_t *accel_calib = NULL;
	struct _wpad_cb *wpdcb = NULL;
	WPADData *lstate = NULL,*wpadd = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan] && WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(__wpads[chan],WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			wpdcb = &__wpdcb[chan];
			if(wpdcb->queue_ext!=NULL) {
				maxbufs = wpdcb->queue_length;
				wpadd = wpdcb->queue_ext;
			} else {
				maxbufs = EVENTQUEUE_LENGTH;
				wpadd = wpdcb->queue_int;
			}
			if(wpdcb->queue_full == 0) {
				_CPU_ISR_Restore(level);
				return WPAD_ERR_QUEUE_EMPTY;
			}
			if(data)
				*data = wpadd[wpdcb->queue_head];
			wpdcb->queue_head++;
			wpdcb->queue_head %= maxbufs;
			wpdcb->queue_full--;
			lstate = &wpdcb->lstate;
			accel_calib = &__wpads[chan]->accel_calib;
			smoothed = WIIMOTE_IS_FLAG_SET(__wpads[chan], WIIUSE_SMOOTHING);
		} else {
			_CPU_ISR_Restore(level);
			return WPAD_ERR_NOT_READY;
		}
	} else {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NO_CONTROLLER;
	}
	
	_CPU_ISR_Restore(level);
	if(data)
		__wpad_calc_data(data,lstate,accel_calib,smoothed);
	return 0;
}

s32 WPAD_DroppedEvents(s32 chan)
{
	u32 level;
	s32 ret;
	int i;
	int dropped = 0;
	
	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_DroppedEvents(i)) < WPAD_ERR_NONE)
				return ret;
			else
				dropped += ret;
		return dropped;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) {
		dropped = __wpdcb[chan].dropped_events;
		__wpdcb[chan].dropped_events = 0;
	}
	_CPU_ISR_Restore(level);
	return dropped;
}

s32 WPAD_Flush(s32 chan)
{
	s32 ret;
	int i;
	int count = 0;
	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_Flush(i)) < WPAD_ERR_NONE)
				return ret;
			else
				count += ret;
		return count;
	}
	
	while((ret = WPAD_ReadEvent(chan, NULL)) >= 0)
		count++;
	if(ret == WPAD_ERR_QUEUE_EMPTY) return count;
	return ret;
}

s32 WPAD_ReadPending(s32 chan, WPADDataCallback datacb)
{
	u32 btns_p = 0;
	u32 btns_h = 0;
	u32 btns_l = 0;
	u32 btns_ch = 0;
	u32 btns_ev = 0;
	u32 btns_nh = 0;
	u32 i;
	s32 count = 0;
	s32 ret;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_ReadPending(i, datacb)) >= WPAD_ERR_NONE)
				count += ret;
		return count;
	}
	
	btns_p = btns_nh = btns_l = wpaddata[chan].btns_h;
	while(1) {
		ret = WPAD_ReadEvent(chan,&wpaddata[chan]);
		if(ret < WPAD_ERR_NONE) break;
		if(datacb)
			datacb(chan, &wpaddata[chan]);
		
		// we ignore everything except _h, since we have our
		// own "fake" _l and everything gets recalculated at
		// the end of the function
		btns_h = wpaddata[chan].btns_h;
		
		/* Button event coalescing:
		 * What we're doing here is get the very first button event
		 * (press or release) for each button. This gets propagated
		 * to the output. Held will therefore report an "old" state
		 * for every button that has changed more than once. This is
		 * intentional: next call to WPAD_ReadPending, if this button
		 * hasn't again changed, the opposite event will fire. This
		 * is the behavior that preserves the most information,
		 * within the limitations of trying to coalesce multiple events
		 * into one. It also keeps the output consistent, if possibly
		 * not fully up to date.
		 */
		
		// buttons that changed that haven't changed before
		btns_ch = (btns_h ^ btns_p) & ~btns_ev;
		btns_p = btns_h;
		// propagate changes in btns_ch to btns_nd
		btns_nh = (btns_nh & ~btns_ch) | (btns_h & btns_ch);
		// store these new changes to btns_ev
		btns_ev |= btns_ch;
		
		count++;
	}
	wpaddata[chan].btns_h = btns_nh;
	wpaddata[chan].btns_l = btns_l;
	wpaddata[chan].btns_d = btns_nh & ~btns_l;
	wpaddata[chan].btns_u = ~btns_nh & btns_l;
	if(ret == WPAD_ERR_QUEUE_EMPTY) return count;
	return ret;
}

s32 WPAD_SetDataFormat(s32 chan, s32 fmt)
{
	u32 level;
	s32 ret;
	int i;
	
	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_SetDataFormat(i, fmt)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) {
		switch(fmt) {
			case WPAD_FMT_BTNS:
			case WPAD_FMT_BTNS_ACC:
			case WPAD_FMT_BTNS_ACC_IR:
				__wpdcb[chan].data_fmt = fmt;
				__wpad_setfmt(chan);
				break;
			default:
				_CPU_ISR_Restore(level);
				return WPAD_ERR_BADVALUE;
		}
	}
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetVRes(s32 chan,u32 xres,u32 yres)
{
	u32 level;
	s32 ret;
	int i;
	
	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_SetVRes(i, xres, yres)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) 
		wiiuse_set_ir_vres(__wpads[chan],xres,yres);

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_GetStatus()
{
	s32 ret;
	u32 level;

	_CPU_ISR_Disable(level);
	ret = __wpads_inited;
	_CPU_ISR_Restore(level);
	
	return ret;
}

s32 WPAD_Probe(s32 chan,u32 *type)
{
	s32 ret;
	u32 level,dev;
	wiimote *wm = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	wm = __wpads[chan];
	if(wm && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_CONNECTED)) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
			dev = WPAD_EXP_NONE;
			if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP)) {
				switch(wm->exp.type) {
					case WPAD_EXP_NUNCHUK:
					case WPAD_EXP_CLASSIC:
					case WPAD_EXP_GUITARHERO3:
						dev = wm->exp.type;
						break;
				}
			}
			if(type!=NULL) *type = dev;
			ret = WPAD_ERR_NONE;
		} else
			ret = WPAD_ERR_NOT_READY;
	} else
		ret = WPAD_ERR_NO_CONTROLLER;
	_CPU_ISR_Restore(level);

	return ret;
}

s32 WPAD_SetEventBufs(s32 chan, WPADData *bufs, u32 cnt)
{
	u32 level;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;

	_CPU_ISR_Disable(level);
	wpdcb = &__wpdcb[chan];
	wpdcb->queue_head = 0;
	wpdcb->queue_tail = 0;
	wpdcb->queue_full = 0;
	wpdcb->queue_length = cnt;
	wpdcb->queue_ext = bufs;
	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

void WPAD_SetPowerButtonCallback(WPADShutdownCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(cb)
		__wpad_powcb = cb;
	else
		__wpad_powcb = __wpad_def_powcb;
	_CPU_ISR_Restore(level);
}

void WPAD_SetBatteryDeadCallback(WPADShutdownCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_batcb = cb;
	_CPU_ISR_Restore(level);
}

s32 WPAD_Disconnect(s32 chan)
{
	u32 level;
	struct _wpad_cb *wpdcb = NULL;

	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}
	
	wpdcb = &__wpdcb[chan];
	__wpad_disconnect(wpdcb);
	_CPU_ISR_Restore(level);

	while(__wpads_active&(0x01<<chan));
	return WPAD_ERR_NONE;
}

void WPAD_Shutdown()
{
	s32 i;
	u32 level;
	struct _wpad_cb *wpdcb = NULL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return;
	}

	SYS_RemoveAlarm(__wpad_timer);
	for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
		wpdcb = &__wpdcb[i];
		__wpad_disconnect(wpdcb);
	}

	__wiiuse_sensorbar_enable(0);
	__wpads_inited = WPAD_STATE_DISABLED;
	_CPU_ISR_Restore(level);
	
	while(__wpads_active);

	BTE_Shutdown();
}

void WPAD_SetIdleTimeout(u32 seconds)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__wpad_idletimeout = seconds;
	_CPU_ISR_Restore(level);
}

s32 WPAD_ScanPads()
{
	return WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
}

s32 WPAD_Rumble(s32 chan, int status)
{
	int i;
	s32 ret;
	u32 level;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_Rumble(i,status)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}
	
	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	if(__wpads[chan]!=NULL) 
		wiiuse_rumble(__wpads[chan],status);

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}

s32 WPAD_SetIdleThresholds(s32 chan, s32 btns, s32 ir, s32 accel, s32 js)
{
	int i;
	s32 ret;
	u32 level;

	if(chan == WPAD_CHAN_ALL) {
		for(i=WPAD_CHAN_0; i<WPAD_MAX_WIIMOTES; i++)
			if((ret = WPAD_SetIdleThresholds(i,btns,ir,accel,js)) < WPAD_ERR_NONE)
				return ret;
		return WPAD_ERR_NONE;
	}
	
	if(chan<WPAD_CHAN_0 || chan>=WPAD_MAX_WIIMOTES) return WPAD_ERR_BAD_CHANNEL;
	
	_CPU_ISR_Disable(level);
	if(__wpads_inited==WPAD_STATE_DISABLED) {
		_CPU_ISR_Restore(level);
		return WPAD_ERR_NOT_READY;
	}

	__wpdcb[chan].thresh.btns = (btns<0) ? -1 : 0;
	__wpdcb[chan].thresh.ir = ir;
	__wpdcb[chan].thresh.acc = accel;
	__wpdcb[chan].thresh.js = js;

	_CPU_ISR_Restore(level);
	return WPAD_ERR_NONE;
}


WPADData *WPAD_Data(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES) return NULL;
	return &wpaddata[chan];
}

u32 WPAD_ButtonsUp(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES) return 0;
	return wpaddata[chan].btns_u;
}

u32 WPAD_ButtonsDown(int chan)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES) return 0;
	return wpaddata[chan].btns_d;
}

u32 WPAD_ButtonsHeld(int chan) 
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES) return 0;
	return wpaddata[chan].btns_h;
}

void WPAD_IR(int chan, struct ir_t *ir)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES || ir==NULL ) return;
	*ir = wpaddata[chan].ir;
}

void WPAD_Orientation(int chan, struct orient_t *orient)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES || orient==NULL ) return;
	*orient = wpaddata[chan].orient;
}

void WPAD_GForce(int chan, struct gforce_t *gforce)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES || gforce==NULL ) return;
	*gforce = wpaddata[chan].gforce;
}

void WPAD_Accel(int chan, struct vec3w_t *accel)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES || accel==NULL ) return;
	*accel = wpaddata[chan].accel;
}
	
void WPAD_Expansion(int chan, struct expansion_t *exp)
{
	if(chan<0 || chan>=WPAD_MAX_WIIMOTES || exp==NULL ) return;
	*exp = wpaddata[chan].exp;
}
