/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2003 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: decoder.h,v 1.3 2005/12/09 09:27:10 shagkur Exp $
 */

# ifndef LIBMAD_DECODER_H
# define LIBMAD_DECODER_H

# include <gctypes.h>
# include "stream.h"
# include "frame.h"
# include "synth.h"

enum mad_decoder_mode {
  MAD_DECODER_MODE_SYNC  = 0,
  MAD_DECODER_MODE_ASYNC
};

enum mad_flow {
  MAD_FLOW_CONTINUE = 0x0000,	/* continue normally */
  MAD_FLOW_STOP     = 0x0010,	/* stop decoding normally */
  MAD_FLOW_BREAK    = 0x0011,	/* stop decoding and signal an error */
  MAD_FLOW_IGNORE   = 0x0020	/* ignore the current frame */
};

struct mad_decoder {
  enum mad_decoder_mode mode;

  s32 options;

  struct {
    s32 pid;
    s32 in;
    s32 out;
  } async;

  struct {
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
  } *sync;

  void *cb_data;

  enum mad_flow (*input_func)(void *, struct mad_stream *);
  enum mad_flow (*header_func)(void *, struct mad_header const *);
  enum mad_flow (*filter_func)(void *,
			       struct mad_stream const *, struct mad_frame *);
  enum mad_flow (*output_func)(void *,
			       struct mad_header const *, struct mad_pcm *);
  enum mad_flow (*error_func)(void *, struct mad_stream *, struct mad_frame *);
  enum mad_flow (*message_func)(void *, void *, u32 *);
};

void mad_decoder_init(struct mad_decoder *, void *,
		      enum mad_flow (*)(void *, struct mad_stream *),
		      enum mad_flow (*)(void *, struct mad_header const *),
		      enum mad_flow (*)(void *,
					struct mad_stream const *,
					struct mad_frame *),
		      enum mad_flow (*)(void *,
					struct mad_header const *,
					struct mad_pcm *),
		      enum mad_flow (*)(void *,
					struct mad_stream *,
					struct mad_frame *),
		      enum mad_flow (*)(void *, void *, u32 *));
s32 mad_decoder_finish(struct mad_decoder *);

# define mad_decoder_options(decoder, opts)  \
    ((void) ((decoder)->options = (opts)))

s32 mad_decoder_run(struct mad_decoder *, enum mad_decoder_mode);
s32 mad_decoder_message(struct mad_decoder *, void *, u32 *);

# endif
