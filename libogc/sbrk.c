#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <stdlib.h>
#include <unistd.h>
#include <reent.h>
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "system.h"

#if defined(HW_RVL)

void* _DEFUN(__libogc_sbrk_r,(ptr,incr),
			 struct _reent *ptr _AND
			 ptrdiff_t incr)
{
	// Stored value of SYS_GetArena2Lo() when transitioning from MEM1 over to MEM2
	// This is used to transition back to MEM1 when freeing memory
	// It also serves as a flag of whether we're in MEM1 or MEM2
	static char *mem2_start = 0;
	
	char *heap_end = 0;
	char *prev_heap = 0;
	u32 level;

	_CPU_ISR_Disable(level);
	
	if(!mem2_start)
		heap_end = SYS_GetArena1Lo();
	else
		heap_end = SYS_GetArena2Lo();
	
	if(mem2_start) {
		// we're in MEM2
		if((heap_end+incr)>(char*)SYS_GetArena2Hi()) {
			// out of MEM2 case
			ptr->_errno = ENOMEM;
			prev_heap = (char *)-1;
		} else if ((heap_end+incr) < mem2_start) {
			// trying to sbrk() back below the MEM2 start barrier
			ptr->_errno = EINVAL;
			prev_heap = (char *)-1;
		} else {
			// success case
			prev_heap = heap_end;
			SYS_SetArena2Lo((void*)(heap_end+incr));
		}
		// if MEM2 area is exactly at the barrier, transition back to MEM1 again
		if(SYS_GetArena2Lo() == mem2_start)
			mem2_start = 0;
	} else {
		// we're in MEM1
		if((heap_end+incr)>(char*)SYS_GetArenaHi()) {
			// out of MEM1, transition into MEM2
			if(((char*)SYS_GetArena2Lo() + incr) > (char*)SYS_GetArena2Hi()) {
				// this increment doesn't fit in available MEM2
				ptr->_errno = ENOMEM;
				prev_heap = (char *)-1;
			} else {
				// MEM2 is available, move into it
				mem2_start = heap_end = prev_heap = SYS_GetArena2Lo();
				SYS_SetArena2Lo((void*)(heap_end+incr));
			}
		} else {
			// MEM1 is available (or we're freeing memory)
			prev_heap = heap_end;
			SYS_SetArenaLo((void*)(heap_end+incr));
		}
	}
	
	_CPU_ISR_Restore(level);

	return (void*)prev_heap;	
}

#else

void* _DEFUN(__libogc_sbrk_r,(ptr,incr),
			 struct _reent *ptr _AND
					 ptrdiff_t incr)
{
	u32 level;
	char *heap_end = 0;
	char *prev_heap = 0;

	_CPU_ISR_Disable(level);
	heap_end = (char*)SYS_GetArenaLo();

	if((heap_end+incr)>(char*)SYS_GetArenaHi()) {

		ptr->_errno = ENOMEM;
		prev_heap = (char *)-1;

	} else {

		prev_heap = heap_end;
		SYS_SetArenaLo((void*)(heap_end+incr));
	}
	_CPU_ISR_Restore(level);

	return (void*)prev_heap;	
}

#endif
