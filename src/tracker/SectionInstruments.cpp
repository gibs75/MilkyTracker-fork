/*
 *  tracker/SectionInstruments.cpp
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
 *  SectionInstruments.cpp
 *  MilkyTracker
 *
 *  Created by Peter Barth on 15.04.05.
 *
 */

// 02.05.2022: changes for BlitStracker fork by J.Hubert 

#include "SectionInstruments.h"
#include "Tracker.h"
#include "TrackerConfig.h"
#include "TabManager.h"
#include "ModuleEditor.h"
#include "EnvelopeEditor.h"
#include "SampleEditor.h"
#include "PianoControl.h"
#include "PatternTools.h"
#include "SectionSamples.h"

#include "Screen.h"
#include "PPUIConfig.h"
#include "Button.h"
#include "CheckBox.h"
#include "CheckBoxLabel.h"
#include "MessageBoxContainer.h"
#include "TransparentContainer.h"
#include "ListBox.h"
#include "RadioGroup.h"
#include "Seperator.h"
#include "Slider.h"
#include "StaticText.h"
#include "EnvelopeEditorControl.h"
#include "PatternEditorControl.h"
#include "DialogBase.h"

#include "PlayerController.h"
#include "InputControlListener.h"
#include "EnvelopeContainer.h"

// OS Interface
#include "PPOpenPanel.h"
#include "PPSavePanel.h"

#include "ControlIDs.h"

// Class which responds to message box clicks
class DialogResponderInstruments : public DialogResponder
{
private:
	SectionInstruments& section;
	
public:
	DialogResponderInstruments(SectionInstruments& section) :
		section(section)
	{
	}
	
	virtual pp_int32 ActionOkay(PPObject* sender)
	{
		switch (reinterpret_cast<PPDialogBase*>(sender)->getID())
		{
			case MESSAGEBOX_ZAPINSTRUMENT:
			{
				section.zapInstrument();
				break;
			}
		}
		return 0;
	}	
};

EnvelopeEditor* SectionInstruments::getEnvelopeEditor()
{
	return tracker.getEnvelopeEditor();
}

void SectionInstruments::showSection(bool bShow)
{
	containerEntire->show(bShow);
}

SectionInstruments::SectionInstruments(Tracker& theTracker) :
	SectionAbstract(theTracker, NULL, new DialogResponderInstruments(*this)),
	containerEntire(NULL),
	containerSampleSlider(NULL),
	containerInstrumentSlider(NULL),
	visible(false)
{}

pp_int32 SectionInstruments::handleEvent(PPObject* sender, PPEvent* event)
{
	PPScreen* screen = tracker.screen;
	ModuleEditor* moduleEditor = tracker.moduleEditor;

	if (event->getID() == eUpdateChanged)
	{
		tracker.updateWindowTitle();
	}
	else if (event->getID() == eCommand || event->getID() == eCommandRepeat)
	{
		switch (reinterpret_cast<PPControl*>(sender)->getID())
		{
			case BUTTON_SAMPLE_RELNOTENUM_OCTUP:
			{
				tracker.getSampleEditor()->increaseRelNoteNum(12);
				update();
				break;
			}

			case BUTTON_SAMPLE_RELNOTENUM_OCTDN:
			{
				tracker.getSampleEditor()->increaseRelNoteNum(-12);
				update();
				break;
			}

			case BUTTON_SAMPLE_RELNOTENUM_NOTEUP:
			{
				tracker.getSampleEditor()->increaseRelNoteNum(1);
				update();
				break;
			}

			case BUTTON_SAMPLE_RELNOTENUM_NOTEDN:
			{
				tracker.getSampleEditor()->increaseRelNoteNum(-1);
				update();
				break;
			}

			case BUTTON_SAMPLE_PLAY_STOP:
			{
				tracker.playerController->stopInstrument(tracker.listBoxInstruments->getSelectedIndex()+1);
				//tracker.stopSong();
				break;
			}
		}
	}
	else if (event->getID() == eValueChanged)
	{
		switch (reinterpret_cast<PPControl*>(sender)->getID())
		{
			case SLIDER_SAMPLE_VOLUME:
			{
				tracker.getSampleEditor()->setFT2Volume(reinterpret_cast<PPSlider*>(sender)->getCurrentValue());
				updateSampleSliders();
				break;
			}

			case SLIDER_SAMPLE_FINETUNE:
			{
				tracker.getSampleEditor()->setFinetune(reinterpret_cast<PPSlider*>(sender)->getCurrentValue()-128);
				updateSampleSliders();
				break;
			}
		}
	}
	
	return 0;
}

void SectionInstruments::init()
{
	init(0, tracker.MAXEDITORHEIGHT()-tracker.INSTRUMENTSECTIONDEFAULTHEIGHT());
}

void SectionInstruments::init(pp_int32 x, pp_int32 y)
{
	PPScreen* screen = tracker.screen;

	containerEntire = new PPTransparentContainer(CONTAINER_ENTIREINSSECTION, screen, this, 
												 PPPoint(0, 0), PPSize(screen->getWidth(), screen->getHeight()));

	// envelope stuff
	pp_int32 w4 = 165;
	pp_int32 w3 = 39;
	pp_int32 w2 = w4+w3;
	pp_int32 w = screen->getWidth() - w2;

	pp_int32 scale = 256*screen->getWidth() / 800;
	

	// ----------------- instrument info ----------------- 
	PPContainer* container = new PPContainer(CONTAINER_INSTRUMENTS_INFO1, screen, this, PPPoint(x, y), PPSize(w4,34+4), false);
	containerEntire->addControl(container);
	containerSampleSlider = container;
	
	container->setColor(TrackerConfig::colorThemeMain);

	container->addControl(new PPStaticText(0, NULL, NULL, PPPoint(x + 4, y + 4), "Volume", true));	
	container->addControl(new PPStaticText(STATICTEXT_SAMPLE_VOLUME, screen, this, PPPoint(x + 4 + 8*9, y + 4), "FF", false));	

	//PPSlider* slider = new PPSlider(SLIDER_SAMPLE_VOLUME, screen, this, PPPoint(x + 4 + 8*7+2, y + 2), 51, true);
	PPSlider* slider = new PPSlider(SLIDER_SAMPLE_VOLUME, screen, this, PPPoint(x + 4 + 8*11+2, y + 2), 68, true);
	slider->setMaxValue(64);
	slider->setBarSize(16384);
	container->addControl(slider);

	container->addControl(new PPStaticText(0, NULL, NULL, PPPoint(x + 4, y + 4 + 24), "F.tune", true));	
	container->addControl(new PPStaticText(STATICTEXT_SAMPLE_FINETUNE, screen, this, PPPoint(x + 4 + 8*7, y + 4 + 24), "-128", false));	

	slider = new PPSlider(SLIDER_SAMPLE_FINETUNE, screen, this, PPPoint(x + 4 + 8*11+2, y + 2 + 24), 68, true);
	slider->setBarSize(16384);
	container->addControl(slider);

	pp_int32 height = container->getSize().height;

	// exit 'n stuff
	auto y4=y;
	pp_int32 nx = x + container->getSize().width;

	container = new PPContainer(CONTAINER_INSTRUMENTS_INFO4, screen, this, PPPoint(nx, y), PPSize(w3,38), false);
	containerEntire->addControl(container);
	container->setColor(TrackerConfig::colorThemeMain);

	PPButton* button = new PPButton(BUTTON_INSTRUMENTEDITOR_EXIT, screen, &tracker, PPPoint(nx + 2, y + 2), PPSize(34, 34));
	button->setText("Exit");

	container->addControl(button);

	// load & save 
	y+=container->getSize().height;

	container = new PPContainer(CONTAINER_INSTRUMENTS_INFO5, screen, this, PPPoint(nx, y), PPSize(w3,39), false);
	containerEntire->addControl(container);
	container->setColor(TrackerConfig::colorThemeMain);

	// copy & paste
	y+=container->getSize().height;

	container = new PPContainer(CONTAINER_INSTRUMENTS_INFO6, screen, this, PPPoint(nx, y), PPSize(w3,27), false);
	containerEntire->addControl(container);
	container->setColor(TrackerConfig::colorThemeMain);

	y+=container->getSize().height;	

	y = y4;

	// autovibrato etc.
	y+=height;

	container = new PPContainer(CONTAINER_INSTRUMENTS_INFO3, screen, this, PPPoint(x, y), PPSize(w4,66), false);
	containerEntire->addControl(container);
	containerInstrumentSlider = container;
	container->setColor(TrackerConfig::colorThemeMain);

	height = container->getSize().height;

	// relative note
	container = new PPContainer(CONTAINER_INSTRUMENTS_INFO2, screen, this, PPPoint(x, y+height), PPSize(w4+39,36+2), false);
	containerEntire->addControl(container);
	container->setColor(TrackerConfig::colorThemeMain);

	y+=height;

	container->addControl(new PPStaticText(0, NULL, NULL, PPPoint(x + 4, y + 2), "Relative note:", true));	
	container->addControl(new PPStaticText(STATICTEXT_SAMPLE_RELNOTE, screen, this, PPPoint(x + 4 + 15*8 - 4, y + 2), "C-4", false));	

	button = new PPButton(BUTTON_SAMPLE_RELNOTENUM_OCTUP, screen, this, PPPoint(x + 4, y + 1 + 12), PPSize(78+19, 11));
	button->setText("Octave up");
	container->addControl(button);

	button = new PPButton(BUTTON_SAMPLE_RELNOTENUM_NOTEUP, screen, this, PPPoint(x + 4 + 79+19, y + 1 + 12), PPSize(78+19, 11));
	button->setText("Note up");
	container->addControl(button);

	button = new PPButton(BUTTON_SAMPLE_RELNOTENUM_OCTDN, screen, this, PPPoint(x + 4, y + 1 + 12 + 12), PPSize(78+19, 11));
	button->setText("Octave dn");
	container->addControl(button);

	button = new PPButton(BUTTON_SAMPLE_RELNOTENUM_NOTEDN, screen, this, PPPoint(x + 4 + 79+19, y + 1 + 24), PPSize(78+19, 11));
	button->setText("Note dn");
	container->addControl(button);

	// piano
	y+=container->getSize().height;
	
	pp_int32 dx = 0;
	containerEntire->adjustContainerSize();
	screen->addControl(containerEntire);

	initialised = true;
	
	showSection(false);	
}

void SectionInstruments::realign()
{
	pp_uint32 maxShould = tracker.MAXEDITORHEIGHT();
	pp_uint32 maxIs = containerEntire->getLocation().y + containerEntire->getSize().height;
	
	if (maxIs != maxShould)
	{
		pp_int32 offset = maxShould - maxIs;
		containerEntire->move(PPPoint(0, offset));
	}
	
	PatternEditorControl* control = tracker.getPatternEditorControl();
	PPScreen* screen = tracker.screen;
	
	if (visible)
	{
		control->setSize(PPSize(screen->getWidth(),
							tracker.MAXEDITORHEIGHT()-tracker.INSTRUMENTSECTIONDEFAULTHEIGHT()-tracker.UPPERSECTIONDEFAULTHEIGHT()));
	}
	else
	{
		control->setSize(PPSize(screen->getWidth(),tracker.MAXEDITORHEIGHT()-tracker.UPPERSECTIONDEFAULTHEIGHT()));
	}
}

void SectionInstruments::show(bool bShow)
{
	SectionAbstract::show(bShow);
	
	visible = bShow;
	containerEntire->show(bShow);
	
	if (!initialised)
	{
		init();
	}

	if (initialised)
	{
		PatternEditorControl* control = tracker.getPatternEditorControl();
		
		realign();
		
		if (bShow)
		{
			if (control)
			{
				tracker.hideInputControl();				
			}
			
			update(false);
		}
		
		showSection(bShow);
	}	
}

void SectionInstruments::updateSampleSliders(bool repaint/* = true*/)
{
	if (!initialised || !visible)
		return;

	PPScreen* screen = tracker.screen;
	PPContainer* container2 = containerSampleSlider;

	SampleEditor* sampleEditor = tracker.getSampleEditor();

	static_cast<PPSlider*>(container2->getControlByID(SLIDER_SAMPLE_VOLUME))->setCurrentValue(sampleEditor->getFT2Volume());	
	static_cast<PPStaticText*>(container2->getControlByID(STATICTEXT_SAMPLE_VOLUME))->setHexValue(sampleEditor->getFT2Volume(), 2);	

	mp_sint32 ft = sampleEditor->getFinetune();

	static_cast<PPSlider*>(container2->getControlByID(SLIDER_SAMPLE_FINETUNE))->setCurrentValue((mp_uint32)(ft+128));	
	static_cast<PPStaticText*>(container2->getControlByID(STATICTEXT_SAMPLE_FINETUNE))->setIntValue(ft, 4, true);	

	screen->paintControl(container2, repaint);
}

void SectionInstruments::updateInstrumentSliders(bool repaint/* = true*/)
{
	if (!initialised || !visible)
		return;

	PPScreen* screen = tracker.screen;
	PPContainer* container4 = containerInstrumentSlider;

	screen->paintControl(container4, repaint);
}

void SectionInstruments::notifyTabSwitch()
{
	if (isVisible())
		realign();
}

/*void SectionInstruments::notifySampleSelect(pp_int32 index)
{
	PianoControl* pianoControl = getPianoControl();				
	if (pianoControl)
		pianoControl->setSampleIndex(index);
}*/

void SectionInstruments::update(bool repaint)
{
	if (!initialised || !visible)
		return;

	PPScreen* screen = tracker.screen;
	ModuleEditor* moduleEditor = tracker.moduleEditor;

	// volume/panning etc.
	updateSampleSliders(false);

	// relative note number
	PPContainer* container3 = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_INSTRUMENTS_INFO2));

	char noteName[40];
	SampleEditor* sampleEditor = tracker.getSampleEditor();

	if (sampleEditor->getRelNoteNum() > 0)
		sprintf(noteName,"    (+%i)",(pp_int32)sampleEditor->getRelNoteNum());
	else
		sprintf(noteName,"    (%i)",(pp_int32)sampleEditor->getRelNoteNum());
	PatternTools::getNoteName(noteName, sampleEditor->getRelNoteNum() + 4*12 + 1, false);

	static_cast<PPStaticText*>(container3->getControlByID(STATICTEXT_SAMPLE_RELNOTE))->setText(noteName);		

	// volfade/autovobrato
	updateInstrumentSliders(false);

	screen->paintControl(container3, false);
	if (repaint)
		screen->update();
}

void SectionInstruments::updateAfterLoad()
{
	tracker.updateInstrumentsListBox(false);
	//tracker.updateSamplesListBox(false);
	tracker.updateSampleEditorAndInstrumentSection(false);
}

pp_int32 SectionInstruments::getNumPredefinedEnvelopes()
{
	return TrackerConfig::numPredefinedEnvelopes;
}
	
void SectionInstruments::handleZapInstrument()
{
	showMessageBox(MESSAGEBOX_ZAPINSTRUMENT, "Zap instrument?");
}

void SectionInstruments::zapInstrument()
{
	tracker.moduleEditor->zapInstrument(tracker.listBoxInstruments->getSelectedIndex());
	tracker.sectionSamples->resetSampleEditor();
	updateAfterLoad();
}

