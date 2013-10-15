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
#ifndef TRACKTEST_HPP_
#define TRACKTEST_HPP_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TrackTest : public CppUnit::TestFixture
{
CPPUNIT_TEST_SUITE(TrackTest);
CPPUNIT_TEST(testSerialiseTrack_WithAllFields);
CPPUNIT_TEST(testSerialiseTrack_WithAllFields_StringsContainNonASCIICharacters);
CPPUNIT_TEST(testSerialiseTrack_WithAllFields_TrackNameWithEscapeCharacters);
CPPUNIT_TEST(testSerialiseTrack_WithAllFields_AlbumNameWithEscapeCharacters);
CPPUNIT_TEST(testSerialiseTrack_WithAllFields_ArtistNameWithEscapeCharacters);
CPPUNIT_TEST(testSerialiseTrack_WithNoAlbum);
CPPUNIT_TEST(testSerialiseTrack_MultipleArtists);
CPPUNIT_TEST_SUITE_END();

	void testSerialiseTrack_WithAllFields();
	void testSerialiseTrack_WithAllFields_StringsContainNonASCIICharacters();
	void testSerialiseTrack_WithAllFields_TrackNameWithEscapeCharacters();
	void testSerialiseTrack_WithAllFields_AlbumNameWithEscapeCharacters();
	void testSerialiseTrack_WithAllFields_ArtistNameWithEscapeCharacters();
	void testSerialiseTrack_WithNoAlbum();
	void testSerialiseTrack_MultipleArtists();
};

#endif /* TRACKTEST_HPP_ */
