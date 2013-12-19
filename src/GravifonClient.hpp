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
#ifndef GRAVIFONCLIENT_HPP_
#define GRAVIFONCLIENT_HPP_

#include <string>
#include <vector>
#include <list>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <cstddef>
#include "dateutil.hpp"

// All strings are utf8-encoded.
class Track
{
public:
	void setTitle(const std::string &trackTitle) { m_title = trackTitle; m_titleSet = true; }
	void addArtist(const std::string &artist) { m_artists.emplace_back(artist); m_artistSet = true; }
	void addAlbumArtist(const std::string &artist) { m_albumArtists.emplace_back(artist); m_albumArtistSet = true; }
	void setAlbumTitle(const std::string &albumTitle) { m_album = albumTitle; m_albumSet = true; }
	void setDurationMillis(const long duration) { m_duration = duration; m_durationSet = true; }

	void appendTo(std::string &str) const;
private:
	std::string m_title;
	std::vector<std::string> m_artists;
	std::vector<std::string> m_albumArtists;
	std::string m_album;
	// Track duration in milliseconds.
	long m_duration;
	bool m_titleSet = false;
	bool m_artistSet = false;
	bool m_albumArtistSet = false;
	bool m_albumSet = false;
	bool m_durationSet = false;
};

struct ScrobbleInfo
{
	ScrobbleInfo() = default;
	ScrobbleInfo(const ScrobbleInfo &) = default;
	ScrobbleInfo(ScrobbleInfo &&) = default;

	ScrobbleInfo &operator=(const ScrobbleInfo &) = default;
	ScrobbleInfo &operator=(ScrobbleInfo &&) = default;

	static bool parse(const std::string &str, ScrobbleInfo &dest);

	// Date and time when scrobble event was initiated.
	DateTime scrobbleStartTimestamp;
	// Date and time when scrobble event was finished.
	DateTime scrobbleEndTimestamp;
	// Scrobble length in milliseconds.
	long scrobbleDuration;
	// Track to scrobble.
	Track track;

	void appendTo(std::string &str) const;
};

class GravifonClient
{
	GravifonClient(const GravifonClient &) = delete;
	GravifonClient(GravifonClient &&) = delete;
	GravifonClient &operator=(const GravifonClient &) = delete;
	GravifonClient &operator=(GravifonClient &&) = delete;
public:
	GravifonClient() : m_mutex(), m_scrobblingThread(), m_cv(), m_startStopMutex()
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_started = false;
		m_configured = false;

		/* This instance is partially initialised here. It will be initialised completely
		 * when ::start() is invoked successfully.
		 */
	}

	~GravifonClient()
	{
		// Synchronising memory before destructing the member fields of this GravifonClient.
		std::lock_guard<std::mutex> lock(m_mutex);
	}

	// username and password are to be in ISO-8859-1; gravifonUrl is to be in the system encoding.
	void configure(const char *gravifonUrl, const std::string &username, const std::string &password);

	void invalidateConfiguration()
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_configured = false;
	}

	/*
	 * Adds a given scrobble to the list of pending scrobbles. Optionally, it saves
	 * the given scrobble to the data file to keep if available even if an emergency
	 * happens. GravifonClient::stop() rewrites the data file, sweeping out all
	 * scrobbles whose processing is completed.
	 *
	 * Nothing is done if this GravifonClient is not started.
	 *
	 * @param scrobbleInfo the track scrobble to process.
	 * @param safeScrobble if true then the scrobble is stored to the data file
	 *         immediately, to save this scrobble even in case of an emergency.
	 *         It is false by default.
	 */
	void scrobble(const ScrobbleInfo &scrobbleInfo, const bool safeScrobbling = false);

	bool start();
	bool stop();

	bool started() const
	{ std::lock_guard<std::mutex> lock(m_mutex);
		return m_started;
	}
private:
	bool loadPendingScrobbles();
	bool storePendingScrobbles();
	void backgroundScrobbling();
	// Returns the number of scrobbles completed (successful and non-processable).
	size_t doScrobbling();

	const static size_t MIN_SCROBBLES_TO_WAIT;
	const static size_t MAX_SCROBBLES_TO_WAIT;

	std::string m_scrobblerUrl;
	// The authentication header encoded in the basic charset.
	std::string m_authHeader;

	std::list<ScrobbleInfo> m_pendingScrobbles;

	mutable size_t m_scrobblesToWait;
	mutable std::mutex m_mutex;
	mutable std::thread m_scrobblingThread;
	mutable std::condition_variable m_cv;
	/* Used to prevent parallel execution of the functions start() and stop().
	 * This is needed for stop() to know that the scrobbling thread is stopped
	 * at some time to store all pending scrobbles to the data file.
	 */
	mutable std::mutex m_startStopMutex;
	/* Indicates if the background scrobbling thread should finish its work
	 * including aborting an active connection to Gravifon if it is established.
	 */
	mutable std::atomic<bool> m_finishScrobblingFlag;
	bool m_started;
	bool m_configured;
};

#endif /* GRAVIFONCLIENT_HPP_ */
