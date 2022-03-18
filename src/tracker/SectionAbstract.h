/*
 *  tracker/SectionAbstract.h
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
 *  SectionAbstract.h
 *  MilkyTracker
 *
 *  Created by Peter Barth on 14.04.05.
 *
 */

// 02.05.2022: changes for BlitStracker fork by J.Hubert 

#ifndef SECTIONABSTRACT__H
#define SECTIONABSTRACT__H

#include "BasicTypes.h"
#include "Event.h"
#include "Screen.h"
#include "Tracker.h"

class PPControl;

class SectionAbstract : public EventListenerInterface
{
private:
	PPControl* lastFocusedControl;

protected:
	Tracker& tracker;
	bool initialised;

	class PPDialogBase* dialog;
	class DialogResponder* responder;
	
protected:
	virtual void showSection(bool bShow) = 0;

	void showMessageBox(pp_uint32 id, const PPString& text, bool yesnocancel = false);
	
public:
	SectionAbstract(Tracker& theTracker, PPDialogBase* dialog = NULL, DialogResponder* responder = NULL) :
		lastFocusedControl(NULL),
		tracker(theTracker),
		initialised(false),
		dialog(dialog),
		responder(responder)
	{
	}
	
	virtual ~SectionAbstract();

	// PPEvent listener
	virtual pp_int32 handleEvent(PPObject* sender, PPEvent* event) = 0;
	
	virtual void init() = 0;
	
	virtual void init(pp_int32 x, pp_int32 y) = 0;
	virtual void show(bool bShow)
	{
	}
	
	virtual void update(bool repaint = true) = 0;
	
	virtual void notifyInstrumentSelect(pp_int32 index) {}
	//virtual void notifySampleSelect(pp_int32 index) {}
	virtual void notifyTabSwitch() {}
	
	friend class Tracker;
};

#endif
