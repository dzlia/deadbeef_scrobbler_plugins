/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013 Dźmitry Laŭčuk

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#include <deadbeef.h>

int gravifonScrobblerStart()
{
	return 0;
}

int gravifonScrobblerStop()
{
	return 0;
}

static const char gravifonScrobblerSettingsDialog[] =
	"property \"Enable scrobbler\" checkbox gravifonScrobbler.enable 0;"
	"property \"Username\" entry gravifonScrobbler.username \"\";"
	"property \"Password\" password gravifonScrobbler.password \"\";"
	"property \"Scrobbler URL\" entry gravifonScrobbler.scrobblerUrl \"\";"
;

int gravifonScrobblerMessage(const uint32_t id, const uintptr_t ctx, const uint32_t p1, const uint32_t p2)
{
    return 0;
}

static DB_misc_t plugin;

static DB_functions_t *deadbeefAPI;

extern "C"
{
	DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api);
}

DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api)
{
	deadbeefAPI = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 0;
	plugin.plugin.version_major = 1;
	plugin.plugin.version_minor = 0;
	plugin.plugin.type = DB_PLUGIN_MISC;
	plugin.plugin.name = "gravifon scrobbler";
	plugin.plugin.descr = "An audio track scrobbler to Gravifon.";
	plugin.plugin.copyright =
		"Copyright (C) 2013 Dźmitry Laŭčuk\n"
		"\n"
		"This program is free software: you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation, either version 3 of the License, or\n"
		"(at your option) any later version.\n"
		"\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details.\n"
		"\n"
		"You should have received a copy of the GNU General Public License\n"
		"along with this program.  If not, see <http://www.gnu.org/licenses/>.\n";

	plugin.plugin.website = "https://github.com/dzidzitop/gravifon_scrobbler_deadbeef_plugin";
	plugin.plugin.start = gravifonScrobblerStart;
	plugin.plugin.stop = gravifonScrobblerStop;
	plugin.plugin.configdialog = gravifonScrobblerSettingsDialog;
	plugin.plugin.message = gravifonScrobblerMessage;

	return DB_PLUGIN(&plugin);
}
