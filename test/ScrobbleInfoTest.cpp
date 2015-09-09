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
#include "ScrobbleInfoTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(ScrobbleInfoTest);

#include <ScrobbleInfo.hpp>
#include <ctime>
#include <afc/FastStringBuffer.hpp>
#include <afc/StringRef.hpp>
#include <afc/utils.h>

using afc::operator"" _s;
using namespace std;

namespace
{
	time_t utcTime(const int year, const int month, const int day, const int hour, const int minute, const int second)
	{
		time_t t;
		time(&t);

		tm * const dateTime = gmtime(&t);
		dateTime->tm_year = year - 1900;
		dateTime->tm_mon = month - 1;
		dateTime->tm_mday = day;
		dateTime->tm_hour = hour;
		dateTime->tm_min = minute;
		dateTime->tm_sec = second;
		dateTime->tm_isdst = -1;

		return timegm(dateTime);
	}
}

void ScrobbleInfoTest::setUp()
{
	const char * const tz = getenv("TZ");
	if (tz != nullptr) {
		m_timeZoneBackup.reset(new string(tz));
	}
	setenv("TZ", "ABC-02:30", true);
	tzset();
}

void ScrobbleInfoTest::tearDown()
{
	if (m_timeZoneBackup != nullptr) {
		setenv("TZ", m_timeZoneBackup->c_str(), true);
	} else {
		unsetenv("TZ");
	}
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_WithAllFields_SingleArtist()
{
	afc::ConstStringRef input = u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T13:40:04+0130",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"Scorpions"}]},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"_s;

	afc::Optional<ScrobbleInfo> result = ScrobbleInfo::parse(input.begin(), input.end());

	CPPUNIT_ASSERT(result.hasValue());

	afc::FastStringBuffer<char, afc::AllocMode::accurate> serialisedScrobble = serialiseAsJson(result.value());

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T13:40:04+0130",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"Scorpions"}]},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), string(serialisedScrobble.c_str()));
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_WithAllFields_MultipleArtists()
{
	afc::ConstStringRef input = u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0100",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"},{"name":"Scorpions"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"ABBA"}]},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"_s;

	afc::Optional<ScrobbleInfo> result = ScrobbleInfo::parse(input.begin(), input.end());

	CPPUNIT_ASSERT(result.hasValue());

	afc::FastStringBuffer<char, afc::AllocMode::accurate> serialisedScrobble = serialiseAsJson(result.value());

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0100",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"},{"name":"Scorpions"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"ABBA"}]},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), string(serialisedScrobble.c_str()));
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_WithAllFields_MultipleAlbumArtists()
{
	afc::ConstStringRef input = u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"ABBA"},{"name":"Scorpions"}]},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"_s;

	afc::Optional<ScrobbleInfo> result = ScrobbleInfo::parse(input.begin(), input.end());

	CPPUNIT_ASSERT(result.hasValue());

	afc::FastStringBuffer<char, afc::AllocMode::accurate> serialisedScrobble = serialiseAsJson(result.value());

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"ABBA"},{"name":"Scorpions"}]},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), string(serialisedScrobble.c_str()));
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_NoAlbum()
{
	afc::ConstStringRef input = u8R"({"scrobble_start_datetime":"2002-01-01T13:12:33+0300",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"_s;

	afc::Optional<ScrobbleInfo> result = ScrobbleInfo::parse(input.begin(), input.end());

	CPPUNIT_ASSERT(result.hasValue());

	afc::FastStringBuffer<char, afc::AllocMode::accurate> serialisedScrobble = serialiseAsJson(result.value());

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T13:12:33+0300",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), string(serialisedScrobble.c_str()));
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_NoAlbumArtists()
{
	for (int i = 0; i < 1000; ++i) {
		afc::ConstStringRef input = u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
				u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
				u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
				u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
				u8R"("album":{"title":"A Night at the Opera"},)"
				u8R"("length":{"amount":207026,"unit":"ms"}}})"_s;

		afc::Optional<ScrobbleInfo> result = ScrobbleInfo::parse(input.begin(), input.end());

		CPPUNIT_ASSERT(result.hasValue());
	}
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_MalformedJson()
{
	afc::ConstStringRef input = u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("length":{"amount":207026,"unit":"ms")"_s;

	afc::Optional<ScrobbleInfo> result = ScrobbleInfo::parse(input.begin(), input.end());

	CPPUNIT_ASSERT(!result.hasValue());
}

void ScrobbleInfoTest::testSerialiseAsJson_ScrobbleInfoWithAllFields()
{
	time_t scrobbleStart = utcTime(2000, 1, 1, 23, 12, 33);
	time_t scrobbleEnd = utcTime(2001, 2, 3, 12, 10, 4);

	ScrobbleInfo scrobbleInfo;
	scrobbleInfo.scrobbleStartTimestamp = scrobbleStart;
	scrobbleInfo.scrobbleEndTimestamp = scrobbleEnd;
	scrobbleInfo.scrobbleDuration = 1001;
	Track &track = scrobbleInfo.track;
	TrackInfoBuilder builder(track);
	builder.setTitle(u8"'39");
	builder.getBuf().reserve(builder.getBuf().size() + u8"Queen\0Jones"_s.size());
	builder.getBuf().append(u8"Queen\0Jones", u8"Queen\0Jones"_s.size());
	builder.artistsProcessed();
	builder.setAlbumTitle(u8"A Night at the Opera");
	builder.getBuf().reserve(builder.getBuf().size() + u8"Scorpions"_s.size());
	builder.getBuf().append(u8"Scorpions", u8"Scorpions"_s.size());
	builder.albumArtistsProcessed();
	builder.build();
	track.setDurationMillis(12);

	afc::FastStringBuffer<char, afc::AllocMode::accurate> result = serialiseAsJson(scrobbleInfo);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2000-01-02T01:42:33+0230",)"
			u8R"("scrobble_end_datetime":"2001-02-03T14:40:04+0230",)"
			u8R"("scrobble_duration":{"amount":1001,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"},{"name":"Jones"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"Scorpions"}]},)"
			u8R"("length":{"amount":12,"unit":"ms"}}})"), string(result.c_str()));
}
