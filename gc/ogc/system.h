/*-------------------------------------------------------------

$Id: system.h,v 1.26 2005/12/17 22:25:30 shagkur Exp $

system.h -- OS functions and initialization

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

$Log: system.h,v $
Revision 1.26  2005/12/17 22:25:30  shagkur
no message

Revision 1.25  2005/12/09 09:21:32  shagkur
no message

Revision 1.24  2005/11/22 07:15:22  shagkur
- Added function for console window initialization

Revision 1.23  2005/11/21 12:37:51  shagkur
Added copyright header(taken from libnds).

-------------------------------------------------------------*/


#ifndef __SYSTEM_H__
#define __SYSTEM_H__


/*! \file system.h 
\brief OS functions and initialization

*/ 

#include <gctypes.h>
#include <gcutil.h>
#include <time.h>
#include <ogc/lwp_queue.h>
#include "gx_struct.h"


/*!
 * \addtogroup sys_resettypes OS reset types
 * @{
 */

#define SYS_RESTART						0			/*!< Reboot the gamecube, force, if necessary, to boot the IPL menu. Cold reset is issued */
#define SYS_HOTRESET					1			/*!< Restart the application. Kind of softreset */
#define SYS_SHUTDOWN					2			/*!< Shutdown the thread system, card management system etc. Leave current thread running and return to caller */

/*!
 *@}
 */


/*!
 * \addtogroup sys_mprotchans OS memory protection channels
 * @{
 */

#define SYS_PROTECTCHAN0				0			/*!< OS memory protection channel 0 */ 
#define SYS_PROTECTCHAN1				1			/*!< OS memory protection channel 1 */
#define SYS_PROTECTCHAN2				2			/*!< OS memory protection channel 2 */
#define SYS_PROTECTCHAN3				3			/*!< OS memory protection channel 2 */
#define SYS_PROTECTCHANMAX				4			/*!< _Termination */

/*!
 *@}
 */


/*!
 * \addtogroup sys_mprotmodes OS memory protection modes
 * @{
 */

#define SYS_PROTECTNONE					0x00000000		/*!< Read and write operations on protected region is granted */
#define SYS_PROTECTREAD					0x00000001		/*!< Read from protected region is permitted */
#define SYS_PROTECTWRITE				0x00000002		/*!< Write to protected region is permitted */
#define SYS_PROTECTRDWR					(SYS_PROTECTREAD|SYS_PROTECTWRITE)	/*!< Read and write operations on protected region is permitted */

/*!
 *@}
 */


/*!
 * \addtogroup sys_mcastmacros OS memory casting macros
 * @{
 */

#define MEM_VIRTUAL_TO_PHYSICAL(x)		(((u32)(x))&~0xC0000000)			/*!< Cast virtual address to physical address, e.g. 0x8xxxxxxx -> 0x0xxxxxxx */
#define MEM_PHYSICAL_TO_K0(x)			(void*)((u32)(x)|0x80000000)		/*!< Cast physical address to cached virtual address, e.g. 0x0xxxxxxx -> 0x8xxxxxxx */
#define MEM_PHYSICAL_TO_K1(x)			(void*)((u32)(x)|0xC0000000)		/*!< Cast physical address to uncached virtual address, e.g. 0x0xxxxxxx -> 0xCxxxxxxx */
#define MEM_K0_TO_K1(x)					(void*)((u32)(x)|0x40000000)		/*!< Cast cached virtual address to uncached virtual address, e.g. 0x8xxxxxxx -> 0xCxxxxxxx */
#define MEM_K1_TO_K0(x)					(void*)((u32)(x)&~0x40000000)		/*!< Cast uncached virtual address to cached virtual address, e.g. 0xCxxxxxxx -> 0x8xxxxxxx */

/*!
 *@}
 */


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*!
 * \typedef struct _syssram syssram
 * \brief holds the stored configuration value from the system SRAM area
 * \param checksum holds the block checksum.
 * \param checksum_in holds the inverse block checksum
 * \param ead0 unknown attribute
 * \param ead1 unknown attribute
 * \param counter_bias bias value for the realtime clock
 * \param display_offsetH pixel offset for the VI
 * \param ntd unknown attribute
 * \param lang language of system
 * \param flags device and operations flag
 */
typedef struct _syssram {
	u16 checksum ATTRIBUTE_PACKED;
	u16 checksum_inv ATTRIBUTE_PACKED;
	u32 ead0 ATTRIBUTE_PACKED;
	u32 ead1 ATTRIBUTE_PACKED;
	u32 counter_bias ATTRIBUTE_PACKED;
	s8 display_offsetH ATTRIBUTE_PACKED;
	u8 ntd ATTRIBUTE_PACKED;
	u8 lang ATTRIBUTE_PACKED;
	u8 flags ATTRIBUTE_PACKED;
} syssram;


/*!
 * \typedef struct _syssramex syssramex
 * \brief holds the stored configuration value from the extended SRAM area
 * \param flash_id[2][12] 96bit memorycard unlock flash ID
 * \param wirelessKbd_id Device ID of last connected wireless keyboard 
 * \param wirelessPad_id[4] 16bit device ID of last connected pad.
 * \param dvderr_code last non-recoverable error from DVD interface
 * \param __padding0 padding
 * \param flashID_chksum[2] 16bit checksum of unlock flash ID
 * \param __padding1[4] padding
 */
typedef struct _syssramex {
	u8 flash_id[2][12] ATTRIBUTE_PACKED;
	u32 wirelessKbd_id ATTRIBUTE_PACKED;
	u16 wirelessPad_id[4] ATTRIBUTE_PACKED;
	u8 dvderr_code ATTRIBUTE_PACKED;
	u8 __padding0 ATTRIBUTE_PACKED;
	u16 flashID_chksum[2] ATTRIBUTE_PACKED;
	u8 __padding1[4] ATTRIBUTE_PACKED;
} syssramex;

typedef struct _sysalarm sysalarm;

typedef void (*alarmcallback)(sysalarm *alarm);

struct _sysalarm {
	alarmcallback alarmhandler;
	void* handle;
	u64 ticks;
	u64 periodic;
	u64 start_per;
};

typedef struct _sys_fontheader {
	u16 font_type ATTRIBUTE_PACKED;
	u16 first_char ATTRIBUTE_PACKED;
	u16 last_char ATTRIBUTE_PACKED;
	u16 inval_char ATTRIBUTE_PACKED;
	u16 asc ATTRIBUTE_PACKED;
	u16 desc ATTRIBUTE_PACKED;
	u16 width ATTRIBUTE_PACKED;
	u16 leading ATTRIBUTE_PACKED;
    u16 cell_width ATTRIBUTE_PACKED;
    u16 cell_height ATTRIBUTE_PACKED;
    u32 sheet_size ATTRIBUTE_PACKED;
    u16 sheet_format ATTRIBUTE_PACKED;
    u16 sheet_column ATTRIBUTE_PACKED;
    u16 sheet_row ATTRIBUTE_PACKED;
    u16 sheet_width ATTRIBUTE_PACKED;
    u16 sheet_height ATTRIBUTE_PACKED;
    u16 width_table ATTRIBUTE_PACKED;
    u32 sheet_image ATTRIBUTE_PACKED;
    u32 sheet_fullsize ATTRIBUTE_PACKED;
    u8  c0 ATTRIBUTE_PACKED;
    u8  c1 ATTRIBUTE_PACKED;
    u8  c2 ATTRIBUTE_PACKED;
    u8  c3 ATTRIBUTE_PACKED;
} sys_fontheader;

typedef void (*resetcallback)(void);
typedef s32 (*resetfunction)(s32 final);
typedef struct _sys_resetinfo sys_resetinfo;

struct _sys_resetinfo {
	lwp_node node;
	resetfunction func;
	u32 prio;
};

/*! \fn void SYS_Init()
\deprecated Performs basic system initialization such as EXI init etc. This function is called from within the crt0 startup code.

\return none
*/
void SYS_Init();


/*! 
 * \fn void* SYS_AllocateFramebuffer(GXRModeObj *rmode)
 * \brief Allocate cacheline aligned memory for the external framebuffer based on the rendermode object.
 * \param[in] rmode pointer to the video/render mode configuration
 *
 * \return pointer to the framebuffer's startaddress. <b><i>NOTE:</i></b> Address returned is aligned to a 32byte boundery!
 */
void* SYS_AllocateFramebuffer(GXRModeObj *rmode);


/*! 
 * \fn s32 SYS_ConsoleInit(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight)
 * \brief Initialize stdout console
 * \param[in] rmode pointer to the video/render mode configuration
 * \param[in] conXOrigin starting pixel in X direction of the console output on the external framebuffer
 * \param[in] conYOrigin starting pixel in Y direction of the console output on the external framebuffer
 * \param[in] conWidth width of the console output 'window' to be drawn
 * \param[in] conHeight height of the console output 'window' to be drawn
 *
 * \return 0 on success, <0 on error
 */
s32 SYS_ConsoleInit(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight);


void SYS_ProtectRange(u32 chan,void *addr,u32 bytes,u32 cntrl);
resetcallback SYS_SetResetCallback(resetcallback cb);
void SYS_StartPMC(u32 mcr0val,u32 mcr1val);
void SYS_DumpPMC();
void SYS_StopPMC();


/*! \fn void SYS_CreateAlarm(sysalarm *alarm)
\brief Create/initialize sysalarm structure
\param[in] alarm pointer to a sysalarm structure to initialize/create.

\return none
*/
void SYS_CreateAlarm(sysalarm *alarm);


/*! \fn void SYS_SetAlarm(sysalarm *alarm,const struct timespec *tp,alarmcallback cb)
\brief Set the alarm parameters for a one-shot alarm, add to the list of alarms and start.
\param[in] alarm pointer to a sysalarm struture to use.
\param[in] tp pointer to timespec structure holding the time to fire the alarm
\param[in] cb pointer to callback which is called when the alarm fires.

\return none
*/
void SYS_SetAlarm(sysalarm *alarm,const struct timespec *tp,alarmcallback cb);


/*! \fn void SYS_SetPeriodicAlarm(sysalarm *alarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb)
\brief Set the alarm parameters for a periodioc alarm, add to the list of alarms and start. The alarm and interval persists as long as SYS_CancelAlarm() isn't called.
\param[in] alarm pointer to a sysalarm struture to use.
\param[in] tp_start pointer to timespec structure holding the time to fire first time the alarm
\param[in] tp_period pointer to timespec structure holding the interval for all following alarm triggers.
\param[in] cb pointer to callback which is called when the alarm fires.

\return none
*/
void SYS_SetPeriodicAlarm(sysalarm *alarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb);


/*! \fn void SYS_RemoveAlarm(sysalarm *alarm)
\brief Set the alarm parameters for a periodioc alarm and start. The alarm and interval persists as long as SYS_CancelAlarm() isn't called.
\param[in] alarm pointer to a sysalarm structure to remove from the list.

\return none
*/
void SYS_RemoveAlarm(sysalarm *alarm);


/*! \fn void SYS_CancelAlarm(sysalarm *alarm)
\brief Cancel the alarm, but do not remove from the list.
\param[in] alarm pointer to a sysalarm structure to cancel.

\return none
*/
void SYS_CancelAlarm(sysalarm *alarm);


void SYS_SetWirelessID(u32 chan,u32 id);
u32 SYS_GetWirelessID(u32 chan);
u32 SYS_GetFontEncoding();
u32 SYS_InitFont(sys_fontheader *font_header);
void SYS_GetFontTexture(s32 c,void **image,s32 *xpos,s32 *ypos,s32 *width);
void SYS_GetFontTexel(s32 c,void *image,s32 pos,s32 stride,s32 *width);
void SYS_ResetSystem(s32 reset,u32 reset_code,s32 force_menu);
void SYS_RegisterResetFunc(sys_resetinfo *info);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
