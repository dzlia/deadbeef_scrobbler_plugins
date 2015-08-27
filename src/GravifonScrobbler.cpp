/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2015 Dźmitry Laŭčuk

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
#include "GravifonScrobbler.hpp"
#include <algorithm>
#include <cassert>
#include <afc/base64.hpp>
#include "HttpClient.hpp"
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <utility>
#include "jsonutil.hpp"
#include "pathutil.hpp"
#include <afc/FastStringBuffer.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/logger.hpp>
#include <afc/StringRef.hpp>
#include <afc/utils.h>
#include "deadbeef_util.hpp"

using namespace std;
using namespace afc;

using afc::operator"" _s;
using afc::logger::logDebug;
using afc::logger::logDebugMsg;
using afc::logger::logError;
using afc::logger::logErrorMsg;

using Json::Value;
using StatusCode = HttpClient::StatusCode;

namespace
{
	template<typename SuccessOp, typename FailureOp, typename InvalidOp>
	inline void processStatus(const Value &status, SuccessOp successOp, FailureOp failureOp, InvalidOp invalidOp)
	{
		if (isType(status, objectValue)) {
			const Value &success = status["ok"];
			if (isType(success, booleanValue)) {
				if (success.asBool() == true) {
					successOp();
					return;
				} else {
					const Value &errCodeValue = status["error_code"];
					if (isType(errCodeValue, intValue)) {
						const unsigned long errorCode = static_cast<unsigned long>(errCodeValue.asInt());
						const Value &errorDescription = status["error_description"];
						if (isType(errorDescription, stringValue)) {
							failureOp(errorCode, errorDescription.asString());
							return;
						} else if (isType(errorDescription, nullValue)) {
							failureOp(errorCode, string());
							return;
						} // else invalid error description type
					}
				}
			}
		}
		invalidOp();
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
			message = "sending the scrobble message has timed out";
			break;
		default:
			message = "unknown error";
		}
		logError("[GravifonScrobbler] Unable to send the scrobble message: '#'.", message);
	}

	inline bool isRecoverableError(const unsigned long errorCode)
	{
		return errorCode < 10000 || errorCode == 10003 || errorCode > 10006;
	}
}

void GravifonScrobbler::stopExtra()
{
	m_scrobblerUrl.clear();
	m_authHeader.clear();
}

void GravifonScrobbler::configure(const char * const serverUrl, const std::size_t serverUrlSize,
		const char * const username, const std::size_t usernameSize,
		const char * const password, const std::size_t passwordSize)
{ lock_guard<mutex> lock(m_mutex);
	afc::FastStringBuffer<char, afc::AllocMode::accurate> tmpUrl(serverUrlSize + "scrobbles"_s.size() + 1);
	auto p = std::copy_n(serverUrl, serverUrlSize, tmpUrl.borrowTail());
	if (serverUrlSize > 0) {
		p = appendToPath(*(p - 1), "scrobbles"_s, p);
	}
	tmpUrl.returnTail(p);

	/* Colon (':') is not allowed to be in a username by Gravifon. This concatenation is safe.
	 * In addition, the character 'colon' in UTF-8 is equivalent to those in ISO-8859-1.
	 */
	afc::FastStringBuffer<char, afc::AllocMode::accurate> token(usernameSize + passwordSize + 1);
	token.append(username, usernameSize);
	token.append(':');
	token.append(password, passwordSize);

	// Curl expects the basic charset in headers.
	afc::FastStringBuffer<char, afc::AllocMode::accurate> tmpAuthHeader("Authorization: Basic "_s.size() + token.size() * 4 / 3); // base64 makes 4 bytes out of 3.
	tmpAuthHeader.append("Authorization: Basic "_s); // HTTP Basic authentication is used.
	tmpAuthHeader.returnTail(afc::encodeBase64(token.begin(), token.size(), tmpAuthHeader.borrowTail()));

	const bool sameScrobblerUrl = afc::equal(m_scrobblerUrl.begin(), m_scrobblerUrl.size(),
			tmpUrl.begin(), tmpUrl.size());
	const bool sameAuthHeader = afc::equal(m_authHeader.begin(), m_scrobblerUrl.size(),
			tmpAuthHeader.begin(), tmpAuthHeader.size());

	if (!sameScrobblerUrl || !sameAuthHeader) {
		// The configuration has changed. Updating it as well as resetting the 'scrobbles to wait' counter.
		if (!sameScrobblerUrl) {
			const std::size_t urlSize = tmpUrl.size();
			m_scrobblerUrl.attach(tmpUrl.detach(), urlSize);
		}
		if (!sameAuthHeader) {
			const std::size_t authHeaderSize = tmpAuthHeader.size();
			m_authHeader.attach(tmpAuthHeader.detach(), authHeaderSize);
		}
		m_scrobblesToWait = minScrobblesToWait();
	}

	m_configured = true;
}

size_t GravifonScrobbler::doScrobbling()
{
	assertLocked();
	assert(!m_pendingScrobbles.empty());

	if (unlikely(!m_configured)) {
		logErrorMsg("Scrobbler is not configured properly."_s);
		return 0;
	}

	if (m_scrobblerUrl.empty()) {
		/* There is no sense to try to send a request because the URL to the scrobbling
		 * service (e.g. Gravifon API) is undefined. The scrobble is added to the list of
		 * pending scrobbles (see above) to be submitted (among other scrobbles) when
		 * the URL is configured to point to a scrobbling server.
		 */
		logErrorMsg("URL to the scrobbling server is undefined."_s);
		return 0;
	}

	// TODO replace it with buffer.
	string body;
	body += u8"[";

	afc::FastStringBuffer<char> buf;
	// Adding up to 20 scrobbles to the request.
	// 20 is the max number of scrobbles in a single request.
	unsigned submittedCount = 0;
	for (auto it = m_pendingScrobbles.begin(), end = m_pendingScrobbles.end();
			submittedCount < 20 && it != end; ++submittedCount, ++it) {
		appendAsJson(*it, buf);
		// TODO Use FastStringBuffer to build body.
		body.append(buf.data(), buf.size());
		body += u8",";
	}
	body.pop_back(); // Removing the redundant comma.
	body += u8"]";

	HttpRequest request;
	request.setBody(body.data(), body.size());
	request.headers.reserve(4);
	request.headers.push_back(m_authHeader.c_str());
	// Curl expects the basic charset in headers.
	request.headers.push_back("Content-Type: application/json; charset=utf-8");
	request.headers.push_back("Accept: application/json");
	request.headers.push_back("Accept-Charset: utf-8");

	// Making a copy of shared data to pass outside the critical section.
	afc::String scrobblerUrlCopy(m_scrobblerUrl);

#ifndef NDEBUG
	const size_t pendingScrobbleCount = m_pendingScrobbles.size();
#endif

	afc::FastStringBuffer<char> responseBody;
	FastStringBufferAppender responseBodyAppender(responseBody);
	StatusCode result;
	HttpResponse response(responseBodyAppender);

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
		logDebug("[LastfmScrobbler] Request body: #",
				std::pair<const char *, const char *>(request.getBody(), request.getBody() + request.getBodySize()));

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().post(scrobblerUrlCopy.c_str(), request, response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	/* Ensure that no scrobbles are deleted by other threads during the HTTP call.
	 * Only the scrobbling thread and ::stop() can do this, and ::stop() must wait for
	 * the scrobbling thread to finish in order to do this.
	 */
	assert(pendingScrobbleCount <= m_pendingScrobbles.size());

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebugMsg("[GravifonScrobbler] An HTTP call is aborted."_s);
		return 0;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return 0;
	}

	logDebug("[GravifonScrobbler] Response status code: '#'.", response.statusCode);

	Value rs;
	if (!Json::Reader().parse(responseBody.data(), responseBody.data() + responseBody.size(), rs, false)) {
		logError("[GravifonScrobbler] Invalid response: '#'.", responseBody.c_str());
		return 0;
	}

	if (response.statusCode == 200) {
		// An array of status entities is expected for a 200 response, one per scrobble submitted.
		if (!isType(rs, arrayValue) || rs.size() != submittedCount) {
			logError("[GravifonScrobbler] Invalid response: '#'.", responseBody.c_str());
			return 0;
		}

		size_t completedCount = 0;
		auto it = m_pendingScrobbles.begin();
		for (auto i = 0u, n = rs.size(); i < n; ++i) {
			processStatus(rs[i],
					// Successful status: if the track is scrobbled successfully then it is removed from the list.
					[&it, &completedCount, this]()
					{
						it = m_pendingScrobbles.erase(it);
						++completedCount;
					},

					/* Error status. If the error is unprocessable then the scrobble is removed from the list;
					 * otherwise another attempt will be done to submit it.
					 */
					[&responseBody, &it, &completedCount, this](
							const unsigned long errorCode, const string &errorDescription)
					{
						afc::FastStringBuffer<char, afc::AllocMode::accurate> scrobbleAsStr = serialiseAsJson(*it);
						if (isRecoverableError(errorCode)) {
							logError("[GravifonScrobbler] Scrobble '#' is not processed. "
									"Error: '#' (#). It will be re-submitted later.",
									scrobbleAsStr.c_str(), errorDescription, errorCode);
							++it;
						} else {
							logError("[GravifonScrobbler] Scrobble '#' cannot be processed. "
									"Error: '#' (#). It is removed as non-processable.",
									scrobbleAsStr.c_str(), errorDescription, errorCode);
							it = m_pendingScrobbles.erase(it);
							++completedCount;
						}
					},

					// Invalid status: report an error and leave the scrobble in the list of pending scrobbles.
					[&responseBody, &it]()
					{
						logError("[GravifonScrobbler] Invalid response: '#'.", responseBody.c_str());
						++it;
					});
		}

		if (completedCount == submittedCount) {
			logDebug("[GravifonScrobbler] Successful response: #", responseBody.c_str());
		}

		return completedCount;
	} else {
		// A global status entity is expected for a non-200 response.
		processStatus(rs,
				// Success status. It is not expected. Report an error and finish processing.
				[&responseBody]()
				{
					logError("[GravifonScrobbler] Unexpected 'ok' global status response: '#'.",
							responseBody.c_str());
				},

				// Error status: report an error and finish processing.
				[&responseBody](const unsigned long errorCode, const string &errorDescription)
				{
					logError("[GravifonScrobbler] Error global status response: '#'. Error: '#' (#).",
							responseBody.c_str(), errorDescription, errorCode);
				},

				// Invalid status: report an error and finish processing.
				[&responseBody]()
				{
					logError("[GravifonScrobbler] Invalid response: '#'.", responseBody.c_str());
				});

		return 0;
	}
}
