/*
 *  tracker/SectionInstruments.h
 *
 *  Copyright 2009 Peter Barth
 *
 *  This file is part of Milkytracker.
 *
 *  Milkytracker is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Milkytracker is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Milkytracker.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  SectionInstruments.h
 *  MilkyTracker
 *
 *  Created by Peter Barth on 15.04.05.
 *
 */

// 02.05.2022: changes for BlitStracker fork by J.Hubert 

#ifndef SECTIONINSTRUMENTS__H
#define SECTIONINSTRUMENTS__H

#include "BasicTypes.h"
#include "Event.h"
#include "SectionAbstract.h"

class PPControl;
class EnvelopeEditorControl;
class EnvelopeEditor;
class PianoControl;
class PPContainer;
class EnvelopeContainer;

class SectionInstruments : public SectionAbstract
{
private:
	PPContainer* containerEntire;

	PPContainer* containerSampleSlider;
	PPContainer* containerInstrumentSlider;
	bool visible;

	EnvelopeEditor* getEnvelopeEditor();
	
protected:
	virtual void showSection(bool bShow);

public:
	SectionInstruments(Tracker& tracker);
	virtual ~SectionInstruments() {}

	// PPEvent listener
	virtual pp_int32 handleEvent(PPObject* sender, PPEvent* event);
	
	virtual void init();

	virtual void init(pp_int32 x, pp_int32 y);

	void realign();

	virtual void show(bool bShow);
	
	void updateSampleSliders(bool repaint = true);
	void updateInstrumentSliders(bool repaint = true);
	
	virtual void update(bool repaint = true);

	virtual void notifyTabSwitch();

	//virtual void notifySampleSelect(pp_int32 index);

	void updateAfterLoad();

	bool isVisible() const { return visible; }
	
	// Get predefined envelopes for storage purpose
	pp_int32 getNumPredefinedEnvelopes();
	
	enum EnvelopeTypes
	{
		EnvelopeTypeVolume,
		EnvelopeTypePanning
	};
	
private:
	void handleZapInstrument();
	void zapInstrument();

	// Responder should be friend
	friend class DialogResponderInstruments;	

	friend class Tracker;
};

#endif

