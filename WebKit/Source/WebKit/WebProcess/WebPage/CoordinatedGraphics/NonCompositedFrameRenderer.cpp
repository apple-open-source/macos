/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "NonCompositedFrameRenderer.h"

#if USE(COORDINATED_GRAPHICS)

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NonCompositedFrameRenderer);

std::unique_ptr<NonCompositedFrameRenderer> NonCompositedFrameRenderer::create(WebPage& webPage)
{
    auto instance = makeUnique<NonCompositedFrameRenderer>(webPage);
    return instance->initialize() ? WTF::move(instance) : nullptr;
}

NonCompositedFrameRenderer::NonCompositedFrameRenderer(WebPage& webPage)
    : m_webPage(webPage)
    , m_surface(AcceleratedSurface::create(m_webPage, [this] {
        m_canRenderNextFrame = true;
        if (m_shouldRenderFollowupFrame) {
            m_shouldRenderFollowupFrame = false;
            display();
        }
    }))
{
}

bool NonCompositedFrameRenderer::initialize()
{
    static_assert(sizeof(GLNativeWindowType) <= sizeof(uint64_t), "GLNativeWindowType must not be longer than 64 bits.");
    m_context = GLContext::create(PlatformDisplay::sharedDisplay(), m_surface->window());
    if (!m_context || !m_context->makeContextCurrent())
        return false;

    m_surface->didCreateCompositingRunLoop(RunLoop::mainSingleton());
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_surface->surfaceID();
    m_webPage.get().send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext), m_webPage.get().drawingArea()->identifier().toUInt64(), { });
    return true;
}

NonCompositedFrameRenderer::~NonCompositedFrameRenderer()
{
    m_surface->willDestroyGLContext();
    m_surface->willDestroyCompositingRunLoop();
}

void NonCompositedFrameRenderer::display()
{
    if (!m_canRenderNextFrame) {
        m_shouldRenderFollowupFrame = true;
        return;
    }

    Ref webPage = m_webPage.get();
    webPage->updateRendering();
    webPage->finalizeRenderingUpdate({ });
    webPage->flushPendingEditorStateUpdate();

    m_surface->willRenderFrame(webPage->size());
    auto* graphicsContext = m_surface->graphicsContext();
    if (!graphicsContext || !m_context->makeContextCurrent())
        return;

    // FIXME: Use render target damage not to re-paint whole rect.
    webPage->drawRect(*graphicsContext, webPage->bounds());

    m_canRenderNextFrame = false;
    m_surface->didRenderFrame();

    webPage->didUpdateRendering();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
