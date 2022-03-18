/*
 *  tracker/Tracker.cpp
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

// 02.05.2022: changes for BlitStracker fork by J.Hubert 

#include <map>
#include "Tracker.h"
#include "TrackerConfig.h"
#include "TabManager.h"
#include "PlayerController.h"
#include "PlayerMaster.h"
#include "PlayerLogic.h"
#include "RecorderLogic.h"
#include "SamplePlayer.h"
#include "SimpleVector.h"
#include "ModuleEditor.h"
#include "TabTitleProvider.h"
#include "PPUI.h"
#include "PatternTools.h"
#include "PatternEditorControl.h"
#include "EnvelopeEditorControl.h"
#include "PianoControl.h"
#include "PeakLevelControl.h"
#include "ScopesControl.h"
#include "TabHeaderControl.h"
#include "SampleEditorControl.h"
#include "TrackerSettingsDatabase.h"
#include "KeyBindings.h"
#include "FileIdentificator.h"
#include "FileExtProvider.h"
#include "Decompressor.h"
#include "Zapper.h"
#include "TitlePageManager.h"
#include "PPMessageBox.h"

#include "ResamplerYM.h"

// Sections
#include "SectionSwitcher.h"
#include "SectionTranspose.h"
#include "SectionAdvancedEdit.h"
#include "SectionDiskMenu.h"
#include "SectionSettings.h"
#include "SectionInstruments.h"
#include "SectionSamples.h"
#include "SectionQuickOptions.h"
#include "SectionOptimize.h"
#include "SectionAbout.h"

#include "InputControlListener.h"

// Some helper messageboxes & button handlers
#include "DialogHandlers.h"
#include "DialogChannelSelector.h"
#include "DialogZap.h"
// Helper class to invoke tools which need parameters
#include "ToolInvokeHelper.h"

#include "ControlIDs.h"

// OS Interface
#include "PPOpenPanel.h"
#include "PPSavePanel.h"

#ifdef NDEBUG
#	define BATCH_SUFFIX ""
#else
#	define BATCH_SUFFIX "_debug"
#endif


static inline pp_int32 myMod(pp_int32 a, pp_int32 b)
{
	pp_int32 res = a%b;
	if (res<0) res+=b;
	return res;
}

pp_int32 Tracker::SCOPESHEIGHT() 
{
	return (48*screen->getHeight()) / 480;
}

pp_int32 Tracker::CURRENTSCOPESHEIGHT() 
{
	if (!scopesControl)
		return 0;
	
	if (scopesControl->isVisible())
		return SCOPESHEIGHT();

	return 0;
}

pp_int32 Tracker::SAMPLESECTIONDEFAULTHEIGHT()		
{ 
	return screen->getHeight() < 480 ? 180 : 240;
}

pp_int32 Tracker::MAXEDITORHEIGHT()
{
	TabHeaderControl* tabControl = static_cast<TabHeaderControl*>(screen->getControlByID(TABHEADER_CONTROL));
	if (tabControl != NULL)
	{
		return tabControl->getNumTabs() > 1 ? screen->getHeight() - TABHEADERHEIGHT() : screen->getHeight();
	}
	return screen->getHeight();
}

Tracker::Tracker() :
	screen(NULL),
	peakLevelControl(NULL),
	scopesControl(NULL),
	messageBoxContainerGeneric(NULL),
	dialog(NULL),
	responder(NULL),
	playTimeText(NULL),
	instrumentChooser(NULL),
	inputContainerCurrent(NULL),
	inputContainerDefault(NULL),
	inputContainerExtended(NULL),
	settingsDatabaseCopy(NULL),
	eventKeyDownBindings(NULL),
	eventKeyDownBindingsMilkyTracker(NULL),
	eventKeyDownBindingsFastTracker(NULL),
	currentFileName(TrackerConfig::untitledSong),
	lastState(false),
	editMode(EditModeFastTracker),
	extendedOrderlist(false),
	followSong(true),
	caughtMouseInUpperLeftCorner(false), 
	useClassicBrowser(false),
	savePanel(NULL),
	fileSystemChangedListener(NULL)
{
	resetStateMemories();

	settingsDatabase = new TrackerSettingsDatabase();

	buildDefaultSettings();
	
	tabManager = new TabManager(*this);
	
	playerMaster = new PlayerMaster(TrackerConfig::numTabs);
	playerController = tabManager->createPlayerController();
	
	moduleEditor = tabManager->createModuleEditor();

	playerLogic = new PlayerLogic(*this);
	recorderLogic = new RecorderLogic(*this);

	// Sections
	sectionSwitcher = new SectionSwitcher(*this);
	
	sections = new PPSimpleVector<SectionAbstract>();
	
	sectionTranspose = new SectionTranspose(*this);
	sections->add(sectionTranspose);
	sectionAdvancedEdit = new SectionAdvancedEdit(*this);
	sections->add(sectionAdvancedEdit);
	sectionDiskMenu = new SectionDiskMenu(*this);
	sections->add(sectionDiskMenu);
	sectionSettings = new SectionSettings(*this);
	sections->add(sectionSettings);
	sectionInstruments = new SectionInstruments(*this);
	sections->add(sectionInstruments);
	sectionSamples = new SectionSamples(*this);
	sections->add(sectionSamples);
	sectionQuickOptions = new SectionQuickOptions(*this);
	sections->add(sectionQuickOptions);
	sectionOptimize = new SectionOptimize(*this);
	sections->add(sectionOptimize);
	sectionAbout = new SectionAbout(*this);
	sections->add(sectionAbout);

	inputControlListener = new InputControlListener(*this);

	toolInvokeHelper = new ToolInvokeHelper(*this);

	pp_int32 i;
	
	muteChannels = new bool[TrackerConfig::maximumPlayerChannels];
	
	for (i = 0; i < TrackerConfig::maximumPlayerChannels; i++)
		muteChannels[i] = false;
		
	initKeyBindings();
}

Tracker::~Tracker()
{
	delete eventKeyDownBindingsMilkyTracker;
	delete eventKeyDownBindingsFastTracker;
	
	delete toolInvokeHelper;
	delete responder;
	delete dialog;
	delete inputControlListener;

	delete sections;
	delete sectionSwitcher;

	delete recorderLogic;
	delete playerLogic;

	delete playerMaster;
	
	delete messageBoxContainerGeneric;
		
	delete[] muteChannels;
	
	delete instrumentChooser;
		
	delete settingsDatabaseCopy;		
	delete settingsDatabase; 
}

PatternEditor* Tracker::getPatternEditor()
{
	return moduleEditor->getPatternEditor();
}

SampleEditor* Tracker::getSampleEditor()
{
	return moduleEditor->getSampleEditor();
}

EnvelopeEditor* Tracker::getEnvelopeEditor()
{
	return moduleEditor->getEnvelopeEditor();
}

pp_int32 Tracker::getOrderListBoxIndex()
{
	return listBoxOrderList->getSelectedIndex();
}

void Tracker::setOrderListIndex(pp_int32 index)
{
	// do not accept values which exceed the current number of orders
	if (index >= moduleEditor->getNumOrders())
		index = moduleEditor->getNumOrders()-1;
		
	listBoxOrderList->setSelectedIndex(index);
	updateOrderlist();
	// fake selection from orderlist, so everything will be updated correctly
	PPEvent e(eSelection, &index, sizeof(index));
	handleEvent(reinterpret_cast<PPObject*>(listBoxOrderList), &e);		
}

bool Tracker::isEditingCurrentOrderlistPattern()
{
	return moduleEditor->isEditingOrderPosition(getOrderListBoxIndex());
}

pp_int32 Tracker::getInstrumentToPlay(pp_int32 note, PlayerController*& playerController)
{
	if (PPControl* ctrl = screen->getModalControl())
	{
		note--;
	
		PPContainer* container = static_cast<PPContainer*>(ctrl);
	
		PPListBox* listBoxSrc = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC));
		PPListBox* listBoxSrcSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));
		PPListBox* listBoxSrcModule = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC3));		
		PPListBox* listBoxDst = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST));
		PPListBox* listBoxDstSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));
		PPListBox* listBoxDstModule = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST3));
		
		if (!listBoxSrc || !listBoxDst)
			return -1;

		PPListBox* focusedListBox = static_cast<PPListBox*>(container->getFocusedControl());
		if (focusedListBox == NULL)
			return getPatternEditorControl()->isInstrumentEnabled() ? listBoxInstruments->getSelectedIndex() + 1 : 0;
		
		// not having any module selection boxes
		if (listBoxSrc && listBoxDst &&
			!listBoxSrcModule && !listBoxDstModule)
		{
			// return the selected index from the focused list box
			if (focusedListBox == listBoxSrc ||
				focusedListBox == listBoxDst)
				return focusedListBox->getSelectedIndex() + 1;
		}
		
		// focus is on instruments		
		if (focusedListBox == listBoxSrc ||
			focusedListBox == listBoxSrcModule)
		{
			playerController = tabManager->getPlayerControllerFromTabIndex(listBoxSrcModule->getSelectedIndex());
			// return the selected index from the focused list box
			return listBoxSrc->getSelectedIndex() + 1;
		}
		else if (focusedListBox == listBoxDst ||
				 focusedListBox == listBoxDstModule)
		{
			playerController = tabManager->getPlayerControllerFromTabIndex(listBoxDstModule->getSelectedIndex());
			// return the selected index from the focused list box
			return listBoxDst->getSelectedIndex() + 1;
		}
		
		// if focus is on one of the samples list boxes set up some sample playing
		// on the sample playing channels of the current player
		ModuleEditor* src = listBoxSrcModule ? tabManager->getModuleEditorFromTabIndex(listBoxSrcModule->getSelectedIndex()) : this->moduleEditor;
		ModuleEditor* dst = listBoxDstModule ? tabManager->getModuleEditorFromTabIndex(listBoxDstModule->getSelectedIndex()) : this->moduleEditor;

		if (focusedListBox == listBoxSrcSmp)
		{
			SamplePlayer samplePlayer(*src, *playerController);
			if (focusedListBox == listBoxSrcSmp)
				samplePlayer.playSample(listBoxSrc->getSelectedIndex(),
										listBoxSrcSmp->getSelectedIndex(), note);
			else
				samplePlayer.playSample(listBoxSrc->getSelectedIndex(),
										note);
			return -1;
		}

		if (focusedListBox == listBoxDstSmp)
		{
			SamplePlayer samplePlayer(*dst, *playerController);
			if (focusedListBox == listBoxDstSmp)
				samplePlayer.playSample(listBoxDst->getSelectedIndex(),
										listBoxDstSmp->getSelectedIndex(), note);
			else
				samplePlayer.playSample(listBoxDst->getSelectedIndex(),
										note);
			return -1;
		}
		
		return focusedListBox->getSelectedIndex() + 1;
	}
	else
	{
		return getPatternEditorControl()->isInstrumentEnabled() ? listBoxInstruments->getSelectedIndex() + 1 : 0;
	}
}

void Tracker::setNumChannels(pp_int32 numChannels, bool repaint/* = true*/)
{
	getPatternEditorControl()->setNumVisibleChannels(numChannels);
	scopesControl->setNumChannels(numChannels);
	updatePatternEditorControl(repaint, false);
}

void Tracker::showSongSettings(bool show)
{
	screen->getControlByID(CONTAINER_ABOUT)->show(show);
	screen->getControlByID(CONTAINER_ORDERLIST)->show(show);
	screen->getControlByID(CONTAINER_SPEED)->show(show);
	screen->getControlByID(CONTAINER_PATTERN)->show(show);
}

void Tracker::showMainOptions(bool show)
{
	screen->getControlByID(CONTAINER_MENU)->show(show);	
}

void Tracker::showMainMenu(bool show, bool showInstrumentSelector)
{
	showSongSettings(show);
	showMainOptions(show);
	if (showInstrumentSelector)
		screen->getControlByID(CONTAINER_INSTRUMENTLIST)->show(show);
}

void Tracker::setModuleNumChannels(pp_uint32 numChannels)
{
	moduleEditor->setNumChannels(numChannels);
	setNumChannels(numChannels);
}

pp_int32 Tracker::handleEvent(PPObject* sender, PPEvent* event)
{
	char buffer[100];

	if (event->getID() == eFileDragDropped)
	{
		if (screen->getModalControl())
			return 0;
		const PPSystemString* str = *(reinterpret_cast<PPSystemString* const*>(event->getDataPtr()));
		loadGenericFileType(*str);
		event->cancel();
	}
	else if (event->getID() == eUpdateChanged)
	{
		updateWindowTitle();
	}
	else if (event->getID() == eKeyDown ||
			 event->getID() == eKeyChar ||
			 event->getID() == eKeyUp) 
	{	
		processShortcuts(event);
	}
	else if (event->getID() == eTimer)
	{
		doFollowSong();
	}
#ifndef __LOWRES__
	else if (event->getID() == eLMouseDown)
	{
		PPPoint* p = (PPPoint*)event->getDataPtr();
		caughtMouseInUpperLeftCorner = (p->x <= TrackerConfig::trackerExitBounds.x && p->y <= TrackerConfig::trackerExitBounds.y) ? true : false;
		if (caughtMouseInUpperLeftCorner)
			event->cancel();
	}
	else if (event->getID() == eLMouseUp)
	{
		PPPoint* p = (PPPoint*)event->getDataPtr();
		
		if ((p->x <= TrackerConfig::trackerExitBounds.x && p->y <= TrackerConfig::trackerExitBounds.y) && caughtMouseInUpperLeftCorner)
		{
			event->cancel();
			eventKeyDownBinding_ExitApplication();
		}
		else
			caughtMouseInUpperLeftCorner = false;
	}
#endif
	else if (event->getID() == eCommand || event->getID() == eCommandRepeat)
	{

		switch (reinterpret_cast<PPControl*>(sender)->getID())
		{
			// test
			/*case BUTTON_MENU_ITEM_0+8:
			{
				if (event->getID() != eCommand)
					break;
				
				eventKeyDownBinding_InvokeSectionHDRecorder();
				break;
			}*/
			
			case BUTTON_INSTRUMENT:
			{
				if (event->getID() != eCommand)
					break;
			
				enableInstrument(!getPatternEditorControl()->isInstrumentEnabled());
				break;
			}

			case STATICTEXT_ABOUT_HEADING:
			{
				if (event->getID() != eCommand)
					break;
			
				TitlePageManager titlePageManager(*screen);
				titlePageManager.setPeakControlHeadingColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
				break;
			}
					
			case BUTTON_ABOUT_SHOWTITLE:
			{
				if (event->getID() != eCommand)
					break;

				TitlePageManager titlePageManager(*screen);
				titlePageManager.showTitlePage(TitlePageManager::PageTitle);
				break;
			}

			case BUTTON_ABOUT_SHOWTIME:
			{
				if (event->getID() != eCommand)
					break;
				TitlePageManager titlePageManager(*screen);
				titlePageManager.showTitlePage(TitlePageManager::PageTime);
				break;
			}
			
			case BUTTON_ABOUT_SHOWPEAK:
			{
				if (event->getID() != eCommand)
					break;
				TitlePageManager titlePageManager(*screen);
				titlePageManager.showTitlePage(TitlePageManager::PagePeak);
				break;
			}

			case BUTTON_ABOUT_FOLLOWSONG:
			{
				if (event->getID() != eCommand)
					break;
				eventKeyDownBinding_ToggleFollowSong();
				break;
			}

			case BUTTON_ABOUT_PROSPECTIVE:
			{
				if (event->getID() != eCommand)
					break;
				eventKeyDownBinding_ToggleProspectiveMode();
				break;
			}

			case BUTTON_ABOUT_WRAPCURSOR:
			{
				if (event->getID() != eCommand)
					break;
				eventKeyDownBinding_ToggleCursorWrapAround();
				break;
			}

			case BUTTON_ABOUT_LIVESWITCH:
			{
				if (event->getID() != eCommand)
					break;
				eventKeyDownBinding_ToggleLiveSwitch();
				break;
			}

			case BUTTON_ORDERLIST_EXTENT:
			{
				if (event->getID() != eCommand)
					break;
				expandOrderlist(!extendedOrderlist);
				screen->paintControl(screen->getControlByID(CONTAINER_ORDERLIST));
				break;
			}
		
			case BUTTON_SPEEDCONTAINERFLIP:
			{
				if (event->getID() != eCommand)
					break;
				flipSpeedSection();
				screen->paintControl(screen->getControlByID(CONTAINER_SPEED));
				break;
			}
					
			// -------- generic message box -------------------
			case PP_MESSAGEBOX_BUTTON_YES:
			case PP_MESSAGEBOX_BUTTON_NO:
			case PP_MESSAGEBOX_BUTTON_CANCEL:
			case PP_MESSAGEBOX_BUTTON_USER1:
			case PP_MESSAGEBOX_BUTTON_USER2:
			case PP_MESSAGEBOX_BUTTON_USER3:
			case PP_MESSAGEBOX_BUTTON_USER4:
			case PP_MESSAGEBOX_BUTTON_USER5:
			case PP_MESSAGEBOX_BUTTON_USER6:
			case PP_MESSAGEBOX_BUTTON_USER7:
			case PP_MESSAGEBOX_BUTTON_USER8:
			case PP_MESSAGEBOX_BUTTON_USER9:
			// little hack, user buttons below PP_MESSAGEBOX_BUTTON_USER10
			// are not allowed to send repeatable pressed down events
				if (event->getID() != eCommand)
					break;
			case PP_MESSAGEBOX_BUTTON_USER10:
			case PP_MESSAGEBOX_BUTTON_USER11:
			case PP_MESSAGEBOX_BUTTON_USER12:
			case PP_MESSAGEBOX_BUTTON_USER13:
			case PP_MESSAGEBOX_BUTTON_USER14:
			case PP_MESSAGEBOX_BUTTON_USER15:
			{
				bool res = messageBoxEventListener(screen->getModalControl()->getID(), 
												   reinterpret_cast<PPControl*>(sender)->getID());
				
				if (res)
					screen->setModalControl(NULL);  // repaints
				
				break;
			}
			
			case MAINMENU_PLAY_SONG:
				playerLogic->playSong();
				break;

			case MAINMENU_PLAY_PATTERN:
				playerLogic->playPattern();
				break;

			case MAINMENU_PLAY_POSITION:
				playerLogic->playPosition();
				break;

			case MAINMENU_STOP:
				playerLogic->stopSong();
				break;

			// -------- ZAP message box ---------------------
			case MAINMENU_ZAP:
				if (event->getID() != eCommand)
					break;

				if (dialog)
					delete dialog;
				
				if (responder)
					delete responder;
					
				responder = new ZapHandler(Zapper(*this));				
				dialog = new DialogZap(screen, responder, PP_DEFAULT_ID);	

				dialog->show();
				break;
			
			// open song	
			case MAINMENU_LOAD:
			{
				if (event->getID() != eCommand)
					break;
				
				eventKeyDownBinding_Open();
				break;
			}

			case MAINMENU_SAVE:
			{
				if (event->getID() != eCommand)
					break;

				eventKeyDownBinding_Save();
				break;
			}

			case MAINMENU_SAVEAS:
			{
				if (event->getID() != eCommand)
					break;

				eventKeyDownBinding_SaveAs();
				break;
			}

			// disk op
			case MAINMENU_DISKMENU:
			{				
				if (event->getID() != eCommand)
					break;
				
				eventKeyDownBinding_InvokeSectionDiskMenu();
				break;
			}

			// Only Fasttracker II editing mode:
			// Edit button
			case MAINMENU_EDIT:
			{
				if (event->getID() != eCommand)
					break;

				switch (editMode)
				{
					case EditModeFastTracker:
						eventKeyDownBinding_ToggleFT2Edit();
						break;

					case EditModeMilkyTracker:
						eventKeyDownBinding_Edit();
						break;
				}
				break;
			}

			// instrument editor
			case MAINMENU_INSEDIT:
			{
				if (event->getID() != eCommand)
					break;

				eventKeyDownBinding_InvokeSectionInstruments();
				break;
			}

			case BUTTON_INSTRUMENTEDITOR_EXIT:
			case BUTTON_SAMPLEEDITOR_EXIT:
			{
				if (event->getID() != eCommand)
					break;

				sectionSwitcher->showBottomSection(SectionSwitcher::ActiveBottomSectionNone);
				screen->paint(true, true);
				break;
			}

			// sample editor
			case MAINMENU_SMPEDIT:
			{
				if (event->getID() != eCommand)
					break;

				eventKeyDownBinding_InvokeSectionSamples();
				break;
			}

			// settings
			case MAINMENU_ADVEDIT:
			{				
				if (event->getID() != eCommand)
					break;
				
				eventKeyDownBinding_InvokeSectionAdvancedEdit();
				break;
			}

			// transpose
			case MAINMENU_TRANSPOSE:
			{				
				if (event->getID() != eCommand)
					break;
				
				eventKeyDownBinding_InvokeSectionTranspose();
				break;
			}

			// settings
			case MAINMENU_CONFIG:
			{
				if (event->getID() != eCommand)
					break;
					
				eventKeyDownBinding_InvokeSectionSettings();
				break;
			}

			// quick options
			case MAINMENU_QUICKOPTIONS:
			{
				if (event->getID() != eCommand)
					break;
					
				eventKeyDownBinding_InvokeSectionQuickOptions();
				break;
			}

			// optimize
			case MAINMENU_OPTIMIZE:
			{
				if (event->getID() != eCommand)
					break;
					
				eventKeyDownBinding_InvokeSectionOptimize();
				break;
			}

			case MAINMENU_ABOUT:
			{
				if (event->getID() != eCommand)
					break;
					
				eventKeyDownBinding_InvokeSectionAbout();
				break;
			}
					
			case BUTTON_ORDERLIST_SONGLENGTH_PLUS:
				moduleEditor->increaseSongLength();
				updateSongLength();
				break;

			case BUTTON_ORDERLIST_SONGLENGTH_MINUS:
				moduleEditor->decreaseSongLength();
				updateSongLength();
				break;
			
			case BUTTON_ORDERLIST_REPEAT_PLUS:
				moduleEditor->increaseRepeatPos();
				updateSongRepeat();
				break;

			case BUTTON_ORDERLIST_REPEAT_MINUS:
				moduleEditor->decreaseRepeatPos();
				updateSongRepeat();
				break;

			// insert position into orderlist
			case BUTTON_ORDERLIST_INSERT:
				moduleEditor->insertNewOrderPosition(getOrderListBoxIndex());
				updateOrderlist();
				playerLogic->continuePlayingSong();
				break;

			// delete current orderlist position
			case BUTTON_ORDERLIST_DELETE:
				moduleEditor->deleteOrderPosition(getOrderListBoxIndex());
				updateOrderlist();
				playerLogic->continuePlayingSong();
				break;

			// insert position into orderlist
			case BUTTON_ORDERLIST_SEQENTRY:
			{
				moduleEditor->seqCurrentOrderPosition(getOrderListBoxIndex());
				updateSongLength(false);
				pp_int32 index = getOrderListBoxIndex()+1;
				setOrderListIndex(index);
				playerLogic->continuePlayingSong();
				break;
			}

			// insert position into orderlist and clone the current selected pattern
			case BUTTON_ORDERLIST_CLNENTRY:
			{
				moduleEditor->seqCurrentOrderPosition(getOrderListBoxIndex(), true);
				updateSongLength(false);
				pp_int32 index = getOrderListBoxIndex()+1;
				setOrderListIndex(index);
				playerLogic->continuePlayingSong();
				break;
			}

			// select next pattern in current orderlist position
			case BUTTON_ORDERLIST_NEXT:
				moduleEditor->increaseOrderPosition(getOrderListBoxIndex());
				updateOrderlist();
				playerLogic->continuePlayingSong();
				break;
			
			// select previous pattern in current orderlist position
			case BUTTON_ORDERLIST_PREVIOUS:
				moduleEditor->decreaseOrderPosition(getOrderListBoxIndex());
				updateOrderlist();
				playerLogic->continuePlayingSong();
				break;

			case BUTTON_OCTAVE_MINUS:
				getPatternEditor()->decreaseCurrentOctave();
				updatePatternAddAndOctave();				
				break;
			case BUTTON_OCTAVE_PLUS:
				getPatternEditor()->increaseCurrentOctave();
				updatePatternAddAndOctave();				
				break;

			case BUTTON_ADD_PLUS:
				getPatternEditorControl()->increaseRowInsertAdd();
				updatePatternAddAndOctave();				
				break;
			
			case BUTTON_ADD_MINUS:
				getPatternEditorControl()->decreaseRowInsertAdd();
				updatePatternAddAndOctave();				
				break;

/*			case BUTTON_BPM_PLUS:
			{
				setChanged();
				mp_sint32 bpm,speed;
				playerController->getSpeed(bpm, speed);
				playerController->setSpeed(bpm+1, speed);				
				updateSpeed();				
				break;
			}
			
			case BUTTON_BPM_MINUS:
			{
				setChanged();
				mp_sint32 bpm,speed;
				playerController->getSpeed(bpm, speed);
				playerController->setSpeed(bpm-1, speed);
				updateSpeed();				
				break;
			}*/
			
			case BUTTON_SPEED_PLUS:
			{
				setChanged();
				mp_sint32 bpm,speed;
				playerController->getSpeed(bpm, speed);
				playerController->setSpeed(bpm, speed+1);
				updateSpeed();				
				break;
			}
			
			case BUTTON_SPEED_MINUS:
			{
				setChanged();
				mp_sint32 bpm,speed;
				playerController->getSpeed(bpm, speed);
				playerController->setSpeed(bpm, speed-1);
				updateSpeed();				
				break;
			}

			// go to next pattern
			case BUTTON_PATTERN_PLUS:
				eventKeyDownBinding_NextPattern();
				break;

			// go to previous pattern
			case BUTTON_PATTERN_MINUS:
				eventKeyDownBinding_PreviousPattern();
				break;
			
			// expand current pattern
			case BUTTON_PATTERN_EXPAND:
				getPatternEditor()->expandPattern();
				updatePatternLength(false);
				screen->update();
				break;
			
			// shrink current pattern
			case BUTTON_PATTERN_SHRINK:
				getPatternEditor()->shrinkPattern();
				updatePatternLength(false);
				screen->update();
				break;

			// grow length
			case BUTTON_PATTERN_SIZE_PLUS:
				getPatternEditor()->resizePattern(moduleEditor->getPattern(moduleEditor->getCurrentPatternIndex())->rows + 1);
				updatePatternLength(false);
				screen->update();
				break;
			
			// decrease length
			case BUTTON_PATTERN_SIZE_MINUS:
				getPatternEditor()->resizePattern(moduleEditor->getPattern(moduleEditor->getCurrentPatternIndex())->rows - 1);
				updatePatternLength(false);
				screen->update();
				break;

			case BUTTON_INSTRUMENTS_PLUS:
				moduleEditor->allocateInstrument();
				updateInstrumentsListBox(false);
				sectionInstruments->update(false);
				screen->update();
				break;

			case BUTTON_INSTRUMENTS_MINUS:
			{
				pp_uint32 i = listBoxInstruments->getSelectedIndex();
				moduleEditor->freeInstrument();
				updateInstrumentsListBox(false);
				
				if (listBoxInstruments->getSelectedIndex() != i)
				{
					getPatternEditorControl()->setCurrentInstrument(listBoxInstruments->getSelectedIndex() + 1);
					updateSampleEditorAndInstrumentSection(false);
				}
				
				screen->update();
				break;
			}

			case BUTTON_TAB_OPEN:
			{
				eventKeyDownBinding_OpenTab();
				break;
			}

			case BUTTON_TAB_CLOSE:
			{
				eventKeyDownBinding_CloseTab();
				break;
			}

			case BUTTON_BLS_CONVERT:
			{
				if (TrackerConfig::untitledSong.compareTo(moduleEditor->getModuleFileNameFull().stripExtension()) != 0) 
				{
					save();
					runBLSBatch(moduleEditor->getModuleFileNameFull(), "BlsConvert" BATCH_SUFFIX ".bat");
				}
				break;
			}

			case BUTTON_YM_TEST:
			{
				runBLSBatch(ResamplerYM::GetInstance()->GetSndSynFilename(), "SynthYMrun" BATCH_SUFFIX ".bat");
				break;
			}

			case BUTTON_YM_EDIT:
			{
				runBLSBatch(ResamplerYM::GetInstance()->GetSndSynFilename(), "SynthYMedit.bat");
				break;
			}

			case BUTTON_BLS_CONVERT_N_PLAY_BLS:
			{
				if (TrackerConfig::untitledSong.compareTo(moduleEditor->getModuleFileNameFull().stripExtension()) != 0) 
				{
					save();
					runBLSBatch(moduleEditor->getModuleFileNameFull(), "BlsConvertNPlay" BATCH_SUFFIX ".bat");
				}
				break;
			}

			case BUTTON_BLS_CONVERT_N_PLAY_BLZ:
			{
				if (TrackerConfig::untitledSong.compareTo(moduleEditor->getModuleFileNameFull().stripExtension()) != 0)
				{
					save();
					runBLSBatch(moduleEditor->getModuleFileNameFull(), "BlzConvertNPlay" BATCH_SUFFIX ".bat");
				}
				break;
			}

			case BUTTON_YM_RELOAD:
			{
				playerLogic->stopSong();

				if ( ResamplerYM::GetInstance()->Reload() )
				{
					fillYMSoundsListBox(listBoxYMsounds);
					refreshYMSoundsIntrumentsMapping();
					screen->paint();
				}
				else
				{
					char temp[512];

					sprintf(temp, "Error loading %s", ResamplerYM::GetInstance()->GetSndSynFilename());
					PPMessageBox messageBox(nullptr, temp, ResamplerYM::GetInstance()->GetError());		
					messageBox.runModal();
				}
				break;
			}
		}
		
		// Check if something has changed
		updateWindowTitle();
		
	}
	else if (event->getID() == ePreSelection)
	{
	}
	else if (event->getID() == eSelection)
	{		
		switch (reinterpret_cast<PPControl*>(sender)->getID())
		{
			case TABHEADER_CONTROL:
			{
				pp_int32 index = *((pp_int32*)event->getDataPtr());
				tabManager->switchToTab(index);
				break;
			}
		
			// new pattern has been selected in the orderlist
			case LISTBOX_ORDERLIST:
			{
				pp_int32 orderIndex = *((pp_int32*)event->getDataPtr());
				moduleEditor->setCurrentOrderIndex(orderIndex);
				moduleEditor->setCurrentPatternIndex(moduleEditor->getOrderPosition(orderIndex));
				
				updatePatternEditorControl(false);
				updatePatternIndex(false);
				updatePatternLength(false);
				screen->update();
				
				if (playerController->isPlayingPattern())
					playerLogic->continuePlayingPattern();
				else
					playerLogic->continuePlayingSong();
				break;
			}

			// new instrument has been selected
			case LISTBOX_INSTRUMENTS:
			{
				pp_int32 index = *((pp_int32*)event->getDataPtr()) + 1;
				selectInstrument(index);		
				screen->update();
				break;
			}

			case LISTBOX_YMSOUNDS:
			{
				pp_int32 instruIndex = listBoxInstruments->getSelectedIndex();

				if ( instruIndex >= 0 )
				{
					pp_int32 indexYMsounds = *((pp_int32*)event->getDataPtr());

					const PPString& ymSoundName = listBoxYMsounds->getItem(indexYMsounds);

					moduleEditor->setInstrumentName(instruIndex, ymSoundName.getStrBuffer(), ModuleEditor::MAX_INSTEXT);
					listBoxInstruments->updateItem(instruIndex, ymSoundName);
					screen->paintControl(listBoxInstruments);
					
					refreshYMSoundsIntrumentsMapping();
				}
				break;
			}

			// Instrument chooser
			case INSTRUMENT_CHOOSER_LIST_SRC3:
			case INSTRUMENT_CHOOSER_LIST_DST3:
			{
				PPContainer* container = static_cast<PPContainer*>(screen->getModalControl());

				PPStaticText* staticText = static_cast<PPStaticText*>(container->getControlByID(INSTRUMENT_CHOOSER_USERSTR1));

				PPListBox* listBoxChangeIns = NULL;
				PPListBox* listBoxChangeSmp = NULL;

				PPListBox* listBoxSrcIns = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC));
				PPListBox* listBoxSrcSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));

				PPListBox* listBoxDstIns = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST));;
				PPListBox* listBoxDstSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));

				if (reinterpret_cast<PPControl*>(sender)->getID() == INSTRUMENT_CHOOSER_LIST_SRC3)
				{
					listBoxChangeIns = listBoxSrcIns;
					listBoxChangeSmp = listBoxSrcSmp;
				}
				else if (reinterpret_cast<PPControl*>(sender)->getID() == INSTRUMENT_CHOOSER_LIST_DST3)
				{
					listBoxChangeIns = listBoxDstIns;
					listBoxChangeSmp = listBoxDstSmp;
				}

				if (listBoxChangeIns && listBoxChangeSmp)
				{
					listBoxChangeIns->clear();
					listBoxChangeSmp->clear();
					
					fillInstrumentListBox(listBoxChangeIns,
										  tabManager->getModuleEditorFromTabIndex((reinterpret_cast<PPListBox*>(sender))->getSelectedIndex()));

					fillSampleListBox(listBoxChangeSmp, listBoxChangeIns->getSelectedIndex(), 
									  tabManager->getModuleEditorFromTabIndex((reinterpret_cast<PPListBox*>(sender))->getSelectedIndex()));
				}

				// update text depending on the type of the instrument chooser dialog
				switch (container->getID())
				{
					case INSTRUMENT_CHOOSER_COPY:
						sprintf(buffer, "Copy ins. %x to %x", listBoxSrcIns->getSelectedIndex()+1, listBoxDstIns->getSelectedIndex()+1);
						break;
					case INSTRUMENT_CHOOSER_SWAP:
						sprintf(buffer, "Swap ins. %x with %x", listBoxSrcSmp->getSelectedIndex()+1, listBoxDstSmp->getSelectedIndex()+1);
						break;
				}

				staticText->setText(buffer);				
				screen->paintControl(screen->getModalControl());				
				break;
			}

			// Instrument chooser
			case INSTRUMENT_CHOOSER_LIST_SRC:
			case INSTRUMENT_CHOOSER_LIST_DST:
			{
				PPContainer* container = static_cast<PPContainer*>(screen->getModalControl());
				
				PPStaticText* staticText = static_cast<PPStaticText*>(container->getControlByID(INSTRUMENT_CHOOSER_USERSTR1));
					
				PPListBox* listBoxSrc = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC));
				PPListBox* listBoxDst = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST));

				PPListBox* listBoxSrcModule = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC3));
				PPListBox* listBoxDstModule = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST3));
				
				ModuleEditor* src = listBoxSrcModule ? tabManager->getModuleEditorFromTabIndex(listBoxSrcModule->getSelectedIndex()) : this->moduleEditor;
				ModuleEditor* dst = listBoxDstModule ? tabManager->getModuleEditorFromTabIndex(listBoxDstModule->getSelectedIndex()) : this->moduleEditor;				
				
				PPListBox* listBoxChange = NULL;
				ModuleEditor* moduleEditor = this->moduleEditor;
				
				// A new instrument has been selected in either of the two instrument list boxes
				// now it's up to update the samples belonging to the instrument in the sample listboxes
				if (reinterpret_cast<PPControl*>(sender)->getID() == INSTRUMENT_CHOOSER_LIST_SRC)
				{
					listBoxChange = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));
					moduleEditor = src;
				}
				else if (reinterpret_cast<PPControl*>(sender)->getID() == INSTRUMENT_CHOOSER_LIST_DST)
				{
					listBoxChange = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));
					moduleEditor = dst;
				}
					
				if (listBoxChange)
				{
					listBoxChange->clear();
					fillSampleListBox(listBoxChange, reinterpret_cast<PPListBox*>(sender)->getSelectedIndex(), moduleEditor);
				}
				
				// update text depending on the type of the instrument chooser dialog
				switch (container->getID())
				{
					case INSTRUMENT_CHOOSER_COPY:
						sprintf(buffer, "Copy ins. %x to %x", listBoxSrc->getSelectedIndex()+1, listBoxDst->getSelectedIndex()+1);
						break;
					case INSTRUMENT_CHOOSER_SWAP:
						sprintf(buffer, "Swap ins. %x with %x", listBoxSrc->getSelectedIndex()+1, listBoxDst->getSelectedIndex()+1);
						break;
					case MESSAGEBOX_INSREMAP:
						sprintf(buffer, "Remap ins. %x to %x", listBoxSrc->getSelectedIndex()+1, listBoxDst->getSelectedIndex()+1);
						break;
				}
				
				staticText->setText(buffer);				
				screen->paintControl(screen->getModalControl());				
				break;
			}

			case INSTRUMENT_CHOOSER_LIST_SRC2:
			case INSTRUMENT_CHOOSER_LIST_DST2:
			{
				PPContainer* container = static_cast<PPContainer*>(screen->getModalControl());
				
				PPStaticText* staticText = static_cast<PPStaticText*>(container->getControlByID(INSTRUMENT_CHOOSER_USERSTR2));
					
				PPListBox* listBoxSrc = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));
				PPListBox* listBoxDst = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));
				
				switch (container->getID())
				{
					case INSTRUMENT_CHOOSER_COPY:
						sprintf(buffer, "Copy smp. %x to %x", listBoxSrc->getSelectedIndex(), listBoxDst->getSelectedIndex());
						break;
					case INSTRUMENT_CHOOSER_SWAP:
						sprintf(buffer, "Swap smp. %x with %x", listBoxSrc->getSelectedIndex(), listBoxDst->getSelectedIndex());
						break;
				}
				
				staticText->setText(buffer);				
				screen->paintControl(screen->getModalControl());
				break;
			}
			
		}

	}
	else if (event->getID() == eValueChanged)
	{
		switch (reinterpret_cast<PPControl*>(sender)->getID())
		{
			case LISTBOX_SONGTITLE:
			{
				moduleEditor->setTitle(**(reinterpret_cast<const PPString* const*>(event->getDataPtr())), ModuleEditor::MAX_TITLETEXT);
				break;
			}
		
			case LISTBOX_INSTRUMENTS:
			{
				moduleEditor->setInstrumentName(listBoxInstruments->getSelectedIndex(), 
												**(reinterpret_cast<const PPString* const*>(event->getDataPtr())), ModuleEditor::MAX_INSTEXT);
				break;
			}
		
			// channels have been muted/unmuted in pattern editor
			case PATTERN_EDITOR:
			{
				const bool* muteChannelsPtr = reinterpret_cast<const bool*>(event->getDataPtr());
				
				for (pp_int32 i = 0; i < TrackerConfig::numPlayerChannels; i++)
				{
					muteChannels[i] = muteChannelsPtr[i];
					bool b = (muteChannels[i] != 0);
					playerController->muteChannel(i, b);
					scopesControl->muteChannel(i, b);
				}
				
				if (scopesControl->needsUpdate())
					screen->paintControl(scopesControl);
				break;
			}
			
			// channels have been muted/unmuted in scopes
			case SCOPES_CONTROL:
			{
				switch (event->getMetaData())
				{
					case ScopesControl::ChangeValueMuting:
					{
						const bool* muteChannelsPtr = reinterpret_cast<const bool*>(event->getDataPtr());
						
						for (pp_int32 i = 0; i < TrackerConfig::numPlayerChannels; i++)
						{
							muteChannels[i] = muteChannelsPtr[i];
							bool b = (muteChannels[i] != 0);
							playerController->muteChannel(i, b);
							getPatternEditorControl()->muteChannel(i, b);
						}
						
						if (scopesControl->needsUpdate())
							screen->paintControl(scopesControl, false);
						screen->paintControl(getPatternEditorControl(), false);
						
						screen->update();
						break;
					}					
					
					case ScopesControl::ChangeValueRecording:
					{
						const pp_uint8* recordChannelsPtr = reinterpret_cast<const pp_uint8*>(event->getDataPtr());
						
						for (pp_int32 i = 0; i < TrackerConfig::numPlayerChannels; i++)
						{
							bool b = (recordChannelsPtr[i] != 0);
							playerController->recordChannel(i, b);
							getPatternEditorControl()->recordChannel(i, b);
						}
						
						playerController->resetFirstPlayingChannel();
						
						if (scopesControl->needsUpdate())
							screen->paintControl(scopesControl, false);
						
						screen->update();
						break;
					}
				}
				break;
			}

		}	

		// Check if something has changed
		updateWindowTitle();
	
	}
	else if (event->getID() == eUpdated)
	{
		PatternEditorControl* patternEditorControl = reinterpret_cast<PatternEditorControl*>(sender);
		PatternEditor* patternEditor = patternEditorControl->getPatternEditor();
		pp_int32 numRows = patternEditor->getNumRows();
		pp_int32 row = patternEditorControl->getCurrentRow();

		bool isPlaying = playerController->isPlaying() && !playerController->isPlayingRowOnly();

		switch (reinterpret_cast<PPControl*>(sender)->getID())
		{
			// The pattern editor sends PPEvent::eUpated when the cursor has been moved
			// It can either be moved out of the current pattern or within the pattern
			// Depending on what the user data says
			case PATTERN_EDITOR:
			{
				switch (*(pp_int32*)event->getDataPtr())
				{
					case PatternEditorControl::AdvanceCodeJustUpdate:
						updatePatternLength();
						updatePatternAddAndOctave();
						break;

					// End of pattern has been wrapped with cursor down
					case PatternEditorControl::AdvanceCodeCursorDownWrappedEnd:
					{
						bool b = isEditingCurrentOrderlistPattern() && listBoxOrderList->isLastEntry();
						// When we're playing and we're not playing a pattern
						// OR when we're not playing and NOT wrapping cursor around
						// => select next order from orderlist
						if ((isPlaying && !playerController->isReallyPlayingPattern()) ||
							(playerController->isPlaying() && playerController->isPlayingPattern() && playerLogic->rowPlay) ||
							(!isPlaying && !patternEditorControl->getWrapAround()))
						{
							// we're editing the pattern from the current selected order and this is not the last order list entry
							// advance to the next order in the list
							if (!b)
							{
								screen->pauseUpdate(true);
								selectNextOrder();
								screen->pauseUpdate(false);
							}
						}
						// Calculate new position within pattern and update player position
						ASSERT(row > 0);
						
						// when we're in wraparound mode and we're not in the last 
						// pattern & row of the last order, wrap around cursor in current pattern
						if (!b || patternEditorControl->getWrapAround())
							patternEditorControl->setRow(myMod(row - numRows, patternEditor->getNumRows()), false);
						else
							patternEditorControl->setRow(numRows-1, false);
						
						updateSongLength(true);
						updatePatternIndex(true);
						updatePatternLength(true);
						updateSongRow();
						return 1;
					}

					// Start of pattern has been wrapped with cursor up
					case PatternEditorControl::AdvanceCodeCursorUpWrappedStart:
					{
						bool b = isEditingCurrentOrderlistPattern() && listBoxOrderList->isFirstEntry();
						// When we're playing and we're not playing a pattern
						// OR when we're not playing and NOT wrapping cursor around
						// => select previous order from orderlist
						if ((isPlaying && !playerController->isReallyPlayingPattern()) ||
							(playerController->isPlaying() && playerController->isPlayingPattern() && playerLogic->rowPlay) ||
							(!isPlaying && !patternEditorControl->getWrapAround()))
						{
							// we're editing the pattern from the current selected order and this is not the last order list entry
							// advance to the previous order in the list
							if (!b)
							{
								screen->pauseUpdate(true);
								selectPreviousOrder();
								screen->pauseUpdate(false);
							}
						}
						// Calculate new position within pattern and update player position
						ASSERT(row < 0);
						
						// when we're in wraparound mode and we're not in the last 
						// pattern & row of the last order, wrap around cursor in current pattern
						if (!b || patternEditorControl->getWrapAround())
							patternEditorControl->setRow(myMod(patternEditor->getNumRows() + row, patternEditor->getNumRows()), false);
						else
							patternEditorControl->setRow(0, false);
							
						updateSongLength(true);
						updatePatternIndex(true);
						updatePatternLength(true);
						updateSongRow();
						return 1;
					}

					// End of pattern has been wrapped with page down key
					case PatternEditorControl::AdvanceCodeCursorPageDownWrappedEnd:
						// Not playing? Do nothing at all
						if (!playerController->isPlaying()/* && !patternEditor->getWrapAround()*/)
							return 0;
						// Last entry in orderlist? Do nothing at all
						//if (listBoxOrderList->isLastEntry())
						//	return 0;

						// Not playing pattern? Select new order
						if (!playerController->isReallyPlayingPattern())
						{
							screen->pauseUpdate(true);
							selectNextOrder(true);
							screen->pauseUpdate(false);
						}

						// Calculate new position within pattern and update player position
						ASSERT(row > 0);
						patternEditorControl->setRow(myMod(row - numRows, patternEditor->getNumRows()), false);
						updateSongLength(true);
						updatePatternIndex(true);
						updatePatternLength(true);
						updateSongRow();
						return 1;

					// Start of pattern has been wrapped with page up key
					case PatternEditorControl::AdvanceCodeCursorPageUpWrappedStart:
						// Not playing? Do nothing at all
						if (!playerController->isPlaying()/* && !patternEditor->getWrapAround()*/)
							return 0;
						// Last entry in orderlist? Do nothing at all
						//if (listBoxOrderList->isFirstEntry())
						//	return 0;

						// Not playing pattern? Select previous order
						if (!playerController->isReallyPlayingPattern())
						{
							screen->pauseUpdate(true);
							selectPreviousOrder(true);
							screen->pauseUpdate(false);
						}

						// Calculate new position within pattern and update player position
						ASSERT(row < 0);
						patternEditorControl->setRow(myMod(patternEditor->getNumRows() + row, patternEditor->getNumRows()), false);
						updateSongLength(true);
						updatePatternIndex(true);
						updatePatternLength(true);
						updateSongRow();
						return 1;

					// End of pattern has been wrapped with page down key
					case PatternEditorControl::AdvanceCodeWrappedEnd:
						if (!patternEditorControl->getWrapAround() && !listBoxOrderList->isLastEntry())
						{
							screen->pauseUpdate(true);
							selectNextOrder(true);
							screen->pauseUpdate(false);
							patternEditorControl->setRow(myMod(row - numRows, patternEditor->getNumRows()), false);
						}
						// if we are in the last order of the order list and in the last row of this order don't advance to the next pattern
						else if (!patternEditorControl->getWrapAround() && listBoxOrderList->isLastEntry())
						{
							patternEditorControl->setRow(numRows - 1, false);							
						}
						else
						{
							// Calculate new position within pattern and update player position
							ASSERT(row > 0);						
							patternEditorControl->setRow(myMod(row - numRows, patternEditor->getNumRows()), false);
						}

						updateSongLength(true);
						updatePatternIndex(true);
						updatePatternLength(true);
						updateSongRow();
						return 1;
						
					// Not wrapped at all... Just calculate new position within pattern and update player position
					case PatternEditorControl::AdvanceCodeSelectNewRow:
						if (shouldFollowSong() && 
							isEditingCurrentOrderlistPattern())
							updateSongRow();
						break;
				}
				break;
			}
		}
	}
	// frontend layer sends PPEvent::eFullScreen when the APP is going fullscreen
	else if (event->getID() == eFullScreen)
	{
		TrackerSettingsDatabase* settingsDatabaseCopySecond = new TrackerSettingsDatabase(*settingsDatabase);
		
		if (settingsDatabaseCopy)
			settingsDatabaseCopy->store("FULLSCREEN", !screen->isFullScreen());
		settingsDatabaseCopySecond->store("FULLSCREEN", !screen->isFullScreen());
		
		applySettings(settingsDatabaseCopySecond, settingsDatabase);
		
		delete settingsDatabase;
		settingsDatabase = settingsDatabaseCopySecond;

		sectionSettings->update();
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// remember:
// user buttons below ID PP_MESSAGEBOX_BUTTON_USER10
// are not allowed to send repeatable pressed down events
///////////////////////////////////////////////////////////////////////////////
bool Tracker::swapAndCopyHandler(pp_int32 messageBoxID, pp_int32 messageBoxButtonID)
{
	PPContainer* container = static_cast<PPContainer*>(screen->getModalControl());
	PPListBox* listBoxSrc = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC));
	PPListBox* listBoxSrcModule = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC3));
	PPListBox* listBoxDst = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST));
	PPListBox* listBoxDstModule = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST3));

	ModuleEditor* src = listBoxSrcModule ? tabManager->getModuleEditorFromTabIndex(listBoxSrcModule->getSelectedIndex()) : this->moduleEditor;
	ModuleEditor* dst = listBoxDstModule ? tabManager->getModuleEditorFromTabIndex(listBoxDstModule->getSelectedIndex()) : this->moduleEditor;

	switch (messageBoxButtonID)
	{
		case PP_MESSAGEBOX_BUTTON_USER1:
		case PP_MESSAGEBOX_BUTTON_USER3:
		{
			switch (messageBoxID)
			{
				case INSTRUMENT_CHOOSER_COPY:
					this->moduleEditor->copyInstrument(*dst, listBoxDst->getSelectedIndex(), 
													   *src, listBoxSrc->getSelectedIndex());
					break;
				case INSTRUMENT_CHOOSER_SWAP:
					this->moduleEditor->swapInstruments(*dst, listBoxDst->getSelectedIndex(), 
														*src, listBoxSrc->getSelectedIndex());
					break;
				default:
					ASSERT(false);
			}
			
			if (messageBoxButtonID == PP_MESSAGEBOX_BUTTON_USER3)
			{
				pp_int32 index = listBoxDst->getSelectedIndex()+1;
				listBoxDst->setSelectedIndex(index, false, true);
				index = listBoxSrc->getSelectedIndex()+1;
				listBoxSrc->setSelectedIndex(index, false, true);
			}
			
			break;
		}
			
		case PP_MESSAGEBOX_BUTTON_USER2:
		case PP_MESSAGEBOX_BUTTON_USER4:
		{
			PPListBox* listBoxSrcSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));
			PPListBox* listBoxDstSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));
			
			switch (messageBoxID)
			{
				case INSTRUMENT_CHOOSER_COPY:
					this->moduleEditor->copySample(*dst, listBoxDst->getSelectedIndex(), listBoxDstSmp->getSelectedIndex(), 
												   *src, listBoxSrc->getSelectedIndex(), listBoxSrcSmp->getSelectedIndex());
					break;
				case INSTRUMENT_CHOOSER_SWAP:
					this->moduleEditor->swapSamples(*dst, listBoxDst->getSelectedIndex(), listBoxDstSmp->getSelectedIndex(), 
													*src, listBoxSrc->getSelectedIndex(), listBoxSrcSmp->getSelectedIndex());
					break;
				default:
					ASSERT(false);
			}
			
			if (messageBoxButtonID == PP_MESSAGEBOX_BUTTON_USER4)
			{
				pp_int32 index = listBoxDstSmp->getSelectedIndex()+1;
				listBoxDstSmp->setSelectedIndex(index, false, true);
				index = listBoxSrcSmp->getSelectedIndex()+1;
				listBoxSrcSmp->setSelectedIndex(index, false, true);
			}
			break;
		}
		
		// swap source/destination list boxes
		case PP_MESSAGEBOX_BUTTON_USER5:
		{
			if (listBoxSrcModule == NULL || listBoxDstModule == NULL)
				return true;

			PPListBox* listBoxSrcSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));
			PPListBox* listBoxDstSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));			
				
			pp_int32 srcModIndex = listBoxSrcModule->getSelectedIndex();
			pp_int32 dstModIndex = listBoxDstModule->getSelectedIndex();

			pp_int32 srcInsIndex = listBoxSrc->getSelectedIndex();
			pp_int32 dstInsIndex = listBoxDst->getSelectedIndex();

			pp_int32 srcSmpIndex = listBoxSrcSmp->getSelectedIndex();
			pp_int32 dstSmpIndex = listBoxDstSmp->getSelectedIndex();

			listBoxSrcModule->setSelectedIndex(dstModIndex, false);
			listBoxDstModule->setSelectedIndex(srcModIndex, false);
			
			updateInstrumentChooser(false);
			
			listBoxSrc->setSelectedIndex(dstInsIndex, false);
			listBoxDst->setSelectedIndex(srcInsIndex, false);			

			listBoxSrcSmp->setSelectedIndex(dstSmpIndex, false);
			listBoxDstSmp->setSelectedIndex(srcSmpIndex, false);		
			
			updateInstrumentChooser(true);
			screen->paint();
			return true;
		}

		// one more instrument in the source module
		case PP_MESSAGEBOX_BUTTON_USER10:
		{
			src->allocateInstrument();
			break;
		}

		// delete one instrument in the source module
		case PP_MESSAGEBOX_BUTTON_USER11:
		{
			src->freeInstrument();
			break;
		}

		// one more instrument in the destination module
		case PP_MESSAGEBOX_BUTTON_USER12:
		{
			dst->allocateInstrument();
			break;
		}

		// delete one instrument in the destination module
		case PP_MESSAGEBOX_BUTTON_USER13:
		{
			dst->freeInstrument();
			break;
		}
		
		case PP_MESSAGEBOX_BUTTON_USER6:
		{	
			// play source
			SamplePlayer samplePlayer(*src, *playerController);
			PPListBox* listBoxSrcSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC2));

			samplePlayer.playSample(listBoxSrc->getSelectedIndex(),
									listBoxSrcSmp->getSelectedIndex(), 
									sectionSamples->getCurrentSamplePlayNote());
			return true;
		}

		case PP_MESSAGEBOX_BUTTON_USER7:
		{
			// play dest
			SamplePlayer samplePlayer(*dst, *playerController);
			PPListBox* listBoxDstSmp = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST2));

			samplePlayer.playSample(listBoxDst->getSelectedIndex(),
									listBoxDstSmp->getSelectedIndex(), 
									sectionSamples->getCurrentSamplePlayNote());
			return true;
		}
		
		default:
			return false;
	}

	updateInstrumentChooser(false);
	
	if (listBoxInstruments->getSelectedIndex() == listBoxDst->getSelectedIndex() &&
		dst == this->moduleEditor)
	{
		updateInstrumentsListBox(false);
		sectionSamples->resetSampleEditor();
	}
	
	PPButton* button = static_cast<PPButton*>(container->getControlByID(PP_MESSAGEBOX_BUTTON_CANCEL));
	button->setText("Done");
	
	sectionSamples->updateAfterLoad();
	
	screen->paint();
	return true;
}

void Tracker::handleQuit()
{
	if (sectionSettings->isVisible())
		sectionSettings->cancelSettings();
	screen->shutDown();
}

bool Tracker::messageBoxEventListener(pp_int32 messageBoxID, pp_int32 messageBoxButtonID)
{
	switch (messageBoxID)
	{
		case MESSAGEBOX_REALLYQUIT:
		{
			if (messageBoxButtonID == PP_MESSAGEBOX_BUTTON_YES)
			{
				handleQuit();
			}
			break;
		}

		case INSTRUMENT_CHOOSER_COPY:
		case INSTRUMENT_CHOOSER_SWAP:
		{
			if (swapAndCopyHandler(messageBoxID, messageBoxButtonID))
				return false;

			break;
		}

		case MESSAGEBOX_INSREMAP:
		{
			switch (messageBoxButtonID)
			{
				case PP_MESSAGEBOX_BUTTON_USER1:
				case PP_MESSAGEBOX_BUTTON_USER2:
				case PP_MESSAGEBOX_BUTTON_USER3:
				case PP_MESSAGEBOX_BUTTON_USER4:
				{
					PPContainer* container = static_cast<PPContainer*>(screen->getModalControl());
					PPListBox* listBoxSrc = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_SRC));
					PPListBox* listBoxDst = static_cast<PPListBox*>(container->getControlByID(INSTRUMENT_CHOOSER_LIST_DST));
					
					pp_int32 newIns = listBoxDst->getSelectedIndex()+1;
					pp_int32 oldIns = listBoxSrc->getSelectedIndex()+1;
					
					pp_int32 res = 0;
					
					switch (messageBoxButtonID)
					{
						case PP_MESSAGEBOX_BUTTON_USER1:
							res = getPatternEditor()->insRemapTrack(oldIns, newIns);
							break;
						case PP_MESSAGEBOX_BUTTON_USER2:
							res = getPatternEditor()->insRemapPattern(oldIns, newIns);
							break;
						case PP_MESSAGEBOX_BUTTON_USER3:
							res = moduleEditor->insRemapSong(oldIns, newIns);
							break;
						case PP_MESSAGEBOX_BUTTON_USER4:
							res = getPatternEditor()->insRemapSelection(oldIns, newIns);
							break;
					}
					
					char buffer[100];
					sprintf(buffer, "%i Instruments have been remapped", res);
					showMessageBox(MESSAGEBOX_UNIVERSAL, buffer, MessageBox_OK);
					
					screen->paint();
					return false;
				}
			}

			break;
		}		
	}
	
	return true;
}


void Tracker::refreshYMSoundsIntrumentsMapping()
{
	std::map<int, int> instrumentIndex2YMsoundIndex;

	for (pp_int32 i = 0 ; i < listBoxInstruments->getNumItems() ; i++)
	{
		const PPString& instrument = listBoxInstruments->getItem(i);

		for (pp_int32 y = 0; y < listBoxYMsounds->getNumItems(); y++)
		{
			if (listBoxYMsounds->getItem(y) == instrument)
			{
				instrumentIndex2YMsoundIndex[i+1] = y;
				break;
			}
		}
	}

	playerController->setInstrumentYMSoundsMapping(instrumentIndex2YMsoundIndex, true);
}

///////////////////////////////////////////////////////////
// check if editing of any textfield is currently
// performed, if so we are not allowed to play instruments
///////////////////////////////////////////////////////////
bool Tracker::isActiveEditing()
{
	// check for focus of song title edit field
	PPContainer* container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_ABOUT));
	PPListBox* listBox = static_cast<PPListBox*>(container->getControlByID(LISTBOX_SONGTITLE));
	
	if (screen->hasFocus(container) && listBox->isEditing())							
		return true;

	container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_INSTRUMENTLIST));
	listBox = listBoxInstruments;

	if (screen->hasFocus(container) && listBox->isEditing())							
		return true;
	
	if (sectionDiskMenu->isActiveEditing())
		return true;

	return false;
}

void Tracker::ensureSongStopped(bool bResetMainVolume, bool suspend)
{
	playerLogic->ensureSongStopped(bResetMainVolume, suspend);
}

void Tracker::ensureSongPlaying(bool continuePlaying)
{
	playerLogic->ensureSongPlaying(continuePlaying);
}

void Tracker::initPlayback()
{
	resetStateMemories();

	playerController->resetPlayTimeCounter();

	recorderLogic->init();
}

void Tracker::setChanged()
{
	moduleEditor->setChanged();
	updateWindowTitle();
}

bool Tracker::shouldFollowSong()
{
	if (playerController->getNextOrderToPlay() != -1 ||
		playerController->getNextPatternToPlay() != -1)
		return false;

	return getFollowSong();
}

bool Tracker::getFollowSong()
{
	return followSong;
}

void Tracker::setFollowSong(bool b, bool repaint/* = true*/)
{
	followSong = b;
	updateAboutToggleButton(BUTTON_ABOUT_FOLLOWSONG, b, repaint);
}

bool Tracker::getProspectiveMode()
{
	return getPatternEditorControl()->getProspective();
}

void Tracker::setProspectiveMode(bool b, bool repaint/* = true*/)
{
	getPatternEditorControl()->setProspective(b);
	updateAboutToggleButton(BUTTON_ABOUT_PROSPECTIVE, b, repaint);
}
	
bool Tracker::getCursorWrapAround()
{
	return getPatternEditorControl()->getWrapAround();
}

void Tracker::setCursorWrapAround(bool b, bool repaint/* = true*/)
{
	getPatternEditorControl()->setWrapAround(b);
	updateAboutToggleButton(BUTTON_ABOUT_WRAPCURSOR, b, repaint);
}

void Tracker::setLiveSwitch(bool b, bool repaint/* = true*/)
{	
	playerLogic->setLiveSwitch(b);
	updateAboutToggleButton(BUTTON_ABOUT_LIVESWITCH, b, repaint);
}

void Tracker::updateSongRow(bool checkFollowSong/* = true*/)
{
	if (checkFollowSong && !shouldFollowSong())
		return;
		
	mp_sint32 row = getPatternEditorControl()->getCurrentRow();
	mp_sint32 pos = listBoxOrderList->getSelectedIndex();
	
	playerController->setPatternPos(pos, row);
}

void Tracker::selectInstrument(pp_int32 instrument)
{
	// instrument index starts at 0 in the module editor
	// but from 1 everywhere else
	moduleEditor->setCurrentInstrumentIndex(instrument-1);

	getPatternEditorControl()->setCurrentInstrument(instrument);
	
	sectionTranspose->setCurrentInstrument(instrument, false);

	// update instrument/sample editor
	// important: sample editor first => will reload sample into sample editor
	updateSampleEditor(false);
	// update instrument/sample editor
	sectionInstruments->update(false);

	for (pp_int32 i = 0; i < sections->size(); i++)
		sections->get(i)->notifyInstrumentSelect(instrument);
}

void Tracker::fillInstrumentListBox(PPListBox* listBox, ModuleEditor* moduleEditor/* = NULL*/)
{
	if (moduleEditor == NULL)
		moduleEditor = this->moduleEditor;
	char name[MP_MAXTEXT];
	for (pp_int32 j = 0; j < moduleEditor->getNumInstruments(); j++)
	{
		memset(name, 0, sizeof(name));
		moduleEditor->getInstrumentName(j, name, ModuleEditor::MAX_INSTEXT);
		listBox->addItem(name);
	}
}

void Tracker::fillSampleListBox(PPListBox* listBox, pp_int32 insIndex, ModuleEditor* moduleEditor/* = NULL*/)
{
	if (moduleEditor == NULL)
		moduleEditor = this->moduleEditor;
	for (pp_int32 j = 0; j < moduleEditor->getNumSamples(insIndex); j++)
	{
		char name[MP_MAXTEXT];
		moduleEditor->getSampleName(insIndex, j, name, ModuleEditor::MAX_SMPTEXT);
		listBox->addItem(name);
	}
}

void Tracker::fillYMSoundsListBox(PPListBox* listBox)
{
	listBox->clear();

	for (pp_uint32 j = 0; j < ResamplerYM::GetNbSounds(); j++)
	{
		char name[MP_MAXTEXT];
		strncpy(name, ResamplerYM::GetSoundName(j), MP_MAXTEXT);
		listBox->addItem(name);
	}
}

void Tracker::fillModuleListBox(PPListBox* listBox)
{
	TabHeaderControl* tabHeader = static_cast<TabHeaderControl*>(screen->getControlByID(TABHEADER_CONTROL));	
	for (pp_int32 i = 0; i < (signed)tabHeader->getNumTabs(); i++)
	{
		TabTitleProvider tabTitleProvider(*tabManager->getModuleEditorFromTabIndex(i));
		listBox->addItem(tabTitleProvider.getTabTitle());
	}
}

void Tracker::rearrangePatternEditorControl()
{
}

void Tracker::rearrangePatternEditorControlOrInstrumentContainer()
{
	if (sectionDiskMenu->isDiskMenuVisible())
		sectionDiskMenu->resizeInstrumentContainer();
	else
		rearrangePatternEditorControl();	
}

void Tracker::showScopes(bool visible, pp_uint32 style)
{
	if (!scopesControl)
		return;

	scopesControl->setAppearance((ScopesControl::AppearanceTypes)style);

	if (visible && scopesControl->isVisible())
		return;

	if (!visible && !scopesControl->isVisible())
		return;

	PPSize size = getPatternEditorControl()->getSize();
	PPPoint location = getPatternEditorControl()->getLocation();

	if (visible)
	{
		scopesControl->hide(false);
		size.height -= scopesControl->getSize().height;
		location.y += scopesControl->getSize().height;
		getPatternEditorControl()->setSize(size);
		getPatternEditorControl()->setLocation(location);
	}
	else
	{
		scopesControl->hide(true);
		size.height += scopesControl->getSize().height;
		location.y -= scopesControl->getSize().height;
		getPatternEditorControl()->setSize(size);
		getPatternEditorControl()->setLocation(location);
	}
	
	// store settings 
	settingsDatabase->store("SCOPES", (visible ? 1 : 0) | (style << 1));
	
	// if config menu is visible, update the toggles
	if (sectionSettings->isVisible())
		sectionSettings->update(false);
	
	screen->paint();
}

void Tracker::setInputControl(SIPs sip)
{
	switch (sip)
	{
		case SIPDefault:
		{
			inputContainerCurrent = inputContainerDefault;
			inputContainerExtended->hide(true);
			inputContainerCurrent->hide(false);
			break;
		}
		
		case SIPExtended:
		{
			inputContainerCurrent = inputContainerExtended;
			inputContainerDefault->hide(true);
			inputContainerCurrent->hide(false);
			break;
		}
	}

	rearrangePatternEditorControlOrInstrumentContainer();

	screen->update();
}

void Tracker::expandOrderlist(bool b)
{
	extendedOrderlist = b;
	
	PPContainer* container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_ORDERLIST));
	
	PPPoint p = container->getLocation();
	pp_int32 x = p.x;
	pp_int32 y = p.y;
	
	ASSERT(container);
	
	if (b)
	{
		container->getControlByID(BUTTON_ORDERLIST_SONGLENGTH_PLUS)->setLocation(PPPoint(x+2 + 78-2, y+2+12+12+12));
		container->getControlByID(BUTTON_ORDERLIST_SONGLENGTH_MINUS)->setLocation(PPPoint(x+2 + 78-2 + 17, y+2+12+12+12));
		
		container->getControlByID(BUTTON_ORDERLIST_REPEAT_PLUS)->hide(true);
		container->getControlByID(BUTTON_ORDERLIST_REPEAT_MINUS)->hide(true);
		container->getControlByID(STATICTEXT_ORDERLIST_SONGLENGTH)->setLocation(PPPoint(x+ 8*12, y+2+12+12+12+2+12));
		container->getControlByID(STATICTEXT_ORDERLIST_REPEAT)->hide(true);
		container->getControlByID(0)->hide(true);
		container->getControlByID(1)->hide(false);
		container->getControlByID(2)->hide(true);
		static_cast<PPButton*>(container->getControlByID(BUTTON_ORDERLIST_EXTENT))->setText(TrackerConfig::stringButtonExtended);
	
		PPSize size = container->getControlByID(LISTBOX_ORDERLIST)->getSize();
		size.height = 60;
		container->getControlByID(LISTBOX_ORDERLIST)->setSize(size);
	}
	else
	{
		container->getControlByID(BUTTON_ORDERLIST_SONGLENGTH_PLUS)->setLocation(PPPoint(x+2 + 78-2, y+2+12+12+12));
		container->getControlByID(BUTTON_ORDERLIST_SONGLENGTH_MINUS)->setLocation(PPPoint(x+2 + 78-2 + 17, y+2+12+12+12));

		container->getControlByID(BUTTON_ORDERLIST_REPEAT_PLUS)->hide(false);
		container->getControlByID(BUTTON_ORDERLIST_REPEAT_MINUS)->hide(false);
		container->getControlByID(STATICTEXT_ORDERLIST_SONGLENGTH)->setLocation(PPPoint(x+2 + 8*7, y+2+12+12+12+2));
		container->getControlByID(STATICTEXT_ORDERLIST_REPEAT)->hide(false);
		container->getControlByID(0)->hide(false);
		container->getControlByID(1)->hide(true);
		container->getControlByID(2)->hide(false);
		static_cast<PPButton*>(container->getControlByID(BUTTON_ORDERLIST_EXTENT))->setText(TrackerConfig::stringButtonCollapsed);

		PPSize size = container->getControlByID(LISTBOX_ORDERLIST)->getSize();
		size.height = 36;
		container->getControlByID(LISTBOX_ORDERLIST)->setSize(size);
	}
	
}

void Tracker::flipSpeedSection()
{
	PPContainer* container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_SPEED));
	ASSERT(container);

	PPControl* control = container->getControlByID(STATICTEXT_SPEED_OCTAVE);
	ASSERT(control);
	
	pp_int32 dy = container->getControlByID(STATICTEXT_SPEED_SPEED)->getLocation().y -
				  container->getControlByID(STATICTEXT_SPEED_BPM)->getLocation().y;
	
	if (control->isHidden())
	{
		// Show octave integer field
		control->hide(false);
		// Show octave description text ("Oct")
		control = container->getControlByID(STATICTEXT_SPEED_OCTAVE_DESC);
		control->hide(false);
		// Show octave minus button
		control = container->getControlByID(BUTTON_OCTAVE_MINUS);
		control->hide(false);
		// Show octave plus button
		control = container->getControlByID(BUTTON_OCTAVE_PLUS);
		control->hide(false);

		// show mainvol text
		control = container->getControlByID(STATICTEXT_SPEED_MAINVOL);
		control->hide(false);
		// show mainvol description text ("Mainvol")
		control = container->getControlByID(STATICTEXT_SPEED_MAINVOL_DESC);
		control->hide(false);
		
		// hide BPM text fields + buttons
		control = container->getControlByID(STATICTEXT_SPEED_BPM);
		control->hide(true);
		control = container->getControlByID(STATICTEXT_SPEED_BPM_DESC);
		control->hide(true);
		//control = container->getControlByID(BUTTON_BPM_MINUS);
		//control->hide(true);
		//control = container->getControlByID(BUTTON_BPM_PLUS);
		//control->hide(true);

		// hide tick speed text fields + buttons
		control = container->getControlByID(BUTTON_SPEED_MINUS);
		control->hide(true);
		control = container->getControlByID(BUTTON_SPEED_PLUS);
		control->hide(true);
		control = container->getControlByID(STATICTEXT_SPEED_SPEED);
		control->hide(true);
		control = container->getControlByID(STATICTEXT_SPEED_SPEED_DESC);
		control->hide(true);

		// Move pattern add text fields + buttons one row upwards
		control = container->getControlByID(STATICTEXT_SPEED_PATTERNADD);
		PPPoint p = control->getLocation();
		p.y-=dy;
		control->setLocation(p);
		control = container->getControlByID(STATICTEXT_SPEED_PATTERNADD_DESC);
		p = control->getLocation();
		p.y-=dy;
		control->setLocation(p);
		control = container->getControlByID(BUTTON_ADD_PLUS);
		p = control->getLocation();
		p.y-=dy;
		control->setLocation(p);
		control = container->getControlByID(BUTTON_ADD_MINUS);
		p = control->getLocation();
		p.y-=dy;
		control->setLocation(p);
	}
	else
	{
		// The hide octave texts + buttons
		control->hide(true);
		control = container->getControlByID(STATICTEXT_SPEED_OCTAVE_DESC);
		control->hide(true);
		control = container->getControlByID(BUTTON_OCTAVE_MINUS);
		control->hide(true);
		control = container->getControlByID(BUTTON_OCTAVE_PLUS);
		control->hide(true);

		// hide mainvol text
		control = container->getControlByID(STATICTEXT_SPEED_MAINVOL);
		control->hide(true);
		control = container->getControlByID(STATICTEXT_SPEED_MAINVOL_DESC);
		control->hide(true);
		
		// show bpm text + buttons
		control = container->getControlByID(STATICTEXT_SPEED_BPM);
		control->hide(false);
		control = container->getControlByID(STATICTEXT_SPEED_BPM_DESC);
		control->hide(false);
		//control = container->getControlByID(BUTTON_BPM_MINUS);
		//control->hide(false);
		//control = container->getControlByID(BUTTON_BPM_PLUS);
		//control->hide(false);

		// show tick speed text + buttons
		control = container->getControlByID(BUTTON_SPEED_MINUS);
		control->hide(false);
		control = container->getControlByID(BUTTON_SPEED_PLUS);
		control->hide(false);
		control = container->getControlByID(STATICTEXT_SPEED_SPEED);
		control->hide(false);
		control = container->getControlByID(STATICTEXT_SPEED_SPEED_DESC);
		control->hide(false);

		// move pattern add text one row downwards
		control = container->getControlByID(STATICTEXT_SPEED_PATTERNADD);
		PPPoint p = control->getLocation();
		p.y+=dy;
		control->setLocation(p);
		control = container->getControlByID(STATICTEXT_SPEED_PATTERNADD_DESC);
		p = control->getLocation();
		p.y+=dy;
		control->setLocation(p);
		control = container->getControlByID(BUTTON_ADD_PLUS);
		p = control->getLocation();
		p.y+=dy;
		control->setLocation(p);
		control = container->getControlByID(BUTTON_ADD_MINUS);
		p = control->getLocation();
		p.y+=dy;
		control->setLocation(p);
	}
}

void Tracker::enableInstrument(bool b)
{
	getPatternEditorControl()->enableInstrument(b);
	PPContainer* container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_INSTRUMENTLIST));
	ASSERT(container);
	PPButton* button = static_cast<PPButton*>(container->getControlByID(BUTTON_INSTRUMENT));	
	ASSERT(button);
	button->setPressed(b);
	listBoxInstruments->setOnlyShowIndexSelection(!b);
	screen->paintControl(listBoxInstruments);
	screen->paintControl(button);
}

void Tracker::commitListBoxChanges()
{
	if (listBoxInstruments->isEditing())
		listBoxInstruments->commitChanges();
}

FileTypes Tracker::getCurrentSelectedSampleSaveType()
{
	return (FileTypes)sectionDiskMenu->getCurrentSelectedSampleSaveType();
}

pp_uint32 Tracker::fileTypeToHint(FileTypes type)
{
	switch (type)
	{
		case FileTypes::FileTypeSongAllModules:			
		case FileTypes::FileTypeSongMOD:
		case FileTypes::FileTypeSongXM:
			return DecompressorBase::HintModules;

		case FileTypes::FileTypePatternXP:
			return DecompressorBase::HintPatterns;

		case FileTypes::FileTypeTrackXT:
			return DecompressorBase::HintTracks;

		case FileTypes::FileTypeSongAllInstruments:
		case FileTypes::FileTypeInstrumentXI:
			return DecompressorBase::HintInstruments;

		case FileTypes::FileTypeSongAllSamples:
		case FileTypes::FileTypeSampleWAV:
		case FileTypes::FileTypeSampleIFF:
			return DecompressorBase::HintSamples;

		default:
			return DecompressorBase::HintAll;

	}
}

void Tracker::prepareLoadSaveUI()
{
}

void Tracker::finishLoadSaveUI()
{
}

bool Tracker::loadGenericFileType(const PPSystemString& fileName)
{
	// we need to find out what file type this is
	// so we can do appropriate loading
	FileIdentificator* fileIdentificator = new FileIdentificator(fileName);
	FileIdentificator::FileTypes type = fileIdentificator->getFileType();
	delete fileIdentificator;
	// check for compression
	if (type == FileIdentificator::FileTypeCompressed)
	{
		// if this is compressed, we try to uncompress it
		// and choose that file type
		PPSystemString tempFile(ModuleEditor::getTempFilename());
		Decompressor decompressor(fileName);
		if (decompressor.decompress(tempFile, (DecompressorBase::Hints)fileTypeToHint(FileTypes::FileTypeAllFiles)))
		{
			fileIdentificator = new FileIdentificator(tempFile);
			type = fileIdentificator->getFileType();
			delete fileIdentificator;
			Decompressor::removeFile(tempFile);
		}
		else
		{
			showMessageBox(MESSAGEBOX_UNIVERSAL, "Unrecognized type/corrupt file", MessageBox_OK);			
			return false;
		}
	}

	switch (type)
	{
		case FileIdentificator::FileTypeModule:
			return loadTypeFromFile(FileTypes::FileTypeSongAllModules, fileName);		
		case FileIdentificator::FileTypeInstrument:
			return loadTypeFromFile(FileTypes::FileTypeSongAllInstruments, fileName);
		case FileIdentificator::FileTypeSample:
			return loadTypeFromFile(FileTypes::FileTypeSongAllSamples, fileName);
		case FileIdentificator::FileTypePattern:
			return loadTypeFromFile(FileTypes::FileTypePatternXP, fileName);
		case FileIdentificator::FileTypeTrack:
			return loadTypeFromFile(FileTypes::FileTypeTrackXT, fileName);
		default:
			return false;
	}
}

bool Tracker::prepareLoading(FileTypes eType, const PPSystemString& fileName, bool suspendPlayer, bool repaint, bool saveCheck)
{
	loadingParameters.deleteFile = false;
	loadingParameters.didOpenTab = false;
	loadingParameters.eType = eType;
	loadingParameters.filename = fileName;	
	loadingParameters.preferredFilename = fileName;
	loadingParameters.suspendPlayer = suspendPlayer;
	loadingParameters.repaint = repaint;

	loadingParameters.res = true;
	
	if (saveCheck && eType == FileTypes::FileTypeSongAllModules && !checkForChangesOpenModule())
		return false;
	
	loadingParameters.lastError = "Error while loading/unknown format";	

	signalWaitState(true);

	if (eType == FileTypes::FileTypeSongAllModules &&
		settingsDatabase->restore("TABS_LOADMODULEINNEWTAB")->getBoolValue() &&
		(moduleEditor->hasChanged() || !moduleEditor->isEmpty()))
	{
		loadingParameters.didOpenTab = true;
		tabManager->openNewTab();
	}

	bool rowPlay = playerController->isPlayingRowOnly();
	loadingParameters.wasPlaying = rowPlay ? false : (playerController->isPlaying() || playerController->isPlayingPattern());
	loadingParameters.wasPlayingPattern = rowPlay ? false : playerController->isPlayingPattern();
	
	if (loadingParameters.suspendPlayer)
	{
		scopesControl->enable(false);
		playerController->suspendPlayer();
	}
	
	// check for compressed file type
	FileIdentificator* fileIdentificator = new FileIdentificator(fileName);
	FileIdentificator::FileTypes type = fileIdentificator->getFileType();
	delete fileIdentificator;
	
	if (type == FileIdentificator::FileTypeCompressed)
	{
		// if this is compressed, try to decompress
		PPSystemString tempFile(ModuleEditor::getTempFilename());
		Decompressor decompressor(fileName);
		if (decompressor.decompress(tempFile, (DecompressorBase::Hints)fileTypeToHint(eType)))
		{
			// we compressed to a temporary file
			// load that instead, but keep the original file name as preferred 
			// base name for the module we're going to edit
			loadingParameters.preferredFilename = loadingParameters.filename;
			loadingParameters.filename = tempFile;
			// delete file after loading, it's temporary
			loadingParameters.deleteFile = true;
		}
		else
		{
			loadingParameters.lastError = "Unrecognized type/corrupt file";
			loadingParameters.res = false;
			finishLoading();
			return false;
		}
	}
	
	return true;
}

bool Tracker::finishLoading()
{
	signalWaitState(false);
	
	if (loadingParameters.repaint)
	{
		RefreshSndSynFilename();
		fillYMSoundsListBox(listBoxYMsounds);
		screen->paint();
		updateWindowTitle(moduleEditor->getModuleFileName());
	}
	
	if (!loadingParameters.res && !loadingParameters.abortLoading)
		showMessageBox(MESSAGEBOX_UNIVERSAL, loadingParameters.lastError, MessageBox_OK);	
	
	if (loadingParameters.suspendPlayer)
	{
		playerController->resumePlayer(true);
		scopesControl->enable(true);
	}
	
	if (loadingParameters.deleteFile)
		Decompressor::removeFile(loadingParameters.filename);
		
	if (!loadingParameters.res && loadingParameters.didOpenTab)
		tabManager->closeTab();
	
	refreshYMSoundsIntrumentsMapping();

	return loadingParameters.abortLoading ? true : loadingParameters.res;
}

bool Tracker::loadTypeFromFile(FileTypes eType, const PPSystemString& fileName, bool suspendPlayer/* = true*/, bool repaint/* = true*/, bool saveCheck/* = true*/)
{
	bool res = prepareLoading(eType, fileName, suspendPlayer, repaint, saveCheck);
	if (!res)
		return false;
	
	switch (eType)
	{
		case FileTypes::FileTypeSongAllModules:
		{
			if (loadingParameters.preferredFilename.length())
				loadingParameters.res = moduleEditor->openSong(loadingParameters.filename,
				loadingParameters.preferredFilename);
			else
				loadingParameters.res = moduleEditor->openSong(loadingParameters.filename,
				NULL);
			
			updateAfterLoad(loadingParameters.res, loadingParameters.wasPlaying, loadingParameters.wasPlayingPattern);			
			break;
		}
			
		case FileTypes::FileTypePatternXP:
		{
			loadingParameters.res = getPatternEditor()->loadExtendedPattern(loadingParameters.filename);
			updateSongInfo();
			break;
		}
			
		case FileTypes::FileTypeTrackXT:
		{
			loadingParameters.res = getPatternEditor()->loadExtendedTrack(loadingParameters.filename);
			updateSongInfo();
			break;
		}
			
		case FileTypes::FileTypeSongAllInstruments:
		{
			loadingParameters.res = moduleEditor->loadInstrument(loadingParameters.filename, listBoxInstruments->getSelectedIndex());
			sectionInstruments->updateAfterLoad();
			break;
		}
			
		case FileTypes::FileTypeSongAllSamples:
		{
			pp_int32 numSampleChannels = moduleEditor->getNumSampleChannels(loadingParameters.filename);
			
			pp_int32 chnIndex = 0;
			if (numSampleChannels <= 0)
			{
				loadingParameters.res = false;
				break;
			}
			else if (numSampleChannels > 1 && 
					 !settingsDatabase->restore("AUTOMIXDOWNSAMPLES")->getIntValue())
			{
				if (dialog)
					delete dialog;
				
				if (responder)
					delete responder;
					
				responder = new SampleLoadChannelSelectionHandler(*this);				
				dialog = new DialogChannelSelector(screen, responder, PP_DEFAULT_ID, "Choose channel to load" PPSTR_PERIODS);	
				
				// Add names of sample channels to instrument box
				for (pp_int32 i = 0; i < numSampleChannels; i++)
					static_cast<DialogChannelSelector*>(dialog)->getListBox()->addItem(moduleEditor->getNameOfSampleChannel(loadingParameters.filename, i));
				
				static_cast<SampleLoadChannelSelectionHandler*>(responder)->setCurrentFileName(loadingParameters.filename);
				static_cast<SampleLoadChannelSelectionHandler*>(responder)->setPreferredFileName(loadingParameters.preferredFilename);
				static_cast<SampleLoadChannelSelectionHandler*>(responder)->suspendPlayer = suspendPlayer;
				
				signalWaitState(false);
				
				dialog->show();
				return true;
			}
			else if (numSampleChannels > 1 && 
					 settingsDatabase->restore("AUTOMIXDOWNSAMPLES")->getIntValue())
			{
				chnIndex = -1;
			}

			if (loadingParameters.preferredFilename.length())
				loadingParameters.res = moduleEditor->loadSample(loadingParameters.filename, 
																 listBoxInstruments->getSelectedIndex(), 
																 0, //listBoxSamples->getSelectedIndex(), 
																 chnIndex,
																 loadingParameters.preferredFilename);
			else
				loadingParameters.res = moduleEditor->loadSample(loadingParameters.filename, 
																 listBoxInstruments->getSelectedIndex(), 
																 0, //listBoxSamples->getSelectedIndex(), 
																 chnIndex);

			sectionSamples->updateAfterLoad();
			break;
		}
			
	}
	
	return finishLoading();
}

// load different things
bool Tracker::loadTypeWithDialog(FileTypes eLoadType, bool suspendPlayer/* = true*/, bool repaint/* = true*/)
{
	FileExtProvider fileExtProvider;

	PPOpenPanel* openPanel = NULL;

	switch (eLoadType)
	{
		case FileTypes::FileTypeSongAllModules:
		{
			if (!checkForChangesOpenModule())
				return false;
			openPanel = new PPOpenPanel(screen, "Open Module");
			openPanel->addExtensions(fileExtProvider.getModuleExtensions());
			break;
		}
	
		case FileTypes::FileTypePatternXP:
		{
			openPanel = new PPOpenPanel(screen, "Open Extended Pattern");
			openPanel->addExtensions(fileExtProvider.getPatternExtensions());
			break;			
		}

		case FileTypes::FileTypeTrackXT:
		{
			openPanel = new PPOpenPanel(screen, "Open Extended Track");
			openPanel->addExtensions(fileExtProvider.getTrackExtensions());
			break;			
		}

		case FileTypes::FileTypeSongAllInstruments:
		{
			openPanel = new PPOpenPanel(screen, "Open Instrument");
			openPanel->addExtensions(fileExtProvider.getInstrumentExtensions());
			break;
		}

		case FileTypes::FileTypeSongAllSamples:
		{
			openPanel = new PPOpenPanel(screen, "Open Sample");
			openPanel->addExtensions(fileExtProvider.getSampleExtensions());
			break;
		}
	}

	if (!openPanel)
		return false;
	
	bool res = true;
		
	if (openPanel->runModal() == PPModalDialog::ReturnCodeOK)
	{
		PPSystemString file = openPanel->getFileName();
	
		if (file.length())
		{
			PPSystemString fileName = file;
			res = loadTypeFromFile(eLoadType, fileName, suspendPlayer, repaint, false);
		}
	
		delete openPanel;
	}
	
	return res;
}

void Tracker::loadType(FileTypes eType)
{
	if (useClassicBrowser)
	{
		prepareLoadSaveUI();

		sectionDiskMenu->selectSaveType(eType);
		eventKeyDownBinding_InvokeSectionDiskMenu();

		finishLoadSaveUI();
	}
	else
	{
		loadTypeWithDialog(eType);
	}
}

bool Tracker::prepareSavingWithDialog(FileTypes eSaveType)
{
	FileExtProvider fileExtProvider;

	currentSaveFileType = eSaveType;
	
	switch (eSaveType)
	{
		case FileTypes::FileTypeSongMOD:
		{
			savePanel = new PPSavePanel(screen, "Save Protracker Module", moduleEditor->getModuleFileName(ModuleEditor::ModSaveTypeMOD));
			savePanel->addExtension(fileExtProvider.getModuleExtension(FileExtProvider::ModuleExtensionMOD),
									fileExtProvider.getModuleDescription(FileExtProvider::ModuleExtensionMOD));
			
			pp_uint32 err = moduleEditor->getPTIncompatibilityCode();
			
			if (err)
			{
				buildMODSaveErrorWarning(err);
				return false;
			}				
			break;
		}
			
		case FileTypes::FileTypeSongXM:
			savePanel = new PPSavePanel(screen, "Save Extended Module", moduleEditor->getModuleFileName(ModuleEditor::ModSaveTypeXM));
			savePanel->addExtension(fileExtProvider.getModuleExtension(FileExtProvider::ModuleExtensionXM),
									fileExtProvider.getModuleDescription(FileExtProvider::ModuleExtensionXM));
			break;
			
		case FileTypes::FileTypePatternXP:
		{
			PPSystemString fileName = moduleEditor->getModuleFileName().stripExtension();
			fileName.append(".xp");
			savePanel = new PPSavePanel(screen, "Save Extended Pattern", fileName);
			savePanel->addExtension(fileExtProvider.getPatternExtension(FileExtProvider::PatternExtensionXP),
									fileExtProvider.getPatternDescription(FileExtProvider::PatternExtensionXP));
			break;			
		}
			
		case FileTypes::FileTypeTrackXT:
		{
			PPSystemString fileName = moduleEditor->getModuleFileName().stripExtension();
			fileName.append(".xt");
			savePanel = new PPSavePanel(screen, "Save Extended Track", fileName);
			savePanel->addExtension(fileExtProvider.getTrackExtension(FileExtProvider::TrackExtensionXT),
									fileExtProvider.getTrackDescription(FileExtProvider::TrackExtensionXT));
			break;			
		}
			
		case FileTypes::FileTypeInstrumentXI:
		{
			PPSystemString sampleFileName = moduleEditor->getInstrumentFileName(listBoxInstruments->getSelectedIndex());
			sampleFileName.append(".xi");
			savePanel = new PPSavePanel(screen, "Save Instrument", sampleFileName);
			savePanel->addExtension(fileExtProvider.getInstrumentExtension(FileExtProvider::InstrumentExtensionXI),
									fileExtProvider.getInstrumentDescription(FileExtProvider::InstrumentExtensionXI));
			break;
		}
			
		case FileTypes::FileTypeSampleWAV:
		{
			PPSystemString sampleFileName = moduleEditor->getSampleFileName(listBoxInstruments->getSelectedIndex(), 0);//listBoxSamples->getSelectedIndex());
			sampleFileName.append(".wav");
			savePanel = new PPSavePanel(screen, "Save uncompressed WAV",sampleFileName);
			savePanel->addExtension(fileExtProvider.getSampleExtension(FileExtProvider::SampleExtensionWAV),
									fileExtProvider.getSampleDescription(FileExtProvider::SampleExtensionWAV));
			break;
		}
			
		case FileTypes::FileTypeSampleIFF:
		{
			PPSystemString sampleFileName = moduleEditor->getSampleFileName(listBoxInstruments->getSelectedIndex(), 0);//listBoxSamples->getSelectedIndex());
			sampleFileName.append(".iff");
			savePanel = new PPSavePanel(screen, "Save uncompressed IFF",sampleFileName);
			savePanel->addExtension(fileExtProvider.getSampleExtension(FileExtProvider::SampleExtensionIFF),
									fileExtProvider.getSampleDescription(FileExtProvider::SampleExtensionIFF));
			break;
		}
	}
	
	return true;
}

// Save different things
bool Tracker::saveTypeWithDialog(FileTypes eSaveType, EventListenerInterface* fileSystemChangedListener/* = NULL*/)
{
	if (savePanel == NULL)
	{
		if (!prepareSavingWithDialog(eSaveType))
		{
			this->fileSystemChangedListener = fileSystemChangedListener;
			return true;
		}
		
		if (!savePanel)
			return true;
	}
	
	bool res = true;
	
	if (savePanel->runModal() == PPModalDialog::ReturnCodeOK)
	{
		PPSystemString file = savePanel->getFileName();
		
		if (file.length())
		{
			loadingParameters.lastError = "Error while saving";
			
			signalWaitState(true);
			
			switch (eSaveType)
			{
				case FileTypes::FileTypeSongMOD:
					commitListBoxChanges();
					res = moduleEditor->saveSong(file, ModuleEditor::ModSaveTypeMOD);
					break;
					
				case FileTypes::FileTypeSongXM:
					commitListBoxChanges();
					res = moduleEditor->saveSong(file, ModuleEditor::ModSaveTypeXM);
					RefreshSndSynFilename();
					break;
					
				case FileTypes::FileTypeTrackXT:
				{
					res = getPatternEditor()->saveExtendedTrack(file);
					break;
				}
					
				case FileTypes::FileTypePatternXP:
				{
					res = getPatternEditor()->saveExtendedPattern(file);
					break;
				}
					
				case FileTypes::FileTypeInstrumentXI:
					commitListBoxChanges();
					res = moduleEditor->saveInstrument(file, listBoxInstruments->getSelectedIndex());
					break;
					
				case FileTypes::FileTypeSampleWAV:
					commitListBoxChanges();
					res = moduleEditor->saveSample(file, listBoxInstruments->getSelectedIndex(), 
												   0,//listBoxSamples->getSelectedIndex(),
												   ModuleEditor::SampleFormatTypeWAV);
					break;
					
				case FileTypes::FileTypeSampleIFF:
					commitListBoxChanges();
					res = moduleEditor->saveSample(file, listBoxInstruments->getSelectedIndex(), 
												   0,//listBoxSamples->getSelectedIndex(),
												   ModuleEditor::SampleFormatTypeIFF);
					break;
			}
			
			if (res && fileSystemChangedListener)
			{
				PPEvent event(eFileSystemChanged);
				fileSystemChangedListener->handleEvent(reinterpret_cast<PPObject*>(this), &event);
			}
			
			signalWaitState(false);

			RefreshSndSynFilename();
			screen->paint();
			updateWindowTitle(moduleEditor->getModuleFileName());
		}
		
	}
	
	if (!res)
		showMessageBox(MESSAGEBOX_UNIVERSAL, loadingParameters.lastError, MessageBox_OK);	
	
	delete savePanel;
	savePanel = NULL;
	fileSystemChangedListener = NULL;
	
	return res;
}

void Tracker::RefreshSndSynFilename()
{
	const char* str = ResamplerYM::GetInstance()->GetSndSynFilename();

	if (strlen(str) >= 64)
	{
		str = str + strlen(str) - 64;
	}

	PPString filename(str);
	static_cast<PPStaticText*>(screen->getControlByID(STATICTEXT_YMSOUNDFILENAME))->setText(filename);
}

bool Tracker::saveCurrentModuleAsSelectedType()
{
	signalWaitState(true);
	
	loadingParameters.lastError = "Error while saving.\nFile might be write protected.";
	
	pp_int32 res = moduleEditor->saveSong(moduleEditor->getModuleFileNameFull(), moduleEditor->getSaveType());
	signalWaitState(false);

	if (!res)
		showMessageBoxSized(MESSAGEBOX_UNIVERSAL, loadingParameters.lastError, MessageBox_OK, -1, -1);	
	
	RefreshSndSynFilename();

	screen->paint();
	
	if (res)
		updateWindowTitle();
	
	return res == 0;
}

void Tracker::saveType(FileTypes eType)
{
	if (useClassicBrowser)
	{
		prepareLoadSaveUI();
	
		sectionDiskMenu->selectSaveType(eType);
		
		if (eType == FileTypes::FileTypeSongWAV)
			sectionDiskMenu->setModuleTypeAdjust(false);
		
		eventKeyDownBinding_InvokeSectionDiskMenu();
		
		if (eType == FileTypes::FileTypeSongWAV)
			sectionDiskMenu->setModuleTypeAdjust(true);
	
		finishLoadSaveUI();
	}
	else
	{
		saveTypeWithDialog(eType);
	}
}

void Tracker::runBLSBatch(const char* modfilename, const char* batchname)
{
	char exePath[1024];
	char cmdline[1024];

	GetModuleFileName(NULL, exePath, sizeof(exePath));

	char* p = strrchr(exePath, '\\');
	
	if (p != NULL)
	{
		*p = 0;

		sprintf (cmdline, "%s\\%s %s", exePath, batchname, modfilename);
		system(cmdline);
	}
}

void Tracker::save()
{
	if (TrackerConfig::untitledSong.compareTo(moduleEditor->getModuleFileNameFull().stripExtension()) == 0)
	{
		saveAs();
	}
	else
	{
		if (moduleEditor->getSaveType() == ModuleEditor::ModSaveTypeMOD)
		{
			pp_uint32 err = moduleEditor->getPTIncompatibilityCode();
			
			if (err)
			{
				// remove save panel
				// => the handler for the upcoming modal dialog
				// will note from the absent save panel that saving will be
				// done to the current document
				if (savePanel)
				{
					delete savePanel;
					savePanel = NULL;
				}
				buildMODSaveErrorWarning((pp_int32)err);
				return;
			}
		}				
		saveCurrentModuleAsSelectedType();
	}
}
	
void Tracker::saveAs()
{
	switch (moduleEditor->getSaveType())
	{
		case ModuleEditor::ModSaveTypeMOD:
			saveType(FileTypes::FileTypeSongMOD);
			break;

		case ModuleEditor::ModSaveTypeXM:
			saveType(FileTypes::FileTypeSongXM);
			break;
			
		default:
			ASSERT(false);
	}
}

void Tracker::handleSaveProceed()
{
	// if there is a save panel present we save with file name
	// selection, otherwise we simply save the current document
	// in the current format
	if (savePanel != NULL)
		saveTypeWithDialog(currentSaveFileType, fileSystemChangedListener);
	else
		saveCurrentModuleAsSelectedType();
}

void Tracker::handleSaveCancel()
{
	delete savePanel;
	savePanel = NULL;
}

void Tracker::buildMODSaveErrorWarning(pp_int32 error)
{
	static const char* warnings[] = {"Song contains more than 31 instruments\nSave anyway?",
									 
									 "Song uses linear frequencies\nSave anyway?",
									 
									 "Song contains incompatible samples \n\n"\
									 "Check for the following conditions:\n"\
									 "* No 16 bit samples                \n"\
									 "* All samples are below 64kb      \n"\
									 "* No ping-pong looping             \n"\
									 "* Panning is set to 0x80 (middle)  \n"\
									 "* Relative note is C-4 (number 0)  \n\nSave anyway?",
									  
									 "Song contains FT2-style instruments\n\n"\
									 "Check for the following conditions:\n"\
									 "* No envelopes                     \n"\
									 "* No autovibrato                   \n"\
									 "* Only one sample per instrument   \n\nSave anyway?",

									 "Incompatible pattern data          \n\n"\
									 "Check for the following conditions:\n"\
									 "* Pattern length is exactly 64 rows\n"\
									 "* Only notes between C-3 and B-5   \n"\
									 "* Volume column is not used        \n"\
									 "* Only effects between 0 and F     \n\nSave anyway?"};
	
	if (dialog)
		delete dialog;
	
	if (responder)
		delete responder;
	
	responder = new SaveProceedHandler(*this);				
				
	dialog = new PPDialogBase(screen, responder, MESSAGEBOX_SAVEPROCEED, "");
	
	showMessageBoxSized(MESSAGEBOX_SAVEPROCEED, warnings[error-1], MessageBox_YESNO, 318, -1, false);	
	
	// Transfer ownership of modal overlayed container to dialog
	// => also set new event listener
	dialog->setMessageBoxContainer(messageBoxContainerGeneric);
	messageBoxContainerGeneric->setEventListener(dialog);
	PPSimpleVector<PPControl>& controls = messageBoxContainerGeneric->getControls();
	for (pp_int32 i = 0; i < controls.size(); i++)
		controls.get(i)->setEventListener(dialog);
	
	messageBoxContainerGeneric = NULL;
	
	dialog->show();
}

void Tracker::signalWaitState(bool b)
{
	screen->signalWaitState(b, TrackerConfig::colorThemeMain);	
}

