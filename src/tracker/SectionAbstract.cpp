/*
 *  tracker/SectionAbstract.cpp
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
 *  SectionAbstract.cpp
 *  MilkyTracker
 *
 *  Created by Peter Barth on 06.01.06.
 *
 */

// 02.05.2022: changes for BlitStracker fork by J.Hubert 

#include "SectionAbstract.h"
#include "ControlIDs.h"
#include "Control.h"
#include "Container.h"
#include "ListBox.h"
#include "Tracker.h"
#include "DialogBase.h"
#include "SectionSwitcher.h"

SectionAbstract::~SectionAbstract()
{
	delete responder;
	delete dialog;
}

void SectionAbstract::showMessageBox(pp_uint32 id, const PPString& text, bool yesnocancel/* = false*/)
{
	if (dialog)
	{
		delete dialog;
		dialog = NULL;
	}

	dialog = new PPDialogBase(tracker.screen, responder, 
							  id, text, 
							  yesnocancel ? 
							  PPDialogBase::MessageBox_YESNOCANCEL :
							  PPDialogBase::MessageBox_OKCANCEL); 	
											  
	dialog->show();
}
