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
#include <cstdint>
#include <memory>
#include "GravifonClient.hpp"
#include <chrono>
#include <mutex>
#include <cstring>

using namespace std;
using std::chrono::system_clock;

namespace
{
	// The character 'Line Feed' in UTF-8.
	static const char UTF8_LF = 0x0a;

	// TODO Ensure that this code is thread-safe.
	static GravifonClient gravifonClient;

	static DB_misc_t plugin = {};
	static DB_functions_t *deadbeef;

	static mutex pluginMutex;

	struct ConfLock
	{
		ConfLock() { deadbeef->conf_lock(); }
		~ConfLock() { deadbeef->conf_unlock(); }
	};

	struct PlaylistLock
	{
		PlaylistLock() { deadbeef->pl_lock(); }
		~PlaylistLock() { deadbeef->pl_unlock(); }
	};

	inline long toLongMillis(const float seconds)
	{
		return static_cast<long>(seconds * 1000);
	}

	unique_ptr<ScrobbleInfo> getScrobbleInfo(ddb_event_trackchange_t * const trackChangeEvent)
	{
		{ PlaylistLock lock;
			DB_playItem_t * const track = trackChangeEvent->from;
			const float trackPlayDuration = trackChangeEvent->playtime; // in seconds

			if (track == nullptr || trackPlayDuration == 0.f) {
				// Nothing to scrobble.
				return nullptr;
			}

			// DeaDBeeF track metadata are returned in UTF-8. No additional conversion is needed.
			const char * const title = deadbeef->pl_find_meta(track, "title");
			if (title == nullptr) {
				// Track title is a required field.
				return nullptr;
			}
			const char * artist = deadbeef->pl_find_meta(track, "artist");
			if (artist == nullptr) {
				artist = deadbeef->pl_find_meta(track, "band");
				if (artist == nullptr) {
					artist = deadbeef->pl_find_meta(track, "album artist");
					if (artist == nullptr) {
						artist = deadbeef->pl_find_meta(track, "albumartist");
						if (artist == nullptr) {
							// Track artist is a required field.
							return nullptr;
						}
					}
				}
			}
			const char * const album = deadbeef->pl_find_meta(track, "album");
			const float trackDuration = deadbeef->pl_get_item_duration(track); // in seconds

			unique_ptr<ScrobbleInfo> scrobbleInfo(new ScrobbleInfo());
			// TODO use monotonic micro/nano clock to calculate exact duration. These measurements are inaccurate.
			scrobbleInfo->scrobbleStartTimestamp = trackChangeEvent->started_timestamp;
			scrobbleInfo->scrobbleEndTimestamp = system_clock::to_time_t(system_clock::now());
			scrobbleInfo->scrobbleDuration = toLongMillis(trackPlayDuration);
			Track &trackInfo = scrobbleInfo->track;
			trackInfo.setTitle(title);
			if (album != nullptr) {
				trackInfo.setAlbumTitle(album);
			}
			trackInfo.setDurationMillis(toLongMillis(trackDuration));

			/* Adding artists, probably multiple ones. DeaDBeeF returns them as
			 * '\n'-separated values within a single string.
			 */
			const char *artistStart = artist, *artistEnd;
			while ((artistEnd = strchr(artistStart, UTF8_LF)) != nullptr) {
				trackInfo.addArtist(string(artistStart, artistEnd));
				artistStart = artistEnd + 1;
			}
			trackInfo.addArtist(artistStart);

			return scrobbleInfo;
		}
	}
}

int gravifonScrobblerStart()
{
	// TODO move initialisation phase to here (now C++ unit initialisation is used).
	// TODO Ensure this code is thread-safe.
	if (!gravifonClient.loadPendingScrobbles()) {
		return 1;
	}
	return 0;
}

int gravifonScrobblerStop()
{
	int result = 0;
	if (!gravifonClient.storePendingScrobbles()) {
		result = 1;
	}
	// TODO Discard other resources properly.
	return result;
}

bool initClient()
{
	{ ConfLock lock;
		const bool enabled = deadbeef->conf_get_int("gravifonScrobbler.enabled", 0);
		if (!enabled) {
			return false;
		}

		// DeaDBeeF configuration records are returned in UTF-8. No additional conversion is needed.
		const char * const scrobblerUrl = deadbeef->conf_get_str_fast("gravifonScrobbler.scrobblerUrl", "");
		if (scrobblerUrl[0] == '\0') {
			return false;
		}

		const char * const username = deadbeef->conf_get_str_fast("gravifonScrobbler.username", "");
		const char * const password = deadbeef->conf_get_str_fast("gravifonScrobbler.password", "");

		// TODO Ensure that this code is thread-safe.
		gravifonClient.configure(scrobblerUrl, username, password);

		return true;
	}
}

int gravifonScrobblerMessage(const uint32_t id, const uintptr_t ctx, const uint32_t p1, const uint32_t p2)
{
	if (id != DB_EV_SONGCHANGED) {
		return 0;
	}

	{ lock_guard<mutex> lock(pluginMutex);
		// TODO distinguish disabled scrobbling and gravifon client init errors
		if (!initClient()) {
			return 0;
		}

		unique_ptr<ScrobbleInfo> scrobbleInfo = getScrobbleInfo(reinterpret_cast<ddb_event_trackchange_t *>(ctx));

		if (scrobbleInfo != nullptr) {
			gravifonClient.scrobble(*scrobbleInfo);
		}
		return 0;
	}
}

extern "C"
{
	DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api);
}

DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api)
{
	deadbeef = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 4;
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
	plugin.plugin.configdialog =
		"property \"Enable scrobbler\" checkbox gravifonScrobbler.enabled 0;"
		"property \"Username\" entry gravifonScrobbler.username \"\";"
		"property \"Password\" password gravifonScrobbler.password \"\";"
		"property \"Scrobbler URL\" entry gravifonScrobbler.scrobblerUrl \"\";";

	plugin.plugin.message = gravifonScrobblerMessage;

	return DB_PLUGIN(&plugin);
}
