/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

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
#ifndef HTTPCLIENT_HPP_
#define HTTPCLIENT_HPP_

#include <string>
#include <vector>
#include <atomic>

struct HttpRequestEntity
{
	std::string body;
	// HttpEntity does not own headers.
	std::vector<const char *> headers;
};

struct HttpResponseEntity
{
	std::string body;
	// HttpEntity does not own headers.
	std::vector<const char *> headers;

	int statusCode;
};

class HttpClient
{
public:
	enum class StatusCode
	{
		SUCCESS, INIT_ERROR, UNKNOWN_ERROR, UNABLE_TO_CONNECT, OPERATION_TIMEOUT, ABORTED_BY_CLIENT
	};

	static const long NO_TIMEOUT = 0L;
private:
	HttpClient(const HttpClient &) = delete;
	HttpClient(HttpClient &&) = delete;
	HttpClient &operator=(const HttpClient &) = delete;
	HttpClient &operator=(HttpClient &&) = delete;
public:
	HttpClient() = default;
	~HttpClient() = default;

	StatusCode get(const char * const url, const HttpRequestEntity &request, HttpResponseEntity &response,
			const long connectionTimeoutMillis, const long socketTimeoutMillis, const std::atomic<bool> &abortFlag)
	{
		return send(HttpMethod::GET, url, request, response, connectionTimeoutMillis, socketTimeoutMillis, abortFlag);
	}

	StatusCode post(const char * const url, const HttpRequestEntity &request, HttpResponseEntity &response,
			const long connectionTimeoutMillis, const long socketTimeoutMillis, const std::atomic<bool> &abortFlag)
	{
		return send(HttpMethod::POST, url, request, response, connectionTimeoutMillis, socketTimeoutMillis, abortFlag);
	}
private:
	enum class HttpMethod {GET, POST};

	StatusCode send(const HttpMethod method, const char * const url, const HttpRequestEntity &request,
			HttpResponseEntity &response, const long connectionTimeoutMillis, const long socketTimeoutMillis,
			const std::atomic<bool> &abortFlag);
};

#endif /* HTTPCLIENT_HPP_ */
