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
#ifndef GRAVIFONSCROBBLER_HPP_
#define GRAVIFONSCROBBLER_HPP_

#include "Scrobbler.hpp"
#include <cstddef>
#include <string>

class GravifonScrobbler : public Scrobbler
{
public:
	virtual ~GravifonScrobbler() {}

	// username and password are to be in ISO-8859-1; serverUrl is to be in the system encoding.
	void configure(const char *serverUrl, const std::string &username, const std::string &password);
protected:
	virtual size_t doScrobbling() override;

	virtual void stopExtra() override;
private:
	std::string m_scrobblerUrl;
	// The authentication header encoded in the basic charset.
	std::string m_authHeader;
};

#endif /* GRAVIFONSCROBBLER_HPP_ */
