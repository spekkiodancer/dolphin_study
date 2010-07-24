// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "CommonPaths.h"

#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <wx/mstream.h>
#include <wx/tipwin.h>
#include <wx/fontdlg.h>

#include "../../DolphinWX/Src/WxUtils.h"

#include "Host.h"

#include "Debugger.h"
#include "DebuggerUIUtil.h"

#include "RegisterWindow.h"
#include "BreakpointWindow.h"
#include "MemoryWindow.h"
#include "JitWindow.h"
#include "FileUtil.h"

#include "CodeWindow.h"
#include "CodeView.h"

#include "Core.h"
#include "HLE/HLE.h"
#include "Boot/Boot.h"
#include "LogManager.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "Debugger/PPCDebugInterface.h"
#include "Debugger/Debugger_SymbolMap.h"
#include "PowerPC/PPCAnalyst.h"
#include "PowerPC/Profiler.h"
#include "PowerPC/PPCSymbolDB.h"
#include "PowerPC/SignatureDB.h"
#include "PowerPC/PPCTables.h"
#include "PowerPC/JitCommon/JitBase.h"
#include "PowerPC/JitCommon/JitCache.h" // for ClearCache()

#include "PluginManager.h"
#include "ConfigManager.h"

extern "C"  // Bitmaps
{
	#include "../resources/toolbar_play.c"
	#include "../resources/toolbar_pause.c"
	#include "../resources/toolbar_add_memorycheck.c"
	#include "../resources/toolbar_delete.c"
	#include "../resources/toolbar_add_breakpoint.c"
}

// Save and load settings
// -----------------------------
void CCodeWindow::Load()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	// The font to override DebuggerFont with
	ini.Get("ShowOnStart", "DebuggerFont", &fontDesc);
	if (!fontDesc.empty())
		DebuggerFont.SetNativeFontInfoUserDesc(wxString::FromAscii(fontDesc.c_str()));

	// Decide what windows to use
	// This stuff really doesn't belong in CodeWindow anymore, does it? It should be
	// in Frame.cpp somewhere, even though it's debugger stuff.
	ini.Get("ShowOnStart", "Code", &bCodeWindow, true);
	ini.Get("ShowOnStart", "Registers", &bRegisterWindow, false);
	ini.Get("ShowOnStart", "Breakpoints", &bBreakpointWindow, false);
	ini.Get("ShowOnStart", "Memory", &bMemoryWindow, false);
	ini.Get("ShowOnStart", "JIT", &bJitWindow, false);
	ini.Get("ShowOnStart", "Sound", &bSoundWindow, false);
	ini.Get("ShowOnStart", "Video", &bVideoWindow, false);
	// Get notebook affiliation
	std::string _Section = StringFromFormat("P - %s",
		(Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives.at(Parent->ActivePerspective).Name.c_str() : "");
	ini.Get(_Section.c_str(), "Log", &iLogWindow, 1);
	ini.Get(_Section.c_str(), "Console", &iConsoleWindow, 1);
	ini.Get(_Section.c_str(), "Code", &iCodeWindow, 1);
	ini.Get(_Section.c_str(), "Registers", &iRegisterWindow, 1);
	ini.Get(_Section.c_str(), "Breakpoints", &iBreakpointWindow, 0);
	ini.Get(_Section.c_str(), "Memory", &iMemoryWindow, 1);
	ini.Get(_Section.c_str(), "JIT", &iJitWindow, 1);
	ini.Get(_Section.c_str(), "Sound", &iSoundWindow, 0);
	ini.Get(_Section.c_str(), "Video", &iVideoWindow, 0);
	// Get floating setting
	ini.Get("Float", "Log", &Parent->bFloatWindow[0], false);
	ini.Get("Float", "Console", &Parent->bFloatWindow[1], false);
	ini.Get("Float", "Code", &Parent->bFloatWindow[2], false);
	ini.Get("Float", "Registers", &Parent->bFloatWindow[3], false);
	ini.Get("Float", "Breakpoints", &Parent->bFloatWindow[4], false);
	ini.Get("Float", "Memory", &Parent->bFloatWindow[5], false);
	ini.Get("Float", "JIT", &Parent->bFloatWindow[6], false);
	ini.Get("Float", "Sound", &Parent->bFloatWindow[7], false);
	ini.Get("Float", "Video", &Parent->bFloatWindow[8], false);

	// Boot to pause or not
	ini.Get("ShowOnStart", "AutomaticStart", &bAutomaticStart, false);
	ini.Get("ShowOnStart", "BootToPause", &bBootToPause, true);
}

void CCodeWindow::Save()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	ini.Set("ShowOnStart", "DebuggerFont", fontDesc);

	// Boot to pause or not
	ini.Set("ShowOnStart", "AutomaticStart", GetMenuBar()->IsChecked(IDM_AUTOMATICSTART));
	ini.Set("ShowOnStart", "BootToPause", GetMenuBar()->IsChecked(IDM_BOOTTOPAUSE));

	// Save windows settings
	//ini.Set("ShowOnStart", "Code", GetMenuBar()->IsChecked(IDM_CODEWINDOW));
	ini.Set("ShowOnStart", "Registers", GetMenuBar()->IsChecked(IDM_REGISTERWINDOW));
	ini.Set("ShowOnStart", "Breakpoints", GetMenuBar()->IsChecked(IDM_BREAKPOINTWINDOW));
	ini.Set("ShowOnStart", "Memory", GetMenuBar()->IsChecked(IDM_MEMORYWINDOW));
	ini.Set("ShowOnStart", "JIT", GetMenuBar()->IsChecked(IDM_JITWINDOW));
	ini.Set("ShowOnStart", "Sound", GetMenuBar()->IsChecked(IDM_SOUNDWINDOW));
	ini.Set("ShowOnStart", "Video", GetMenuBar()->IsChecked(IDM_VIDEOWINDOW));
	std::string _Section = StringFromFormat("P - %s",
		(Parent->ActivePerspective < Parent->Perspectives.size())
		? Parent->Perspectives.at(Parent->ActivePerspective).Name.c_str() : "");
	ini.Set(_Section.c_str(), "Log", iLogWindow);
	ini.Set(_Section.c_str(), "Console", iConsoleWindow);
	ini.Set(_Section.c_str(), "Code", iCodeWindow);
	ini.Set(_Section.c_str(), "Registers", iRegisterWindow);
	ini.Set(_Section.c_str(), "Breakpoints", iBreakpointWindow);
	ini.Set(_Section.c_str(), "Memory", iMemoryWindow);
	ini.Set(_Section.c_str(), "JIT", iJitWindow);
	ini.Set(_Section.c_str(), "Sound", iSoundWindow);
	ini.Set(_Section.c_str(), "Video", iVideoWindow);
	// Save floating setting
	ini.Set("Float", "Log", !!FindWindowById(IDM_LOGWINDOW_PARENT));
	ini.Set("Float", "Console", !!FindWindowById(IDM_CONSOLEWINDOW_PARENT));
	ini.Set("Float", "Code", !!FindWindowById(IDM_CODEWINDOW_PARENT));
	ini.Set("Float", "Registers", !!FindWindowById(IDM_REGISTERWINDOW_PARENT));
	ini.Set("Float", "Breakpoints", !!FindWindowById(IDM_BREAKPOINTWINDOW_PARENT));
	ini.Set("Float", "Memory", !!FindWindowById(IDM_MEMORYWINDOW_PARENT));
	ini.Set("Float", "JIT", !!FindWindowById(IDM_JITWINDOW_PARENT));
	ini.Set("Float", "Sound", !!FindWindowById(IDM_SOUNDWINDOW_PARENT));
	ini.Set("Float", "Video", !!FindWindowById(IDM_VIDEOWINDOW_PARENT));

	ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

// Symbols, JIT, Profiler
// ----------------
void CCodeWindow::CreateMenuSymbols()
{
	wxMenu *pSymbolsMenu = new wxMenu;
	pSymbolsMenu->Append(IDM_CLEARSYMBOLS, _T("&Clear symbols"));
	// pSymbolsMenu->Append(IDM_CLEANSYMBOLS, _T("&Clean symbols (zz)"));
	pSymbolsMenu->Append(IDM_SCANFUNCTIONS, _T("&Generate symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_LOADMAPFILE, _T("&Load symbol map"));
	pSymbolsMenu->Append(IDM_SAVEMAPFILE, _T("&Save symbol map"));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_SAVEMAPFILEWITHCODES, _T("Save code"),
		wxString::FromAscii("Save the entire disassembled code. This may take a several seconds"
		" and may require between 50 and 100 MB of hard drive space. It will only save code"
		" that are in the first 4 MB of memory, if you are debugging a game that load .rel"
		" files with code to memory you may want to increase that to perhaps 8 MB, you can do"
		" that from SymbolDB::SaveMap().")
		);

	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_CREATESIGNATUREFILE, _T("&Create signature file..."));
	pSymbolsMenu->Append(IDM_USESIGNATUREFILE, _T("&Use signature file..."));
	pSymbolsMenu->AppendSeparator();
	pSymbolsMenu->Append(IDM_PATCHHLEFUNCTIONS, _T("&Patch HLE functions"));
	pSymbolsMenu->Append(IDM_RENAME_SYMBOLS, _T("&Rename symbols from file..."));
	pMenuBar->Append(pSymbolsMenu, _T("&Symbols"));

	wxMenu *pProfilerMenu = new wxMenu;
	pProfilerMenu->Append(IDM_PROFILEBLOCKS, _T("&Profile blocks"), wxEmptyString, wxITEM_CHECK);
	pProfilerMenu->AppendSeparator();
	pProfilerMenu->Append(IDM_WRITEPROFILE, _T("&Write to profile.txt, show"));
	pMenuBar->Append(pProfilerMenu, _T("&Profiler"));
}

void CCodeWindow::OnProfilerMenu(wxCommandEvent& event)
{
	if (Core::GetState() == Core::CORE_RUN) {
		event.Skip();
		return;
	}
	switch (event.GetId())
	{
	case IDM_PROFILEBLOCKS:
		jit->ClearCache();
		Profiler::g_ProfileBlocks = GetMenuBar()->IsChecked(IDM_PROFILEBLOCKS);
		break;
	case IDM_WRITEPROFILE:
		Profiler::WriteProfileResults("profiler.txt");
		WxUtils::Launch("profiler.txt");
		break;
	}
}

void CCodeWindow::OnSymbolsMenu(wxCommandEvent& event)
{
	Parent->ClearStatusBar();

	if (Core::GetState() == Core::CORE_UNINITIALIZED) return;

	std::string mapfile = CBoot::GenerateMapFilename();
	switch (event.GetId())
	{
	case IDM_CLEARSYMBOLS:
		if(!AskYesNo("Do you want to clear the list of symbol names?", "Confirm", wxYES_NO)) return;
		g_symbolDB.Clear();
		Host_NotifyMapLoaded();
		break;
	case IDM_CLEANSYMBOLS:
		g_symbolDB.Clear("zz");
		Host_NotifyMapLoaded();
		break;
	case IDM_SCANFUNCTIONS:
		{
		PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
		SignatureDB db;
		if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
		{
			db.Apply(&g_symbolDB);
			Parent->StatusBarMessage("Generated symbol names from '%s'", TOTALDB);
		}
		else
		{
			Parent->StatusBarMessage("'%s' not found, no symbol names generated", TOTALDB);
		}
		// HLE::PatchFunctions();
		// Update GUI
		NotifyMapLoaded();
		break;
		}
	case IDM_LOADMAPFILE:
		if (!File::Exists(mapfile.c_str()))
		{
			g_symbolDB.Clear();
			PPCAnalyst::FindFunctions(0x81300000, 0x81800000, &g_symbolDB);
			SignatureDB db;
			if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
				db.Apply(&g_symbolDB);
			Parent->StatusBarMessage("'%s' not found, scanning for common functions instead", mapfile.c_str());
		}
		else
		{
			g_symbolDB.LoadMap(mapfile.c_str());
			Parent->StatusBarMessage("Loaded symbols from '%s'", mapfile.c_str());
		}
		HLE::PatchFunctions();
		NotifyMapLoaded();
		break;
	case IDM_SAVEMAPFILE:
		g_symbolDB.SaveMap(mapfile.c_str());
		break;
	case IDM_SAVEMAPFILEWITHCODES:
		g_symbolDB.SaveMap(mapfile.c_str(), true);
		break;

	case IDM_RENAME_SYMBOLS:
		{
			wxString path = wxFileSelector(
				_T("Apply signature file"), wxEmptyString,
				wxEmptyString, wxEmptyString,
				_T("Dolphin Symbol Rename File (*.sym)|*.sym"),
				wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);
			if (! path.IsEmpty())
			{
				FILE *f = fopen(path.mb_str(), "r");
				if (!f)
					return;

				while (!feof(f))
				{
					char line[512];
					fgets(line, 511, f);
					if (strlen(line) < 4)
						continue;

					u32 address, type;
					char name[512];
					sscanf(line, "%08x %02i %s", &address, &type, name);

					Symbol *symbol = g_symbolDB.GetSymbolFromAddr(address);
					if (symbol) {
						symbol->name = line+12;
					}
				}
				fclose(f);
				Host_NotifyMapLoaded();
			}
		}
		break;

	case IDM_CREATESIGNATUREFILE:
		{
			wxTextEntryDialog input_prefix(
				this,
				wxString::FromAscii("Only export symbols with prefix:\n(Blank for all symbols)"),
				wxGetTextFromUserPromptStr,
				wxEmptyString);

			if (input_prefix.ShowModal() == wxID_OK)
			{
				std::string prefix(input_prefix.GetValue().mb_str());

				wxString path = wxFileSelector(
					_T("Save signature as"), wxEmptyString, wxEmptyString, wxEmptyString,
					_T("Dolphin Signature File (*.dsy)|*.dsy;"), wxFD_SAVE,
					this);
				if (!path.IsEmpty())
				{
					SignatureDB db;
					db.Initialize(&g_symbolDB, prefix.c_str());
					std::string filename(path.mb_str());
					db.Save(path.mb_str());
				}
			}
		}
		break;
	case IDM_USESIGNATUREFILE:
		{
			wxString path = wxFileSelector(
				_T("Apply signature file"), wxEmptyString, wxEmptyString, wxEmptyString,
				_T("Dolphin Signature File (*.dsy)|*.dsy;"), wxFD_OPEN | wxFD_FILE_MUST_EXIST,
				this);
			if (!path.IsEmpty())
			{
				SignatureDB db;
				db.Load(path.mb_str());
				db.Apply(&g_symbolDB);
			}
		}
		NotifyMapLoaded();
		break;
	case IDM_PATCHHLEFUNCTIONS:
		HLE::PatchFunctions();
		Update();
		break;
	}
}

void CCodeWindow::NotifyMapLoaded()
{
	if (!codeview) return;

	g_symbolDB.FillInCallers();
	//symbols->Show(false); // hide it for faster filling
	symbols->Freeze();	// HyperIris: wx style fast filling
	symbols->Clear();
	for (PPCSymbolDB::XFuncMap::iterator iter = g_symbolDB.GetIterator(); iter != g_symbolDB.End(); ++iter)
	{
		int idx = symbols->Append(wxString::FromAscii(iter->second.name.c_str()));
		symbols->SetClientData(idx, (void*)&iter->second);
	}
	symbols->Thaw();
	//symbols->Show(true);
	Update();
}

void CCodeWindow::OnSymbolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	if (index >= 0) {
		Symbol* pSymbol = static_cast<Symbol *>(symbols->GetClientData(index));
		if (pSymbol != NULL)
		{
			if(pSymbol->type == Symbol::SYMBOL_DATA)
			{
				if(m_MemoryWindow)// && m_MemoryWindow->IsVisible())
					m_MemoryWindow->JumpToAddress(pSymbol->address);
			}
			else
			{
				JumpToAddress(pSymbol->address);
			}
		}
	}
}

void CCodeWindow::OnSymbolListContextMenu(wxContextMenuEvent& event)
{
}

// Change the global DebuggerFont
void CCodeWindow::OnChangeFont(wxCommandEvent& event)
{
	wxFontData data;
	data.SetInitialFont(GetFont());

	wxFontDialog dialog(this, data);
	if ( dialog.ShowModal() == wxID_OK )
	{
		DebuggerFont = dialog.GetFontData().GetChosenFont();
		fontDesc = std::string(DebuggerFont.GetNativeFontInfoUserDesc().mb_str());
	}
}

// Toogle windows

void CCodeWindow::OpenPages()
{
	ToggleCodeWindow(true);
	if (bRegisterWindow)
		ToggleRegisterWindow(true);
	if (bBreakpointWindow)
		ToggleBreakPointWindow(true);
	if (bMemoryWindow)
		ToggleMemoryWindow(true);
	if (bJitWindow)
		ToggleJitWindow(true);
	if (bSoundWindow)
		ToggleDLLWindow(IDM_SOUNDWINDOW, true);
	if (bVideoWindow)
		ToggleDLLWindow(IDM_VIDEOWINDOW, true);
}

void CCodeWindow::OnToggleWindow(wxCommandEvent& event)
{
	bool bShow = GetMenuBar()->IsChecked(event.GetId());

	switch(event.GetId())
	{
		case IDM_REGISTERWINDOW:
			ToggleRegisterWindow(bShow);
			break;
		case IDM_BREAKPOINTWINDOW:
			ToggleBreakPointWindow(bShow);
			break;
		case IDM_MEMORYWINDOW:
			ToggleMemoryWindow(bShow);
			break;
		case IDM_JITWINDOW:
			ToggleJitWindow(bShow);
			break;
		case IDM_SOUNDWINDOW:
			ToggleDLLWindow(IDM_SOUNDWINDOW, bShow);
			break;
		case IDM_VIDEOWINDOW:
			ToggleDLLWindow(IDM_VIDEOWINDOW, bShow);
			break;
	}
	event.Skip();
}

void CCodeWindow::ToggleCodeWindow(bool bShow)
{
	if (bShow)
		Parent->DoAddPage(this, iCodeWindow,
			   	Parent->bFloatWindow[IDM_CODEWINDOW - IDM_LOGWINDOW]);
	else // Hide
		Parent->DoRemovePage(this);
}

void CCodeWindow::ToggleRegisterWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_REGISTERWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_RegisterWindow)
		   	m_RegisterWindow = new CRegisterWindow(Parent, IDM_REGISTERWINDOW);
		Parent->DoAddPage(m_RegisterWindow, iRegisterWindow,
			   	Parent->bFloatWindow[IDM_REGISTERWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
		Parent->DoRemovePage(m_RegisterWindow, false);
}

void CCodeWindow::ToggleBreakPointWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_BREAKPOINTWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_BreakpointWindow)
		   	m_BreakpointWindow = new CBreakPointWindow(this, Parent, IDM_BREAKPOINTWINDOW);
		Parent->DoAddPage(m_BreakpointWindow, iBreakpointWindow,
			   	Parent->bFloatWindow[IDM_BREAKPOINTWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
		Parent->DoRemovePage(m_BreakpointWindow, false);
}

void CCodeWindow::ToggleMemoryWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_MEMORYWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_MemoryWindow)
		   	m_MemoryWindow = new CMemoryWindow(Parent, IDM_MEMORYWINDOW);
		Parent->DoAddPage(m_MemoryWindow, iMemoryWindow,
			   	Parent->bFloatWindow[IDM_MEMORYWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
		Parent->DoRemovePage(m_MemoryWindow, false);
}

void CCodeWindow::ToggleJitWindow(bool bShow)
{
	GetMenuBar()->FindItem(IDM_JITWINDOW)->Check(bShow);
	if (bShow)
	{
		if (!m_JitWindow)
		   	m_JitWindow = new CJitWindow(Parent, IDM_JITWINDOW);
		Parent->DoAddPage(m_JitWindow, iJitWindow,
			   	Parent->bFloatWindow[IDM_JITWINDOW - IDM_LOGWINDOW]);
	}
	else // Close
		Parent->DoRemovePage(m_JitWindow, false);
}

// Notice: This windows docking will produce several wx debugging messages for plugin
// windows when ::GetWindowRect and ::DestroyWindow fails in wxApp::CleanUp() for the
// plugin.

// Toggle Sound Debugging Window
void CCodeWindow::ToggleDLLWindow(int Id, bool bShow)
{
	std::string DLLName;
	int PluginType, i;
	bool bFloat;
	wxPanel *Win;

	switch(Id)
	{
		case IDM_SOUNDWINDOW:
			DLLName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str();
			PluginType = PLUGIN_TYPE_DSP;
			i = iSoundWindow;
			bFloat = Parent->bFloatWindow[IDM_SOUNDWINDOW - IDM_LOGWINDOW];
			break;
		case IDM_VIDEOWINDOW:
			DLLName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str();
			PluginType = PLUGIN_TYPE_VIDEO;
			i = iVideoWindow;
			bFloat = Parent->bFloatWindow[IDM_VIDEOWINDOW - IDM_LOGWINDOW];
			break;
		default:
			PanicAlert("CCodeWindow::ToggleDLLWindow called with invalid Id");
			return;
	}

	if (bShow)
	{
		// Show window
		Win = (wxPanel *)CPluginManager::GetInstance().OpenDebug(Parent,
				DLLName.c_str(), (PLUGIN_TYPE)PluginType, bShow);

		if (Win)
		{
			Win->Show();
			Win->SetId(Id);
			Parent->DoAddPage(Win, i, bFloat);
		}
	}
	else
	{
		Win = (wxPanel *)FindWindowById(Id);
		if (Win)
		{
			Parent->DoRemovePage(Win, false);
			Win->Close();
			Win->Destroy();
		}
	}
	GetMenuBar()->FindItem(Id)->Check(bShow && !!Win);
}
