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
#include <afc/StringRef.hpp>
#include <afc/FastStringBuffer.hpp>
#include <cstring>
#include <algorithm>

enum UrlPartType {
	// Characters are URL encoded.
	ordinary,
	// No characters are URL encoded.
	raw
};

template<UrlPartType type = ordinary>
class UrlPart
{
public:
	explicit constexpr UrlPart(const char * const value) : UrlPart(value, std::strlen(value)) {}
	explicit constexpr UrlPart(const char * const value, const std::size_t n) : m_value(value), m_size(n) {}
	explicit constexpr UrlPart(const afc::ConstStringRef value) : UrlPart(value.value(), value.size()) {}
	explicit constexpr UrlPart(const std::string &value) : UrlPart(value.c_str(), value.size()) {}

	constexpr const char *value() const { return m_value; };
	constexpr std::size_t size() const { return m_size; }
	constexpr std::size_t maxEncodedSize() const;
private:
	const char * const m_value;
	const std::size_t m_size;
};

template<>
constexpr std::size_t UrlPart<ordinary>::maxEncodedSize() const
{
	// Each character can be escaped as %xx.
	return 3 * m_size;
}

template<>
constexpr std::size_t UrlPart<raw>::maxEncodedSize() const { return m_size; }

class UrlBuilder
{
private:
	UrlBuilder(const UrlBuilder &) = delete;
	UrlBuilder(UrlBuilder &&) = delete;
	UrlBuilder &operator=(const UrlBuilder &) = delete;
	UrlBuilder &operator=(UrlBuilder &&) = delete;
public:
	UrlBuilder(const char * const urlBase) : UrlBuilder(urlBase, std::strlen(urlBase)) {}

	UrlBuilder(const char * const urlBase, const std::size_t n) : m_buf(), m_hasParams(false)
	{
		/* Many short urls have length less than 64 characters. Setting this value
		 * as the minimal capacity to minimise re-allocations.
		 */
		m_buf.reserve(std::max(std::size_t(64), n));
		m_buf.append(urlBase, n);
	}

	UrlBuilder(const afc::ConstStringRef urlBase) : UrlBuilder(urlBase.value(), urlBase.size()) {}
	UrlBuilder(const std::string &urlBase) : UrlBuilder(urlBase.c_str(), urlBase.size()) {}

	~UrlBuilder() = default;

	inline UrlBuilder &param(const char * const name, const char * const value)
	{
		return paramName(name).paramValue(value);
	}

	inline UrlBuilder &param(const char * const name, const std::size_t nameSize,
			const char * const value, const std::size_t valueSize)
	{
		// The maximal length of the escaped parameter with '?'/'&' and '='.
		m_buf.reserve(m_buf.size() + 2 + (3 * nameSize + valueSize));

		appendParamPrefix();
		appendUrlEncoded(name, nameSize);
		m_buf += '=';
		appendUrlEncoded(value, valueSize);
		return *this;
	}

	inline UrlBuilder &param(const afc::ConstStringRef name, const afc::ConstStringRef value)
	{
		return param(name.value(), name.size(), value.value(), value.size());
	}

	inline UrlBuilder &paramName(const char * const name) { return paramName(name, std::strlen(name)); }

	inline UrlBuilder &paramName(const char * const name, const std::size_t nameSize)
	{
		// The maximal length of the escaped parameter name with '?'/'&'.
		m_buf.reserve(m_buf.size() + 1 + 3 * nameSize);

		appendParamPrefix();
		appendUrlEncoded(name, nameSize);
		return *this;
	}

	inline UrlBuilder &paramName(const afc::ConstStringRef name) { return paramName(name.value(), name.size()); }

	inline UrlBuilder &paramName(const std::string &name) { return paramName(name.c_str(), name.size()); }

	inline UrlBuilder &paramValue(const char * const value) { return paramValue(value, std::strlen(value)); }

	inline UrlBuilder &paramValue(const char * const value, const std::size_t valueSize)
	{
		// The maximal length of the escaped parameter value with '='.
		m_buf.reserve(m_buf.size() + 1 + 3 * valueSize);

		m_buf += '=';
		appendUrlEncoded(value, valueSize);
		return *this;
	}

	inline UrlBuilder &paramValue(const afc::ConstStringRef value) { return paramValue(value.value(), value.size()); }

	inline UrlBuilder &paramValue(const std::string &value) { return paramValue(value.c_str(), value.size()); }

	// Neither name nor value are URL-encoded.
	inline UrlBuilder &rawParam(const char * const name, const char * const value)
	{
		return rawParamName(name).rawParamValue(value);
	}

	// Neither name nor value are URL-encoded.
	inline UrlBuilder &rawParam(const char * const name, const std::size_t nameSize,
			const char * const value, const std::size_t valueSize)
	{
		// The maximal length of the raw parameter with '?'/'&' and '='.
		m_buf.reserve(m_buf.size() + 2 + nameSize + valueSize);

		appendParamPrefix();
		m_buf.append(name, nameSize);
		m_buf += '=';
		m_buf.append(value, valueSize);
		return *this;
	}

	// Neither name nor value are URL-encoded.
	inline UrlBuilder &rawParam(const afc::ConstStringRef name, const afc::ConstStringRef value)
	{
		return rawParam(name.value(), name.size(), value.value(), value.size());
	}

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const char * const name) { return rawParamName(name, std::strlen(name)); }

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const char * const name, const std::size_t nameSize)
	{
		// The maximal length of the raw parameter name with '?'/'&'.
		m_buf.reserve(m_buf.size() + 1 + nameSize);

		appendParamPrefix();
		m_buf.append(name, nameSize);
		return *this;
	}

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const afc::ConstStringRef name) { return rawParamName(name.value(), name.size()); }

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const std::string &name) { return rawParamName(name.c_str(), name.size()); }

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const char * const value) { return rawParamValue(value, std::strlen(value)); }

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const char * const value, const std::size_t valueSize)
	{
		// The maximal length of the raw parameter value with '='.
		m_buf.reserve(m_buf.size() + 1 + valueSize);

		m_buf += '=';
		m_buf.append(value, valueSize);
		return *this;
	}

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const afc::ConstStringRef value)
	{
		return rawParamValue(value.value(), value.size());
	}

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const std::string &value) { return rawParamValue(value.c_str(), value.size()); }

	template<typename... Parts>
	inline void params(Parts... parts)
	{
		static_assert((sizeof...(parts) % 2) == 0, "Number of URL parts must be even.");

		const std::size_t estimatedEncodedSize = sizeof...(parts) * 2 + maxEncodedSize(parts...);
		m_buf.reserve(m_buf.size() + estimatedEncodedSize);

		appendParamName(parts...);
	}

	const char *c_str() const { return m_buf.c_str(); }
	const std::size_t size() const { return m_buf.size(); }
private:
	inline void appendParamPrefix()
	{
		m_buf += m_hasParams ? '&' : '?';
		// Has params is assigned in either case to produce less jumps.
		m_hasParams = true;
	}

	inline void appendUrlEncoded(const char *str, const std::size_t n)
	{
		for (std::size_t i = 0; i < n; ++i) {
			appendUrlEncoded(str[i], m_buf);
		}
	}

	static void appendUrlEncoded(char c, afc::FastStringBuffer<char> &dest);

	template<typename... Parts>
	static constexpr std::size_t maxEncodedSize(const UrlPart<ordinary> part, Parts ...parts)
	{
		return part.maxEncodedSize() + maxEncodedSize(parts...);
	}

	template<typename... Parts>
	static constexpr std::size_t maxEncodedSize(const UrlPart<raw> part, Parts... parts)
	{
		return part.maxEncodedSize() + maxEncodedSize(parts...);
	}

	// Terminates the maxEncodedSize() template recusion.
	static constexpr std::size_t maxEncodedSize() { return 0; }

	template<typename... Parts>
	void appendParamName(const UrlPart<ordinary> name, Parts ...parts)
	{
		// ::params() ensured that there is enough capacity.
		appendParamPrefix();
		appendUrlEncoded(name.value(), name.size());
		appendParamValue(parts...);
	}

	template<typename... Parts>
	void appendParamName(const UrlPart<raw> name, Parts ...parts)
	{
		// ::params() ensured that there is enough capacity.
		appendParamPrefix();
		m_buf.append(name.value(), name.size());
		appendParamValue(parts...);
	}

	// Terminates the maxEncodedSize() template recusion.
	void appendParamName() {}

	template<typename... Parts>
	void appendParamValue(const UrlPart<ordinary> value, Parts ...parts)
	{
		// ::params() ensured that there is enough capacity.
		m_buf += '=';
		appendUrlEncoded(value.value(), value.size());
		appendParamName(parts...);
	}

	template<typename... Parts>
	void appendParamValue(const UrlPart<raw> value, Parts ...parts)
	{
		// ::params() ensured that there is enough capacity.
		m_buf += '=';
		m_buf.append(value.value(), value.size());
		appendParamName(parts...);
	}

	afc::FastStringBuffer<char> m_buf;
	bool m_hasParams;
};

#endif /* URLBUILDER_HPP_ */
