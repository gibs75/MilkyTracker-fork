/*-----------------------------------------------------------------------------------------------
  The MIT License (MIT)

  Copyright (c) 2015-2022 J.Hubert

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
  and associated documentation files (the "Software"), 
  to deal in the Software without restriction, including without limitation the rights to use, 
  copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
  and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies 
  or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-------------------------------------------------------------------------------------------------*/

#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <string.h>
#include <mmsystem.h>
#include <dsound.h>
#include <math.h>
#include <malloc.h>

#undef MEM_FREE  // collision...

extern "C"
{
    #include "DEMOSDK\BASTYPES.H"
    #include "DEMOSDK\STANDARD.H"
}

#include "DEMOSDK\PC\YMEMUL.H"

#define emul_SOUND_FREQ 50000

#define EMUL_YM_NBCHANNELS 3
#define EMUL_YM_CHIPFREQ 2*1000*1000

extern "C"
{
#   include "EXTERN\sound.c"
}
    

static u8 emulYMselectedReg = 0;

extern "C" void emulYMselectReg(u8 _regnum)
{
    emulYMselectedReg = _regnum;
}

extern "C" void emulYMwriteReg(u8 _data)
{
    Sound_WriteReg (emulYMselectedReg, _data);
}

static u8 emuYMinPortCallback (int port_id, void* user_data)
{
    return 0;
}

static void emuYMoutPortCallback (int port_id, u8 data, void* user_data)
{
}


extern "C" void EMULinitYM (void)
{
    Ym2149_Init();
}

static s32 g_EMULleftChannelAmp = 64;
static s32 g_EMULrightChannelAmp = 64;


extern "C" void EMULplaysound (int* _data, u32 _nbSamples)
{
    u32  i;
    s32* d = (s32*)_data;

    for (i = 0 ; i < _nbSamples ; i++)
    {
		s32 ymvalue = YM2149_NextSample();

		s32 val = ymvalue * g_EMULleftChannelAmp;
		val >>= 4;

        *d++ = val;

        val = ymvalue * g_EMULrightChannelAmp;
		val >>= 4;

		*d++ = val;
	}
}
