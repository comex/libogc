#ifndef __OS_H__
#define __OS_H__

#ifdef WIN32
	/* windows */
	#define isnan(x)		_isnan(x)
	#define isinf(x)		!_finite(x)

	/* disable warnings I don't care about */
	#pragma warning(disable:4244)		/* possible loss of data conversion	*/
	#pragma warning(disable:4273)		/* inconsistent dll linkage			*/
	#pragma warning(disable:4217)
#else
	/* nix/gekko */
	#ifdef GEKKO
		#include <gccore.h>
		#include <ogcsys.h>
		#include "bte.h"
		#include "network.h"
		#include "lwp_queue.h"
		#include "asm.h"
		#include "processor.h"
		#include "lwp_wkspace.h"
	#else
	#endif
#endif

#endif
