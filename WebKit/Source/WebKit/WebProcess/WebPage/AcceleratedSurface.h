/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/Damage.h>
#include <WebCore/IntSize.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WTF {
class RunLoop;
}

namespace WebCore {
class Region;
}

namespace WebKit {

class ThreadedCompositor;
class WebPage;

class AcceleratedSurface {
    WTF_MAKE_NONCOPYABLE(AcceleratedSurface);
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedSurface);
public:
    static std::unique_ptr<AcceleratedSurface> create(ThreadedCompositor&, WebPage&, Function<void()>&& frameCompleteHandler);
    virtual ~AcceleratedSurface() = default;

    virtual uint64_t window() const { ASSERT_NOT_REACHED(); return 0; }
    virtual uint64_t surfaceID() const { ASSERT_NOT_REACHED(); return 0; }
    virtual bool resize(const WebCore::IntSize&);
    virtual bool shouldPaintMirrored() const { return false; }

    virtual void didCreateGLContext() { }
    virtual void willDestroyGLContext() { }
    virtual void finalize() { }
    virtual void willRenderFrame() { }
    virtual void didRenderFrame() { }

#if ENABLE(DAMAGE_TRACKING)
    void setFrameDamage(WebCore::Damage&& damage)
    {
        if (!damage.isEmpty())
            m_frameDamage = WTFMove(damage);
        else
            m_frameDamage = std::nullopt;
    }
    const std::optional<WebCore::Damage>& frameDamage() const { return m_frameDamage; }
    virtual const std::optional<WebCore::Damage>& frameDamageSinceLastUse() { return m_frameDamage; }
#endif

    virtual void didCreateCompositingRunLoop(WTF::RunLoop&) { }
    virtual void willDestroyCompositingRunLoop() { }

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    virtual void preferredBufferFormatsDidChange() { }
#endif

    virtual void visibilityDidChange(bool) { }
    virtual bool backgroundColorDidChange();

    void clearIfNeeded();

protected:
    AcceleratedSurface(WebPage&, Function<void()>&& frameCompleteHandler);

    void frameComplete() const;

    WeakRef<WebPage> m_webPage;
    Function<void()> m_frameCompleteHandler;
    WebCore::IntSize m_size;
    std::atomic<bool> m_isOpaque { true };
#if ENABLE(DAMAGE_TRACKING)
    std::optional<WebCore::Damage> m_frameDamage;
#endif
};

} // namespace WebKit
