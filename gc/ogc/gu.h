#ifndef __GU_H__
#define __GU_H__

#include <gctypes.h>

#ifdef GEKKO
#define MTX_USE_PS
#undef MTX_USE_C
#endif

#ifndef GEKKO
#define MTX_USE_C
#undef MTX_USE_PS
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define M_PI		3.14159265358979323846
#define M_DTOR		(3.14159265358979323846/180.0)

#define	FTOFIX32(x)	(s32)((x) * (f32)0x00010000)
#define	FIX32TOF(x)	((f32)(x) * (1.0f / (f32)0x00010000))
#define	FTOFRAC8(x)	((s32) MIN(((x) * (128.0f)), 127.0f) & 0xff)

#define DegToRad(a)   ( (a) *  0.01745329252f )
#define RadToDeg(a)   ( (a) * 57.29577951f )

#define guMtxRowCol(mt,row,col)		(mt[row][col])

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _vecf {
	f32 x,y,z;
} Vector;

typedef struct _qrtn {
	f32 x,y,z,w;
} Quaternion;

typedef f32	Mtx[3][4];
typedef f32 (*MtxP)[4];
typedef f32 ROMtx[4][3];
typedef f32 (*ROMtxP)[3];
typedef f32 Mtx33[3][3];
typedef f32 (*Mtx33P)[3];
typedef f32 Mtx44[4][4];
typedef f32 (*Mtx44P)[4];

void guFrustum(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f);
void guPerspective(Mtx44 mt,f32 fovy,f32 aspect,f32 n,f32 f);
void guOrtho(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f);

void guLightPerspective(Mtx mt,f32 fovY,f32 aspect,f32 scaleS,f32 scaleT,f32 transS,f32 transT);
void guLightOrtho(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 scaleS,f32 scaleT,f32 transS,f32 transT);
void guLightFrustum(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 scaleS,f32 scaleT,f32 transS,f32 transT);

void guLookAt(Mtx mt,Vector *camPos,Vector *camUp,Vector *target);

void guVecHalfAngle(Vector *a,Vector *b,Vector *half);

void c_guVecAdd(Vector *a,Vector *b,Vector *ab);
void c_guVecSub(Vector *a,Vector *b,Vector *ab);
void c_guVecScale(Vector *src,Vector *dst,f32 scale);
void c_guVecNormalize(Vector *v);
void c_guVecMultiply(Mtx mt,Vector *src,Vector *dst);
void c_guVecCross(Vector *a,Vector *b,Vector *axb);
void c_guVecMultiplySR(Mtx mt,Vector *src,Vector *dst);
f32 c_guVecDotProduct(Vector *a,Vector *b);

#ifdef GEKKO
void ps_guVecAdd(register Vector *a,register Vector *b,register Vector *ab);
void ps_guVecSub(register Vector *a,register Vector *b,register Vector *ab);
void ps_guVecScale(register Vector *src,register Vector *dst,f32 scale);
void ps_guVecNormalize(register Vector *v);
void ps_guVecCross(register Vector *a,register Vector *b,register Vector *axb);
void ps_guVecMultiply(register Mtx mt,register Vector *src,register Vector *dst);
void ps_guVecMultiplySR(register Mtx mt,register Vector *src,register Vector *dst);
f32 ps_guVecDotProduct(register Vector *a,register Vector *b);
#endif	//GEKKO

void c_guQuatAdd(Quaternion *a,Quaternion *b,Quaternion *ab);
void c_guQuatSub(Quaternion *a,Quaternion *b,Quaternion *ab);
void c_guQuatMultiply(Quaternion *a,Quaternion *b,Quaternion *ab);

#ifdef GEKKO
void ps_guQuatAdd(register Quaternion *a,register Quaternion *b,register Quaternion *ab);
void ps_guQuatSub(register Quaternion *a,register Quaternion *b,register Quaternion *ab);
void ps_guQuatMultiply(Quaternion *a,Quaternion *b,Quaternion *ab);
void ps_guQuatScale(Quaternion *q,Quaternion *r,register f32 scale);
void ps_guQuatNormalize(Quaternion *src,Quaternion *r,register Quaternion *unit);
f32 ps_guQuatDotProduct(Quaternion *p,Quaternion *q);
#endif

void c_guMtxIdentity(Mtx mt);
void c_guMtxCopy(Mtx src,Mtx dst);
void c_guMtxConcat(Mtx a,Mtx b,Mtx ab);
void c_guMtxScale(Mtx mt,f32 xS,f32 yS,f32 zS);
void c_guMtxScaleApply(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS);
void c_guMtxTrans(Mtx mt,f32 xT,f32 yT,f32 zT);
void c_guMtxTransApply(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT);
u32 c_guMtxInverse(Mtx src,Mtx inv);
void c_guMtxTranspose(Mtx src,Mtx xPose);
void c_guMtxRotRad(Mtx mt,const char axis,f32 rad);
void c_guMtxRotTrig(Mtx mt,const char axis,f32 sinA,f32 cosA);
void c_guMtxRotAxisRad(Mtx mt,Vector *axis,f32 rad);
void c_guMtxReflect(Mtx m,Vector *p,Vector *n);

#ifdef GEKKO
void ps_guMtxIdentity(register Mtx mt);
void ps_guMtxCopy(register Mtx src,register Mtx dst);
void ps_guMtxConcat(register Mtx a,register Mtx b,register Mtx ab);
void ps_guMtxTranspose(register Mtx src,register Mtx xPose);
u32 ps_guMtxInverse(register Mtx src,register Mtx inv);
u32 ps_guMtxInvXpos(register Mtx src,register Mtx invx);
void ps_guMtxScale(register Mtx mt,register f32 xS,register f32 yS,register f32 zS);
void ps_guMtxScaleApply(register Mtx src,register Mtx dst,register f32 xS,register f32 yS,register f32 zS);
void ps_guMtxTrans(register Mtx mt,register f32 xT,register f32 yT,register f32 zT);
void ps_guMtxTransApply(register Mtx src,register Mtx dst,register f32 xT,register f32 yT,register f32 zT);
void ps_guMtxRotRad(register Mtx mt,const register char axis,register f32 rad);
void ps_guMtxRotTrig(register Mtx mt,const register char axis,register f32 sinA,register f32 cosA);
void ps_guMtxRotAxisRad(register Mtx mt,register Vector *axis,register f32 tmp0);
void ps_guMtxReflect(register Mtx m,register Vector *p,register Vector *n);
#endif	//GEKKO

#ifdef MTX_USE_C

#define guVecAdd				c_guVecAdd
#define guVecSub				c_guVecSub
#define guVecScale				c_guVecScale
#define guVecNormalize			c_guVecNormalize
#define guVecMultiply			c_guVecMultiply
#define guVecCross				c_guVecCross
#define guVecMultiplySR			c_guVecMultiplySR
#define guVecDotProduct			c_guVecDotProduct

#define guQuatAdd				c_guQuatAdd
#define guQuatSub				c_guQuatSub
#define guQuatMultiply			c_guQuatMultiply

#define guMtxIdentity			c_guMtxIdentity
#define guMtxCopy				c_guMtxCopy
#define guMtxConcat				c_guMtxConcat
#define guMtxScale				c_guMtxScale
#define guMtxScaleApply			c_guMtxScaleApply
#define guMtxTrans				c_guMtxTrans
#define guMtxTransApply			c_guMtxTransApply
#define guMtxInverse			c_guMtxInverse
#define guMtxTranspose			c_guMtxTranspose
#define guMtxRotRad				c_guMtxRotRad
#define guMtxRotTrig			c_guMtxRotTrig
#define guMtxRotAxisRad			c_guMtxRotAxisRad
#define guMtxReflect			c_guMtxReflect

#else //MTX_USE_C

#define guVecAdd				ps_guVecAdd
#define guVecSub				ps_guVecSub
#define guVecScale				ps_guVecScale
#define guVecNormalize			ps_guVecNormalize
#define guVecMultiply			ps_guVecMultiply
#define guVecCross				ps_guVecCross
#define guVecMultiplySR			ps_guVecMultiplySR
#define guVecDotProduct			ps_guVecDotProduct

#define guQuatAdd				ps_guQuatAdd
#define guQuatSub				ps_guQuatSub

#define guMtxIdentity			ps_guMtxIdentity
#define guMtxCopy				ps_guMtxCopy
#define guMtxConcat				ps_guMtxConcat
#define guMtxScale				ps_guMtxScale
#define guMtxScaleApply			ps_guMtxScaleApply
#define guMtxTrans				ps_guMtxTrans
#define guMtxTransApply			ps_guMtxTransApply
#define guMtxInverse			ps_guMtxInverse
#define guMtxTranspose			ps_guMtxTranspose
#define guMtxRotRad				ps_guMtxRotRad
#define guMtxRotTrig			ps_guMtxRotTrig
#define guMtxRotAxisRad			ps_guMtxRotAxisRad
#define guMtxReflect			ps_guMtxReflect

#endif //MTX_USE_PS

#define guMtxRotDeg(mt,axis,deg)		guMtxRotRad(mt,axis,DegToRad(deg))
#define guMtxRotAxisDeg(mt,axis,deg)	guMtxRotAxisRad(mt,axis,DegToRad(deg))

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
