/*
 * Copyright (c) 2009, The MilkyTracker Team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of the <ORGANIZATION> nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ChannelMixer.h
 *  MilkyPlay
 *
 *  Created by Peter Barth on Tue Oct 19 2004.
 *
 *  This class is pretty much emulating a Gravis Ultrasound with a timer set to 250Hz
 *  i.e. mixerHandler() will call a timer routine 250 times per second while mixing 
 *  the audio stream in between.
 */
#ifndef __CHANNELMIXER_H__
#define __CHANNELMIXER_H__

#include <math.h>

#include "MilkyPlayCommon.h"
#include "AudioDriverBase.h"
#include "Mixable.h"

#define MP_FP_CEIL(x)			(((x)+65535)>>16)
#define MP_FP_MUL(a, b)			((mp_sint32)(((mp_int64)(a)*(mp_int64)(b))>>16))

#define MP_INCREASESMPPOS(intpart, fracpart, fp, fractbits)	\
	intpart+=((fp)>>(fractbits)); \
	fracpart+=((fp)&(1<<(fractbits))-1); \
	if (((fracpart)>>(fractbits))==1) \
	{ \
		intpart++; \
		fracpart&=(1<<(fractbits))-1; \
	} 

class ErrorInfo
{
	char message[256];
	bool critical;
public:

	ErrorInfo(const char* _message, bool _critical = false)
		: critical (_critical)
	{
		strncpy(message, _message, sizeof(message));
		message[sizeof(message)-1] = 0;
	}

	const char* getMessage() const { return message; }
	bool isCritical() const { return critical; }
};


class ChannelMixer;  
typedef void (ChannelMixer::*TSetFreq)(mp_sint32 c, mp_sint32 f);

class ResamplerYM;

struct MixerSettings
{
	// These are arranged in a way, so that the ramping flag toggles with the
	// LSB (bit 0) if you count through them 
	enum ResamplerTypes
	{
		MIXER_NORMAL,
		MIXER_NORMAL_RAMPING,

		MIXER_LERPING,
		MIXER_LERPING_RAMPING,

		MIXER_BLS,
		MIXER_BLS_RAMPING,
		
		MIXER_DUMMY,
		
		MIXER_INVALID
	};
	
	enum
	{
		// pretty large buffer for most systems
		BUFFERSIZE_DEFAULT	= 8192
	};

	enum
	{
		NUMRESAMPLERTYPES = MIXER_INVALID,
	};
};

class ChannelMixer : public Mixable
{
public:
	enum
	{
		// This is the basis for timing & mixing
		// 250hz timer
		MP_TIMERFREQ		= 250,	
		MP_BASEFREQ			= 48000,	// is chosen because (48000 % 250) == 0
		// period in samples for MP_TIMERFREQ
		MP_BEATLENGTH		= (MP_BASEFREQ/MP_TIMERFREQ),
		// mixer state flags
		MP_SAMPLE_FILTERLP	= 65536,
		MP_SAMPLE_MUTE		= 32768,
		MP_SAMPLE_ONESHOT	= 8192,
		MP_SAMPLE_FADEOFF	= 4096,
		MP_SAMPLE_FADEOUT	= 2048,
		MP_SAMPLE_FADEIN	= 1024,
		MP_SAMPLE_PLAY		= 256,
		MP_SAMPLE_BACKWARD	= 128,
		
		MP_INVALID_VALUE	= 0x7FFFFFFF,
		MP_FILTERPRECISION	= 8		
	};

	static inline mp_sint32 fixedmul(mp_sint32 a,mp_sint32 b) { return MP_FP_MUL(a,b); }
	static inline mp_sint32 fixeddiv(mp_sint32 a,mp_sint32 b) { return ((mp_sint32)(((mp_int64)(a)*65536/(mp_int64)(b)))); }

	// this is a subset of the channel state which is stored along
	// with the time progress, so you can do a look up of the state
	// even with large buffer sizes 
	struct TTimeRecord
	{
		mp_uint32			flags;					// bit 8 = sample played
													// bit 9 = sample direction (0 = forward, 1 = backward)
													// bit 10-11 = sample ticker used to represent ramping states
													// bit 12 = scheduled to stop
													// bit 13 = one shot looping
													// bit 15 = mute channel
		const mp_sbyte*		sample;					// pointer to sample
		mp_sint32			smppos;					// 32 bit integer part of sample position
		mp_sint32			volPan;					// 32 bits, upper 16 bits = pan, lower 16 bits = vol
		mp_sint32			smplen;
		mp_sint32			smpposfrac;				// 16 bit fractional part of sample position
		mp_sint32			smpadd;					// 16:16 fixedpoint increment
		mp_sint32			loopend;
		mp_sint32			loopstart;
		mp_sint32			fixedtime;				// for amiga resampler (running time)
		mp_sint32			fixedtimefrac;			// for sinc/amiga resamplers (running time fraction)
		
		TTimeRecord() :
			flags(0),
			sample(NULL),
			smppos(0),
			volPan(0),
			smplen(0),
			smpposfrac(0),
			smpadd(0),
			loopend(0),
			loopstart(0),
			fixedtime(0),
			fixedtimefrac(0)
		{
		}
	};

	struct TMixerChannel 
	{
		mp_uint32			flags;					// bit 8 = sample played
													// bit 9 = sample direction (0 = forward, 1 = backward)
													// bit 10-11 = sample ticker used to represent ramping states
													// bit 12 = scheduled to stop
													// bit 13 = one shot looping
													// bit 15 = mute channel
		const mp_sbyte*		sample;					// pointer to sample
		mp_sint32			smplen;
		mp_sint32			smppos;					// 32 bit integer part of sample position
		mp_sint32			smpposfrac;				// 16 bit fractional part of sample position
		mp_sint32			smpadd;					// 16:16 fixed point increment
		mp_sint32			rsmpadd;				// fixed point reciprocal of the increment
		mp_sint32			loopend;				// loop end
		mp_sint32			loopendcopy;			// Temporary placeholder for one-shot looping
		mp_sint32			loopstart;				// loop start

		mp_sint32			finalvolr;
		mp_sint32			finalvoll;

		mp_ubyte			shiftvol;
		mp_sint32			vol;
		mp_sint32			pan;
		
		mp_sint32			rampFromVolStepR;
		mp_sint32			rampFromVolStepL;

		mp_sint32			a,b,c;					// Filter coefficients
		mp_sint32			currsample;				// sample history for filtering
		mp_sint32			prevsample;				// see above

		mp_sint32			cutoff;
		mp_sint32			resonance;

		mp_sint32			fixedtime;				// for amiga resampler (running time)
		mp_sint32			fixedtimefrac;			// for sinc/amiga resamplers (running time fraction)

		mp_uint32			timeRecordSize;
		TTimeRecord*		timeRecord;
		mp_sint32			index;					// For Amiga resampler
		
        // BLS hack
        mp_ubyte		    bitmask;
		mp_ubyte			bitshift;
		mp_ubyte			STebalanceLeft;	
		mp_ubyte			STebalanceRight;	
		mp_char				mute;
		mp_sint32			STemastervol;

		static mp_ubyte		amplitudeToLMC[65];

		// YM hack
		mp_ubyte			isymchannel;
		mp_ubyte			ymchannel;

		TMixerChannel() :
			timeRecordSize(0),
			timeRecord(NULL)
		{
			clear(0);
		}

		TMixerChannel(bool fastContruction) :
			timeRecordSize(0),
			timeRecord(NULL)
		{
		}
		
		~TMixerChannel()
		{
			if (timeRecord)
				delete[] timeRecord;
		}

		void clear(mp_uint32 _channel)
		{
			flags				= 0;
			sample				= NULL;
			smplen				= 0;
			smppos				= 0;
			smpposfrac			= 0;
			smpadd				= 0;
			rsmpadd				= 0;
			loopend				= 0;
			loopendcopy			= 0;
			loopstart			= 0;
			
			finalvolr			= 0;
			finalvoll			= 0;
			
			vol					= 0;
			pan					= 128;
			
			rampFromVolStepR	= 0;
			rampFromVolStepL	= 0;
			
			a = b = c			= 0;
			currsample			= 0;
			prevsample			= 0;
			
			cutoff				= MP_INVALID_VALUE;
			resonance			= MP_INVALID_VALUE;
			
			fixedtime			= 0;
			fixedtimefrac		= 0;
			index				= -1;		// is filled during runtime
			
			bitshift			= 0;
			bitmask				= 0xFF;

			isymchannel			= false;

			switch (_channel)
			{
			case 0:
			case 3:
				STebalanceLeft		= amplitudeToLMC[64];
				STebalanceRight		= 0;
				break;
			case 1:
			case 2:
				STebalanceLeft		= 0;
				STebalanceRight		= amplitudeToLMC[64];
				break;
			case 4:
			case 5:
			case 6:
				isymchannel			= true;
				ymchannel			= _channel - 4;
			}

			mute = false;
			STemastervol			= 255 * 255;

			if (timeRecord)
				memset(timeRecord, 0, sizeof(TTimeRecord) * timeRecordSize);
		}
		
		void reallocTimeRecord(mp_uint32 size)
		{
			delete[] timeRecord;
			timeRecordSize = size;
			timeRecord = new TTimeRecord[size];
		}

		static mp_ubyte GetShiftFromVolume (mp_uword vol)
		{
			if (vol > 48)
				return 0;
			else if (vol > 24)  
				return 1;
			else if (vol > 12) 
				return 2;
			else if (vol > 6) 
				return 3;
			else if (vol > 3) 
				return 4;
			else if (vol > 1) 
				return 5;
			else if (vol > 0)
				return 6;
			else if (vol == 0)
				return 8;

			return 8;
		}
	};
	
	class ResamplerBase
	{
	public:
		virtual ~ResamplerBase() {}
			
		// walk along the sample
		// intpart is the 32 bit integer part of the position
		// fracpart is the fractional part of the position
		// fp is the amount of samples to advance
		// is the resolution of the fractional part in bits (default is 16)
/*		static inline void advanceSamplePosition(mp_sint32& intpart, 
												 mp_sint32& fracpart, 
												 const mp_sint32 fp, 
												 const mp_sint32 fracbits = 16)
		{
			MP_INCREASESMPPOS(intpart, fracpart, fp, fracbits);
		}*/
		
		// if this resampler is doing ramping
		virtual bool isRamping() = 0;
		// this resampler can perform a normal block add to the mixing buffer
		virtual bool supportsNoChecking() = 0;
		// optional: if this resampler is able to perform a full checked walk along the sample
		virtual bool supportsFullChecking() = 0;
		
		// see above, you will need to implement at least one of the following
		virtual void addBlockNoCheck(mp_sint32* buffer, TMixerChannel* chn, mp_uint32 count) 
		{
			// if this is called your own derived resampler is not properly working
			ASSERT(false);
		}
		
		// see above for comments
		virtual void addBlockFull(mp_sint32* buffer, TMixerChannel* chn, mp_uint32 count)
		{
			ASSERT(false);	
		}
		
		// in case the resampler needs to get hold of the current mixing frequency
		virtual void setFrequency(mp_sint32 frequency) { }

		// in case the resampler needs to get hold of the current number of channels
		virtual void setNumChannels(mp_sint32 num) { }
	};

private:	
	mp_uint32	mixerNumAllocatedChannels;	// Number of channels to be allocated by mixer
	mp_uint32	mixerNumActiveChannels;		// Number of channels to be mixed
	mp_uint32	mixerLastNumAllocatedChannels;
	
	mp_uint32	mixFrequency;				// Mixing frequency
	mp_uint32	rMixFrequency;				// 0x7FFFFFFF/mixFrequency
	
	mp_sint32*  mixbuffBeatPacket;
	mp_uint32	mixBufferSize;				// this is the resulting buffer size in 16 bit words

	mp_uint32	beatPacketSize;				// size of 1/250 of a second in samples
	mp_uint32	numBeatPackets;				// how many of these fit in our buffer size
	mp_uint32	lastBeatRemainder;			// used while filling the buffer, if the buffer is not an exact multiple of beatPacketSize
		
	TMixerChannel*	channel;
	TMixerChannel*  newChannel;
	
	mp_sint32		masterVolume;			// mixer master volume	
	mp_sint32		panningSeparation;		// panning separation from 0 (mono) to 256 (full stereo)
	
	MixerSettings::ResamplerTypes	resamplerType;
	TSetFreq		setFreqFuncTable[MixerSettings::NUMRESAMPLERTYPES];			// If different precisions are used, use other frequency calculation procedures	
	ResamplerBase*  resamplerTable[MixerSettings::NUMRESAMPLERTYPES];

	bool			paused;
	bool			disableMixing;
	bool			allowFilters;

	void			setFrequency(mp_sint32 frequency);
	
	void			addChannels(mp_uint32 numChannels, mp_sint32* buffer32,mp_sint32 beatNum, mp_sint32 beatlength);
	void		    addChannelsNormal(mp_uint32 numChannels, mp_sint32* buffer32,mp_sint32 beatNum, mp_sint32 beatlength);		
	void			addChannelsRamping(mp_uint32 numChannels, mp_sint32* buffer32,mp_sint32 beatNum, mp_sint32 beatlength);		
	
	inline void		timer(mp_uint32 beatIndex)
	{
		timerHandler(beatIndex <= getNumBeatPackets() ? beatIndex : getNumBeatPackets());
	}
	
	void			reallocChannels();
	void			clearChannels();

protected: 
	bool			isYMChannel (mp_uint32 channelindex) const { assert(channelindex < mixerNumActiveChannels); return channel[channelindex].isymchannel; }
	mp_uint32		getYMChannel(mp_uint32 channelindex) const { assert(channelindex < mixerNumActiveChannels); assert(channel[channelindex].isymchannel); return channel[channelindex].ymchannel; }

public:
					ChannelMixer(mp_uint32 numChannels, 
								 mp_uint32 frequency, 
								 MixerSettings::ResamplerTypes resampleTypes,
								 bool mainplayer);				
	
	virtual			~ChannelMixer();
	
	static void		addChannelToResampler(ResamplerBase* resampler, TMixerChannel* chn, mp_sint32* buffer32, const mp_sint32 beatlength, const mp_sint32 beatSize);		

	mp_uint32		getMixBufferSize() const { return mixBufferSize; }	
	void			mix(mp_sint32* buffer, mp_uint32 numSamples);	
	void			updateSampleCounter(mp_sint32 numSamples) { sampleCounter+=numSamples; }
	void			resetSampleCounter() { sampleCounter=0; }
	
	mp_sint32		initDevice();
	mp_sint32		closeDevice();
	
	void			stop();
	mp_sint32		pause();
	mp_sint32		resume();
	
	void			setDisableMixing(bool disableMixing) { this->disableMixing = disableMixing; }
	void			setAllowFilters(bool allowFilters) { this->allowFilters = allowFilters; }
	bool			getAllowFilters() const { return allowFilters; }

	void			resetChannelsFull();
	void			resetChannelsWithoutMuting();
	
	void			setResamplerType(MixerSettings::ResamplerTypes type);
	MixerSettings::ResamplerTypes	getResamplerType() const { return resamplerType; }
	bool			isRamping()  const { return resamplerTable[resamplerType]->isRamping(); }
	
	virtual mp_sint32 adjustFrequency(mp_uint32 frequency);
	mp_sint32		getMixFrequency() { return mixFrequency; }
	
	static mp_sint32 beatPacketsToBufferSize(mp_uint32 mixFrequency, mp_uint32 numBeats);	
	virtual mp_sint32 setBufferSize(mp_uint32 bufferSize);
	
	mp_uint32		getBeatPacketSize() const { return beatPacketSize; }
	mp_uint32		getNumBeatPackets() const { return mixBufferSize / beatPacketSize; } 

	// volume control
	void			setMasterVolume(mp_sint32 vol) { masterVolume = vol; }
	mp_sint32		getMasterVolume() const { return masterVolume; }	
	// panning control
	void			setPanningSeparation(mp_sint32 separation) { panningSeparation = separation; }
	mp_sint32		getPanningSeparation() const { return panningSeparation; }
	
	bool			isInitialized() const { return initialized; }
	bool			isPlaying() const { return startPlay; }

	mp_sint32		getNumActiveChannels();
	mp_sint32		getNumAllocatedChannels() const { return mixerNumActiveChannels; }

	mp_int64		getSampleCounter() const { return sampleCounter; }
	
	mp_sint32		getBeatIndexFromSamplePos(mp_uint32 smpPos) const;
	
	ResamplerBase*  getCurrentResampler() const { return resamplerTable[resamplerType]; }
	
protected:
	bool			initialized;
	bool			startPlay;

	mp_int64		sampleCounter;			// number of samples played (per song)

	void			startMixer() 
	{
		lastBeatRemainder = 0;
	}

	void			setNumChannels(mp_uint32 num);

	void			setActiveChannels(mp_uint32 num);

	ResamplerYM*     ymresampler;

public:
	void			setBitmask(mp_sint32 c,mp_ubyte bitmask) { channel[c].bitmask = bitmask; }
	void			setVol(mp_sint32 c,mp_sint32 v) { channel[c].vol = v; }
	mp_sint32		getVol(mp_sint32 c) { return channel[c].vol; }
				
	void			setPan(mp_sint32 c,mp_sint32 p)	{ channel[c].pan = p; }

	void			setSTeBalance(mp_sint32 c,mp_ubyte STebalance);
	void			setSTeVolume(mp_sint32 c,mp_sint32 vol,mp_sint32 mainVolume,mp_sint32 mastervolume)
	{
		channel[c].STemastervol = mainVolume * mastervolume;
		channel[c].bitshift = TMixerChannel::GetShiftFromVolume(vol >> 2);
	}


	void			setFreq(mp_sint32 c,mp_sint32 f) 
	{ 
		(this->*setFreqFuncTable[resamplerType])(c,f);
	}
	
	// Default low precision calculations
	void			setChannelFrequency(mp_sint32 c, mp_sint32 f);
						
	void			setFilterAttributes(mp_sint32 c, mp_sint32 cutoff, mp_sint32 resonance);
	
	void			playSample(mp_sint32 c, // channel
							   mp_sbyte* smp, // sample buffer
							   mp_sint32 smplen, // sample size
							   mp_sint32 smpoffs, // sample offset 
							   mp_sint32 smpoffsfrac,
							   bool smpOffsetWrap,
							   mp_sint32 lstart, // loop start
							   mp_sint32 len, // loop end
							   mp_sint32 flags,
							   bool ramp = true);

	bool			isChannelPlaying(mp_sint32 c) { return (channel[c].flags&MP_SAMPLE_PLAY) != 0; }

	void			stopSample(mp_sint32 c) { channel[c].flags&=~(MP_SAMPLE_FADEIN|MP_SAMPLE_FADEOUT); channel[c].flags|=MP_SAMPLE_FADEOFF; }	

	void			breakLoop(mp_sint32 c) { channel[c].flags&=~3; channel[c].loopend = channel[c].smplen; }

	// handle with care
	void			setSamplePos(mp_sint32 c, mp_sint32 pos) { channel[c].smppos = pos; channel[c].smpposfrac = 0; }
	
	mp_sint32		getSamplePos(mp_sint32 c) { return channel[c].smppos; }
	mp_sint32		getSamplePosFrac(mp_sint32 c) { return channel[c].smpposfrac; }
	
	void			setBackward(mp_sint32 c)
	{
		if (channel[c].flags & MP_SAMPLE_PLAY)
		{
			channel[c].flags |= MP_SAMPLE_BACKWARD;
			if (channel[c].smppos < channel[c].loopstart)
			{
				channel[c].smppos = channel[c].loopend;
			}
		}
	}

	void			setForward(mp_sint32 c)
	{
		if (channel[c].flags & MP_SAMPLE_PLAY)
		{
			channel[c].flags &= ~MP_SAMPLE_BACKWARD;
			if (channel[c].smppos > channel[c].loopend)
			{
				channel[c].smppos = channel[c].loopstart;
			}
		}
	}
	
	void			muteChannel(mp_sint32 c, bool m);

	bool			isChannelMuted(mp_sint32 c);
	
	mp_uint32		getSyncSampleCounter();

protected:
	// timer procedure for mixing
	virtual void	timerHandler(mp_sint32 currentBeatPacket) = 0;
	void		   	panToVol(ChannelMixer::TMixerChannel *chn, mp_sint32 &left, mp_sint32 &right);
	static mp_sint32 panLUT[257];

#ifdef MILKYTRACKER
	friend class PlayerController;
#endif

#ifdef __MPTIMETRACKING__
public:
	void mixData(mp_sint32 c, 
				 mp_sint32* buffer, 
				 mp_sint32 count, 
				 mp_sint32 sampleShift,
				 mp_sint32 fMul = 0, 
				 mp_sint32 bufferIndex = -1, 
				 mp_sint32 packetIndex = -1) const;
#endif
};

#endif
