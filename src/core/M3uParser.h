#pragma once

#include "Types.h"

#include <QByteArray>

namespace iptv {

// Parses #EXTM3U playlists (including Xtream "m3u_plus" output).
//
// Handles: UTF-8 BOM, CRLF, attributes in any order, commas and quotes inside
// attribute values, #EXTGRP groups, and URL lines without #EXTINF.
// Entries are classified as live/movie/series from the URL path
// (".../movie/..." and ".../series/..." are what Xtream servers use).
class M3uParser
{
public:
    static SourceContent parse(const QByteArray &data);

    // True if the data looks like an M3U playlist at all.
    static bool looksLikeM3u(const QByteArray &data);
};

} // namespace iptv
