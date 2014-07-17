/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2014 Dźmitry Laŭčuk

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

#include "LastfmScrobbler.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include "HttpClient.hpp"
#include "UrlBuilder.hpp"
#include <afc/number.h>
#include <afc/StringRef.hpp>
#include "logger.hpp"
#include <afc/md5.hpp>
#include <afc/Tokeniser.hpp>
#include <afc/ensure_ascii.hpp>
#include <utility>
#include <afc/dateutil.hpp>
#include <afc/utils.h>

using namespace std;
using namespace afc;
using StatusCode = HttpClient::StatusCode;

namespace
{
	inline UrlBuilder buildAuthUrl(const string &scrobblerUrl, const string &username, const string &password)
	{
		constexpr size_t maxTimestampSize = afc::maxPrintedSize<long, 10>();

		/* Auth token is generated as md5(md5(password) + timestamp),
		 * where md5 is a lowercase hex-encoded ASCII MD5 hash and + is concatenation.
		 */
		constexpr size_t digestSize = 32; // Each octet is represented as two characters.
		char tmp[digestSize + maxTimestampSize];
		// md5(password). In addition, the timestamp is stored right after the end of this digest.
		// TODO check that this reinterpret_cast conforms to the C++11 standard.
		char * const timestampStart = md5String(reinterpret_cast<const unsigned char *>(password.c_str()), password.size(), tmp);
		// md5(password) + timestamp
		char * const timestampEnd = printNumber<long, 10>(now().millis() / 1000, timestampStart);
		// Finally, generating authToken.
		char authToken[digestSize];
		md5String(reinterpret_cast<const unsigned char *>(tmp), timestampEnd - tmp, authToken);

		// TODO set real client ID and version.
		return UrlBuilder(scrobblerUrl,
				UrlPart<raw>("hs"_s), UrlPart<raw>("true"_s),
				UrlPart<raw>("p"_s), UrlPart<raw>("1.2.1"_s),
				UrlPart<raw>("c"_s), UrlPart<raw>("tst"_s),
				UrlPart<raw>("v"_s), UrlPart<raw>("1.0"_s),
				UrlPart<raw>("u"_s), UrlPart<>(username),
				/* Neither timestamp nor authToken need to be URL-encoded since they are
				 * decimal and hex numbers, respectively.
				 */
				UrlPart<raw>("t"_s), UrlPart<raw>(timestampStart, timestampEnd - timestampStart),
				UrlPart<raw>("a"_s), UrlPart<raw>(authToken, digestSize));
	}

	void appendScrobbleInfo(UrlBuilder &builder, const ScrobbleInfo &scrobbleInfo, const unsigned char index)
	{
		assert(index < 50); // max amount of scrobbles per request.

		const Track &track = scrobbleInfo.track;
		assert(track.hasTitle());
		assert(track.hasArtist());

		// TODO optimise parameter name/value creation. They can be allocated statically.
		const string idx(to_string(index));
		const string scrobbleStartTs(to_string(scrobbleInfo.scrobbleStartTimestamp.timestamp().millis() / 1000));
		const string trackLength(to_string(track.getDurationMillis() / 1000));

		builder.params(
				// The artist name. Required.
				UrlPart<raw>("a[" + idx + "]"), UrlPart<>(track.getArtists()[0]),
				// The track title. Required.
				UrlPart<raw>("t[" + idx + "]"), UrlPart<>(track.getTitle()),
				// The time the track started playing, in UNIX timestamp format. Required.
				UrlPart<raw>("i[" + idx + "]"), UrlPart<raw>(scrobbleStartTs),
				// The source of the track. Required. 'Chosen by the user' in all cases.
				UrlPart<raw>("o[" + idx + "]"), UrlPart<raw>("P"_s),
				// TODO Support track ratings.
				// A single character denoting the rating of the track. Empty, since not applicable.
				UrlPart<raw>("r[" + idx + "]"), UrlPart<raw>(""_s),
				// The length of the track in seconds. Required for 'Chosen by the user'.
				UrlPart<raw>("l[" + idx + "]"), UrlPart<raw>(trackLength),
				// The album title, or an empty string if not known.
				UrlPart<raw>("b[" + idx + "]"), UrlPart<>(track.hasAlbumTitle() ? track.getAlbumTitle() : string()),
				// TODO Support track numbers.
				// The position of the track on the album, or an empty string if not known.
				UrlPart<raw>("n[" + idx + "]"), UrlPart<>(""_s),
				// TODO Support MusicBrainz Track IDs.
				// The MusicBrainz Track ID, or an empty string if not known.
				UrlPart<raw>("m[" + idx + "]"), UrlPart<>(""_s));
	}

	inline void reportHttpClientError(const StatusCode result)
	{
		assert(result != StatusCode::SUCCESS);
		const char *message;
		switch (result) {
		case StatusCode::UNABLE_TO_CONNECT:
			message = "unable to connect";
			break;
		case StatusCode::OPERATION_TIMEOUT:
			message = "sending the request has timed out";
			break;
		default:
			message = "unknown error";
		}
		std::fprintf(stderr, "[LastfmScrobbler] Unable to send the request: %s\n", message);
	}
}

void LastfmScrobbler::stopExtra()
{
	m_scrobblerUrl.clear();
}

void LastfmScrobbler::configure(const char * const serverUrl, const string &username, const string &password)
{ lock_guard<mutex> lock(m_mutex);
	assert(serverUrl != nullptr);

	bool reconfigured = false;

	if (m_scrobblerUrl != serverUrl) {
		m_scrobblerUrl = serverUrl;
		reconfigured = true;
	}
	if (m_username != username) {
		m_username = username;
		reconfigured = true;
	}
	if (m_password != password) {
		m_password = password;
		reconfigured = true;
	}

	if (reconfigured) {
		// The configuration has changed. Updating it as well as resetting the 'scrobbles to wait' counter.
		m_scrobblesToWait = MIN_SCROBBLES_TO_WAIT;
		m_authenticated = false;
	}

	m_configured = true;
}

std::size_t LastfmScrobbler::doScrobbling()
{
	assertLocked();
	assert(!m_pendingScrobbles.empty());

	if (!m_configured) {
		fputs("Scrobbler is not configured properly.", stderr);
		return 0;
	}

	if (m_scrobblerUrl.empty()) {
		/* There is no sense to try to send a request because the URL to the scrobbling
		 * service is undefined. The scrobble is added to the list of pending scrobbles
		 * (see above) to be submitted (among other scrobbles) when the URL is configured
		 * to point to a scrobbling server.
		 */
		fputs("URL to the scrobbling server is undefined.", stderr);
		return 0;
	}

	if (!ensureAuthenticated()) {
		return 0;
	}

	// 50 is the max number of scrobbles that can be submitted within a single request.
	constexpr unsigned maxScrobblesPerRequest = 50;

	// Adding up to maxScrobblesPerRequest scrobbles to the request.
	UrlBuilder builder(queryOnly,
			// TODO URL-encode session ID right after it is obtained during the authentication process.
			UrlPart<raw>("s"_s), UrlPart<>(m_sessionId));

	/* Points to the position after the end of the chunk of scrobbles submitted and is used
	 * to remove these scrobbles from the queue if they are submitted successfully.
	 */
	auto chunkEnd = m_pendingScrobbles.begin();
	unsigned submittedCount = 0;
	for (auto end = m_pendingScrobbles.end(); chunkEnd != end && submittedCount < maxScrobblesPerRequest;
			++chunkEnd, ++submittedCount) {
		appendScrobbleInfo(builder, *chunkEnd, submittedCount);
	}

	HttpEntity request;
	request.body.assign(builder.data(), builder.size());

	// Making a copy of shared data to pass outside the critical section.
	const string submissionUrlCopy(m_submissionUrl);

#ifndef NDEBUG
	const size_t pendingScrobbleCount = m_pendingScrobbles.size();
#endif

	StatusCode result;
	HttpResponseEntity response;

	/* Each HTTP call is performed outside the critical section so that other threads can:
	 * - add scrobbles without waiting for this call to finish
	 * - stop this Scrobbler by invoking Scrobbler::stop(). In this case this HTTP call
	 * is aborted and the scrobbles involved are left in the list of pending scrobbles
	 * so that they can be stored to the data file and be completed later.
	 *
	 * It is safe to unlock the mutex because:
	 * - no shared data is accessed outside the critical section
	 * - other threads cannot delete scrobbles in the meantime (because the scrobbling thread
	 * assumes that the scrobbles being submitted are the first {submittedCount} elements in
	 * the list of pending scrobbles.
	 *
	 * In addition, it is safe to use m_finishScrobblingFlag outside the critical section
	 * because it is atomic.
	 */
	{ UnlockGuard unlockGuard(m_mutex);
		logDebug(string("[LastfmScrobbler] Submission URL: ") + submissionUrlCopy);
		logDebug(string("[LastfmScrobbler] Submission request body: ") + request.body);

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().post(submissionUrlCopy.c_str(), request, response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	/* Ensure that no scrobbles are deleted by other threads during the HTTP call.
	 * Only the scrobbling thread and ::stop() can do this, and ::stop() must wait for
	 * the scrobbling thread to finish in order to do this.
	 */
	assert(pendingScrobbleCount <= m_pendingScrobbles.size());

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebug("[LastfmScrobbler] An HTTP call is aborted.");
		return 0;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return 0;
	}

	const string &responseBody = response.body;

	logDebug(string("[LastfmScrobbler] Submission response status code: ") + to_string(response.statusCode));
	logDebug(string("[LastfmScrobbler] Submission response body: ") + responseBody);

	if (response.statusCode != 200) {
		fputs("[LastfmScrobbler] An error is encountered while submitting the scrobbles to Last.fm.", stderr);
		return false;
	}

	const std::size_t end = responseBody.find('\n');
	if (end == string::npos) {
		fprintf(stderr, "[LastfmScrobbler] Invalid response body (missing line feed): '%s'.\n", responseBody.c_str());
		return false;
	}
	if (end == 2 && responseBody[0] == 'O' && responseBody[1] == 'K') {
		logDebug("[LastfmScrobbler] The scrobbles are submitted successfully.");

		// TODO use deque instead of list.
		m_pendingScrobbles.erase(m_pendingScrobbles.begin(), chunkEnd);
		return submittedCount;
	} else {
		// TODO handle non-OK responses differently (e.g. if BANNED then disable the plugin).
		fprintf(stderr, "[LastfmScrobbler] Unable to submit scrobbles to Last.fm. Reason: '%s'.\n",
				string(0, end).c_str());
		return 0;
	}

	return 0;
}

inline bool LastfmScrobbler::ensureAuthenticated()
{
	assertLocked();

	if (m_authenticated) {
		return true;
	}

	logDebug("[LastfmScrobbler] Authenticating the user...");

	// TODO set real client ID and version.
	const UrlBuilder url = buildAuthUrl(m_scrobblerUrl, m_username, m_password);

	StatusCode result;
	HttpResponseEntity response;

	/* Each HTTP call is performed outside the critical section so that other threads can:
	 * - add scrobbles without waiting for this call to finish
	 * - stop this Scrobbler by invoking Scrobbler::stop(). In this case this HTTP call
	 * is aborted and the scrobbles involved are left in the list of pending scrobbles
	 * so that they can be stored to the data file and be completed later.
	 *
	 * It is safe to unlock the mutex because no shared data is accessed outside the critical section.
	 *
	 * In addition, it is safe to use m_finishScrobblingFlag outside the critical section
	 * because it is atomic.
	 */
	{ UnlockGuard unlockGuard(m_mutex);
		logDebug(string("[LastfmScrobbler] Authentication URL: ") + string(url.c_str(), url.size()));

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().get(url.c_str(), HttpEntity(), response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebug("[LastfmScrobbler] An HTTP call is aborted.");
		return false;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return false;
	}

	string &responseBody = response.body;

	logDebug(string("[LastfmScrobbler] Authentication response status code: ") + to_string(response.statusCode));
	logDebug(string("[LastfmScrobbler] Authentication response body: ") + responseBody);

	if (response.statusCode != 200) {
		fputs("[LastfmScrobbler] An error is encountered while authenticating the user to Last.fm.", stderr);
		return false;
	}

	string::iterator start, end;
	Tokeniser<> t(responseBody, '\n');
	t.next(start, end);
	if (end - start == 2 && *start == 'O' && *(start + 1) == 'K') {
		if (!t.hasNext()) { // Session ID.
			fprintf(stderr, "[LastfmScrobbler] Invalid response body: '%s'.\n", responseBody.c_str());
			return false;
		}
		m_sessionId = std::move(t.next());

		if (!t.hasNext()) { // Now-playing URL. It is ignored for now.
			fprintf(stderr, "[LastfmScrobbler] Invalid response body: '%s'.\n", responseBody.c_str());
			return false;
		}
		t.skip();

		if (!t.hasNext()) { // Submission URL.
			fprintf(stderr, "[LastfmScrobbler] Invalid response body: '%s'.\n", responseBody.c_str());
			return false;
		}
		m_submissionUrl = std::move(t.next());

		logDebug("[LastfmScrobbler] The user is authenticated...");
		m_authenticated = true;
		return true;
	} else {
		// TODO handle non-OK responses differently (e.g. if BANNED then disable the plugin).
		fprintf(stderr, "[LastfmScrobbler] Unable to authenticate the user to Last.fm. Reason: '%s'.\n",
				string(start, end).c_str());
		return false;
	}
}
