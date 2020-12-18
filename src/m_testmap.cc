//------------------------------------------------------------------------
//  TEST (PLAY) THE MAP
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2016 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "Errors.hpp"

#include "main.h"

#include "m_files.h"
#include "m_loadsave.h"
#include "w_wad.h"

#include "ui_window.h"


static SString QueryName(const SString &port = Port_name, const SString &cgame = Game_name)
{
	SYS_ASSERT(port);

	SString game = cgame;

	if (port.noCaseEqual("vanilla"))
	{
		if (! game)
			game = "doom2";

		return "vanilla_" + game;
	}

	return port;
}


class UI_PortPathDialog : public UI_Escapable_Window
{
public:
	Fl_Output *exe_display;

	Fl_Button *ok_but;
	Fl_Button *cancel_but;

	// the chosen EXE name, or NULL if cancelled
	SString exe_name;

	bool want_close;

public:
	void SetEXE(const SString &newbie)
	{
		exe_name = newbie;

		// NULL is ok here
		exe_display->value(exe_name.c_str());

		if (!exe_name.empty() && FileExists(exe_name))
			ok_but->activate();
		else
			ok_but->deactivate();
	}

	static void ok_callback(Fl_Widget *w, void *data)
	{
		UI_PortPathDialog * that = (UI_PortPathDialog *)data;

		that->want_close = true;
	}

	static void close_callback(Fl_Widget *w, void *data)
	{
		UI_PortPathDialog * that = (UI_PortPathDialog *)data;

		that->SetEXE("");

		that->want_close = true;
	}

	static void find_callback(Fl_Widget *w, void *data)
	{
		UI_PortPathDialog * that = (UI_PortPathDialog *)data;

		Fl_Native_File_Chooser chooser;

		chooser.title("Pick the executable file");
		chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
#ifdef WIN32
		chooser.filter("Executables\t*.exe");
#endif

		// FIXME : if we have an exe_filename already, and folder exists, go there
		//         [ especially for vanilla -- look in path of Iwad_name ]
		chooser.directory(Main_FileOpFolder().c_str());

		switch (chooser.show())
		{
			case -1:  // error
				DLG_Notify("Unable to use that exe:\n\n%s", chooser.errmsg());
				return;

			case 1:  // cancelled
				return;

			default:
				break;  // OK
		}

		// we assume the chosen file exists

		that->SetEXE(chooser.filename());
	}

public:
	UI_PortPathDialog(const SString &port_name) :
		UI_Escapable_Window(560, 250, "Port Settings"),
		want_close(false)
	{
		char message_buf[256];

		snprintf(message_buf, sizeof(message_buf), "Setting up location of the executable (EXE) for %s.", port_name.c_str());

		Fl_Box *header = new Fl_Box(FL_NO_BOX, 20, 20, w() - 40, 30, "");
		header->copy_label(message_buf);
		header->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

		header = new Fl_Box(FL_NO_BOX, 20, 55, w() - 40, 30,
		           "This is only needed for the Test Map command.");
		header->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);


		exe_display = new Fl_Output(98, 100, w()-200, 26, "Exe path: ");

		Fl_Button *find_but = new Fl_Button(w()-80, 100, 60, 26, "Find");
		find_but->callback((Fl_Callback*)find_callback, this);

		/* bottom buttons */

		Fl_Group * grp = new Fl_Group(0, h() - 60, w(), 70);
		grp->box(FL_FLAT_BOX);
		grp->color(WINDOW_BG, WINDOW_BG);
		{
			cancel_but = new Fl_Button(w() - 260, h() - 45, 95, 30, "Cancel");
			cancel_but->callback(close_callback, this);

			ok_but = new Fl_Button(w() - 120, h() - 45, 95, 30, "OK");
			ok_but->labelfont(FL_HELVETICA_BOLD);
			ok_but->callback(ok_callback, this);
			ok_but->shortcut(FL_Enter);
			ok_but->deactivate();
		}
		grp->end();

		end();

		resizable(NULL);

		callback(close_callback, this);
	}

	virtual ~UI_PortPathDialog()
	{ }

	// returns true if user clicked OK
	bool Run()
	{
		set_modal();
		show();

		while (! want_close)
			Fl::wait(0.2);

		return !exe_name.empty();
	}
};


bool M_PortSetupDialog(const SString &port, const SString &game)
{
	SString name_buf;

	if (port.noCaseEqual("vanilla"))
		name_buf = "Vanilla " + game.asTitle() + '\n';
	else if (port.noCaseEqual("mbf"))	// temp hack for aesthetics
		name_buf = "MBF";
	else
		name_buf = port.asTitle();

	UI_PortPathDialog *dialog = new UI_PortPathDialog(name_buf);

	// populate the EXE name from existing info, if exists
	port_path_info_t *info = M_QueryPortPath(QueryName(port, game));

	if (info && info->exe_filename)
		dialog->SetEXE(info->exe_filename);

	bool ok = dialog->Run();

	if (ok)
	{
		// persist the new port settings
		info = M_QueryPortPath(QueryName(port, game), true /* create_it */);

		// FIXME: check result??
		info->exe_filename = GetAbsolutePath(dialog->exe_name);

		M_SaveRecent();
	}

	delete dialog;
	return ok;
}


//------------------------------------------------------------------------


static SString CalcEXEName(const port_path_info_t *info)
{
	// make the executable name relative, since we chdir() to its folder
	SString basename = GetBaseName(info->exe_filename);
	return SString(".") + DIR_SEP_CH + basename;
}


static SString CalcWarpString()
{
	SYS_ASSERT(!Level_name.empty());
	// FIXME : EDGE allows a full name: -warp MAP03
	//         Eternity too.
	//         ZDOOM too, but different syntax: +map MAP03

	// most common syntax is "MAP##" or "MAP###"
	if(Level_name.length() >= 4 && Level_name.noCaseStartsWith("MAP") && isdigit(Level_name[3]))
	{
		long number = strtol(Level_name.c_str() + 3, nullptr, 10);
		return StringPrintf("-warp %ld", number);
	}

	// detect "E#M#" syntax of Ultimate-Doom and Heretic, which need
	// a pair of numbers after -warp
	if(Level_name.length() >= 4 && !isdigit(Level_name[0]) && isdigit(Level_name[1]) &&
	   !isdigit(Level_name[2]) && isdigit(Level_name[3]))
	{
		return StringPrintf("-warp %c %s", Level_name[1], Level_name.c_str() + 3);
	}

	// map name is non-standard, find the first digit group and hope
	// for the best...

	size_t digitPos = Level_name.findDigit();
	if(digitPos != std::string::npos)
	{
		return SString("-warp ") + (Level_name.c_str() + digitPos);
	}

	// no digits at all, oh shit!
	return "";
}


static void AppendWadName(SString &str, const SString &name, const SString &parm = NULL)
{
	SString abs_name = GetAbsolutePath(name);

	if (parm)
	{
		str += parm;
		str += ' ';
	}

	str += abs_name;
	str += ' ';
}


static SString GrabWadNames(const port_path_info_t *info)
{
	SString wad_names;

	bool has_file = false;

	int use_merge = 0;

	// see if we should use the "-merge" parameter, which is
	// required for Chocolate-Doom and derivates like Crispy Doom.
	// TODO : is there a better way to do this?
	if (Port_name.noCaseEqual("vanilla"))
	{
		use_merge = 1;
	}

	// always specify the iwad
	AppendWadName(wad_names, game_wad->PathName(), "-iwad");

	// add any resource wads
	for (const Wad_file *wad : master_dir)
	{
		if (wad == game_wad || wad == edit_wad)
			continue;

		AppendWadName(wad_names, wad->PathName(),
					  (use_merge == 1) ? "-merge" : (use_merge == 0 && !has_file) ? "-file" : NULL);

		if (use_merge)
			use_merge++;
		else
			has_file = true;
	}

	// the current PWAD, if exists, must be last
	if (edit_wad)
		AppendWadName(wad_names, edit_wad->PathName(), !has_file ? "-file" : NULL);

	return wad_names;
}


void CMD_TestMap()
{
	if (MadeChanges)
	{
		if (DLG_Confirm("Cancel|&Save",
		                "You have unsaved changes, do you want to save them now "
						"and build the nodes?") <= 0)
		{
			return;
		}

		if (! M_SaveMap())
			return;
	}


	// check if we know the executable path, if not then ask
	port_path_info_t *info = M_QueryPortPath(QueryName());

	if (! (info && M_IsPortPathValid(info)))
	{
		if (! M_PortSetupDialog(Port_name, Game_name))
			return;

		info = M_QueryPortPath(QueryName());
	}

	// this generally can't happen, but we check anyway...
	if (! (info && M_IsPortPathValid(info)))
	{
		Beep("invalid path to executable");
		return;
	}


	// remember the previous working directory
	static char old_dir[FL_PATH_MAX];

	if (getcwd(old_dir, sizeof(old_dir)) == NULL)
	{
		old_dir[0] = 0;
	}

	// change working directory to be same as the executable
	SString folder = FilenameGetPath(info->exe_filename);

	LogPrintf("Changing current dir to: %s\n", folder.c_str());

	if (! FileChangeDir(folder))
	{
		// FIXME : a notify dialog
		Beep("chdir failed!");
		return;
	}


	// build the command string

	SString cmd_buffer = StringPrintf("%s %s %s",
									  CalcEXEName(info).c_str(), GrabWadNames(info).c_str(), 
									  CalcWarpString().c_str());

	LogPrintf("Testing map using the following command:\n");
	LogPrintf("--> %s\n", cmd_buffer.c_str());

	Status_Set("TESTING MAP");

	main_win->redraw();
	Fl::wait(0.1);
	Fl::wait(0.1);


	/* Go baby! */

	int status = system(cmd_buffer.c_str());

	if (status == 0)
		Status_Set("Result: OK");
	else
		Status_Set("Result code: %d\n", status);

	LogPrintf("--> result code: %d\n", status);


	// restore previous working directory
	if (old_dir[0])
	{
		FileChangeDir(old_dir);
	}

	main_win->redraw();
	Fl::wait(0.1);
	Fl::wait(0.1);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
