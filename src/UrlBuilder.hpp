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
#ifndef URLBUILDER_HPP_
#define URLBUILDER_HPP_

#include <string>

class UrlBuilder
{
private:
	UrlBuilder(const UrlBuilder &) = delete;
	UrlBuilder(UrlBuilder &&) = delete;
	UrlBuilder &operator=(const UrlBuilder &) = delete;
	UrlBuilder &operator=(UrlBuilder &&) = delete;
public:
	UrlBuilder(const std::string &urlBase) : m_buf(urlBase), m_hasParams(false) {}
	UrlBuilder(std::string &&urlBase) : m_buf(urlBase), m_hasParams(false) {}

	~UrlBuilder() = default;

	inline UrlBuilder &param(const std::string &name, const std::string &value)
	{
		if (m_hasParams) {
			m_buf += '&';
		} else {
			m_buf += '?';
			m_hasParams = true;
		}
		appendUrlEncoded(name, m_buf);
		m_buf += '=';
		appendUrlEncoded(value, m_buf);
		return *this;
	}

	const std::string &toString() const { return m_buf; }
private:
	void appendUrlEncoded(const std::string &str, std::string &dest);

	std::string m_buf;
	bool m_hasParams;
};

#endif /* URLBUILDER_HPP_ */
