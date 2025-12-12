/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DDModelPlayer.h"

#if ENABLE(GPU_PROCESS_MODEL)

#import "Document.h"
#import "GPU.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerContentsDisplayDelegate.h"
#import "HTMLModelElement.h"
#import "ModelDDInlineConverters.h"
#import "ModelDDTypes.h"
#import "Navigator.h"
#import "Page.h"
#import "PlatformCALayer.h"
#import "PlatformCALayerDelegatedContents.h"
#import "WebGPU.h"

namespace WebCore {

class ModelDisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<ModelDisplayBufferDisplayDelegate> create(DDModelPlayer& modelPlayer, bool isOpaque = true, float contentsScale = 1)
    {
        return adoptRef(*new ModelDisplayBufferDisplayDelegate(modelPlayer, isOpaque, contentsScale));
    }
    // GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
        layer.setContentsScale(m_contentsScale);
        layer.setContentsFormat(m_contentsFormat);
    }
    void display(PlatformCALayer& layer) final
    {
        if (layer.isOpaque() != m_isOpaque)
            layer.setOpaque(m_isOpaque);
        if (m_displayBuffer) {
            layer.setContentsFormat(m_contentsFormat);
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, { }, std::nullopt });
        } else
            layer.clearContents();

        layer.setNeedsDisplay();

        if (RefPtr player = m_modelPlayer.get())
            player->update();

    }
    GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayer::CompositingCoordinatesOrientation::TopDown;
    }
    void setDisplayBuffer(const WTF::MachSendRight& displayBuffer)
    {
        if (!displayBuffer) {
            m_displayBuffer = { };
            return;
        }

        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;

        m_displayBuffer = MachSendRight { displayBuffer };
    }
    void setContentsFormat(ContentsFormat contentsFormat)
    {
        m_contentsFormat = contentsFormat;
    }
    void setOpaque(bool opaque)
    {
        m_isOpaque = opaque;
    }
private:
    ModelDisplayBufferDisplayDelegate(DDModelPlayer& modelPlayer, bool isOpaque, float contentsScale)
        : m_modelPlayer(modelPlayer)
        , m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }
    ThreadSafeWeakPtr<DDModelPlayer> m_modelPlayer;
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
    bool m_isOpaque;
    ContentsFormat m_contentsFormat { ContentsFormat::RGBA8 };
};

Ref<DDModelPlayer> DDModelPlayer::create(Page& page, ModelPlayerClient& client)
{
    return adoptRef(*new DDModelPlayer(page, client));
}

DDModelPlayer::DDModelPlayer(Page& page, ModelPlayerClient& client)
: m_client { client }
, m_id { ModelPlayerIdentifier::generate() }
, m_page(page)
{
}

DDModelPlayer::~DDModelPlayer()
{
}

void DDModelPlayer::ensureOnMainThreadWithProtectedThis(Function<void(Ref<DDModelPlayer>)>&& task)
{
    ensureOnMainThread([protectedThis = Ref { *this }, task = WTFMove(task)]() mutable {
        task(protectedThis);
    });
}

// MARK: - ModelPlayer overrides.

void DDModelPlayer::load(Model& modelSource, LayoutSize size)
{
    RefPtr corePage = m_page.get();
    m_modelLoader = nil;
    if (!m_modelLoader) {
        RefPtr document = corePage->localTopDocument();
        if (!document)
            return;

        RefPtr window = document->window();
        if (!window)
            return;

        RefPtr gpu = window->protectedNavigator()->gpu();
        if (!gpu)
            return;

        m_currentModel = gpu->backing().createModelBacking(size.width().toUnsigned(), size.height().toUnsigned(), [protectedThis = Ref { *this }] (Vector<MachSendRight>&& surfaceHandles) {
            if (surfaceHandles.size())
                protectedThis->m_displayBuffers = WTFMove(surfaceHandles);
        });

        m_modelLoader = adoptNS([[WebUSDModelLoader alloc] init]);
        RetainPtr nsURL = modelSource.url().createNSURL();
        Ref protectedThis = Ref { *this };
        [m_modelLoader setCallbacksWithModelAddedCallback:^(WebAddMeshRequest *addRequest) {
            ensureOnMainThreadWithProtectedThis([addRequest] (Ref<DDModelPlayer> protectedThis) {
                if (RefPtr client = protectedThis->m_client.get())
                    client->didFinishLoading(protectedThis.get());

                if (protectedThis->m_currentModel)
                    protectedThis->m_currentModel->addMesh(toCpp(addRequest));

                [protectedThis->m_modelLoader requestCompleted:addRequest];
            });
        } modelUpdatedCallback:^(WebUpdateMeshRequest *updateRequest) {
            ensureOnMainThreadWithProtectedThis([updateRequest] (Ref<DDModelPlayer> protectedThis) {
                if (protectedThis->m_currentModel)
                    protectedThis->m_currentModel->update(toCpp(updateRequest));

                [protectedThis->m_modelLoader requestCompleted:updateRequest];
            });
        } textureAddedCallback:^(WebDDAddTextureRequest *addTexture) {
            ensureOnMainThreadWithProtectedThis([addTexture] (Ref<DDModelPlayer> protectedThis) {
                if (protectedThis->m_currentModel)
                    protectedThis->m_currentModel->addTexture(toCpp(addTexture));

                [protectedThis->m_modelLoader requestCompleted:addTexture];
            });
        } textureUpdatedCallback:^(WebDDUpdateTextureRequest *updateTexture) {
            ensureOnMainThreadWithProtectedThis([updateTexture] (Ref<DDModelPlayer> protectedThis) {
                if (protectedThis->m_currentModel)
                    protectedThis->m_currentModel->updateTexture(toCpp(updateTexture));

                [protectedThis->m_modelLoader requestCompleted:updateTexture];
            });
        } materialAddedCallback:^(WebDDAddMaterialRequest *addMaterial) {
            ensureOnMainThreadWithProtectedThis([addMaterial] (Ref<DDModelPlayer> protectedThis) {
                if (protectedThis->m_currentModel)
                    protectedThis->m_currentModel->addMaterial(toCpp(addMaterial));

                [protectedThis->m_modelLoader requestCompleted:addMaterial];
            });
        } materialUpdatedCallback:^(WebDDUpdateMaterialRequest *updateMaterial) {
            ensureOnMainThreadWithProtectedThis([updateMaterial] (Ref<DDModelPlayer> protectedThis) {
                if (protectedThis->m_currentModel)
                    protectedThis->m_currentModel->updateMaterial(toCpp(updateMaterial));

                [protectedThis->m_modelLoader requestCompleted:updateMaterial];
            });
        }];
        [m_modelLoader loadModelFrom:nsURL.get()];
    }
}

void DDModelPlayer::sizeDidChange(LayoutSize)
{
}

PlatformLayer* DDModelPlayer::layer()
{
    return nullptr;
}

std::optional<LayerHostingContextIdentifier> DDModelPlayer::layerHostingContextIdentifier()
{
    return std::nullopt;
}

void DDModelPlayer::enterFullscreen()
{
}

void DDModelPlayer::handleMouseDown(const LayoutPoint&, MonotonicTime)
{
}

void DDModelPlayer::handleMouseMove(const LayoutPoint&, MonotonicTime)
{
}

void DDModelPlayer::handleMouseUp(const LayoutPoint&, MonotonicTime)
{
}

void DDModelPlayer::getCamera(CompletionHandler<void(std::optional<HTMLModelElementCamera>&&)>&&)
{
}

void DDModelPlayer::setCamera(HTMLModelElementCamera, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void DDModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void DDModelPlayer::setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::setIsMuted(bool, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::updateScene()
{
}

ModelPlayerAccessibilityChildren DDModelPlayer::accessibilityChildren()
{
    return { };
}

WebCore::ModelPlayerIdentifier DDModelPlayer::identifier() const
{
    return m_id;
}

const MachSendRight* DDModelPlayer::displayBuffer() const
{
    if (m_currentTexture >= m_displayBuffers.size())
        return nullptr;

    return &m_displayBuffers[m_currentTexture];
}

GraphicsLayerContentsDisplayDelegate* DDModelPlayer::contentsDisplayDelegate()
{
    if (!m_contentsDisplayDelegate) {
        RefPtr modelDisplayDelegate = ModelDisplayBufferDisplayDelegate::create(*this);
        m_contentsDisplayDelegate = modelDisplayDelegate;
        modelDisplayDelegate->setDisplayBuffer(*displayBuffer());
    }

    return m_contentsDisplayDelegate.get();
}

void DDModelPlayer::update()
{
    [m_modelLoader update:1.0 / 60.0];
    if (RefPtr currentModel = m_currentModel)
        currentModel->render();

    if (++m_currentTexture >= m_displayBuffers.size())
        m_currentTexture = 0;
    if (auto* machSendRight = displayBuffer(); machSendRight && contentsDisplayDelegate())
        RefPtr { m_contentsDisplayDelegate }->setDisplayBuffer(*machSendRight);

    if (RefPtr client = m_client.get())
        client->didUpdateDisplayDelegate(*this);
}

}

#endif
