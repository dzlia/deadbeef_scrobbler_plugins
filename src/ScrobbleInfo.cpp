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
#include "ScrobbleInfo.hpp"
#include <algorithm>
#include <bitset>
#include <cstddef>
#include <utility>
#include <afc/builtin.hpp>
#include <afc/dateutil.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/json.hpp>
#include <afc/logger.hpp>
#include <afc/number.h>
#include <afc/StringRef.hpp>
#include <afc/utils.h>

using afc::operator"" _s;

namespace
{
	std::bitset<256> initJsonCharsToEscape()
	{
		std::bitset<256> result;
		for (auto i = 0; i < 0x20; ++i) {
			result.set(i);
		}
		result.set('"');
		result.set('\\');
		result.set('/');
		result.set(0x7f);
		return result;
	}

	static const std::bitset<256> jsonCharsToEscape = initJsonCharsToEscape();

	template<typename Iterator>
	Iterator writeJsonString(const char * const srcBegin, const char * const srcEnd, register Iterator dest)
	{
		for (auto p = srcBegin; p < srcEnd; ++p) {
			const char c = *p;
			// Truncated to an octet just in case non-octet bytes are used.
			const unsigned char uc = static_cast<unsigned char>(c) & 0xff;
			if (!jsonCharsToEscape[uc]) {
				*dest++ = c;
			} else {
				switch (c) {
				case '"':
				case '\\':
				case '/':
					*dest++ = '\\';
					*dest++ = c;
					break;
				case '\b':
					dest = afc::copy("\\b"_s, dest);
					break;
				case '\f':
					dest = afc::copy("\\f"_s, dest);
					break;
				case '\n':
					dest = afc::copy("\\n"_s, dest);
					break;
				case '\r':
					dest = afc::copy("\\r"_s, dest);
					break;
				case '\t':
					dest = afc::copy("\\t"_s, dest);
					break;
				default:
					dest = afc::copy("\\u00"_s, dest);
					dest = afc::octetToHex(c, dest);
					break;
				}
			}
		}
		return dest;
	}

	inline std::size_t artistCount(const char * const artistsBegin, const char * const artistsEnd) noexcept
	{
		return std::count_if(artistsBegin, artistsEnd,
				[](const char c) -> bool { return c == Track::multiTagSeparator(); }) + 1;
	}

	template<typename Iterator>
	inline Iterator writeArtists(const char * const artistsBegin, const char * const artistsEnd, register Iterator dest)
	{
		// Artists are '\n'-separated values within a single string.
		const char *start = artistsBegin, *end = artistsEnd;
		for (const char *p = start; p != end; ++p) {
			if (*p == Track::multiTagSeparator()) {
				dest = afc::copy(R"({"name":")"_s, dest);
				dest = writeJsonString(start, p, dest);
				dest = afc::copy(R"("},)"_s, dest);
				start = p + 1;
			}
		}
		// Adding the last tag.
		dest = afc::copy(R"({"name":")"_s, dest);
		dest = writeJsonString(start, artistsEnd, dest);
		dest = afc::copy(R"("})"_s, dest);
		return dest;
	}

	inline std::size_t maxJsonSize(const ScrobbleInfo &scrobbleInfo) noexcept
	{
		const Track &track = scrobbleInfo.track;

		assert(track.getTitleBegin() != track.getTitleEnd());
		assert(track.getArtistsBegin() != track.getArtistsEnd());

		/* Each free-text label can be escaped so it is multiplied by six to cover the case
		 * when each character is unicode-escaped.
		 */
		constexpr auto maxPrintedCharSize = 6;

		std::size_t maxSize = 0;
		maxSize += 2; // {}
		maxSize += R"("scrobble_start_datetime":"")"_s.size() + afc::maxISODateTimeSize();
		maxSize += 1; // ,
		maxSize += R"("scrobble_end_datetime":"")"_s.size() + afc::maxISODateTimeSize();
		maxSize += 1; // ,
		maxSize += R"("scrobble_duration":{"amount":,"unit":"ms"})"_s.size() +
				afc::maxPrintedSize<decltype(scrobbleInfo.scrobbleDuration), 10>();
		maxSize += 1; // ,
		maxSize += R"("track":{})"_s.size();
		maxSize += R"("title":"")"_s.size() + maxPrintedCharSize * (track.getTitleEnd() - track.getTitleBegin());
		maxSize += 1; // ,

		{ // artists
			const char * const artistsBegin = track.getArtistsBegin();
			const char * const artistsEnd = track.getArtistsEnd();
			const std::size_t count= artistCount(artistsBegin, artistsEnd);
			maxSize += R"("artists":[])"_s.size();
			maxSize += R"({"name":""},)"_s.size() * count - 1; // With commas; the last comma is removed.
			maxSize += maxPrintedCharSize * (artistsEnd - artistsBegin - count);
			maxSize += 1; // ,
		}

		const std::size_t albumTitleSize = track.getAlbumTitleEnd() - track.getAlbumTitleBegin();
		if (albumTitleSize > 0) {
			maxSize += R"("album":{"title":""})"_s.size();
			maxSize += maxPrintedCharSize * albumTitleSize;
			const char * const albumArtistsBegin = track.getAlbumArtistsBegin();
			const char * const albumArtistsEnd = track.getAlbumArtistsEnd();
			const std::size_t albumArtistsSize = albumArtistsEnd - albumArtistsBegin;
			if (albumArtistsSize > 0) {
				const std::size_t count= artistCount(albumArtistsBegin, albumArtistsEnd);
				maxSize += 1; // ,
				maxSize += R"("artists":[])"_s.size();
				maxSize += R"({"name":""},)"_s.size() * count - 1; // With commas; the last comma is removed.
				maxSize += maxPrintedCharSize * (albumArtistsSize  - count);
			}
			maxSize += 1; // ,
		}

		maxSize += R"("length":{"amount":,"unit":"ms"})"_s.size() +
				afc::maxPrintedSize<decltype(track.getDurationMillis()), 10>();

		return maxSize;
	}

	template<typename Iterator>
	inline Iterator appendAsJsonImpl(const ScrobbleInfo &scrobbleInfo, register Iterator dest) noexcept
	{
		const Track &track = scrobbleInfo.track;

		dest = afc::copy(R"({"scrobble_start_datetime":")"_s, dest);
		dest = afc::formatISODateTime(scrobbleInfo.scrobbleStartTimestamp, dest);
		dest = afc::copy(R"(","scrobble_end_datetime":")"_s, dest);
		dest = afc::formatISODateTime(scrobbleInfo.scrobbleEndTimestamp, dest);
		dest = afc::copy(R"(","scrobble_duration":{"amount":)"_s, dest);
		dest = afc::printNumber<10>(scrobbleInfo.scrobbleDuration, dest);
		dest = afc::copy(R"(,"unit":"ms"},"track":{"title":")"_s, dest);
		dest = writeJsonString(track.getTitleBegin(), track.getTitleEnd(), dest);
		// At least single artist is expected.
		dest = afc::copy(R"(","artists":[)"_s, dest);
		dest = writeArtists(track.getArtistsBegin(), track.getArtistsEnd(), dest);
		const char * const albumTitleBegin = track.getAlbumTitleBegin();
		const char * const albumTitleEnd = track.getAlbumTitleEnd();
		if (albumTitleBegin != albumTitleEnd) {
			dest = afc::copy(R"(],"album":{"title":")"_s, dest);
			dest = writeJsonString(albumTitleBegin, albumTitleEnd, dest);
			const char * const albumArtistsBegin = track.getAlbumArtistsBegin();
			const char * const albumArtistsEnd = track.getAlbumArtistsEnd();
			if (albumArtistsBegin != albumArtistsEnd) {
				dest = afc::copy(R"(","artists":[)"_s, dest);
				dest = writeArtists(albumArtistsBegin, albumArtistsEnd, dest);
				dest = afc::copy(R"(]},"length":{"amount":)"_s, dest);
			} else {
				dest = afc::copy(R"("},"length":{"amount":)"_s, dest);
			}
		} else {
			dest = afc::copy(R"(],"length":{"amount":)"_s, dest);
		}
		dest = afc::printNumber<10>(track.getDurationMillis(), dest);
		dest = afc::copy(R"(,"unit":"ms"}}})"_s, dest);

		return dest;
	}

	template<typename ErrorHandler>
	inline const char *parseText(const char * const begin, const char * const end, afc::FastStringBuffer<char> &dest, ErrorHandler &errorHandler)
	{
		auto textParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			return afc::json::parseCharsToUTF8(begin, end, [&](const char c) { dest.reserveForOne(); dest.append(c); }, errorHandler);
		};

		return afc::json::parseString<const char *, decltype(textParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, textParser, errorHandler);
	}

	template<typename ErrorHandler>
	inline const char *parseDateTime(const char * const begin, const char * const end, afc::TimestampTZ &dest, ErrorHandler &errorHandler) noexcept
	{
		auto timestampParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char * const valueEnd = std::find(begin, end, u8"\""[0]);
			if (valueEnd == end) {
				errorHandler.prematureEnd();
				return end;
			}
			if (likely(parseISODateTime(afc::String(begin, valueEnd).c_str(), dest))) {
				return valueEnd;
			} else {
				errorHandler.malformedJson(begin);
				return end;
			}
		};

		return afc::json::parseString<const char *, decltype(timestampParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, timestampParser, errorHandler);
	}

	// TOOD defined noexcept
	template<typename ErrorHandler>
	inline const char *parseDuration(const char * const begin, const char * const end, long &dest, ErrorHandler &errorHandler)
	{
		auto durationParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			auto amountValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				auto durationAmountParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler)
				{
					const char * const p = afc::parseNumber<10, afc::ParseMode::scan>(
							begin, end, dest, [&](const char * const pos) { errorHandler.malformedJson(pos); });
					if (unlikely(!errorHandler.valid())) {
						return end;
					}
					return p;
				};

				return afc::json::parseNumber<const char *, decltype(durationAmountParser) &, ErrorHandler, afc::json::noSpaces>
						(begin, end, durationAmountParser, errorHandler);
			};

			auto unitValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				auto unitParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
				{
					const char * const unitEnd = std::find(begin, end, u8"\""[0]);
					if (unitEnd == end) {
						return end;
					}
					switch (unitEnd - begin) {
					case 1:
						if (likely(begin[0] == u8"s"[0])) {
							dest *= 1000;
							return unitEnd;
						} else {
							errorHandler.malformedJson(begin);
							return end;
						}
					case 2:
						if (unlikely(begin[0] != u8"m"[0] || begin[1] != u8"s"[0])) {
							errorHandler.malformedJson(begin);
							return end;
						}
						return unitEnd;
					default:
						errorHandler.malformedJson(begin);
						return end;
					}
				};

				return afc::json::parseString<const char *, decltype(unitParser) &, ErrorHandler, afc::json::noSpaces>
						(begin, end, unitParser, errorHandler);
			};

			const char *p = begin;

			p = afc::json::parsePropertyValue<const char *, decltype(amountValueParser) &, ErrorHandler, afc::json::noSpaces>(
					p, end, "amount", "amount"_s.size(), amountValueParser, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			return afc::json::parsePropertyValue<const char *, decltype(unitValueParser) &, ErrorHandler, afc::json::noSpaces>(
					p, end, "unit", "unit"_s.size(), unitValueParser, errorHandler);
		};

		return afc::json::parseObject<const char *, decltype(durationParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, durationParser, errorHandler);
	}

	template<typename ErrorHandler>
	inline const char *parseArtists(const char * const begin, const char * const end, afc::FastStringBuffer<char> &dest, ErrorHandler &errorHandler)
	{
		auto artistElementParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			auto artistParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler)
			{
				auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
				{
					if (unlikely(end - begin < 4 || begin[0] != u8"n"[0] || begin[1] != u8"a"[0] || begin[2] != u8"m"[0] || begin[3] != u8"e"[0])) {
						errorHandler.malformedJson(begin);
						return end;
					} else {
						return begin + 4;
					}
				};

				const char *p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
						(begin, end, propNameParser, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = parseText(p, end, dest, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				dest.reserveForOne();
				dest.append(Track::multiTagSeparator());

				return p;
			};

			return afc::json::parseObject<const char *, decltype(artistParser) &, ErrorHandler, afc::json::noSpaces>
					(begin, end, artistParser, errorHandler);
		};

		const char * const p = afc::json::parseArray<const char *, decltype(artistElementParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, artistElementParser, errorHandler);
		if (dest.size() > 0) { // removing extra separator
			dest.resize(dest.size() - 1);
		}
		return p;
	}

	template<typename ErrorHandler>
	inline const char *parseAlbum(const char * const begin, const char * const end, Track &dest, ErrorHandler &errorHandler)
	{
		auto albumParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char *p = begin;

			auto titleValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				auto p = parseText(begin, end, dest.m_data, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				dest.m_albumArtistsBegin = dest.m_data.size();
				return p;
			};

			auto artistsValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				auto p = parseArtists(begin, end, dest.m_data, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}
				return p;
			};

			p = afc::json::parsePropertyValue<const char *, decltype(titleValueParser) &, ErrorHandler, afc::json::noSpaces>(
					p, end, "title", "title"_s.size(), titleValueParser, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}
			if (unlikely(p == end)) {
				errorHandler.prematureEnd();
				return end;
			}

			const char c = *p;
			if (c == u8"}"[0]) { // No artists field.
				return p;
			} else if (likely(c == u8","[0])) {
				++p;
				return afc::json::parsePropertyValue<const char *, decltype(artistsValueParser) &, ErrorHandler, afc::json::noSpaces>(
						p, end, "artists", "artists"_s.size(), artistsValueParser, errorHandler);
			} else {
				errorHandler.malformedJson(p);
				return end;
			}
		};

		return afc::json::parseObject<const char *, decltype(albumParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, albumParser, errorHandler);
	}

	template<typename ErrorHandler>
	inline const char *parseTrack(const char * const begin, const char * const end, Track &dest, ErrorHandler &errorHandler)
	{
		auto trackParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char *p = begin;

			auto titleValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				auto p = parseText(begin, end, dest.m_data, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				dest.m_artistsBegin = dest.m_data.size();
				return p;
			};

			auto artistsValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				auto p = parseArtists(begin, end, dest.m_data, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				dest.m_albumTitleBegin = dest.m_data.size();
				return p;
			};

			auto lengthValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				long trackDuration;
				auto p = parseDuration(begin, end, trackDuration, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				dest.setDurationMillis(trackDuration);
				return p;
			};

			p = afc::json::parsePropertyValue<const char *, decltype(titleValueParser) &, ErrorHandler, afc::json::noSpaces>(
					p, end, "title", "title"_s.size(), titleValueParser, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			p = afc::json::parsePropertyValue<const char *, decltype(artistsValueParser) &, ErrorHandler, afc::json::noSpaces>(
					p, end, "artists", "artists"_s.size(), artistsValueParser, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			// TODO optimise property name matching.
			const char *propNameBegin;
			std::size_t propNameSize;

			auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				propNameBegin = begin;
				const char * const propNameEnd = std::find(begin, end, u8"\""[0]);
				propNameSize = propNameEnd - propNameBegin;
				return propNameEnd;
			};

			p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
					(p, end, propNameParser, errorHandler);

			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			if (afc::equal("album", "album"_s.size(), propNameBegin, propNameSize)) {
				p = parseAlbum(p, end, dest, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				return afc::json::parsePropertyValue<const char *, decltype(lengthValueParser) &, ErrorHandler, afc::json::noSpaces>(
						p, end, "length", "length"_s.size(), lengthValueParser, errorHandler);
			} else if (afc::equal("length", "length"_s.size(), propNameBegin, propNameSize)) {
				dest.m_albumArtistsBegin = dest.m_albumTitleBegin = dest.m_data.size(); // No album.

				return lengthValueParser(p, end, errorHandler);
			} else {
				errorHandler.malformedJson(p);
				return end;
			}
		};

		return afc::json::parseObject<const char *, decltype(trackParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, trackParser, errorHandler);
	}
}

bool ScrobbleInfo::parse(const char * const begin, const char * const end, ScrobbleInfo &dest)
{
	class ErrorHandler
	{
	public:
		ErrorHandler(void) : m_valid(true) {}

		void malformedJson(const char *pos)
		{
			// TODO handle error;
			m_valid = false;
		}

		void prematureEnd()
		{
			// TODO handle error;
			m_valid = false;
		}

		bool valid() { return m_valid; }
	private:
		bool m_valid;
	} errorHandler;

	auto scrobbleParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
	{
		auto scrobbleStartTimeValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			return parseDateTime(begin, end, dest.scrobbleStartTimestamp, errorHandler);
		};

		auto scrobbleEndTimeValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			return parseDateTime(begin, end, dest.scrobbleEndTimestamp, errorHandler);
		};

		auto scrobbleDurationValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			return parseDuration(begin, end, dest.scrobbleDuration, errorHandler);
		};

		auto trackValueParser = [&dest](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			return parseTrack(begin, end, dest.track, errorHandler);
		};

		const char *p = begin;

		p = afc::json::parsePropertyValue<const char *, decltype(scrobbleStartTimeValueParser) &, ErrorHandler, afc::json::noSpaces>(
				p, end, "scrobble_start_datetime", "scrobble_start_datetime"_s.size(), scrobbleStartTimeValueParser, errorHandler);
		if (unlikely(!errorHandler.valid())) {
			return end;
		}

		p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
		if (unlikely(!errorHandler.valid())) {
			return end;
		}

		p = afc::json::parsePropertyValue<const char *, decltype(scrobbleEndTimeValueParser) &, ErrorHandler, afc::json::noSpaces>(
				p, end, "scrobble_end_datetime", "scrobble_end_datetime"_s.size(), scrobbleEndTimeValueParser, errorHandler);
		if (unlikely(!errorHandler.valid())) {
			return end;
		}

		p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
		if (unlikely(!errorHandler.valid())) {
			return end;
		}

		p = afc::json::parsePropertyValue<const char *, decltype(scrobbleDurationValueParser) &, ErrorHandler, afc::json::noSpaces>(
				p, end, "scrobble_duration", "scrobble_duration"_s.size(), scrobbleDurationValueParser, errorHandler);
		if (unlikely(!errorHandler.valid())) {
			return end;
		}

		p = afc::json::parseComma<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
		if (unlikely(!errorHandler.valid())) {
			return end;
		}

		return afc::json::parsePropertyValue<const char *, decltype(trackValueParser) &, ErrorHandler, afc::json::noSpaces>(
				p, end, "track", "track"_s.size(), trackValueParser, errorHandler);
	};

	const char * const p = afc::json::parseObject<const char *, decltype(scrobbleParser) &, ErrorHandler, afc::json::noSpaces>
			(begin, end, scrobbleParser, errorHandler);

	if (unlikely(!errorHandler.valid() || p != end)) {
		// TODO handle error.
		return false;
	}

	return true;
}

afc::FastStringBuffer<char, afc::AllocMode::accurate> serialiseAsJson(const ScrobbleInfo &scrobbleInfo)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	afc::FastStringBuffer<char, afc::AllocMode::accurate> buf(maxSize);

	buf.returnTail(appendAsJsonImpl(scrobbleInfo, buf.borrowTail()));

	return buf;
}

void appendAsJson(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &dest)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	dest.reserve(dest.size() + maxSize);

	dest.returnTail(appendAsJsonImpl(scrobbleInfo, dest.borrowTail()));
}
