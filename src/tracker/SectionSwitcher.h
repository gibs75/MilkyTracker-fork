/*
 *  tracker/SectionSwitcher.h
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
 *  SectionSwitcher.h
 *  MilkyTracker
 *
 *  Created by Peter Barth on 09.04.08.
 *
 */

 // 02.05.2022: changes for BlitStracker fork by J.Hubert 

#ifndef __SECTIONSWITCHER_H__
#define __SECTIONSWITCHER_H__

#include "BasicTypes.h"

class SectionSwitcher
{
public:
	enum ActiveBottomSections
	{
		ActiveBottomSectionNone = 0,
		ActiveBottomSectionInstrumentEditor,
		ActiveBottomSectionSampleEditor
	};
	
private:
	class Tracker& tracker;

	ActiveBottomSections bottomSection;
	class SectionAbstract* currentUpperSection;	

public:
	SectionSwitcher(Tracker& tracker);

	// General bottom sections show/hide
	void showBottomSection(ActiveBottomSections section, bool paint = true);
	void showUpperSection(SectionAbstract* section, bool hideSIP = true);
};

#endif
