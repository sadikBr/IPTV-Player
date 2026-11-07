#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace iptv {

// What the app needs to know about a published release (GitHub Releases).
struct ReleaseInfo
{
    QString version;      // "0.8.0" (tag without the leading "v")
    QString notes;        // Markdown
    QUrl pageUrl;         // release page in the browser
    QString assetName;    // file for this platform, empty if none
    QUrl assetUrl;
    qint64 assetSize = 0;
    QByteArray sha256;    // hex, lower case; empty if GitHub didn't give one
    bool valid = false;
};

namespace Updates {

// -1 if a < b, 0 if equal, 1 if a > b. "v0.10.0" > "0.9.3"; "0.8" == "0.8.0";
// anything after "-" or "+" is ignored.
int compareVersions(const QString &a, const QString &b);

// Parses a GitHub "latest release" JSON and picks the asset called
// assetName (if given). Drafts and pre-releases are not valid.
ReleaseInfo parseRelease(const QByteArray &json, const QString &assetName);

} // namespace Updates
} // namespace iptv
