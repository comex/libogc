#ifndef __BTE_MSG_H__
#define __BTE_MSG_H__

#include "btpbuf.h"

enum btemsg_type {
	BTEMSG_INPUT,
	BTEMSG_CALLBACK
};

struct bte_msg
{
	enum btemsg_type type;
	union {
		struct {
			struct bte_pcb *pcb;
			struct pbuf *p;
		} inp;
		struct {
			void (*f)(void*);
			void *ctx;
		} cb;
	} msg;
};

#endif
