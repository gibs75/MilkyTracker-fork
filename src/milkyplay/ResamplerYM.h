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

#ifndef __RESAMPLERYM_H__
#define __RESAMPLERYM_H__

#include <stdio.h>

#include "ResamplerMacros.h"

extern "C"
{
	struct SNDsynYMsound_;
}

// Resampler without interpolation or ramping

class ResamplerYM : public ChannelMixer::ResamplerBase
{
public:
	static const mp_uint32 BUFFERCOPY_LENGTH = 2000;

private:
	char m_path[512];
	char m_default[512];

	mp_ubyte m_STebalanceLeft;
	mp_ubyte m_STebalanceRight;

	float getSTeBalanceAmp(mp_ubyte _lmcvalue)
	{
		const float dbchn  = -2.0f * (float)(20 - (_lmcvalue & 0x3F));   
		return powf(10.0f, dbchn / 10.0f);
	}

	void getVolumeLR (ChannelMixer::TMixerChannel* chn, mp_sint32& voll, mp_sint32& volr)
	{
		float fvolL = getSTeBalanceAmp(m_STebalanceLeft)  * 64.0f;
		float fvolR = getSTeBalanceAmp(m_STebalanceRight) * 64.0f;

		voll = mp_sint32(fvolL);
		volr = mp_sint32(fvolR);
	}

	void fillBufferCopy (mp_sint32* buffer, mp_uint32 count, mp_sint32 voll, mp_sint32 volr);

	bool isinitialized;

	mp_sint32* m_cache;

	static ResamplerYM* ms_instance;

	mp_sint32 m_bufferCopy[BUFFERCOPY_LENGTH];
	mp_sint32 m_bufferCopyIndex = 0; 

public:
	ResamplerYM();

	bool Init();
	void UpdateScore();
	const char* GetError();

	mp_sint32* getBufferCopy(mp_uint32 _index, mp_uint32 _count);

	void Stop();
	bool Reload();
	void SetMute(mp_uint32 _index, bool _mute);

	const char* GetSndSynFilename() const { return m_path; }
	const char* GetSndSynDefaultFilename() const { return m_default; }

	void SetSndSynFilename(const char* _filepath) { strcpy(m_path, _filepath); }

	void SetSTeBalanceLeft  (mp_ubyte _left) { m_STebalanceLeft = _left; }
	void SetSTeBalanceRight (mp_ubyte _right) { m_STebalanceRight = _right; }

	static mp_uint32 GetNbSounds();
	static const char* GetSoundName (mp_uint32 _index);

	virtual bool isRamping() { return false; }
	virtual bool supportsFullChecking() { return true; }
	virtual bool supportsNoChecking() { return true; }

	virtual void addBlockFull(mp_sint32* buffer, ChannelMixer::TMixerChannel* chn, mp_uint32 count);
	virtual void addBlockNoCheck(mp_sint32* buffer, ChannelMixer::TMixerChannel* chn, mp_uint32 count);

	static ResamplerYM* GetInstance() { return ms_instance; }

	static const mp_ubyte YM_SET_COMMAND = 0x80;

	struct YMparam
	{
		mp_ubyte keys;
		mp_sbyte finetune;
		mp_ubyte portamientoticks;
		mp_ubyte scorevolume;
		bool     scorevolumeset;
		bool     keyspressed;
		bool     keysjustpressed;
		int		 soundsindex;
		mp_sbyte pitchbendrange;
		mp_ubyte pitchbendticks;

		void Init();
	};

	YMparam ymparams[3];
};


#endif
