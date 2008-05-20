#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "dynamics.h"
#include "definitions.h"
#include "wiiuse_internal.h"
#include "nunchuk.h"
#include "io.h"

/**
 *	@brief Find what buttons are pressed.
 *
 *	@param nc		Pointer to a nunchuk_t structure.
 *	@param msg		The message byte specified in the event packet.
 */
static void nunchuk_pressed_buttons(struct nunchuk_t* nc, ubyte now) {
	/* message is inverted (0 is active, 1 is inactive) */
	now = ~now & NUNCHUK_BUTTON_ALL;

	/* preserve old btns pressed */
	nc->btns_last = nc->btns;

	/* pressed now & were pressed, then held */
	nc->btns_held = (now & nc->btns);

	/* were pressed or were held & not pressed now, then released */
	nc->btns_released = ((nc->btns | nc->btns_held) & ~now);

	/* buttons pressed now */
	nc->btns = now;
}

int nunchuk_handshake(struct wiimote_t *wm,struct nunchuk_t *nc,ubyte *data,uword len)
{
	int i;
	int offset = 0;

	nc->btns = 0;
	nc->btns_held = 0;
	nc->btns_released = 0;
	nc->flags = &wm->flags;
	nc->accel_calib = wm->accel_calib;

	for(i=0;i<len;i++) data[i] = (data[i]^0x17)+0x17;
	if(data[offset]==0xff) {
		if(data[offset+16]==0xff) {
			wiiuse_read_data(wm,data,WM_EXP_MEM_CALIBR,EXP_HANDSHAKE_LEN,wiiuse_handshake_expansion);
			return 0;
		}
		offset += 16;
	}

	nc->accel_calib.cal_zero.x = data[offset + 0];
	nc->accel_calib.cal_zero.y = data[offset + 1];
	nc->accel_calib.cal_zero.z = data[offset + 2];
	nc->accel_calib.cal_g.x = data[offset + 4];
	nc->accel_calib.cal_g.y = data[offset + 5];
	nc->accel_calib.cal_g.z = data[offset + 6];
	nc->js.max.x = data[offset + 8];
	nc->js.min.x = data[offset + 9];
	nc->js.center.x = data[offset + 10];
	nc->js.max.y = data[offset + 11];
	nc->js.min.y = data[offset + 12];
	nc->js.center.y = data[offset + 13];
#ifndef GEKKO
	nc->orient_threshold = wm->orient_threshold;
	nc->accel_threshold = wm->accel_threshold;
#endif
	wm->event = WIIUSE_NUNCHUK_INSERTED;
	wm->exp.type = EXP_NUNCHUK;

	WIIMOTE_DISABLE_STATE(wm,WIIMOTE_STATE_EXP_HANDSHAKE);
	WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_EXP_HANDSHAKE_COMPLETE);
	return 1;
}

/**
 *	@brief The nunchuk disconnected.
 *
 *	@param nc		A pointer to a nunchuk_t structure.
 */
void nunchuk_disconnected(struct nunchuk_t* nc) {
	//printf("nunchuk_disconnected()\n");
	memset(nc, 0, sizeof(struct nunchuk_t));
}

/**
 *	@brief Handle nunchuk event.
 *
 *	@param nc		A pointer to a nunchuk_t structure.
 *	@param msg		The message specified in the event packet.
 */
void nunchuk_event(struct nunchuk_t* nc, ubyte* msg) {
	int i;

	/* decrypt data */
	for (i = 0; i < 6; ++i)
		msg[i] = (msg[i] ^ 0x17) + 0x17;

	/* get button states */
	nunchuk_pressed_buttons(nc, msg[5]);

	nc->js.pos.x = msg[0];
	nc->js.pos.y = msg[1];
#ifndef GEKKO
	/* calculate joystick state */
	calc_joystick_state(&nc->js, nc->js.pos.x, nc->js.pos.y);
#endif
	/* calculate orientation */
	nc->accel.x = msg[2];
	nc->accel.y = msg[3];
	nc->accel.z = msg[4];
#ifndef GEKKO
	calculate_orientation(&nc->accel_calib, &nc->accel, &nc->orient, NUNCHUK_IS_FLAG_SET(nc, WIIUSE_SMOOTHING));
	calculate_gforce(&nc->accel_calib, &nc->accel, &nc->gforce);
#endif
}

