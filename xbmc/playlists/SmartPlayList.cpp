/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "SmartPlayList.h"
#include "Util.h"
#include "dbwrappers/Database.h"
#include "filesystem/File.h"
#include "filesystem/SmartPlaylistDirectory.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "utils/DatabaseUtils.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/log.h"
#include "utils/StreamDetails.h"
#include "utils/StringUtils.h"
#include "utils/StringValidation.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/XMLUtils.h"

using namespace XFILE;

typedef struct
{
  char string[17];
  Field field;
  CDatabaseQueryRule::FIELD_TYPE type;
  StringValidation::Validator validator;
  bool browseable;
  int localizedString;
} translateField;

static const translateField fields[] = {
  { "none",              FieldNone,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 231 },
  { "filename",          FieldFilename,                CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 561 },
  { "path",              FieldPath,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  573 },
  { "album",             FieldAlbum,                   CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  558 },
  { "albumartist",       FieldAlbumArtist,             CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  566 },
  { "artist",            FieldArtist,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  557 },
  { "tracknumber",       FieldTrackNumber,             CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 554 },
  { "role",              FieldRole,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true, 38033 },
  { "comment",           FieldComment,                 CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 569 },
  { "review",            FieldReview,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 183 },
  { "themes",            FieldThemes,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21895 },
  { "moods",             FieldMoods,                   CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 175 },
  { "styles",            FieldStyles,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 176 },
  { "type",              FieldAlbumType,               CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 564 },
  { "label",             FieldMusicLabel,              CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21899 },
  { "title",             FieldTitle,                   CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  556 },
  { "sorttitle",         FieldSortTitle,               CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 171 },
  { "year",              FieldYear,                    CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  true,  562 },
  { "time",              FieldTime,                    CDatabaseQueryRule::SECONDS_FIELD,  StringValidation::IsTime,             false, 180 },
  { "playcount",         FieldPlaycount,               CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 567 },
  { "lastplayed",        FieldLastPlayed,              CDatabaseQueryRule::DATE_FIELD,     NULL,                                 false, 568 },
  { "inprogress",        FieldInProgress,              CDatabaseQueryRule::BOOLEAN_FIELD,  NULL,                                 false, 575 },
  { "rating",            FieldRating,                  CDatabaseQueryRule::NUMERIC_FIELD,  CSmartPlaylistRule::ValidateRating,   false, 563 },
  { "userrating",        FieldUserRating,              CDatabaseQueryRule::NUMERIC_FIELD,  CSmartPlaylistRule::ValidateMyRating, false, 38018 },
  { "votes",             FieldVotes,                   CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 205 },
  { "top250",            FieldTop250,                  CDatabaseQueryRule::NUMERIC_FIELD,  NULL,                                 false, 13409 },
  { "mpaarating",        FieldMPAA,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 20074 },
  { "dateadded",         FieldDateAdded,               CDatabaseQueryRule::DATE_FIELD,     NULL,                                 false, 570 },
  { "genre",             FieldGenre,                   CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  515 },
  { "plot",              FieldPlot,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 207 },
  { "plotoutline",       FieldPlotOutline,             CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 203 },
  { "tagline",           FieldTagline,                 CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 202 },
  { "set",               FieldSet,                     CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  20457 },
  { "director",          FieldDirector,                CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  20339 },
  { "actor",             FieldActor,                   CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  20337 },
  { "writers",           FieldWriter,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  20417 },
  { "airdate",           FieldAirDate,                 CDatabaseQueryRule::DATE_FIELD,     NULL,                                 false, 20416 },
  { "hastrailer",        FieldTrailer,                 CDatabaseQueryRule::BOOLEAN_FIELD,  NULL,                                 false, 20423 },
  { "studio",            FieldStudio,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  572 },
  { "country",           FieldCountry,                 CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  574 },
  { "tvshow",            FieldTvShowTitle,             CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  20364 },
  { "status",            FieldTvShowStatus,            CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 126 },
  { "season",            FieldSeason,                  CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 20373 },
  { "episode",           FieldEpisodeNumber,           CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 20359 },
  { "numepisodes",       FieldNumberOfEpisodes,        CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 20360 },
  { "numwatched",        FieldNumberOfWatchedEpisodes, CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 21457 },
  { "videoresolution",   FieldVideoResolution,         CDatabaseQueryRule::NUMERIC_FIELD,  NULL,                                 false, 21443 },
  { "videocodec",        FieldVideoCodec,              CDatabaseQueryRule::TEXTIN_FIELD,   NULL,                                 false, 21445 },
  { "videoaspect",       FieldVideoAspectRatio,        CDatabaseQueryRule::NUMERIC_FIELD,  NULL,                                 false, 21374 },
  { "audiochannels",     FieldAudioChannels,           CDatabaseQueryRule::NUMERIC_FIELD,  NULL,                                 false, 21444 },
  { "audiocodec",        FieldAudioCodec,              CDatabaseQueryRule::TEXTIN_FIELD,   NULL,                                 false, 21446 },
  { "audiolanguage",     FieldAudioLanguage,           CDatabaseQueryRule::TEXTIN_FIELD,   NULL,                                 false, 21447 },
  { "audiocount",        FieldAudioCount,              CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 21481 }, 
  { "subtitlecount",     FieldSubtitleCount,           CDatabaseQueryRule::NUMERIC_FIELD,  StringValidation::IsPositiveInteger,  false, 21482 },
  { "subtitlelanguage",  FieldSubtitleLanguage,        CDatabaseQueryRule::TEXTIN_FIELD,   NULL,                                 false, 21448 },
  { "random",            FieldRandom,                  CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 590 },
  { "playlist",          FieldPlaylist,                CDatabaseQueryRule::PLAYLIST_FIELD, NULL,                                 true,  559 },
  { "virtualfolder",     FieldVirtualFolder,           CDatabaseQueryRule::PLAYLIST_FIELD, NULL,                                 true,  614 },
  { "tag",               FieldTag,                     CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 true,  20459 },
  { "instruments",       FieldInstruments,             CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21892 },
  { "biography",         FieldBiography,               CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21887 },
  { "born",              FieldBorn,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21893 },
  { "bandformed",        FieldBandFormed,              CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21894 },
  { "disbanded",         FieldDisbanded,               CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21896 },
  { "died",              FieldDied,                    CDatabaseQueryRule::TEXT_FIELD,     NULL,                                 false, 21897 }
};

static const size_t NUM_FIELDS = sizeof(fields) / sizeof(translateField);

typedef struct
{
  std::string name;
  Field field;
  bool canMix;
  int localizedString;
} group;

static const group groups[] = { { "",           FieldUnknown,   false,    571 },
                                { "none",       FieldNone,      false,    231 },
                                { "sets",       FieldSet,       true,   20434 },
                                { "genres",     FieldGenre,     false,    135 },
                                { "years",      FieldYear,      false,    652 },
                                { "actors",     FieldActor,     false,    344 },
                                { "directors",  FieldDirector,  false,  20348 },
                                { "writers",    FieldWriter,    false,  20418 },
                                { "studios",    FieldStudio,    false,  20388 },
                                { "countries",  FieldCountry,   false,  20451 },
                                { "artists",    FieldArtist,    false,    133 },
                                { "albums",     FieldAlbum,     false,    132 },
                                { "tags",       FieldTag,       false,  20459 },
                              };

static const size_t NUM_GROUPS = sizeof(groups) / sizeof(group);

#define RULE_VALUE_SEPARATOR  " / "

CSmartPlaylistRule::CSmartPlaylistRule()
{
}

int CSmartPlaylistRule::TranslateField(const char *field) const
{
  for (unsigned int i = 0; i < NUM_FIELDS; i++)
    if (StringUtils::EqualsNoCase(field, fields[i].string)) return fields[i].field;
  return FieldNone;
}

std::string CSmartPlaylistRule::TranslateField(int field) const
{
  for (unsigned int i = 0; i < NUM_FIELDS; i++)
    if (field == fields[i].field) return fields[i].string;
  return "none";
}

SortBy CSmartPlaylistRule::TranslateOrder(const char *order)
{
  return SortUtils::SortMethodFromString(order);
}

std::string CSmartPlaylistRule::TranslateOrder(SortBy order)
{
  std::string sortOrder = SortUtils::SortMethodToString(order);
  if (sortOrder.empty())
    return "none";

  return sortOrder;
}

Field CSmartPlaylistRule::TranslateGroup(const char *group)
{
  for (unsigned int i = 0; i < NUM_GROUPS; i++)
  {
    if (StringUtils::EqualsNoCase(group, groups[i].name))
      return groups[i].field;
  }

  return FieldUnknown;
}

std::string CSmartPlaylistRule::TranslateGroup(Field group)
{
  for (unsigned int i = 0; i < NUM_GROUPS; i++)
  {
    if (group == groups[i].field)
      return groups[i].name;
  }

  return "";
}

std::string CSmartPlaylistRule::GetLocalizedField(int field)
{
  for (unsigned int i = 0; i < NUM_FIELDS; i++)
    if (field == fields[i].field) return g_localizeStrings.Get(fields[i].localizedString);
  return g_localizeStrings.Get(16018);
}

CDatabaseQueryRule::FIELD_TYPE CSmartPlaylistRule::GetFieldType(int field) const
{
  for (unsigned int i = 0; i < NUM_FIELDS; i++)
    if (field == fields[i].field) return fields[i].type;
  return TEXT_FIELD;
}

bool CSmartPlaylistRule::IsFieldBrowseable(int field)
{
  for (unsigned int i = 0; i < NUM_FIELDS; i++)
    if (field == fields[i].field) return fields[i].browseable;

  return false;
}

bool CSmartPlaylistRule::Validate(const std::string &input, void *data)
{
  if (data == NULL)
    return true;

  CSmartPlaylistRule *rule = (CSmartPlaylistRule*)data;

  // check if there's a validator for this rule
  StringValidation::Validator validator = NULL;
  for (unsigned int i = 0; i < NUM_FIELDS; i++)
  {
    if (rule->m_field == fields[i].field)
    {
        validator = fields[i].validator;
        break;
    }
  }
  if (validator == NULL)
    return true;

  // split the input into multiple values and validate every value separately
  std::vector<std::string> values = StringUtils::Split(input, RULE_VALUE_SEPARATOR);
  for (std::vector<std::string>::const_iterator it = values.begin(); it != values.end(); ++it)
  {
    if (!validator(*it, data))
      return false;
  }

  return true;
}

bool CSmartPlaylistRule::ValidateRating(const std::string &input, void *data)
{
  char *end = NULL;
  std::string strRating = input;
  StringUtils::Trim(strRating);

  double rating = std::strtod(strRating.c_str(), &end);
  return (end == NULL || *end == '\0') &&
         rating >= 0.0 && rating <= 10.0;
}

bool CSmartPlaylistRule::ValidateMyRating(const std::string &input, void *data)
{
  std::string strRating = input;
  StringUtils::Trim(strRating);

  int rating = atoi(strRating.c_str());
  return StringValidation::IsPositiveInteger(input, data) && rating <= 10;
}

std::vector<Field> CSmartPlaylistRule::GetFields(const std::string &type)
{
  std::vector<Field> fields;
  bool isVideo = false;
  if (type == "mixed")
  {
    fields.push_back(FieldGenre);
    fields.push_back(FieldAlbum);
    fields.push_back(FieldArtist);
    fields.push_back(FieldAlbumArtist);
    fields.push_back(FieldTitle);
    fields.push_back(FieldYear);
    fields.push_back(FieldTime);
    fields.push_back(FieldTrackNumber);
    fields.push_back(FieldFilename);
    fields.push_back(FieldPath);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldLastPlayed);
  }
  else if (type == "songs")
  {
    fields.push_back(FieldGenre);
    fields.push_back(FieldAlbum);
    fields.push_back(FieldArtist);
    fields.push_back(FieldAlbumArtist);
    fields.push_back(FieldTitle);
    fields.push_back(FieldYear);
    fields.push_back(FieldTime);
    fields.push_back(FieldTrackNumber);
    fields.push_back(FieldFilename);
    fields.push_back(FieldPath);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldLastPlayed);
    fields.push_back(FieldRating);
    fields.push_back(FieldUserRating);
    fields.push_back(FieldComment);
    fields.push_back(FieldMoods);
  }
  else if (type == "albums")
  {
    fields.push_back(FieldGenre);
    fields.push_back(FieldAlbum);
    fields.push_back(FieldArtist);        // any artist
    fields.push_back(FieldAlbumArtist);  // album artist
    fields.push_back(FieldYear);
    fields.push_back(FieldReview);
    fields.push_back(FieldThemes);
    fields.push_back(FieldMoods);
    fields.push_back(FieldStyles);
    fields.push_back(FieldAlbumType);
    fields.push_back(FieldMusicLabel);
    fields.push_back(FieldRating);
    fields.push_back(FieldUserRating);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldPath);
  }
  else if (type == "artists")
  {
    fields.push_back(FieldArtist);
    fields.push_back(FieldGenre);
    fields.push_back(FieldMoods);
    fields.push_back(FieldStyles);
    fields.push_back(FieldInstruments);
    fields.push_back(FieldBiography);
    fields.push_back(FieldBorn);
    fields.push_back(FieldBandFormed);
    fields.push_back(FieldDisbanded);
    fields.push_back(FieldDied);
    fields.push_back(FieldRole);
    fields.push_back(FieldPath);
  }
  else if (type == "tvshows")
  {
    fields.push_back(FieldTitle);
    fields.push_back(FieldPlot);
    fields.push_back(FieldTvShowStatus);
    fields.push_back(FieldVotes);
    fields.push_back(FieldRating);
    fields.push_back(FieldUserRating);
    fields.push_back(FieldYear);
    fields.push_back(FieldGenre);
    fields.push_back(FieldDirector);
    fields.push_back(FieldActor);
    fields.push_back(FieldNumberOfEpisodes);
    fields.push_back(FieldNumberOfWatchedEpisodes);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldPath);
    fields.push_back(FieldStudio);
    fields.push_back(FieldMPAA);
    fields.push_back(FieldDateAdded);
    fields.push_back(FieldLastPlayed);
    fields.push_back(FieldInProgress);
    fields.push_back(FieldTag);
  }
  else if (type == "episodes")
  {
    fields.push_back(FieldTitle);
    fields.push_back(FieldTvShowTitle);
    fields.push_back(FieldPlot);
    fields.push_back(FieldVotes);
    fields.push_back(FieldRating);
    fields.push_back(FieldUserRating);
    fields.push_back(FieldTime);
    fields.push_back(FieldWriter);
    fields.push_back(FieldAirDate);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldLastPlayed);
    fields.push_back(FieldInProgress);
    fields.push_back(FieldGenre);
    fields.push_back(FieldYear); // premiered
    fields.push_back(FieldDirector);
    fields.push_back(FieldActor);
    fields.push_back(FieldEpisodeNumber);
    fields.push_back(FieldSeason);
    fields.push_back(FieldFilename);
    fields.push_back(FieldPath);
    fields.push_back(FieldStudio);
    fields.push_back(FieldMPAA);
    fields.push_back(FieldDateAdded);
    fields.push_back(FieldTag);
    isVideo = true;
  }
  else if (type == "movies")
  {
    fields.push_back(FieldTitle);
    fields.push_back(FieldPlot);
    fields.push_back(FieldPlotOutline);
    fields.push_back(FieldTagline);
    fields.push_back(FieldVotes);
    fields.push_back(FieldRating);
    fields.push_back(FieldUserRating);
    fields.push_back(FieldTime);
    fields.push_back(FieldWriter);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldLastPlayed);
    fields.push_back(FieldInProgress);
    fields.push_back(FieldGenre);
    fields.push_back(FieldCountry);
    fields.push_back(FieldYear); // premiered
    fields.push_back(FieldDirector);
    fields.push_back(FieldActor);
    fields.push_back(FieldMPAA);
    fields.push_back(FieldTop250);
    fields.push_back(FieldStudio);
    fields.push_back(FieldTrailer);
    fields.push_back(FieldFilename);
    fields.push_back(FieldPath);
    fields.push_back(FieldSet);
    fields.push_back(FieldTag);
    fields.push_back(FieldDateAdded);
    isVideo = true;
  }
  else if (type == "musicvideos")
  {
    fields.push_back(FieldTitle);
    fields.push_back(FieldGenre);
    fields.push_back(FieldAlbum);
    fields.push_back(FieldYear);
    fields.push_back(FieldArtist);
    fields.push_back(FieldFilename);
    fields.push_back(FieldPath);
    fields.push_back(FieldPlaycount);
    fields.push_back(FieldLastPlayed);
    fields.push_back(FieldRating);
    fields.push_back(FieldUserRating);
    fields.push_back(FieldTime);
    fields.push_back(FieldDirector);
    fields.push_back(FieldStudio);
    fields.push_back(FieldPlot);
    fields.push_back(FieldTag);
    fields.push_back(FieldDateAdded);
    isVideo = true;
  }
  if (isVideo)
  {
    fields.push_back(FieldVideoResolution);
    fields.push_back(FieldAudioChannels);
    fields.push_back(FieldAudioCount);
    fields.push_back(FieldSubtitleCount);
    fields.push_back(FieldVideoCodec);
    fields.push_back(FieldAudioCodec);
    fields.push_back(FieldAudioLanguage);
    fields.push_back(FieldSubtitleLanguage);
    fields.push_back(FieldVideoAspectRatio);
  }
  fields.push_back(FieldPlaylist);
  fields.push_back(FieldVirtualFolder);
  
  return fields;
}

std::vector<SortBy> CSmartPlaylistRule::GetOrders(const std::string &type)
{
  std::vector<SortBy> orders;
  orders.push_back(SortByNone);
  if (type == "mixed")
  {
    orders.push_back(SortByGenre);
    orders.push_back(SortByAlbum);
    orders.push_back(SortByArtist);
    orders.push_back(SortByTitle);
    orders.push_back(SortByYear);
    orders.push_back(SortByTime);
    orders.push_back(SortByTrackNumber);
    orders.push_back(SortByFile);
    orders.push_back(SortByPath);
    orders.push_back(SortByPlaycount);
    orders.push_back(SortByLastPlayed);
  }
  else if (type == "songs")
  {
    orders.push_back(SortByGenre);
    orders.push_back(SortByAlbum);
    orders.push_back(SortByArtist);
    orders.push_back(SortByTitle);
    orders.push_back(SortByYear);
    orders.push_back(SortByTime);
    orders.push_back(SortByTrackNumber);
    orders.push_back(SortByFile);
    orders.push_back(SortByPath);
    orders.push_back(SortByPlaycount);
    orders.push_back(SortByLastPlayed);
    orders.push_back(SortByRating);
    orders.push_back(SortByUserRating);
  }
  else if (type == "albums")
  {
    orders.push_back(SortByGenre);
    orders.push_back(SortByAlbum);
    orders.push_back(SortByArtist);        // any artist
    orders.push_back(SortByYear);
    //orders.push_back(SortByThemes);
    //orders.push_back(SortByMoods);
    //orders.push_back(SortByStyles);
    orders.push_back(SortByAlbumType);
    //orders.push_back(SortByMusicLabel);
    orders.push_back(SortByRating);
    orders.push_back(SortByUserRating);
    orders.push_back(SortByPlaycount);
  }
  else if (type == "artists")
  {
    orders.push_back(SortByArtist);
  }
  else if (type == "tvshows")
  {
    orders.push_back(SortBySortTitle);
    orders.push_back(SortByTvShowStatus);
    orders.push_back(SortByVotes);
    orders.push_back(SortByRating);
    orders.push_back(SortByUserRating);
    orders.push_back(SortByYear);
    orders.push_back(SortByGenre);
    orders.push_back(SortByNumberOfEpisodes);
    orders.push_back(SortByNumberOfWatchedEpisodes);
    //orders.push_back(SortByPlaycount);
    orders.push_back(SortByPath);
    orders.push_back(SortByStudio);
    orders.push_back(SortByMPAA);
    orders.push_back(SortByDateAdded);
    orders.push_back(SortByLastPlayed);
  }
  else if (type == "episodes")
  {
    orders.push_back(SortByTitle);
    orders.push_back(SortByTvShowTitle);
    orders.push_back(SortByVotes);
    orders.push_back(SortByRating);
    orders.push_back(SortByUserRating);
    orders.push_back(SortByTime);
    orders.push_back(SortByPlaycount);
    orders.push_back(SortByLastPlayed);
    orders.push_back(SortByYear); // premiered/dateaired
    orders.push_back(SortByEpisodeNumber);
    orders.push_back(SortBySeason);
    orders.push_back(SortByFile);
    orders.push_back(SortByPath);
    orders.push_back(SortByStudio);
    orders.push_back(SortByMPAA);
    orders.push_back(SortByDateAdded);
  }
  else if (type == "movies")
  {
    orders.push_back(SortBySortTitle);
    orders.push_back(SortByVotes);
    orders.push_back(SortByRating);
    orders.push_back(SortByUserRating);
    orders.push_back(SortByTime);
    orders.push_back(SortByPlaycount);
    orders.push_back(SortByLastPlayed);
    orders.push_back(SortByGenre);
    orders.push_back(SortByCountry);
    orders.push_back(SortByYear); // premiered
    orders.push_back(SortByMPAA);
    orders.push_back(SortByTop250);
    orders.push_back(SortByStudio);
    orders.push_back(SortByFile);
    orders.push_back(SortByPath);
    orders.push_back(SortByDateAdded);
  }
  else if (type == "musicvideos")
  {
    orders.push_back(SortByTitle);
    orders.push_back(SortByGenre);
    orders.push_back(SortByAlbum);
    orders.push_back(SortByYear);
    orders.push_back(SortByArtist);
    orders.push_back(SortByFile);
    orders.push_back(SortByPath);
    orders.push_back(SortByPlaycount);
    orders.push_back(SortByLastPlayed);
    orders.push_back(SortByTime);
    orders.push_back(SortByRating);
    orders.push_back(SortByUserRating);
    orders.push_back(SortByStudio);
    orders.push_back(SortByDateAdded);
  }
  orders.push_back(SortByRandom);
	
  return orders;
}

std::vector<Field> CSmartPlaylistRule::GetGroups(const std::string &type)
{
  std::vector<Field> groups;
  groups.push_back(FieldUnknown);

  if (type == "artists")
    groups.push_back(FieldGenre);
  else if (type == "albums")
    groups.push_back(FieldYear);
  if (type == "movies")
  {
    groups.push_back(FieldNone);
    groups.push_back(FieldSet);
    groups.push_back(FieldGenre);
    groups.push_back(FieldYear);
    groups.push_back(FieldActor);
    groups.push_back(FieldDirector);
    groups.push_back(FieldWriter);
    groups.push_back(FieldStudio);
    groups.push_back(FieldCountry);
    groups.push_back(FieldTag);
  }
  else if (type == "tvshows")
  {
    groups.push_back(FieldGenre);
    groups.push_back(FieldYear);
    groups.push_back(FieldActor);
    groups.push_back(FieldDirector);
    groups.push_back(FieldStudio);
    groups.push_back(FieldTag);
  }
  else if (type == "musicvideos")
  {
    groups.push_back(FieldArtist);
    groups.push_back(FieldAlbum);
    groups.push_back(FieldGenre);
    groups.push_back(FieldYear);
    groups.push_back(FieldDirector);
    groups.push_back(FieldStudio);
    groups.push_back(FieldTag);
  }

  return groups;
}

std::string CSmartPlaylistRule::GetLocalizedGroup(Field group)
{
  for (unsigned int i = 0; i < NUM_GROUPS; i++)
  {
    if (group == groups[i].field)
      return g_localizeStrings.Get(groups[i].localizedString);
  }

  return g_localizeStrings.Get(groups[0].localizedString);
}

bool CSmartPlaylistRule::CanGroupMix(Field group)
{
  for (unsigned int i = 0; i < NUM_GROUPS; i++)
  {
    if (group == groups[i].field)
      return groups[i].canMix;
  }

  return false;
}

std::string CSmartPlaylistRule::GetLocalizedRule() const
{
  return StringUtils::Format("%s %s %s", GetLocalizedField(m_field).c_str(), GetLocalizedOperator(m_operator).c_str(), GetParameter().c_str());
}

std::string CSmartPlaylistRule::GetVideoResolutionQuery(const std::string &parameter) const
{
  std::string retVal(" IN (SELECT DISTINCT idFile FROM streamdetails WHERE iVideoWidth ");
  int iRes = (int)std::strtol(parameter.c_str(), NULL, 10);

  int min, max;
  if (iRes >= 1080)     { min = 1281; max = INT_MAX; }
  else if (iRes >= 720) { min =  961; max = 1280; }
  else if (iRes >= 540) { min =  721; max =  960; }
  else                  { min =    0; max =  720; }

  switch (m_operator)
  {
    case OPERATOR_EQUALS:
      retVal += StringUtils::Format(">= %i AND iVideoWidth <= %i", min, max);
      break;
    case OPERATOR_DOES_NOT_EQUAL:
      retVal += StringUtils::Format("< %i OR iVideoWidth > %i", min, max);
      break;
    case OPERATOR_LESS_THAN:
      retVal += StringUtils::Format("< %i", min);
      break;
    case OPERATOR_GREATER_THAN:
      retVal += StringUtils::Format("> %i", max);
      break;
    default:
      break;
  }

  retVal += ")";
  return retVal;
}

std::string CSmartPlaylistRule::GetBooleanQuery(const std::string &negate, const std::string &strType) const
{
  if (strType == "movies")
  {
    if (m_field == FieldInProgress)
      return "movie_view.idFile " + negate + " IN (SELECT DISTINCT idFile FROM bookmark WHERE type = 1)";
    else if (m_field == FieldTrailer)
      return negate + GetField(m_field, strType) + "!= ''";
  }
  else if (strType == "episodes")
  {
    if (m_field == FieldInProgress)
      return "episode_view.idFile " + negate + " IN (SELECT DISTINCT idFile FROM bookmark WHERE type = 1)";
  }
  else if (strType == "tvshows")
  {
    if (m_field == FieldInProgress)
      return negate + " ("
                          "(tvshow_view.watchedcount > 0 AND tvshow_view.watchedcount < tvshow_view.totalCount) OR "
                          "(tvshow_view.watchedcount = 0 AND EXISTS "
                            "(SELECT 1 FROM episode_view WHERE episode_view.idShow = " + GetField(FieldId, strType) + " AND episode_view.resumeTimeInSeconds > 0)"
                          ")"
                       ")";
  }
  return "";
}

CDatabaseQueryRule::SEARCH_OPERATOR CSmartPlaylistRule::GetOperator(const std::string &strType) const
{
  SEARCH_OPERATOR op = CDatabaseQueryRule::GetOperator(strType);
  if ((strType == "tvshows" || strType == "episodes") && m_field == FieldYear)
  { // special case for premiered which is a date rather than a year
    // TODO: SMARTPLAYLISTS do we really need this, or should we just make this field the premiered date and request a date?
    if (op == OPERATOR_EQUALS)
      op = OPERATOR_CONTAINS;
    else if (op == OPERATOR_DOES_NOT_EQUAL)
      op = OPERATOR_DOES_NOT_CONTAIN;
  }
  return op;
}

std::string CSmartPlaylistRule::FormatParameter(const std::string &operatorString, const std::string &param, const CDatabase &db, const std::string &strType) const
{
  // special-casing
  if (m_field == FieldTime)
  { // translate time to seconds
    std::string seconds = StringUtils::Format("%li", StringUtils::TimeStringToSeconds(param));
    return db.PrepareSQL(operatorString.c_str(), seconds.c_str());
  }
  return CDatabaseQueryRule::FormatParameter(operatorString, param, db, strType);
}

std::string CSmartPlaylistRule::FormatLinkQuery(const char *field, const char *table, const MediaType& mediaType, const std::string& mediaField, const std::string& parameter)
{
  // NOTE: no need for a PrepareSQL here, as the parameter has already been formatted
  return StringUtils::Format(" EXISTS (SELECT 1 FROM %s_link"
                             "         JOIN %s ON %s.%s_id=%s_link.%s_id"
                             "         WHERE %s_link.media_id=%s AND %s.name %s AND %s_link.media_type = '%s')",
                             field, table, table, table, field, table, field, mediaField.c_str(), table, parameter.c_str(), field, mediaType.c_str());
}

std::string CSmartPlaylistRule::FormatWhereClause(const std::string &negate, const std::string &oper, const std::string &param,
                                                 const CDatabase &db, const std::string &strType) const
{
  std::string parameter = FormatParameter(oper, param, db, strType);

  std::string query;
  std::string table;
  if (strType == "songs")
  {
    table = "songview";

    if (m_field == FieldGenre)
      query = negate + " EXISTS (SELECT 1 FROM song_genre, genre WHERE song_genre.idSong = " + GetField(FieldId, strType) + " AND song_genre.idGenre = genre.idGenre AND genre.strGenre" + parameter + ")";
    else if (m_field == FieldArtist)
      query = negate + " EXISTS (SELECT 1 FROM song_artist, artist WHERE song_artist.idSong = " + GetField(FieldId, strType) + " AND song_artist.idArtist = artist.idArtist AND artist.strArtist" + parameter + ")";
    else if (m_field == FieldAlbumArtist)
      query = negate + " EXISTS (SELECT 1 FROM album_artist, artist WHERE album_artist.idAlbum = " + table + ".idAlbum AND album_artist.idArtist = artist.idArtist AND artist.strArtist" + parameter + ")";
    else if (m_field == FieldLastPlayed && (m_operator == OPERATOR_LESS_THAN || m_operator == OPERATOR_BEFORE || m_operator == OPERATOR_NOT_IN_THE_LAST))
      query = GetField(m_field, strType) + " is NULL or " + GetField(m_field, strType) + parameter;
  }
  else if (strType == "albums")
  {
    table = "albumview";

    if (m_field == FieldGenre)
      query = negate + " EXISTS (SELECT 1 FROM song, song_genre, genre WHERE song.idAlbum = " + GetField(FieldId, strType) + " AND song.idSong = song_genre.idSong AND song_genre.idGenre = genre.idGenre AND genre.strGenre" + parameter + ")";
    else if (m_field == FieldArtist)
      query = negate + " EXISTS (SELECT 1 FROM song, song_artist, artist WHERE song.idAlbum = " + GetField(FieldId, strType) + " AND song.idSong = song_artist.idSong AND song_artist.idArtist = artist.idArtist AND artist.strArtist" + parameter + ")";
    else if (m_field == FieldAlbumArtist)
      query = negate + " EXISTS (SELECT 1 FROM album_artist, artist WHERE album_artist.idAlbum = " + GetField(FieldId, strType) + " AND album_artist.idArtist = artist.idArtist AND artist.strArtist" + parameter + ")";
    else if (m_field == FieldPath)
      query = negate + " EXISTS (SELECT 1 FROM song JOIN path on song.idpath = path.idpath WHERE song.idAlbum = " + GetField(FieldId, strType) + " AND path.strPath" + parameter + ")";
  }
  else if (strType == "artists")
  {
    table = "artistview";

    if (m_field == FieldGenre)
    {
      query = negate + " (EXISTS (SELECT DISTINCT song_artist.idArtist FROM song_artist, song_genre, genre WHERE song_artist.idArtist = " + GetField(FieldId, strType) + " AND song_artist.idRole = 1 AND song_artist.idSong = song_genre.idSong AND song_genre.idGenre = genre.idGenre AND genre.strGenre" + parameter + ")";
      query += " OR ";
      query += "EXISTS (SELECT DISTINCT album_artist.idArtist FROM album_artist, album_genre, genre WHERE album_artist.idArtist = " + GetField(FieldId, strType) + " AND album_artist.idAlbum = album_genre.idAlbum AND album_genre.idGenre = genre.idGenre AND genre.strGenre" + parameter + "))";
    }
    else if (m_field == FieldRole)
    {
      query = negate + " (EXISTS (SELECT DISTINCT song_artist.idArtist FROM song_artist, role WHERE song_artist.idArtist = " + GetField(FieldId, strType) + " AND song_artist.idRole = role.idRole AND role.strRole" + parameter + "))";
    }
    else if (m_field == FieldPath)
    {
      query = negate + " (EXISTS (SELECT DISTINCT song_artist.idArtist FROM song_artist JOIN song ON song.idSong = song_artist.idSong JOIN path ON song.idpath = path.idpath ";
      query += "WHERE song_artist.idArtist = " + GetField(FieldId, strType) + " AND song_artist.idRole = 1 AND path.strPath" + parameter + "))";
    }
  }
  else if (strType == "parameter_only")
  {
    query = parameter;
  }

  else if (strType == "roles")
  {
    query = negate + " role.strRole" + parameter;
  }
  else if (strType == "movies")
  {
    table = "movie_view";

    if (m_field == FieldGenre)
      query = negate + FormatLinkQuery("genre", "genre", MediaTypeMovie, GetField(FieldId, strType), parameter);
    else if (m_field == FieldDirector)
      query = negate + FormatLinkQuery("director", "actor", MediaTypeMovie, GetField(FieldId, strType), parameter);
    else if (m_field == FieldActor)
      query = negate + FormatLinkQuery("actor", "actor", MediaTypeMovie, GetField(FieldId, strType), parameter);
    else if (m_field == FieldWriter)
      query = negate + FormatLinkQuery("writer", "actor", MediaTypeMovie, GetField(FieldId, strType), parameter);
    else if (m_field == FieldStudio)
      query = negate + FormatLinkQuery("studio", "studio", MediaTypeMovie, GetField(FieldId, strType), parameter);
    else if (m_field == FieldCountry)
      query = negate + FormatLinkQuery("country", "country", MediaTypeMovie, GetField(FieldId, strType), parameter);
    else if ((m_field == FieldLastPlayed || m_field == FieldDateAdded) && (m_operator == OPERATOR_LESS_THAN || m_operator == OPERATOR_BEFORE || m_operator == OPERATOR_NOT_IN_THE_LAST))
      query = GetField(m_field, strType) + " IS NULL OR " + GetField(m_field, strType) + parameter;
    else if (m_field == FieldTag)
      query = negate + FormatLinkQuery("tag", "tag", MediaTypeMovie, GetField(FieldId, strType), parameter);
  }
  else if (strType == "musicvideos")
  {
    table = "musicvideo_view";

    if (m_field == FieldGenre)
      query = negate + FormatLinkQuery("genre", "genre", MediaTypeMusicVideo, GetField(FieldId, strType), parameter);
    else if (m_field == FieldArtist || m_field == FieldAlbumArtist)
      query = negate + FormatLinkQuery("actor", "actor", MediaTypeMusicVideo, GetField(FieldId, strType), parameter);
    else if (m_field == FieldStudio)
      query = negate + FormatLinkQuery("studio", "studio", MediaTypeMusicVideo, GetField(FieldId, strType), parameter);
    else if (m_field == FieldDirector)
      query = negate + FormatLinkQuery("director", "actor", MediaTypeMusicVideo, GetField(FieldId, strType), parameter);
    else if ((m_field == FieldLastPlayed || m_field == FieldDateAdded) && (m_operator == OPERATOR_LESS_THAN || m_operator == OPERATOR_BEFORE || m_operator == OPERATOR_NOT_IN_THE_LAST))
      query = GetField(m_field, strType) + " IS NULL OR " + GetField(m_field, strType) + parameter;
    else if (m_field == FieldTag)
      query = negate + FormatLinkQuery("tag", "tag", MediaTypeMusicVideo, GetField(FieldId, strType), parameter);
  }
  else if (strType == "tvshows")
  {
    table = "tvshow_view";

    if (m_field == FieldGenre)
      query = negate + FormatLinkQuery("genre", "genre", MediaTypeTvShow, GetField(FieldId, strType), parameter);
    else if (m_field == FieldDirector)
      query = negate + FormatLinkQuery("director", "actor", MediaTypeTvShow, GetField(FieldId, strType), parameter);
    else if (m_field == FieldActor)
      query = negate + FormatLinkQuery("actor", "actor", MediaTypeTvShow, GetField(FieldId, strType), parameter);
    else if (m_field == FieldStudio)
      query = negate + FormatLinkQuery("studio", "studio", MediaTypeTvShow, GetField(FieldId, strType), parameter);
    else if (m_field == FieldMPAA)
      query = negate + " (" + GetField(m_field, strType) + parameter + ")";
    else if ((m_field == FieldLastPlayed || m_field == FieldDateAdded) && (m_operator == OPERATOR_LESS_THAN || m_operator == OPERATOR_BEFORE || m_operator == OPERATOR_NOT_IN_THE_LAST))
      query = GetField(m_field, strType) + " IS NULL OR " + GetField(m_field, strType) + parameter;
    else if (m_field == FieldPlaycount)
      query = "CASE WHEN COALESCE(" + GetField(FieldNumberOfEpisodes, strType) + " - " + GetField(FieldNumberOfWatchedEpisodes, strType) + ", 0) > 0 THEN 0 ELSE 1 END " + parameter;
    else if (m_field == FieldTag)
      query = negate + FormatLinkQuery("tag", "tag", MediaTypeTvShow, GetField(FieldId, strType), parameter);
  }
  else if (strType == "episodes")
  {
    table = "episode_view";

    if (m_field == FieldGenre)
      query = negate + FormatLinkQuery("genre", "genre", MediaTypeTvShow, (table + ".idShow").c_str(), parameter);
    else if (m_field == FieldTag)
      query = negate + FormatLinkQuery("tag", "tag", MediaTypeTvShow, (table + ".idShow").c_str(), parameter);
    else if (m_field == FieldDirector)
      query = negate + FormatLinkQuery("director", "actor", MediaTypeEpisode, GetField(FieldId, strType), parameter);
    else if (m_field == FieldActor)
      query = negate + FormatLinkQuery("actor", "actor", MediaTypeEpisode, GetField(FieldId, strType), parameter);
    else if (m_field == FieldWriter)
      query = negate + FormatLinkQuery("writer", "actor", MediaTypeEpisode, GetField(FieldId, strType), parameter);
    else if ((m_field == FieldLastPlayed || m_field == FieldDateAdded) && (m_operator == OPERATOR_LESS_THAN || m_operator == OPERATOR_BEFORE || m_operator == OPERATOR_NOT_IN_THE_LAST))
      query = GetField(m_field, strType) + " IS NULL OR " + GetField(m_field, strType) + parameter;
    else if (m_field == FieldStudio)
      query = negate + FormatLinkQuery("studio", "studio", MediaTypeTvShow, (table + ".idShow").c_str(), parameter);
    else if (m_field == FieldMPAA)
      query = negate + " (" + GetField(m_field, strType) +  parameter + ")";
  }
  if (m_field == FieldVideoResolution)
    query = table + ".idFile" + negate + GetVideoResolutionQuery(param);
  else if (m_field == FieldAudioChannels)
    query = negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND iAudioChannels " + parameter + ")";
  else if (m_field == FieldVideoCodec)
    query = negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND strVideoCodec " + parameter + ")";
  else if (m_field == FieldAudioCodec)
    query = negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND strAudioCodec " + parameter + ")";
  else if (m_field == FieldAudioLanguage)
    query = negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND strAudioLanguage " + parameter + ")";
  else if (m_field == FieldSubtitleLanguage)
    query = negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND strSubtitleLanguage " + parameter + ")";
  else if (m_field == FieldVideoAspectRatio)
    query = negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND fVideoAspect " + parameter + ")";
  else if (m_field == FieldAudioCount)
    query = db.PrepareSQL(negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND streamdetails.iStreamtype = %i GROUP BY streamdetails.idFile HAVING COUNT(streamdetails.iStreamType) " + parameter + ")",CStreamDetail::AUDIO);
  else if (m_field == FieldSubtitleCount)
    query = db.PrepareSQL(negate + " EXISTS (SELECT 1 FROM streamdetails WHERE streamdetails.idFile = " + table + ".idFile AND streamdetails.iStreamType = %i GROUP BY streamdetails.idFile HAVING COUNT(streamdetails.iStreamType) " + parameter + ")",CStreamDetail::SUBTITLE);
  if (m_field == FieldPlaycount && strType != "songs" && strType != "albums" && strType != "tvshows")
  { // playcount IS stored as NULL OR number IN video db
    if ((m_operator == OPERATOR_EQUALS && param == "0") ||
        (m_operator == OPERATOR_DOES_NOT_EQUAL && param != "0") ||
        (m_operator == OPERATOR_LESS_THAN))
    {
      std::string field = GetField(FieldPlaycount, strType);
      query = field + " IS NULL OR " + field + parameter;
    }
  }
  if (query.empty())
    query = CDatabaseQueryRule::FormatWhereClause(negate, oper, param, db, strType);
  return query;
}

std::string CSmartPlaylistRule::GetField(int field, const std::string &type) const
{
  if (field >= FieldUnknown && field < FieldMax)
    return DatabaseUtils::GetField((Field)field, CMediaTypes::FromString(type), DatabaseQueryPartWhere);
  return "";
}

std::string CSmartPlaylistRuleCombination::GetWhereClause(const CDatabase &db, const std::string& strType, std::set<std::string> &referencedPlaylists) const
{
  std::string rule;
  
  // translate the combinations into SQL
  for (CDatabaseQueryRuleCombinations::const_iterator it = m_combinations.begin(); it != m_combinations.end(); ++it)
  {
    if (it != m_combinations.begin())
      rule += m_type == CombinationAnd ? " AND " : " OR ";
    std::shared_ptr<CSmartPlaylistRuleCombination> combo = std::static_pointer_cast<CSmartPlaylistRuleCombination>(*it);
    if (combo)
      rule += "(" + combo->GetWhereClause(db, strType, referencedPlaylists) + ")";
  }

  // translate the rules into SQL
  for (CDatabaseQueryRules::const_iterator it = m_rules.begin(); it != m_rules.end(); ++it)
  {
    // don't include playlists that are meant to be displayed
    // as a virtual folders in the SQL WHERE clause
    if ((*it)->m_field == FieldVirtualFolder)
      continue;
    
    //Skip those rules that need a combined SQL clause
    //e.g. "artists" list genre, path or role rules that are related to song data
    //They will be dealt with separately
    if (IsComplexRule(strType, (*it)->m_field))
      continue;

    if (!rule.empty())
      rule += m_type == CombinationAnd ? " AND " : " OR ";
    rule += "(";
    std::string currentRule;
    if ((*it)->m_field == FieldPlaylist)
    {
      std::string playlistFile = CSmartPlaylistDirectory::GetPlaylistByName((*it)->m_parameter.at(0), strType);
      if (!playlistFile.empty() && referencedPlaylists.find(playlistFile) == referencedPlaylists.end())
      {
        referencedPlaylists.insert(playlistFile);
        CSmartPlaylist playlist;
        if (playlist.Load(playlistFile))
        {
          std::string playlistQuery;
          // only playlists of same type will be part of the query
          if (playlist.GetType() == strType || (playlist.GetType() == "mixed" && (strType == "songs" || strType == "musicvideos")) || playlist.GetType().empty())
          {
            playlist.SetType(strType);
            playlistQuery = playlist.GetWhereClause(db, referencedPlaylists);
          }
          if (playlist.GetType() == strType)
          {
            if ((*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_EQUAL)
              currentRule = StringUtils::Format("NOT (%s)", playlistQuery.c_str());
            else
              currentRule = playlistQuery;
          }
        }
      }
    }
    else
      currentRule = (*it)->GetWhereClause(db, strType);
    // if we don't get a rule, we add '1' or '0' so the query is still valid and doesn't fail
    if (currentRule.empty())
      currentRule = m_type == CombinationAnd ? "'1'" : "'0'";
    rule += currentRule;
    rule += ")";
  }
  
  //Now get clause for any complex/interrealed rules
  
  std::string complexClause = GetArtistSongClause(db, strType);
  if (!complexClause.empty())
  {
    if (!rule.empty())
      rule += m_type == CombinationAnd ? " AND " : " OR ";
    rule += complexClause;
  }

  //Now get any common clause that applies to all rules
  std::string commonClause = GetCommonClause(db, strType);
  if (!commonClause.empty())
  {
    if (!rule.empty())
      rule = "(" + rule + ") AND ";
    rule += commonClause;
  }
  return rule;
}

void CSmartPlaylistRuleCombination::GetVirtualFolders(const std::string& strType, std::vector<std::string> &virtualFolders) const
{
  for (CDatabaseQueryRuleCombinations::const_iterator it = m_combinations.begin(); it != m_combinations.end(); ++it)
  {
    std::shared_ptr<CSmartPlaylistRuleCombination> combo = std::static_pointer_cast<CSmartPlaylistRuleCombination>(*it);
    if (combo)
      combo->GetVirtualFolders(strType, virtualFolders);
  }

  for (CDatabaseQueryRules::const_iterator it = m_rules.begin(); it != m_rules.end(); ++it)
  {
    if (((*it)->m_field != FieldVirtualFolder && (*it)->m_field != FieldPlaylist) || (*it)->m_operator != CDatabaseQueryRule::OPERATOR_EQUALS)
      continue;

    std::string playlistFile = CSmartPlaylistDirectory::GetPlaylistByName((*it)->m_parameter.at(0), strType);
    if (playlistFile.empty())
      continue;

    if ((*it)->m_field == FieldVirtualFolder)
      virtualFolders.push_back(playlistFile);
    else
    {
      // look for any virtual folders in the expanded playlists
      CSmartPlaylist playlist;
      if (!playlist.Load(playlistFile))
        continue;

      if (CSmartPlaylist::CheckTypeCompatibility(playlist.GetType(), strType))
        playlist.GetVirtualFolders(virtualFolders);
    }
  }
}

void CSmartPlaylistRuleCombination::AddRule(const CSmartPlaylistRule &rule)
{
  std::shared_ptr<CSmartPlaylistRule> ptr(new CSmartPlaylistRule(rule));
  m_rules.push_back(ptr);
}

bool CSmartPlaylistRuleCombination::IsComplexRule(const std::string& strType, const int field) const
{
  // The SQL clauses for some rules are interdependent and can not be implemented in isolation
  // e.g. the song related rules on "artists" playlists involving genre, path and/or role fields
  return strType == "artists" && (field == FieldGenre || field == FieldPath || field == FieldRole);
}

std::string CSmartPlaylistRuleCombination::GetCommonClause(const CDatabase &db, const std::string& strType) const
{ 
  // Any where clauses to be applied collectively in addition to some rules combinations
  // But also rule dependent, so done here rather than in GetFilter
  if (strType != "artists" || m_rules.empty())
    return std::string();

  // If there are any role rules, no clause
  for (CDatabaseQueryRules::const_iterator it = m_rules.begin(); it != m_rules.end(); ++it)
  {
    if ((*it)->m_field == FieldRole)
      return std::string();
  }

  // ###Needs to be a rule rather than system setting
  bool albumartistsonly = !CSettings::GetInstance().GetBool(CSettings::SETTING_MUSICLIBRARY_SHOWCOMPILATIONARTISTS);

  // A single non-negated genre rule and not albumartistsonly, no clause
  if (m_rules.size() == 1 && !albumartistsonly)
  {
    CDatabaseQueryRules::const_iterator it = m_rules.begin();
    if ((*it)->m_field == FieldGenre)
    {
      bool negated = ((*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_CONTAIN ||
        (*it)->m_operator == CDatabaseQueryRule::OPERATOR_FALSE ||
        (*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_EQUAL);
      if (!negated)
        return std::string();
    }
  }

  // Build commmon clause
  std::string strSQL;
  strSQL = "artistview.idArtist IN (SELECT album_artist.idArtist FROM album_artist)"; // Includes compliation albums hence "Various artists"    
  if (!albumartistsonly)
    strSQL += " OR artistview.idArtist IN (SELECT song_artist.idArtist FROM song_artist WHERE song_artist.idRole = 1)";
  strSQL = "(" + strSQL + ")";
  return strSQL;
}

std::string CSmartPlaylistRuleCombination::GetArtistSongClause(const CDatabase &db, const std::string& strType) const
{
  /* The SQL clauses for some combinations of rules on "artists" playlists are interdependent and can not be implemented
  in isolation. In particular genre, path and/or role rules apply to songs the artist contributes to, rather than just 
  being fields from the artist table. The AND combination needs to find the songs which satisfy *all* the rules and then
  the artists, not just the artist with a song that satisfies one rule,  and another song that satisfies another rule.
  */
  if (strType != "artists")
    return std::string();

  CSmartPlaylistWhereClause roleWhere;
  roleWhere.SetCombination(m_type == CombinationAnd);
  roleWhere.SetORWhere("EXISTS(SELECT 1 FROM song_artist as sa1, role as r1 WHERE sa1.idArtist = artistview.idartist AND sa1.idRole = r1.idRole AND r1.strRole");
  roleWhere.SetFirstWhere("sa1.idArtist = artistview.idArtist AND ");
  roleWhere.SetRuleFromFormat("song_artist as sa%i, role as r%i");
  roleWhere.SetRuleWhereFormat2("sa%i.idRole = r%i.idRole AND r%i.strRole");
  roleWhere.SetRuleWhereFormat3(" AND sa%i.idSong = sa1.idSong AND sa%i.idArtist = sa1.idArtist AND ");

  CSmartPlaylistWhereClause genreWhere;
  genreWhere.SetCombination(m_type == CombinationAnd);
  genreWhere.SetORWhere("EXISTS(SELECT 1 FROM song_artist as sa1, song_genre as sg1, genre as g1 "
                       "WHERE sa1.idArtist = artistview.idArtist AND sa1.idRole = 1 "
                       "AND sg1.idSong = sa1.idSong AND sg1.idGenre = g1.idGenre AND g1.strGenre");
  genreWhere.SetFirstWhere("");
  genreWhere.SetRuleFromFormat("song_genre as sg%i, genre as g%i");
  genreWhere.SetRuleWhereFormat2("sg%i.idSong = sa1.idSong AND sg%i.idGenre = g%i.idGenre AND g%i.strGenre");
  genreWhere.SetRuleWhereFormat3(" AND ");

  CSmartPlaylistWhereClause pathWhere;
  pathWhere.SetCombination(m_type == CombinationAnd);
  pathWhere.SetORWhere("EXISTS (SELECT 1 FROM song_artist as sa1, song as song1, path as path1 "
                       "WHERE sa1.idArtist = artistview.idArtist AND sa1.idRole = 1 "
                       "AND song1.idSong = sa1.idSong AND path1.idpath = song1.idpath AND path1.strPath");
  pathWhere.SetFirstWhere("");
  pathWhere.SetRuleFromFormat("song as song%i, path as path%i");
  pathWhere.SetRuleWhereFormat2("song%i.idSong = sa1.idSong AND path%i.idpath = song%i.idpath AND path%i.strPath");
  pathWhere.SetRuleWhereFormat3(" AND ");

  // Translate the interdependent rules into SQL
  for (CDatabaseQueryRules::const_iterator it = m_rules.begin(); it != m_rules.end(); ++it)
  {
    if (!IsComplexRule(strType, (*it)->m_field))
      continue;
    
    std::string negate;
    if ((*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_CONTAIN ||
        (*it)->m_operator == CDatabaseQueryRule::OPERATOR_FALSE ||
        (*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_EQUAL)
      negate = " NOT";
    std::string parameter = (*it)->GetWhereClause(db, "parameter_only"); // Clause is formatted parameter in brackets
    StringUtils::TrimLeft(parameter, "(");
    StringUtils::TrimRight(parameter, ")");

    if ((*it)->m_field == FieldRole)
      roleWhere.AddRule(negate, parameter);
    else if ((*it)->m_field == FieldGenre)
      genreWhere.AddRule(negate, parameter);
    else if ((*it)->m_field == FieldPath)
      pathWhere.AddRule(negate, parameter);
  }

  std::string currentRule;
  if (!roleWhere.IsEmpty() || !genreWhere.IsEmpty() || !pathWhere.IsEmpty())
  {   
    if (m_type != CombinationAnd)
    {
      currentRule = roleWhere.GetWhereClause();
      if (!currentRule.empty() && !genreWhere.IsEmpty())
        currentRule += " OR ";
      currentRule += genreWhere.GetWhereClause();
      if (!currentRule.empty() && !pathWhere.IsEmpty())
        currentRule += " OR ";
      currentRule += pathWhere.GetWhereClause();
      if (roleWhere.IsEmpty())
      { //Add album_artist clauses, created from song_artist SQL
        std::string albumClause = currentRule;
        StringUtils::Replace(albumClause, "song_genre", "album_genre");
        StringUtils::Replace(albumClause, "idSong", "idAlbum");
        StringUtils::Replace(albumClause, "song_artist", "album_artist");
        StringUtils::Replace(albumClause, " AND sa1.idRole = 1", "");
        currentRule += " OR " + albumClause;
      }
    }
    else
    {
      if (!roleWhere.IsEmpty())
        currentRule = roleWhere.GetWhereClause();
      else  //No role rules, role = 1
        currentRule = " EXISTS (SELECT 1 FROM song_artist as sa1 WHERE sa1.idArtist = artistview.idArtist AND sa1.idRole = 1";
      if (!genreWhere.IsEmpty())
        currentRule += " AND " + genreWhere.GetWhereClause() + ")";
      if (!pathWhere.IsEmpty())
        currentRule += " AND " + pathWhere.GetWhereClause() + ")";
      currentRule += ")";
      if (roleWhere.IsEmpty())
      {//album_genre and path stuff. There must be one or both of these or not be here.
       // Make a album_genre clause to join with itself out of the song_genre ones      
        std::string albumRule = "EXISTS (SELECT 1 FROM album_artist WHERE album_artist.idArtist = artistview.idArtist";
        if (!genreWhere.IsEmpty())
        {          
          std::string albumGenre = genreWhere.GetWhereClause();
          StringUtils::Replace(albumGenre, "song_genre", "album_genre");
          StringUtils::Replace(albumGenre, "idSong", "idAlbum");
          StringUtils::Replace(albumGenre, "sa1.", "album_artist.");
          albumRule += " AND " + albumGenre + ")";
        }
        if (!pathWhere.IsEmpty())
        {          
          std::string albumPath = pathWhere.GetWhereClause();
          StringUtils::Replace(albumPath, "song_genre", "album_genre");
          StringUtils::Replace(albumPath, "idSong", "idAlbum");
          StringUtils::Replace(albumPath, "sa1.", "album_artist.");
          albumRule += " AND " + albumPath + ")";
        }
        albumRule += ")";
        currentRule += " OR (" + albumRule + ")";
      }
    }
  }
  if (!currentRule.empty())
    currentRule = "(" + currentRule + ")";
  return currentRule;
}

std::string CSmartPlaylistRuleCombination::GetChildClause(const CDatabase &db, const std::string& strType) const
{
  //Applying artists genre, path and/or role song level rules to albums for drilling down
  if (strType != "albums" || m_rules.empty())
    return std::string();
  
  CSmartPlaylistWhereClause roleWhere;
  roleWhere.SetCombination(m_type == CombinationAnd);
  roleWhere.SetORWhere("EXISTS(SELECT 1 FROM song, song_artist as sa1, role as r1 "
      "WHERE song.idAlbum = albumview.idAlbum AND sa1.idArtist = artistview.idartist "
      "AND song.idSong = sa1.idSong AND sa1.idRole = r1.idRole AND r1.strRole");
  roleWhere.SetFirstFrom("song, ");
  roleWhere.SetFirstWhere("song.idAlbum = albumview.idAlbum AND sa1.idArtist = artistview.idArtist AND song.idSong = sa1.idSong AND ");
  roleWhere.SetRuleFromFormat("song_artist as sa%i, role as r%i");
  roleWhere.SetRuleWhereFormat2("sa%i.idRole = r%i.idRole AND r%i.strRole");
  roleWhere.SetRuleWhereFormat3(" AND sa%i.idSong = sa1.idSong AND sa%i.idArtist = sa1.idArtist AND ");

  CSmartPlaylistWhereClause genreWhere;
  genreWhere.SetCombination(m_type == CombinationAnd);
  genreWhere.SetORWhere("EXISTS(SELECT 1 FROM song, song_artist as sa1, song_genre as sg1, genre as g1 "
    "WHERE song.idAlbum = albumview.idAlbum AND sa1.idArtist = artistview.idArtist AND sa1.idRole = 1 "
    "AND song.idSong = sa1.idSong AND sg1.idSong = sa1.idSong AND sg1.idGenre = g1.idGenre AND g1.strGenre");
  genreWhere.SetFirstFrom("");
  genreWhere.SetFirstWhere("");
  genreWhere.SetRuleFromFormat("song_genre as sg%i, genre as g%i");
  genreWhere.SetRuleWhereFormat2("sg%i.idSong = sa1.idSong AND sg%i.idGenre = g%i.idGenre AND g%i.strGenre");
  genreWhere.SetRuleWhereFormat3(" AND ");

  CSmartPlaylistWhereClause pathWhere;
  pathWhere.SetCombination(m_type == CombinationAnd);
  pathWhere.SetORWhere("EXISTS (SELECT 1 FROM song_artist as sa1, song as song1, path as path1 "
    "WHERE song1.idAlbum = albumview.idAlbum AND sa1.idArtist = artistview.idArtist AND sa1.idRole = 1 "
    "AND song1.idSong = sa1.idSong AND path1.idpath = song1.idpath AND path1.strPath");
  pathWhere.SetFirstFrom("");
  pathWhere.SetFirstWhere("");
  pathWhere.SetRuleFromFormat("song as song%i, path as path%i");
  pathWhere.SetRuleWhereFormat2("song%i.idSong = sa1.idSong AND path%i.idpath = song%i.idpath AND path%i.strPath");
  pathWhere.SetRuleWhereFormat3(" AND ");

  // Translate the interreated rules into SQL
  for (CDatabaseQueryRules::const_iterator it = m_rules.begin(); it != m_rules.end(); ++it)
  {
    if ((*it)->m_field != FieldGenre && (*it)->m_field != FieldPath && (*it)->m_field != FieldRole)
      continue;
    
    std::string negate;
    if ((*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_CONTAIN ||
        (*it)->m_operator == CDatabaseQueryRule::OPERATOR_FALSE ||
        (*it)->m_operator == CDatabaseQueryRule::OPERATOR_DOES_NOT_EQUAL)
        negate = " NOT";
    std::string parameter = (*it)->GetWhereClause(db, "parameter_only"); // Clause is formatted parameter in brackets
    StringUtils::TrimLeft(parameter, "(");
    StringUtils::TrimRight(parameter, ")");

    if ((*it)->m_field == FieldRole)
      roleWhere.AddRule(negate, parameter);
    else if ((*it)->m_field == FieldGenre)
      genreWhere.AddRule(negate, parameter);
    else if ((*it)->m_field == FieldPath)
      pathWhere.AddRule(negate, parameter); 
  }

  std::string currentRule;
  if (!roleWhere.IsEmpty() || !genreWhere.IsEmpty() || !pathWhere.IsEmpty())
  {
    if (m_type != CombinationAnd)
    {
      currentRule = roleWhere.GetWhereClause();
      if (!currentRule.empty() && !genreWhere.IsEmpty())
        currentRule += " OR ";
      currentRule += genreWhere.GetWhereClause();
      if (!currentRule.empty() && !pathWhere.IsEmpty())
        currentRule += " OR ";
      currentRule += pathWhere.GetWhereClause();
      if (roleWhere.IsEmpty())
      { //Add album_artist clauses, created from song_artist SQL
        std::string albumClause = currentRule;
        StringUtils::Replace(albumClause, "song_artist", "album_artist");
        StringUtils::Replace(albumClause, " AND sa1.idRole = 1", "");

        StringUtils::Replace(albumClause, "FROM song, ", "FROM ");
        StringUtils::Replace(albumClause, "song_genre", "album_genre");
        StringUtils::Replace(albumClause, "song.idAlbum = albumview.idAlbum", "sa1.idAlbum = albumview.idAlbum");
        StringUtils::Replace(albumClause, " AND song.idSong = sa1.idSong", "");
        StringUtils::Replace(albumClause, "sg1.idSong = sa1.idSong", "sg1.idAlbum = sa1.idAlbum");

        StringUtils::Replace(albumClause, "song1.idSong = sa1.idSong", "song1.idAlbum = sa1.idAlbum");
        currentRule += " OR " + albumClause;
      }
    }
    else
    {
      if (!roleWhere.IsEmpty())
        currentRule = roleWhere.GetWhereClause();
      else  //No role rules, role = 1
        currentRule = " EXISTS (SELECT 1 FROM song, song_artist as sa1 WHERE song.idAlbum = albumview.idAlbum AND sa1.idArtist = artistview.idArtist AND sa1.idRole = 1 AND song.idSong = sa1.idSong";
      if (!genreWhere.IsEmpty())
        currentRule += " AND " + genreWhere.GetWhereClause() + ")";
      if (!pathWhere.IsEmpty())
        currentRule += " AND " + pathWhere.GetWhereClause() + ")";
      currentRule += ")";
      if (roleWhere.IsEmpty())
      {//album_genre and path stuff. There must be one or both of these or not be here.
       // Make a album_genre clause to join with itself out of the song_genre ones      
        std::string albumRule = "EXISTS (SELECT 1 FROM album_artist WHERE album_artist.idAlbum = albumview.idAlbum AND album_artist.idArtist = artistview.idArtist";
        if (!genreWhere.IsEmpty())
        {
          std::string albumGenre = genreWhere.GetWhereClause();
          StringUtils::Replace(albumGenre, "song_genre", "album_genre");
          StringUtils::Replace(albumGenre, "idSong", "idAlbum");
          StringUtils::Replace(albumGenre, "sa1.", "album_artist.");
          albumRule += " AND " + albumGenre + ")";
        }
        if (!pathWhere.IsEmpty())
        {
          std::string albumPath = pathWhere.GetWhereClause();
          StringUtils::Replace(albumPath, "song_genre", "album_genre");
          StringUtils::Replace(albumPath, "idSong", "idAlbum");
          StringUtils::Replace(albumPath, "sa1.", "album_artist.");
          albumRule += " AND " + albumPath + ")";
        }
        albumRule += ")";
        currentRule += " OR (" + albumRule + ")";
      }
    }
  }
  if (!currentRule.empty())
    currentRule = "(" + currentRule + ")";
  return currentRule;
}


bool CSmartPlaylistRuleCombination::SongRulesOnly()
{
  // Reduce rules to just those that are based on song properties
  auto rulecount = m_rules.size();
  for (CDatabaseQueryRules::const_iterator it = m_rules.begin(); it != m_rules.end();)
  {
    int field = (*it)->m_field;
    if (field != FieldGenre && field != FieldRole && field != FieldPath)
      it = m_rules.erase(it);
    else
      ++it;
  }
  return rulecount > m_rules.size();
}

std::string CSmartPlaylistWhereClause::GetWhereClause() const
{
  return m_strFrom + m_strWhere;
}

bool CSmartPlaylistWhereClause::IsEmpty()
{
  return m_count == 0;
}

void CSmartPlaylistWhereClause::AddRule(const std::string negate, const std::string strParameter)
{
  /* Add to where clause for rule parameter.
     For AND combinations this builds a where clause that is a series of joins of a table with itself
     like this:     
     EXISTS (SELECT 1 FROM FirstForm + FromFmt(1) + FromFmt(2)... + FromFmt(i)...
     WHERE FirstWhere + WhereFmt2(1) + 
     WhereFmt3(2) + WhereFmt2(2) + 
     ...
     WhereFmt3(i)+ WhereFmt2(i)... 

     Final bracket is missing to allow other subqueries to be nested within it later.

     For OR combinations it simply and adds a clause for the parameter.
  */

  if (strParameter.empty())
    return;

  m_count++;
  if (!m_combinationAND)
  {
    if (!m_strWhere.empty())
      m_strWhere += " OR ";
    m_strWhere += negate + m_strOrWhere + strParameter + ")";
  }
  else
  {
    std::string clausefmt;
    if (m_strFrom.empty())
    {
      m_strFrom = negate + " EXISTS (SELECT 1 FROM " + m_strFirstFrom;
      clausefmt = " WHERE " + m_strFirstWhere + m_strRuleWhereFmt2;
    }
    else
    {
      m_strFrom += ", ";
      clausefmt = m_strRuleWhereFmt3 + m_strRuleWhereFmt2;
    }
    m_strFrom += m_strRuleFromFmt;
    StringUtils::Replace(m_strFrom, "%i", std::to_string(m_count));

    //Parameter may contain %i so replace format in whereclause with count before appending to where string
    StringUtils::Replace(clausefmt, "%i", std::to_string(m_count));
    m_strWhere += clausefmt + strParameter;
  }
}

CSmartPlaylist::CSmartPlaylist()
{
  Reset();
}

bool CSmartPlaylist::OpenAndReadName(const CURL &url)
{
  if (readNameFromPath(url) == NULL)
    return false;

  return !m_playlistName.empty();
}

const TiXmlNode* CSmartPlaylist::readName(const TiXmlNode *root)
{
  if (root == NULL)
    return NULL;

  const TiXmlElement *rootElem = root->ToElement();
  if (rootElem == NULL)
    return NULL;

  if (!root || !StringUtils::EqualsNoCase(root->Value(),"smartplaylist"))
  {
    CLog::Log(LOGERROR, "Error loading Smart playlist");
    return NULL;
  }

  // load the playlist type
  const char* type = rootElem->Attribute("type");
  if (type)
    m_playlistType = type;
  // backward compatibility:
  if (m_playlistType == "music")
    m_playlistType = "songs";
  if (m_playlistType == "video")
    m_playlistType = "musicvideos";

  // load the playlist name
  XMLUtils::GetString(root, "name", m_playlistName);

  return root;
}

const TiXmlNode* CSmartPlaylist::readNameFromPath(const CURL &url)
{
  CFileStream file;
  if (!file.Open(url))
  {
    CLog::Log(LOGERROR, "Error loading Smart playlist %s (failed to read file)", url.GetRedacted().c_str());
    return NULL;
  }

  m_xmlDoc.Clear();
  file >> m_xmlDoc;

  const TiXmlNode *root = readName(m_xmlDoc.RootElement());
  if (m_playlistName.empty())
  {
    m_playlistName = CUtil::GetTitleFromPath(url.Get());
    if (URIUtils::HasExtension(m_playlistName, ".xsp"))
      URIUtils::RemoveExtension(m_playlistName);
  }

  return root;
}

const TiXmlNode* CSmartPlaylist::readNameFromXml(const std::string &xml)
{
  if (xml.empty())
  {
    CLog::Log(LOGERROR, "Error loading empty Smart playlist");
    return NULL;
  }

  m_xmlDoc.Clear();
  if (!m_xmlDoc.Parse(xml))
  {
    CLog::Log(LOGERROR, "Error loading Smart playlist (failed to parse xml: %s)", m_xmlDoc.ErrorDesc());
    return NULL;
  }

  const TiXmlNode *root = readName(m_xmlDoc.RootElement());

  return root;
}

bool CSmartPlaylist::load(const TiXmlNode *root)
{
  if (root == NULL)
    return false;

  return LoadFromXML(root);
}

bool CSmartPlaylist::Load(const CURL &url)
{
  return load(readNameFromPath(url));
}

bool CSmartPlaylist::Load(const std::string &path)
{
  const CURL pathToUrl(path);
  return load(readNameFromPath(pathToUrl));
}

bool CSmartPlaylist::Load(const CVariant &obj)
{
  if (!obj.isObject())
    return false;

  // load the playlist type
  if (obj.isMember("type") && obj["type"].isString())
    m_playlistType = obj["type"].asString();

  // backward compatibility
  if (m_playlistType == "music")
    m_playlistType = "songs";
  if (m_playlistType == "video")
    m_playlistType = "musicvideos";

  // load the playlist name
  if (obj.isMember("name") && obj["name"].isString())
    m_playlistName = obj["name"].asString();

  if (obj.isMember("rules"))
    m_ruleCombination.Load(obj["rules"], this);

  if (obj.isMember("group") && obj["group"].isMember("type") && obj["group"]["type"].isString())
  {
    m_group = obj["group"]["type"].asString();
    if (obj["group"].isMember("mixed") && obj["group"]["mixed"].isBoolean())
      m_groupMixed = obj["group"]["mixed"].asBoolean();
  }

  // now any limits
  if (obj.isMember("limit") && (obj["limit"].isInteger() || obj["limit"].isUnsignedInteger()) && obj["limit"].asUnsignedInteger() > 0)
    m_limit = (unsigned int)obj["limit"].asUnsignedInteger();

  // and order
  if (obj.isMember("order") && obj["order"].isMember("method") && obj["order"]["method"].isString())
  {
    const CVariant &order = obj["order"];
    if (order.isMember("direction") && order["direction"].isString())
      m_orderDirection = StringUtils::EqualsNoCase(order["direction"].asString(), "ascending") ? SortOrderAscending : SortOrderDescending;

    if (order.isMember("ignorefolders") && obj["ignorefolders"].isBoolean())
      m_orderAttributes = obj["ignorefolders"].asBoolean() ? SortAttributeIgnoreFolders : SortAttributeNone;

    m_orderField = CSmartPlaylistRule::TranslateOrder(obj["order"]["method"].asString().c_str());
  }

  return true;
}

bool CSmartPlaylist::LoadFromXml(const std::string &xml)
{
  return load(readNameFromXml(xml));
}

bool CSmartPlaylist::LoadFromXML(const TiXmlNode *root, const std::string &encoding)
{
  if (!root)
    return false;

  std::string tmp;
  if (XMLUtils::GetString(root, "match", tmp))
    m_ruleCombination.SetType(StringUtils::EqualsNoCase(tmp, "all") ? CSmartPlaylistRuleCombination::CombinationAnd : CSmartPlaylistRuleCombination::CombinationOr);

  // now the rules
  const TiXmlNode *ruleNode = root->FirstChild("rule");
  while (ruleNode)
  {
    CSmartPlaylistRule rule;
    if (rule.Load(ruleNode, encoding))
      m_ruleCombination.AddRule(rule);

    ruleNode = ruleNode->NextSibling("rule");
  }

  const TiXmlElement *groupElement = root->FirstChildElement("group");
  if (groupElement != NULL && groupElement->FirstChild() != NULL)
  {
    m_group = groupElement->FirstChild()->ValueStr();
    const char* mixed = groupElement->Attribute("mixed");
    m_groupMixed = mixed != NULL && StringUtils::EqualsNoCase(mixed, "true");
  }

  // now any limits
  // format is <limit>25</limit>
  XMLUtils::GetUInt(root, "limit", m_limit);

  // and order
  // format is <order direction="ascending">field</order>
  const TiXmlElement *order = root->FirstChildElement("order");
  if (order && order->FirstChild())
  {
    const char *direction = order->Attribute("direction");
    if (direction)
      m_orderDirection = StringUtils::EqualsNoCase(direction, "ascending") ? SortOrderAscending : SortOrderDescending;

    const char *ignorefolders = order->Attribute("ignorefolders");
    if (ignorefolders != NULL)
      m_orderAttributes = StringUtils::EqualsNoCase(ignorefolders, "true") ? SortAttributeIgnoreFolders : SortAttributeNone;

    m_orderField = CSmartPlaylistRule::TranslateOrder(order->FirstChild()->Value());
  }
  return true;
}

bool CSmartPlaylist::LoadFromJson(const std::string &json)
{
  if (json.empty())
    return false;

  CVariant obj = CJSONVariantParser::Parse((const unsigned char *)json.c_str(), json.size());
  return Load(obj);
}

bool CSmartPlaylist::Save(const std::string &path) const
{
  CXBMCTinyXML doc;
  TiXmlDeclaration decl("1.0", "UTF-8", "yes");
  doc.InsertEndChild(decl);

  TiXmlElement xmlRootElement("smartplaylist");
  xmlRootElement.SetAttribute("type",m_playlistType.c_str());
  TiXmlNode *pRoot = doc.InsertEndChild(xmlRootElement);
  if (!pRoot)
    return false;

  // add the <name> tag
  XMLUtils::SetString(pRoot, "name", m_playlistName);

  // add the <match> tag
  XMLUtils::SetString(pRoot, "match", m_ruleCombination.GetType() == CSmartPlaylistRuleCombination::CombinationAnd ? "all" : "one");

  // add <rule> tags
  m_ruleCombination.Save(pRoot);

  // add <group> tag if necessary
  if (!m_group.empty())
  {
    TiXmlElement nodeGroup("group");
    if (m_groupMixed)
      nodeGroup.SetAttribute("mixed", "true");
    TiXmlText group(m_group.c_str());
    nodeGroup.InsertEndChild(group);
    pRoot->InsertEndChild(nodeGroup);
  }

  // add <limit> tag
  if (m_limit)
    XMLUtils::SetInt(pRoot, "limit", m_limit);

  // add <order> tag
  if (m_orderField != SortByNone)
  {
    TiXmlText order(CSmartPlaylistRule::TranslateOrder(m_orderField).c_str());
    TiXmlElement nodeOrder("order");
    nodeOrder.SetAttribute("direction", m_orderDirection == SortOrderDescending ? "descending" : "ascending");
    if (m_orderAttributes & SortAttributeIgnoreFolders)
      nodeOrder.SetAttribute("ignorefolders", "true");
    nodeOrder.InsertEndChild(order);
    pRoot->InsertEndChild(nodeOrder);
  }
  return doc.SaveFile(path);
}

bool CSmartPlaylist::Save(CVariant &obj, bool full /* = true */) const
{
  if (obj.type() == CVariant::VariantTypeConstNull)
    return false;

  obj.clear();
  // add "type"
  obj["type"] = m_playlistType;

  // add "rules"
  CVariant rulesObj = CVariant(CVariant::VariantTypeObject);
  if (m_ruleCombination.Save(rulesObj))
    obj["rules"] = rulesObj;

  // add "group"
  if (!m_group.empty())
  {
    obj["group"]["type"] = m_group;
    obj["group"]["mixed"] = m_groupMixed;
  }

  // add "limit"
  if (full && m_limit)
    obj["limit"] = m_limit;

  // add "order"
  if (full && m_orderField != SortByNone)
  {
    obj["order"] = CVariant(CVariant::VariantTypeObject);
    obj["order"]["method"] = CSmartPlaylistRule::TranslateOrder(m_orderField);
    obj["order"]["direction"] = m_orderDirection == SortOrderDescending ? "descending" : "ascending";
    obj["order"]["ignorefolders"] = (m_orderAttributes & SortAttributeIgnoreFolders);
  }

  return true;
}

bool CSmartPlaylist::SaveAsJson(std::string &json, bool full /* = true */) const
{
  CVariant xsp(CVariant::VariantTypeObject);
  if (!Save(xsp, full))
    return false;

  json = CJSONVariantWriter::Write(xsp, true);
  return json.size() > 0;
}

void CSmartPlaylist::Reset()
{
  m_ruleCombination.clear();
  m_limit = 0;
  m_orderField = SortByNone;
  m_orderDirection = SortOrderNone;
  m_orderAttributes = SortAttributeNone;
  m_playlistType = "songs"; // sane default
  m_group.clear();
  m_groupMixed = false;
}

void CSmartPlaylist::SetName(const std::string &name)
{
  m_playlistName = name;
}

void CSmartPlaylist::SetType(const std::string &type)
{
  m_playlistType = type;
}

bool CSmartPlaylist::IsVideoType() const
{
  return IsVideoType(m_playlistType);
}

bool CSmartPlaylist::IsMusicType() const
{
  return IsMusicType(m_playlistType);
}

bool CSmartPlaylist::IsVideoType(const std::string &type)
{
  return type == "movies" || type == "tvshows" || type == "episodes" ||
         type == "musicvideos" || type == "mixed";
}

bool CSmartPlaylist::IsMusicType(const std::string &type)
{
  return type == "artists" || type == "albums" ||
         type == "songs" || type == "mixed";
}

std::string CSmartPlaylist::GetWhereClause(const CDatabase &db, std::set<std::string> &referencedPlaylists) const
{
  return m_ruleCombination.GetWhereClause(db, GetType(), referencedPlaylists);
}

void CSmartPlaylist::GetVirtualFolders(std::vector<std::string> &virtualFolders) const
{
  m_ruleCombination.GetVirtualFolders(GetType(), virtualFolders);
}

std::string CSmartPlaylist::GetSaveLocation() const
{
  if (m_playlistType == "mixed")
    return "mixed";
  if (IsMusicType())
    return "music";
  // all others are video
  return "video";
}

void CSmartPlaylist::GetAvailableFields(const std::string &type, std::vector<std::string> &fieldList)
{
  std::vector<Field> typeFields = CSmartPlaylistRule::GetFields(type);
  for (std::vector<Field>::const_iterator field = typeFields.begin(); field != typeFields.end(); ++field)
  {
    for (unsigned int i = 0; i < NUM_FIELDS; i++)
    {
      if (*field == fields[i].field)
        fieldList.push_back(fields[i].string);
    }
  }
}

bool CSmartPlaylist::IsEmpty(bool ignoreSortAndLimit /* = true */) const
{
  bool empty = m_ruleCombination.empty();
  if (empty && !ignoreSortAndLimit)
    empty = m_limit <= 0 && m_orderField == SortByNone && m_orderDirection == SortOrderNone;

  return empty;
}

bool CSmartPlaylist::CheckTypeCompatibility(const std::string &typeLeft, const std::string &typeRight)
{
  if (typeLeft == typeRight)
    return true;

  if (typeLeft == "mixed" &&
     (typeRight == "songs" || typeRight == "musicvideos"))
    return true;

  if (typeRight == "mixed" &&
     (typeLeft == "songs" || typeLeft == "musicvideos"))
    return true;

  return false;
}

CDatabaseQueryRule *CSmartPlaylist::CreateRule() const
{
  return new CSmartPlaylistRule();
}
CDatabaseQueryRuleCombination *CSmartPlaylist::CreateCombination() const
{
  return new CSmartPlaylistRuleCombination();
}

std::string CSmartPlaylist::GetChildWhereClause(const CDatabase &db, const std::string &type) const
{
  if (!IsMusicType())
    return std::string();
  if (m_playlistType == "artists" && type != "albums" && type != "songs")
    return std::string();
  if (m_playlistType == "albums" && type != "songs")
    return std::string();
  CSmartPlaylistRuleCombination rules = m_ruleCombination;
  bool includeartists = rules.SongRulesOnly();
  // Convert song rules for artist into "albums" rules
  if (m_playlistType == "artists" && type == "albums")
  {
    return rules.GetChildClause(db, type);
  }
  return std::string();
}