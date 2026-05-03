#pragma once

#include <WString.h>

struct RemoteArtifact {
  String buildId;
  String downloadUrl;
};

using UpdateErrorReporter = void (*)(const String &message);

bool fetchLatestArtifactsManifest(const char *owner,
                                  const char *repo,
                                  const char *ref,
                                  const char *userAgent,
                                  RemoteArtifact &firmwareArtifact,
                                  RemoteArtifact &spiffsArtifact,
                                  UpdateErrorReporter reportError);

String readSpiffsBuildId(const char *versionFilePath);

bool applyRemoteArtifact(const RemoteArtifact &artifact,
                        int updateCommand,
                        const char *userAgent,
                        UpdateErrorReporter reportError);