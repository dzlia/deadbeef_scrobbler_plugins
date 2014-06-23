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
#include <afc/ensure_ascii.hpp>
#include <cstddef>

class UrlBuilder
{
private:
	UrlBuilder(const UrlBuilder &) = delete;
	UrlBuilder(UrlBuilder &&) = delete;
	UrlBuilder &operator=(const UrlBuilder &) = delete;
	UrlBuilder &operator=(UrlBuilder &&) = delete;
public:
	UrlBuilder(const char * const urlBase) : m_buf(urlBase), m_hasParams(false) {}
	UrlBuilder(const std::string &urlBase) : m_buf(urlBase), m_hasParams(false) {}
	UrlBuilder(std::string &&urlBase) : m_buf(urlBase), m_hasParams(false) {}

	~UrlBuilder() = default;

	inline UrlBuilder &param(const char * const name, const char * const value)
	{
		if (m_hasParams) {
			m_buf += '&';
		} else {
			m_buf += '?';
			m_hasParams = true;
		}
		appendUrlEncoded(name);
		m_buf += '=';
		appendUrlEncoded(value);
		return *this;
	}

	inline UrlBuilder &param(const char * const name, const size_t nameSize,
			const char * const value, const size_t valueSize)
	{
		if (m_hasParams) {
			m_buf += '&';
		} else {
			m_buf += '?';
			m_hasParams = true;
		}
		appendUrlEncoded(name, nameSize);
		m_buf += '=';
		appendUrlEncoded(value, valueSize);
		return *this;
	}

	const std::string &toString() const { return m_buf; }
private:
	void appendUrlEncoded(const char *str);
	void appendUrlEncoded(const char *str, const size_t n);

	std::string m_buf;
	bool m_hasParams;
};

#endif /* URLBUILDER_HPP_ */
