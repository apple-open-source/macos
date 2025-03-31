/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteMediaPlayerManagerProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "VideoReceiverEndpointMessage.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/VideoTargetFactory.h>
#include <wtf/LoggerHelper.h>

namespace WebKit {

#if ENABLE(LINEAR_MEDIA_PLAYER)
PlatformVideoTarget RemoteMediaPlayerManagerProxy::videoTargetForIdentifier(const std::optional<WebCore::VideoReceiverEndpointIdentifier>& identifier)
{
    if (identifier)
        return m_videoTargetCache.get(*identifier);
    return nullptr;
}

PlatformVideoTarget RemoteMediaPlayerManagerProxy::takeVideoTargetForMediaElementIdentifier(WebCore::HTMLMediaElementIdentifier mediaElementIdentifier, WebCore::MediaPlayerIdentifier playerIdentifier)
{
    auto cachedEntry = m_videoReceiverEndpointCache.find(mediaElementIdentifier);
    if (cachedEntry == m_videoReceiverEndpointCache.end())
        return nullptr;

    if (cachedEntry->value.playerIdentifier != playerIdentifier) {
        ALWAYS_LOG(LOGIDENTIFIER, "moving target from player ", cachedEntry->value.playerIdentifier->loggingString(), " to player ", playerIdentifier.loggingString());
        if (RefPtr mediaPlayer = this->mediaPlayer(cachedEntry->value.playerIdentifier))
            mediaPlayer->setVideoTarget(nullptr);
        cachedEntry->value.playerIdentifier = playerIdentifier;
    }

    return videoTargetForIdentifier(cachedEntry->value.endpointIdentifier);
}

void RemoteMediaPlayerManagerProxy::handleVideoReceiverEndpointMessage(const VideoReceiverEndpointMessage& endpointMessage)
{
    // A message with an empty endpoint signals that the VideoTarget should be uncached and
    // removed from the existing player.
    if (!endpointMessage.endpoint()) {
        m_videoTargetCache.remove(endpointMessage.endpointIdentifier());
        auto cacheEntry = m_videoReceiverEndpointCache.takeOptional(endpointMessage.mediaElementIdentifier());
        if (!cacheEntry)
            return;

        if (RefPtr mediaPlayer = this->mediaPlayer(cacheEntry->playerIdentifier))
            mediaPlayer->setVideoTarget(nullptr);

        return;
    }

    // Handle caching or uncaching of VideoTargets. Because a VideoTarget can only be created
    // once during the lifetime of an endpoint, we should avoid re-creating these VideoTargets.
    auto ensureVideoTargetResult = m_videoTargetCache.ensure(endpointMessage.endpointIdentifier(), [&] {
        return WebCore::VideoTargetFactory::createTargetFromEndpoint(endpointMessage.endpoint());
    });
    PlatformVideoTarget cachedVideoTarget = ensureVideoTargetResult.iterator->value;

    auto cacheResult = m_videoReceiverEndpointCache.add(endpointMessage.mediaElementIdentifier(), VideoRecevierEndpointCacheEntry { endpointMessage.playerIdentifier(), endpointMessage.endpointIdentifier() });
    if (cacheResult.isNewEntry) {
        // If no entry for the specified mediaElementIdentifier exists, set the new target
        // on the specified player.
        if (RefPtr mediaPlayer = this->mediaPlayer(endpointMessage.playerIdentifier())) {
            ALWAYS_LOG(LOGIDENTIFIER, "New entry for player ", endpointMessage.playerIdentifier()->loggingString());
            mediaPlayer->setVideoTarget(cachedVideoTarget);
        }

        return;
    }

    // A previously cached entry already exists
    auto& cachedEntry = cacheResult.iterator->value;
    auto cachedPlayerIdentifier = cachedEntry.playerIdentifier;
    auto cachedEndpointIdentifier = cachedEntry.endpointIdentifier;

    // If nothing has actually changed, bail.
    if (cachedPlayerIdentifier == endpointMessage.playerIdentifier()
        && cachedEndpointIdentifier == endpointMessage.endpointIdentifier())
        return;

    // If the VideoTarget has been cleared, remove the entry from the cache entirely.
    if (!cachedVideoTarget) {
        if (RefPtr mediaPlayer = this->mediaPlayer(cachedPlayerIdentifier)) {
            ALWAYS_LOG(LOGIDENTIFIER, "Cache cleared; removing target from player ", cachedPlayerIdentifier->loggingString());
            mediaPlayer->setVideoTarget(nullptr);
        } else
            ALWAYS_LOG(LOGIDENTIFIER, "Cache cleared; no current player target");

        m_videoReceiverEndpointCache.remove(cacheResult.iterator);
        return;
    }

    RefPtr cachedPlayer = mediaPlayer(cachedPlayerIdentifier);

    if (cachedPlayerIdentifier != endpointMessage.playerIdentifier() && cachedPlayer) {
        // A endpoint can only be used by one MediaPlayer at a time, so if the playerIdentifier
        // has changed, first remove the endpoint from that cached MediaPlayer.
        ALWAYS_LOG(LOGIDENTIFIER, "Update entry; removing target from player ", cachedPlayerIdentifier->loggingString());
        cachedPlayer->setVideoTarget(nullptr);
    }

    // Then set the new target, which may have changed, on the specified MediaPlayer.
    if (RefPtr mediaPlayer = this->mediaPlayer(endpointMessage.playerIdentifier())) {
        ALWAYS_LOG(LOGIDENTIFIER, "Update entry; ", !cachedVideoTarget ? "removing target" : "setting target", " on player ", endpointMessage.playerIdentifier()->loggingString());
        mediaPlayer->setVideoTarget(cachedVideoTarget);
    }

    // Otherwise, update the cache entry with updated values.
    cachedEntry.playerIdentifier = endpointMessage.playerIdentifier();
    cachedEntry.endpointIdentifier = endpointMessage.endpointIdentifier();
}

void RemoteMediaPlayerManagerProxy::handleVideoReceiverSwapEndpointsMessage(const VideoReceiverSwapEndpointsMessage& swapMessage)
{
    auto sourceCacheEntry = m_videoReceiverEndpointCache.takeOptional(swapMessage.sourceMediaElementIdentifier());
    RefPtr sourcePlayer = mediaPlayer(swapMessage.sourceMediaPlayerIdentifier());
    auto sourceTarget = sourceCacheEntry ? videoTargetForIdentifier(sourceCacheEntry->endpointIdentifier) : nullptr;

    auto destinationCacheEntry = m_videoReceiverEndpointCache.takeOptional(swapMessage.destinationMediaElementIdentifier());
    RefPtr destinationPlayer = mediaPlayer(swapMessage.destinationMediaPlayerIdentifier());
    auto destinationTarget = destinationCacheEntry ? videoTargetForIdentifier(destinationCacheEntry->endpointIdentifier) : nullptr;

    ALWAYS_LOG(LOGIDENTIFIER, "swapping from media element ", swapMessage.sourceMediaElementIdentifier().loggingString(), " to media element ", swapMessage.destinationMediaElementIdentifier().loggingString());

    // To avoid two media players using the VideoTarget simultaneously, set both players
    // to have null targets before continuing
    if (sourcePlayer)
        sourcePlayer->setVideoTarget(nullptr);

    if (destinationPlayer)
        destinationPlayer->setVideoTarget(nullptr);

    if (sourcePlayer)
        sourcePlayer->setVideoTarget(destinationTarget);

    if (destinationPlayer)
        destinationPlayer->setVideoTarget(sourceTarget);

    if (sourceCacheEntry) {
        sourceCacheEntry->playerIdentifier = swapMessage.destinationMediaPlayerIdentifier();
        m_videoReceiverEndpointCache.set(swapMessage.destinationMediaElementIdentifier(), *sourceCacheEntry);
    }

    if (destinationCacheEntry) {
        destinationCacheEntry->playerIdentifier = swapMessage.sourceMediaPlayerIdentifier();
        m_videoReceiverEndpointCache.set(swapMessage.sourceMediaElementIdentifier(), *destinationCacheEntry);
    }
}

#endif

}

#endif
