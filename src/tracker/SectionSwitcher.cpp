/*
 *  tracker/SectionSwitcher.cpp
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
 *  SectionSwitcher.cpp
 *  MilkyTracker
 *
 *  Created by Peter Barth on 09.04.08.
 *
 */

// 02.05.2022: changes for BlitStracker fork by J.Hubert 

#include "SectionSwitcher.h"
#include "Tracker.h"
#include "Screen.h"
#include "Container.h"
#include "SectionInstruments.h"
#include "SectionSamples.h"
#include "ScopesControl.h"
#include "PatternEditorControl.h"

#include "ControlIDs.h"

SectionSwitcher::SectionSwitcher(Tracker& tracker) :
	tracker(tracker),
	bottomSection(ActiveBottomSectionNone),
	currentUpperSection(NULL)
{
}

// General bottom sections show/hide
void SectionSwitcher::showBottomSection(ActiveBottomSections section, bool paint/* = true*/)
{
	switch (bottomSection)
	{
		case ActiveBottomSectionInstrumentEditor:
			tracker.sectionInstruments->show(false);
			break;
		case ActiveBottomSectionSampleEditor:
			tracker.sectionSamples->show(false);
			break;
		case ActiveBottomSectionNone:
			break;
	}

	if (bottomSection != section)
		bottomSection = section;
	else
		bottomSection = ActiveBottomSectionNone;
	
	switch (bottomSection)
	{
		case ActiveBottomSectionInstrumentEditor:
			tracker.sectionInstruments->show(true);
			break;
		case ActiveBottomSectionSampleEditor:
			tracker.sectionSamples->show(true);
			break;
		case ActiveBottomSectionNone:
			tracker.rearrangePatternEditorControl();
			break;
	}	
	
	if (paint)
		tracker.screen->paint();
}

void SectionSwitcher::showUpperSection(SectionAbstract* section, bool hideSIP/* = true*/)
{
	tracker.screen->pauseUpdate(true);
	if (currentUpperSection)
	{
		currentUpperSection->show(false);
	}
	if (section)
	{
		if (hideSIP)
			tracker.hideInputControl();

		section->show(true);
	}
	tracker.screen->pauseUpdate(false);
	tracker.screen->update();
	currentUpperSection = section;
}
