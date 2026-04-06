#pragma once

#include <WString.h>

struct RemoteArtifact {
  String buildId;
  String downloadUrl;
};

using UpdateErrorReporter = void (*)(const String &message);

bool fetchLatestArtifact(const char *owner,
                        const char *repo,
                        const char *ref,
                        const char *path,
                        const char *userAgent,
                        RemoteArtifact &artifact,
                        UpdateErrorReporter reportError);

String readSpiffsBuildId(const char *versionFilePath);

bool applyRemoteArtifact(const RemoteArtifact &artifact,
                        int updateCommand,
                        const char *userAgent,
                        UpdateErrorReporter reportError);