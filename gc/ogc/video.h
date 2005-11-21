/*-------------------------------------------------------------

$Id: video.h,v 1.15 2005/11/21 12:37:51 shagkur Exp $

video.h -- VIDEO subsystem

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

$Log: video.h,v $
Revision 1.15  2005/11/21 12:37:51  shagkur
Added copyright header(taken from libnds).
Introduced RCS $Id$ and $Log$ token in project files.


-------------------------------------------------------------*/


#ifndef __VIDEO_H__
#define __VIDEO_H__

/*! \file video.h 
\brief VIDEO subsystem

*/ 

#include <gctypes.h>
#include "gx_struct.h"
#include "video_types.h"

#define VIDEO_PadFramebufferWidth(width)     ((u16)(((u16)(width) + 15) & ~15))			/*!< macro to pad the width to a multiple of 16 */

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*!
 * \addtogroup gxrmode_obj VIDEO render mode objects
 * @{
 */

extern GXRModeObj TVNtsc240Ds;				/*!< Video and render mode configuration for 240 lines,singlefield NTSC mode */ 
extern GXRModeObj TVNtsc240DsAa;			/*!< Video and render mode configuration for 240 lines,singlefield,antialiased NTSC mode */ 
extern GXRModeObj TVNtsc240Int;				/*!< Video and render mode configuration for 240 lines,interlaced NTSC mode */ 
extern GXRModeObj TVNtsc240IntAa;			/*!< Video and render mode configuration for 240 lines,interlaced,antialiased NTSC mode */ 
extern GXRModeObj TVNtsc480IntDf;			/*!< Video and render mode configuration for 480 lines,interlaced,doublefield NTSC mode */ 
extern GXRModeObj TVNtsc480IntAa;			/*!< Video and render mode configuration for 480 lines,interlaced,doublefield,antialiased NTSC mode */ 
extern GXRModeObj TVMpal480IntDf;
extern GXRModeObj TVPal264Ds;
extern GXRModeObj TVPal264DsAa;
extern GXRModeObj TVPal264Int;
extern GXRModeObj TVPal264IntAa;
extern GXRModeObj TVPal524IntAa;
extern GXRModeObj TVPal528Int;
extern GXRModeObj TVPal528IntDf;
extern GXRModeObj TVPal574IntDfScale;

/*!
 * @}
 */


/*! \typedef void (*VIRetraceCallback)(u32 retraceCnt)
\brief function pointer typedef for the user's retrace callback
\param[in] retraceCnt current retrace count
*/
typedef void (*VIRetraceCallback)(u32 retraceCnt);

/*! \fn void VIDEO_Init()
\brief Initializes the VIDEO subsystem. This call should be done in the early stages of your main()

\return none
*/
void VIDEO_Init();


/*! \fn void VIDEO_Flush()
\brief Flush the shadow registers to the drivers video registers.

\return none
*/
void VIDEO_Flush();


/*! \fn void VIDEO_SetBlack(boolean black)
\brief Blackout the VIDEO interface.
\param[in] black Boolean flag to determine whether to blackout the VI or not.

\return none
*/
void VIDEO_SetBlack(boolean black);


/*! \fn u32 VIDEO_GetNextField()
\brief Get the next field in DS mode.

\return field (0:1)
*/
u32 VIDEO_GetNextField();


/*! \fn u32 VIDEO_GetCurrentLine()
\brief Get current video line

\return linenumber
*/
u32 VIDEO_GetCurrentLine();


/*! \fn u32 VIDEO_GetCurrentTvMode()
\brief Get current configured TV mode

\return tv mode (PAL,NTSC,MPAL)
*/
u32 VIDEO_GetCurrentTvMode();


/*! \fn void VIDEO_Configure(GXRModeObj *rmode)
\brief Configure the VI with the given render mode object
\param[in] rmode pointer to a GXRModeObj, specifying the mode.

\return none
*/
void VIDEO_Configure(GXRModeObj *rmode);


/*! \fn void VIDEO_ClearFrameBuffer(GXRModeObj *rmode,void *fb,u32 color)
\brief Clear the given framebuffer.
\param[in] rmode pointer to a GXRModeObj, specifying the mode.
\param[in] fb pointer to the startaddress of the framebuffer to clear.
\param[in] color YUYUV value to use for clearing.

\return none
*/
void VIDEO_ClearFrameBuffer(GXRModeObj *rmode,void *fb,u32 color);


/*! \fn void VIDEO_WaitVSync(void)
\brief Wait on the next vertical retrace

\return none
*/
void VIDEO_WaitVSync(void);


/*! \fn void VIDEO_SetNextFramebuffer(void *fb)
\brief Set the framebuffer for the next VI register update.

\return none
*/
void VIDEO_SetNextFramebuffer(void *fb);


/*! \fn void VIDEO_SetNextRightFramebuffer(void *fb)
\brief Set the right framebuffer for the next VI register update. This is used for 3D Gloves for instance.

\return none
*/
void VIDEO_SetNextRightFramebuffer(void *fb);


/*! \fn VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback)
\brief Set the Pre-Retrace callback function. This function is called within the video interrupt handler before the VI registers will be updated.
\param[in] callback pointer to the callback function which is called at pre-retrace.

\return Old pre-retrace callback or NULL
*/
VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback);


/*! \fn VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback)
\brief Set the Post-Retrace callback function. This function is called within the video interrupt handler after the VI registers are updated.
\param[in] callback pointer to the callback function which is called at post-retrace.

\return Old post-retrace callback or NULL
*/
VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
