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
#ifndef HTTPCLIENT_HPP_
#define HTTPCLIENT_HPP_

#include <string>
#include <vector>

struct HttpEntity
{
	// HttpEntity does not own anything.
	std::string body;
	std::vector<const char *> headers;
};

class HttpClient
{
	HttpClient(const HttpClient &) = delete;
	HttpClient(HttpClient &&) = delete;
	HttpClient &operator=(const HttpClient &) = delete;
	HttpClient &operator=(HttpClient &&) = delete;
public:
	HttpClient();
	~HttpClient() = default;

	// TODO support timeouts
	int send(const std::string &url, const HttpEntity &request, HttpEntity &response);
};

#endif /* HTTPCLIENT_HPP_ */
