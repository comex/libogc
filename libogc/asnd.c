/* ASNDLIB -> accelerated sound lib using the DSP

Copyright (c) 2008 Hermes <www.entuwii.net>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are 
permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of 
  conditions and the following disclaimer. 
- Redistributions in binary form must reproduce the above copyright notice, this list 
  of conditions and the following disclaimer in the documentation and/or other 
  materials provided with the distribution. 
- The names of the contributors may not be used to endorse or promote products derived 
  from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <ogcsys.h>
#include <gccore.h>

#include "asnd.h"
#include "unistd.h"

#include "dsp_mixer.h" // dsp code


// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_ARDMA         0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

static vu16* const dsp_reg = (u16*)0xCC005000;

static int DSP_DI_HANDLER=1;

static int global_pause=1;
static u32 global_counter=0;
static void (*global_callback)()=NULL;

static u32 time_of_process;
static u32 my_time;


u32 gettick();

typedef struct 
{
	void *out_buf;	// output buffer 4096 bytes aligned to 32

	u32 delay_samples; // samples per delay to start (48000 == 1sec)

	u32 flags;	// (step<<16) | (loop<<2) | (type & 3) used in DSP side

	u32 start_addr; // internal addr counter
	u32 end_addr;   // end voice physical pointer(bytes without alignament, but remember it reads in blocks of 32 bytes (use padding to the end))

	u32 freq;	// freq operation

	s16 left, right; // internally used to store de last sample played

	u32 counter;	// internally used to convert freq to 48000Hz samples

	u16 volume_l,volume_r; // volume (from 0 to 256)

	u32 start_addr2;	// initial voice2 physical pointer (bytes aligned  32 bytes) (to do a ring)
	u32 end_addr2;		// end voice2 physical pointer(bytes without alignament, but remember it reads in blocks of 32 bytes (use padding to the end))

	u16 volume2_l,volume2_r; // volume (from 0 to 256) for voice 2

	u32 backup_addr; // initial voice physical pointer backup (bytes aligned to 32 bytes): It is used for test pointers purpose

	u32 tick_counter; // voice tick counter

	u32 pad[2];

} t_sound_data;

#define MAX_SND_VOICES 16     // max playable voices

static t_sound_data sound_data[MAX_SND_VOICES] __attribute__ ((aligned (32)));
static t_sound_data sound_data_dma __attribute__ ((aligned (32)));

struct
{
	void (*func)(s32 voice);
} snd_callbacks[MAX_SND_VOICES];


/************************************************************************************************************************************************/
/* DSP FLOW:


ASND_Init:
--------
	load dsp program

	DSP_SendMailTo(0x0123); // command to fix the data operation
	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL(&sound_data_dma)); // send the data operation mem
	AUDIO_Init(NULL);
	.....

From Audio DMA Interrupt Handler:
--------------------------------

	Play the DMA and refresh the datas for the operation mem.

	voice=0; // select voice 0
	DSP_SendMailTo(0x111);	// compute the first voice in the DSP, filling with zeroes the internal output buffer
	//DSP_SendMailTo(0x112);	// compute the first voice in the DSP  mixing the samples with the external output buffer 

From DSP Interrupt Handler:
--------------------------

	Update the data operation mem (sound_data_dma)

	if(voice>=16) DSP_SendMailTo(0x666); // Send the internal buffer to the external output buffer (4096 bytes for 1024 Stereo samples 16 bits)

	else DSP_SendMailTo(0x222); // compute next voice in the DSP mixing the samples internally


*/

/************************************************************************************************************************************************/

#undef SND_BUFFERSIZE
#define SND_BUFFERSIZE (4096) // don´t modify this value

#define VOICE_UPDATEADD   256
#define VOICE_UPDATE      128
#define VOICE_VOLUPDATE    64
#define VOICE_PAUSE        32
#define VOICE_SETLOOP       4

static s16 audio_buf[2][SND_BUFFERSIZE] __attribute__ ((aligned (32)));
static s32 curr_audio_buf=0;

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

// UTILS

static char * snd_set0b( char *p, int n)
{
	while(n>0) {*p++=0;n--;}
	return p;
}

static s32 * snd_set0w( s32 *p, int n)
{
	while(n>0) {*p++=0;n--;}
	return p;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
// DSP IRQ HANDLER

static int snd_chan=0;

static int dsp_complete=1;


static void my_dsp_handler(u32 irq, void *p)
{
int n;
u32 mail;
	
	dsp_reg[5] = (dsp_reg[5]&~(DSPCR_AIINT|DSPCR_ARINT)) |DSPCR_DSPINT;

	if(DSP_DI_HANDLER) return;

	mail = DSP_ReadMailFrom();

/*
mail==0xbebe0002  normal end
mail==0xbebe0003  end and buffer changed
*/
		
	DCInvalidateRange(&sound_data_dma, sizeof(t_sound_data));


	if(snd_chan>=16) 
		{
		if(!global_pause) global_counter++;

		if(!dsp_complete)
			{
			time_of_process=(gettick()-my_time)*1000/TB_TIMER_CLOCK;
			}
		
		dsp_complete=1;
		return;
		}

	sound_data_dma.freq=sound_data[snd_chan].freq;

	if(sound_data[snd_chan].flags & VOICE_UPDATE) // new song
		{
		sound_data[snd_chan].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_PAUSE | VOICE_UPDATEADD);
		//sound_data[snd_chan].out_buf= (void *) MEM_VIRTUAL_TO_PHYSICAL((void *) audio_buf[curr_audio_buf]);
		sound_data_dma=sound_data[snd_chan];
		}
	else
		{
		
		if(sound_data[snd_chan].flags & VOICE_VOLUPDATE)
			{
			sound_data[snd_chan].flags &=~VOICE_VOLUPDATE;
			sound_data_dma.volume_l=sound_data_dma.volume2_l=sound_data[snd_chan].volume2_l;
			sound_data_dma.volume_r=sound_data_dma.volume2_r=sound_data[snd_chan].volume2_r;
			}

		
		//if(mail==0xbebe0003) sound_data_dma.flags|=VOICE_SETCALLBACK;

		if(sound_data_dma.start_addr>=sound_data_dma.end_addr || !sound_data_dma.start_addr)
			{
			sound_data_dma.backup_addr=sound_data_dma.start_addr=sound_data_dma.start_addr2;
			sound_data_dma.end_addr=sound_data_dma.end_addr2;
			if(!(sound_data[snd_chan].flags & VOICE_SETLOOP)) {sound_data_dma.start_addr2=0;sound_data_dma.end_addr2=0;}
			sound_data_dma.volume_l=sound_data_dma.volume2_l;
			sound_data_dma.volume_r=sound_data_dma.volume2_r;
			}

		if(sound_data[snd_chan].start_addr2 && (sound_data[snd_chan].flags & VOICE_UPDATEADD))
			{
			sound_data[snd_chan].flags &=~VOICE_UPDATEADD;
			if(!sound_data[snd_chan].start_addr || !sound_data_dma.start_addr)
				{
				sound_data_dma.backup_addr=sound_data_dma.start_addr=sound_data[snd_chan].start_addr2;
				sound_data_dma.end_addr=sound_data[snd_chan].end_addr2;
				sound_data_dma.start_addr2=sound_data[snd_chan].start_addr2;
				sound_data_dma.end_addr2=sound_data[snd_chan].end_addr2;
				if(!(sound_data[snd_chan].flags & VOICE_SETLOOP)) {sound_data_dma.start_addr2=0;sound_data_dma.end_addr2=0;}
				sound_data_dma.volume_l=sound_data[snd_chan].volume2_l;
				sound_data_dma.volume_r=sound_data[snd_chan].volume2_r;
				}
			else
				{
				sound_data_dma.start_addr2=sound_data[snd_chan].start_addr2;
				sound_data_dma.end_addr2=sound_data[snd_chan].end_addr2;
				sound_data_dma.volume2_l=sound_data[snd_chan].volume2_l;
				sound_data_dma.volume2_r=sound_data[snd_chan].volume2_r;
				}
			
			}

		if(!snd_callbacks[snd_chan].func && (!sound_data_dma.start_addr && !sound_data_dma.start_addr2)) sound_data[snd_chan].flags=0;
		sound_data_dma.flags=sound_data[snd_chan].flags;
		sound_data[snd_chan]=sound_data_dma;
		}

	if(sound_data[snd_chan].flags>>16)
			{
			if(!sound_data[snd_chan].delay_samples && !(sound_data[snd_chan].flags & VOICE_PAUSE)) sound_data[snd_chan].tick_counter++;
			}

	snd_chan++;
	
	if(!snd_callbacks[snd_chan].func && (!sound_data[snd_chan].start_addr && !sound_data[snd_chan].start_addr2)) sound_data[snd_chan].flags=0;
	
	while(snd_chan<16 && !(sound_data[snd_chan].flags>>16)) snd_chan++;
		
	if(snd_chan>=16) 
		{
		snd_chan++;
		DCFlushRange(&sound_data_dma, sizeof(t_sound_data));
		DSP_SendMailTo(0x666);
		return;
		}

	sound_data_dma=sound_data[snd_chan];

	DCFlushRange(&sound_data_dma, sizeof(t_sound_data));
	DSP_SendMailTo(0x222); // send the voice and mix the samples of the buffer

	// callback strategy for next channel
	n=snd_chan+1;

	while(n<16 && !(sound_data[n].flags>>16)) n++;

	if(n<16)
		{
		if(!sound_data[n].start_addr2 && (sound_data[n].flags>>16) && snd_callbacks[n].func) snd_callbacks[n].func(n);

		if(sound_data[snd_chan].flags & VOICE_VOLUPDATE)
			{
			sound_data[snd_chan].flags &=~VOICE_VOLUPDATE;
			}

		if(sound_data[n].flags & VOICE_UPDATE) // new song
			{
			sound_data[n].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_PAUSE | VOICE_UPDATEADD);
			}

		if(!snd_callbacks[n].func && (!sound_data[n].start_addr && !sound_data[n].start_addr2)) sound_data[n].flags=0;
		}
}


/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
// AUDIO CALLBACK

static void audio_dma_callback()
{
int n;

unsigned mail;

	AUDIO_StopDMA();
	AUDIO_InitDMA((u32)audio_buf[curr_audio_buf],SND_BUFFERSIZE);


	if(DSP_DI_HANDLER || global_pause)
		{
		snd_set0w((s32 *) audio_buf[curr_audio_buf],SND_BUFFERSIZE/4);
		DCFlushRange(audio_buf[curr_audio_buf],SND_BUFFERSIZE);
		}

	AUDIO_StartDMA();

	if(DSP_DI_HANDLER || global_pause) return;



	if(dsp_complete==0) {return;} // wait to the DSP

	curr_audio_buf ^= 1;

	dsp_complete=0;
	DCInvalidateRange(&sound_data_dma, sizeof(t_sound_data));

	for(n=0;n<16;n++)
		{
		sound_data[n].out_buf= (void *) MEM_VIRTUAL_TO_PHYSICAL((void *) audio_buf[curr_audio_buf]);
		}


	if(global_callback) global_callback(); // call to global call back


	snd_chan=0;
	
	if(!sound_data[snd_chan].start_addr2 && (sound_data[snd_chan].flags>>16) && snd_callbacks[snd_chan].func) snd_callbacks[snd_chan].func(snd_chan);
	
	if(sound_data[snd_chan].flags & VOICE_VOLUPDATE)
			{
			sound_data[snd_chan].flags &=~VOICE_VOLUPDATE;
			}

	if(sound_data[snd_chan].flags & VOICE_UPDATE) // new song
		{
		sound_data[snd_chan].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE | VOICE_PAUSE | VOICE_UPDATEADD);
		}
	else
		{
		
		if(sound_data[snd_chan].start_addr>=sound_data[snd_chan].end_addr)
			{
			sound_data[snd_chan].backup_addr=sound_data[snd_chan].start_addr=sound_data[snd_chan].start_addr2;sound_data[snd_chan].start_addr2=0;
			sound_data[snd_chan].end_addr=sound_data[snd_chan].end_addr2;sound_data[snd_chan].end_addr2=0;
			sound_data[snd_chan].volume_l=sound_data[snd_chan].volume2_l;
			sound_data[snd_chan].volume_r=sound_data[snd_chan].volume2_r;
			}

		if(sound_data[snd_chan].start_addr2 && (sound_data[snd_chan].flags & VOICE_UPDATEADD))
			{
			sound_data[snd_chan].flags &=~VOICE_UPDATEADD;

			if(!sound_data[snd_chan].start_addr)
				{
				sound_data[snd_chan].backup_addr=sound_data[snd_chan].start_addr=sound_data[snd_chan].start_addr2;
				sound_data[snd_chan].end_addr=sound_data[snd_chan].end_addr2;
				if(!(sound_data[snd_chan].flags & VOICE_SETLOOP)) {sound_data[snd_chan].start_addr2=0;sound_data[snd_chan].end_addr2=0;}
				sound_data[snd_chan].volume_l=sound_data[snd_chan].volume2_l;
				sound_data[snd_chan].volume_r=sound_data[snd_chan].volume2_r;
				}
			
			}
		
		}

	if(!snd_callbacks[snd_chan].func && (!sound_data[snd_chan].start_addr && !sound_data[snd_chan].start_addr2)) sound_data[snd_chan].flags=0;
	sound_data_dma=sound_data[snd_chan];
	

	DCFlushRange(&sound_data_dma, sizeof(t_sound_data));

	if(DSP_CheckMailFrom()) // paranoid code XDDDD
		mail = DSP_ReadMailFrom();

	
	my_time=gettick();

	DSP_SendMailTo(0x111); // send the first voice and clear the buffer

	// callback strategy for next channel
	n=snd_chan+1;

	while(n<16 && !(sound_data[n].flags>>16)) n++;
		
	if(n<16)
		{
		if(!sound_data[n].start_addr2 && (sound_data[n].flags>>16) && snd_callbacks[n].func) snd_callbacks[n].func(n);

		if(sound_data[n].flags & (VOICE_VOLUPDATE | VOICE_UPDATEADD))
			{
			sound_data[n].flags &=~(VOICE_VOLUPDATE | VOICE_UPDATEADD);
			}

		if(sound_data[n].flags & VOICE_UPDATE) // new song
			{
			sound_data[n].flags &=~(VOICE_UPDATE | VOICE_VOLUPDATE| VOICE_PAUSE | VOICE_UPDATEADD);
			}

		if(!snd_callbacks[n].func && (!sound_data[n].start_addr && !sound_data[n].start_addr2)) sound_data[n].flags=0;
		}

}

static int asnd_init=0;

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_Init()
{
u32 mail;
int n;
unsigned level;
static dsptask_t task;

	ASND_End();

	global_pause=1;
	global_counter=0;
	global_callback=NULL;

	for(n=0;n<16;n++)
		{
		snd_set0w((void *) &sound_data[n], sizeof(t_sound_data)/4);
		}

	DSP_DI_HANDLER=1;

	if(!asnd_init)
		{
		asnd_init=1;
		DSP_Init();
		level=IRQ_Disable();
		IRQ_Request(IRQ_DSP_DSP, my_dsp_handler,NULL);
		IRQ_Restore(level);

		snd_set0b((void *) &task,sizeof(task));

		task.init_vec=0x10;
		DCFlushRange(dsp_mixer, size_dsp_mixer);
		task.iram_maddr=(void *)MEM_VIRTUAL_TO_PHYSICAL(dsp_mixer);
		task.iram_len=size_dsp_mixer;
		task.iram_addr=0;

		DSP_AddTask(&task);
		}


	DSP_SendMailTo(0x0123); // command to fix the data operation
	while(DSP_CheckMailTo());
	while(!DSP_CheckMailFrom());
	mail = DSP_ReadMailFrom();

	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL(&sound_data_dma)); //send the data operation mem
	while(DSP_CheckMailTo());
	while(!DSP_CheckMailFrom());
	mail = DSP_ReadMailFrom();

	AUDIO_Init(NULL);
	AUDIO_StopDMA();

	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);

	snd_set0w((s32 *) audio_buf[0], SND_BUFFERSIZE>>2);
	snd_set0w((s32 *) audio_buf[1], SND_BUFFERSIZE>>2);

	DCFlushRange(audio_buf[0],SND_BUFFERSIZE);
	DCFlushRange(audio_buf[1],SND_BUFFERSIZE);

	AUDIO_RegisterDMACallback(audio_dma_callback);
	DSP_DI_HANDLER=0;

	dsp_complete=1;
	AUDIO_InitDMA((u32)audio_buf[curr_audio_buf],SND_BUFFERSIZE);
	AUDIO_StartDMA();
	

}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_End()
{
	if(asnd_init)
		{
		AUDIO_StopDMA();
		DSP_DI_HANDLER=1;
		usleep(100);
		AUDIO_RegisterDMACallback(NULL);
		asnd_init=0;
		DSP_DI_HANDLER=1;
		}
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_SetVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r, void (*callback) (s32 voice))
{
u32 level;
u32 flag_h=0;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(size_snd<=0 || snd==NULL) return SND_INVALID; // invalid voice

	DCFlushRange(snd, size_snd);

	if(pitch<1) pitch=1;
	if(pitch>MAX_PITCH) pitch=MAX_PITCH;

	volume_l &=255;
	volume_r &=255;

	delay=(u32) (48000LL*((u64) delay)/1000LL);

	format&=3;

	switch(format)
		{
		case 0:
			flag_h=1<<16;break;
		case 1:
			flag_h=2<<16;break;
		case 2:
			flag_h=2<<16;break;
		case 3:
			flag_h=4<<16;break;
		}

	format|= flag_h | VOICE_UPDATE;

	level=IRQ_Disable();

	sound_data[voice].left=0;
	sound_data[voice].right=0;
	sound_data[voice].counter=0;

	sound_data[voice].freq=pitch;

	sound_data[voice].delay_samples=delay;

	sound_data[voice].volume_l= volume_l;
	sound_data[voice].volume_r= volume_r;

	sound_data[voice].backup_addr=sound_data[voice].start_addr=MEM_VIRTUAL_TO_PHYSICAL(snd);
	sound_data[voice].end_addr=MEM_VIRTUAL_TO_PHYSICAL(snd)+(size_snd);

	sound_data[voice].start_addr2=0;
	sound_data[voice].end_addr2=0;	

	sound_data[voice].volume2_l= volume_l;
	sound_data[voice].volume2_r= volume_r;

	sound_data[voice].flags=format;
	sound_data[voice].tick_counter=0;

	snd_callbacks[voice].func=(void*) callback;
	IRQ_Restore(level);

return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_SetInfiniteVoice(s32 voice, s32 format, s32 pitch,s32 delay, void *snd, s32 size_snd, s32 volume_l, s32 volume_r)
{
u32 level;
u32 flag_h=0;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(size_snd<=0 || snd==NULL) return SND_INVALID; // invalid voice

	DCFlushRange(snd, size_snd);

	if(pitch<1) pitch=1;
	if(pitch>MAX_PITCH) pitch=MAX_PITCH;

	volume_l &=255;
	volume_r &=255;

	delay=(u32) (48000LL*((u64) delay)/1000LL);

	format&=3;

	switch(format)
		{
		case 0:
			flag_h=1<<16;break;
		case 1:
			flag_h=2<<16;break;
		case 2:
			flag_h=2<<16;break;
		case 3:
			flag_h=4<<16;break;
		}

	format|= flag_h | VOICE_UPDATE | VOICE_SETLOOP;

	level=IRQ_Disable();

	sound_data[voice].left=0;
	sound_data[voice].right=0;
	sound_data[voice].counter=0;

	sound_data[voice].freq=pitch;

	sound_data[voice].delay_samples=delay;

	sound_data[voice].volume_l= volume_l;
	sound_data[voice].volume_r= volume_r;

	sound_data[voice].backup_addr=sound_data[voice].start_addr=MEM_VIRTUAL_TO_PHYSICAL(snd);
	sound_data[voice].end_addr=MEM_VIRTUAL_TO_PHYSICAL(snd)+(size_snd);

	sound_data[voice].start_addr2=sound_data[voice].start_addr;
	sound_data[voice].end_addr2=sound_data[voice].end_addr;	

	sound_data[voice].volume2_l= volume_l;
	sound_data[voice].volume2_r= volume_r;

	sound_data[voice].flags=format;
	sound_data[voice].tick_counter=0;

	snd_callbacks[voice].func=NULL;
	IRQ_Restore(level);

return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_AddVoice(s32 voice, void *snd, s32 size_snd)
{
u32 level;
s32 ret=SND_OK;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	if(size_snd<=0 || snd==NULL) return SND_INVALID; // invalid voice

	if((sound_data[voice].flags & (VOICE_UPDATE | VOICE_UPDATEADD)) || !(sound_data[voice].flags>>16)) return SND_INVALID; // busy or unused voice

	DCFlushRange(snd, size_snd);
	level=IRQ_Disable();
	
	if(sound_data[voice].start_addr2==0)
		{
		
		sound_data[voice].start_addr2=MEM_VIRTUAL_TO_PHYSICAL(snd);
		sound_data[voice].end_addr2=MEM_VIRTUAL_TO_PHYSICAL(snd)+(size_snd);
	
		sound_data[voice].flags&=~VOICE_SETLOOP;
		sound_data[voice].flags|=VOICE_UPDATEADD;
		} else ret=SND_BUSY;

	IRQ_Restore(level);

return ret;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_TestVoiceBufferReady(s32 voice)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return 0; // invalid voice: not ready (of course XD)
	if(sound_data[voice].start_addr && sound_data[voice].start_addr2) return 0; // not ready

return 1; // ready
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_TestPointer(s32 voice, void *pointer)
{
u32 level;
u32 addr2=(u32) MEM_VIRTUAL_TO_PHYSICAL(pointer);
int ret=SND_OK;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	level=IRQ_Disable();

	if(sound_data[voice].backup_addr==addr2 /*&& sound_data[voice].end_addr>(addr2)*/) ret=SND_BUSY;
	else
	if(sound_data[voice].start_addr2==addr2 /*&& sound_data[voice].end_addr2>(addr2)*/) ret=SND_BUSY;

	IRQ_Restore(level);

return ret;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_PauseVoice(s32 voice, s32 pause)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice
	if(pause) sound_data[voice].flags|=VOICE_PAUSE; else sound_data[voice].flags&=~VOICE_PAUSE;
	
return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_StopVoice(s32 voice)
{
u32 level;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice
	
	level=IRQ_Disable();

	sound_data[voice].backup_addr=sound_data[voice].start_addr=sound_data[voice].start_addr2=0;
	sound_data[voice].end_addr=sound_data[voice].end_addr2=0;
	sound_data[voice].flags=0;

	IRQ_Restore(level);

return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_StatusVoice(s32 voice)
{
u32 level;
s32 status=SND_WORKING;

if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice

	level=IRQ_Disable();
	if(!(sound_data[voice].flags>>16)) status=SND_UNUSED;
	if(sound_data[voice].flags & VOICE_PAUSE) status=SND_WAITING;
	IRQ_Restore(level);

return status;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_ChangeVolumeVoice(s32 voice, s32 volume_l, s32 volume_r)
{
u32 level;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice
	
	volume_l &=255;
	volume_r &=255;

	level=IRQ_Disable();
	sound_data[voice].flags |=VOICE_VOLUPDATE;
	sound_data[voice].volume_l= sound_data[voice].volume2_l= volume_l;
	sound_data[voice].volume_r= sound_data[voice].volume2_r= volume_r;
	IRQ_Restore(level);

return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetTickCounterVoice(s32 voice)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return 0; // invalid voice

	return (sound_data[voice].tick_counter * SND_BUFFERSIZE/4);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetTimerVoice(s32 voice)
{
	if(voice<0 || voice>=MAX_SND_VOICES) return 0; // invalid voice
	
	return (sound_data[voice].tick_counter * SND_BUFFERSIZE/4)/48;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_Pause(s32 pause)
{
	global_pause=pause;

}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_Is_Paused()
{
	return global_pause;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetTime()
{
	return (global_counter * SND_BUFFERSIZE/4)/48;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetSampleCounter()
{
	return (global_counter * SND_BUFFERSIZE/4);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetSamplesPerTick()
{
	return (SND_BUFFERSIZE/4);
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_SetTime(u32 time)
{
	global_counter=48*time;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void ASND_SetCallback(void (*callback)())
{
	global_callback=callback;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_GetAudioRate()
{
	return 48000;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_GetFirstUnusedVoice()
{

s32 n;	
	
	for(n=1;n<MAX_SND_VOICES;n++)
	   if(!(sound_data[n].flags>>16)) return n;
    
	if(!(sound_data[n].flags>>16)) return 0; // voice 0 is a special case

return SND_INVALID;  // all voices used

}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ASND_ChangePitchVoice(s32 voice, s32 pitch)
{
u32 level;

	if(voice<0 || voice>=MAX_SND_VOICES) return SND_INVALID; // invalid voice
	
	if(pitch<1) pitch=1;
	if(pitch>144000) pitch=144000;

	level=IRQ_Disable();
	sound_data[voice].freq= pitch;
	IRQ_Restore(level);

return SND_OK;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

u32 ASND_GetDSP_PercentUse()
{
	return (time_of_process)*100/21333; // time_of_process = nanoseconds , 1024 samples= 21333 nanoseconds
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int ANote2Freq(int note, int freq_base,int note_base)
{
int n;
static int one=1;
static u32 tab_piano_frac[12];

	if(one)
		{
		float note=1.0f;
		one=0;
		for(n=0;n<12;n++) // table note
			{
			tab_piano_frac[n]=(u32)(10000.0f*note);
			note*=1.0594386f;
			}
		}

	// obtiene octava 3 (notas 36 a 47)

	n=(note/12)-(note_base/12);
	if(n>=0) freq_base<<=n;
		else freq_base>>= -n;


	if(freq_base<=0x1ffff) // Math precision
		n=(s32) (((u32)freq_base)*tab_piano_frac[(note % 12)]/tab_piano_frac[(note_base % 12)]);
	else 
		n=(s32) (((u64)freq_base)*((u64) tab_piano_frac[(note % 12)])/((u64) tab_piano_frac[(note_base % 12)]));
	  

return n;
}

// END

