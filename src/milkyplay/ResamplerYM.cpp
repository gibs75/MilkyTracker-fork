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

#include "ChannelMixer.h"

#include "ResamplerMacros.h"

#include "ResamplerYM.h"
 
#include <algorithm>

extern "C"
{
#	undef MEM_FREE
#	undef ASSERT

#	include "DEMOSDK/BASTYPES.H"
#	include "DEMOSDK/SYNTHYM.H"

	bool				g_ymplayerInitialized = false;
	SNDYMplayer			g_ymplayer;
	SNDYMsoundSet		g_ymsoundSet;

	// allocator
	static void* stdAlloc(void* _alloc, u32 _size)
	{
		IGNORE_PARAM(_alloc);
		return malloc(_size);
	}

	static void stdFree(void* _alloc, void* _adr)
	{
		IGNORE_PARAM(_alloc);
		free(_adr);
	}

	MEMallocator stdAllocator = { NULL, stdAlloc, stdAlloc, stdFree };

	void TrackOutputDebugString(char* temp) 
	{
		OutputDebugString(temp);
	}

}

#include "DEMOSDK/PC/YMEMUL.H"

const mp_uint32 CACHE_LENGTH	  = 4096;

 // Resampler without interpolation or ramping
ResamplerYM* ResamplerYM::ms_instance = nullptr;

void ResamplerYM::YMparam::Init()
{
	memset(this, 0, sizeof(*this));
	scorevolume = 15;
}

ResamplerYM::ResamplerYM() 
	: isinitialized(false) 
{
	for (auto& i : ymparams)
	{
		i.Init();
	}

	memset(m_bufferCopy, 0, sizeof(m_bufferCopy));

	m_cache = new mp_sint32[CACHE_LENGTH];

	m_STebalanceLeft = m_STebalanceRight = 20;

	assert(ms_instance == nullptr);
	ms_instance = this;
}

const char* ResamplerYM::GetError()
{
	return SNDYMgetError();
}

mp_sint32* ResamplerYM::getBufferCopy(mp_uint32 _index, mp_uint32 _count)
{
	assert((_index + _count) <= BUFFERCOPY_LENGTH);
	return &m_bufferCopy[_index];
}

bool ResamplerYM::Init() 
{
	if (isinitialized == false)
	{
		SNDYMsoundSet* soundsSet = NULL;

		memset(&g_ymplayer, 0, sizeof(g_ymplayer));
		EMULinitYM();
		GetCurrentDirectory(sizeof(m_default), m_default);
		strcat(m_default, "\\SYNTHYM.INI");
		isinitialized = true;
		SNDYMinitPlayer (&stdAllocator, &g_ymplayer, &g_ymsoundSet);
	}

	return isinitialized;
}

void ResamplerYM::Stop()
{
	SNDYMstop(&g_ymplayer);
	memset(ymparams, 0, sizeof(ymparams));

	for (u32 t = 0 ; t < SND_YM_NB_CHANNELS ; t++)
	{ 
		ymparams[t].scorevolume = 15;
	}
}

bool ResamplerYM::Reload()
{
	Stop();

	SNDYMfreeSounds(&stdAllocator, &g_ymsoundSet);

	if (m_path[0] != 0)
	{	
		return SNDYMloadSounds (&stdAllocator, m_path, &g_ymsoundSet);
	}
	else
	{
		return true;
	}
}

mp_uint32 ResamplerYM::GetNbSounds()
{
	return g_ymsoundSet.nbSounds;
}

const char* ResamplerYM::GetSoundName(mp_uint32 _index)
{
	assert (_index < g_ymsoundSet.nbSounds);
	return g_ymsoundSet.names[_index];
}

void ResamplerYM::SetMute(mp_uint32 _index, bool _mute)
{
	assert(_index < 3);
	g_ymplayer.commands[_index].mute = _mute;

	if (_mute)
	{
		ymparams[_index].keyspressed = ymparams[_index].keysjustpressed = false;
	}
}

void ResamplerYM::UpdateScore()
{
	SNDYMcommand* command = g_ymplayer.commands;


	for (int i = 0; i < SND_YM_NB_CHANNELS; i++, command++)
	{
		auto& ymparam = ymparams[i];

		command->soundindex = ymparam.soundsindex;

		command->pitchbendrange    = ymparam.pitchbendrange;
		command->pitchbendticks    = ymparam.pitchbendticks;
		command->portamientoticks  = ymparam.portamientoticks;
		command->key			   = ymparam.keys;
		command->finetune		   = ymparam.finetune;
		command->pressed		   = ymparam.keyspressed;
		command->justpressed	   = ymparam.keysjustpressed;
		command->scorevolume	   = ymparam.scorevolume;
		command->scorevolumeset    = ymparam.scorevolumeset;

		ymparam.scorevolumeset	   = false;
		ymparam.keysjustpressed    = false;
		ymparam.pitchbendrange	   = 0;
	}

#	ifdef _DEBUG
	char temp[128];

	sprintf(temp, "%d %d %d - %d %d %d - %d %d %d\n", 
		g_ymplayer.commands[0].justpressed, g_ymplayer.commands[0].pressed, g_ymplayer.commands[0].key, 
		g_ymplayer.commands[1].justpressed, g_ymplayer.commands[1].pressed, g_ymplayer.commands[1].key, 
		g_ymplayer.commands[2].justpressed, g_ymplayer.commands[2].pressed, g_ymplayer.commands[2].key );

	OutputDebugString(temp);
#	endif

	if (g_ymplayer.soundSet->nbSounds > 0)
	{
		SNDYMupdate(&g_ymplayer);
	}
}


void ResamplerYM::fillBufferCopy (mp_sint32* buffer, mp_uint32 count, mp_sint32 voll, mp_sint32 volr)
{
	mp_sint32* p = m_cache;

	for (unsigned t = 0 ; t < (count << 1) ; )
	{
		buffer [t] += (m_cache[t] * voll) >> 6;
		t++;
		buffer [t] += (m_cache[t] * volr) >> 6;
		t++;
	}

	unsigned remain = count << 1;

	while (remain > 0)
	{
		unsigned max = (BUFFERCOPY_LENGTH - m_bufferCopyIndex);

		unsigned lentocopy = std::min(remain, max);

		memcpy(&m_bufferCopy[m_bufferCopyIndex], p, lentocopy * sizeof(*m_bufferCopy));
		p += lentocopy;
		
		m_bufferCopyIndex += lentocopy;
		if (m_bufferCopyIndex >= BUFFERCOPY_LENGTH)
		{
			m_bufferCopyIndex = 0;
		}

		remain -= lentocopy;
	}
}

void ResamplerYM::addBlockFull(mp_sint32* buffer, ChannelMixer::TMixerChannel* chn, mp_uint32 count)
{
	mp_sint32 voll = 0;
	mp_sint32 volr = 0;

	getVolumeLR(chn, voll, volr);

	//FULLMIXER_TEMPLATE(FULLMIXER_8BIT_BLS, FULLMIXER_16BIT_NORMAL, 16, 0);

	EMULplaysound(m_cache, count);

	fillBufferCopy(buffer, count, voll, volr);
}

void ResamplerYM::addBlockNoCheck(mp_sint32* buffer, ChannelMixer::TMixerChannel* chn, mp_uint32 count)
{
	mp_sint32 voll = 0;
	mp_sint32 volr = 0;

	getVolumeLR(chn, voll, volr);

	//mp_sint32 sd1, sd2;

	//NOCHECKMIXER_TEMPLATE(NOCHECKMIXER_8BIT_BLS, NOCHECKMIXER_16BIT_NORMAL);
	EMULplaysound(m_cache, count);

	fillBufferCopy(buffer, count, voll, volr);
}

