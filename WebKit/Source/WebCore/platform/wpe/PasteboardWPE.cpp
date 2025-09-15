/*
 * Copyright (C) 2014-2015, 2025 Igalia S.L.
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
#include "Pasteboard.h"

#if PLATFORM(WPE)
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SelectionData.h"
#include "SharedBuffer.h"
#include <wtf/URL.h>

namespace WebCore {

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTFMove(context), "CLIPBOARD"_s);
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context, const String& name)
    : m_context(WTFMove(context))
    , m_name(name)
    , m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(m_name))
{
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context)
    : m_context(WTFMove(context))
{
}

void Pasteboard::writeString(const String&, const String&)
{
    notImplemented();
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    SelectionData data;
    data.setText(text);
    platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());
    SelectionData data;
    data.setURL(pasteboardURL.url, pasteboardURL.title);
    platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    SelectionData data;
    if (!pasteboardImage.url.url.isEmpty()) {
        data.setURL(pasteboardImage.url.url, pasteboardImage.url.title);
        data.setMarkup(pasteboardImage.url.markup);
    }
    data.setImage(pasteboardImage.image.get());
    platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
}

void Pasteboard::write(const PasteboardBuffer&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardWebContent& pasteboardContent)
{
    SelectionData data;
    data.setText(pasteboardContent.text);
    data.setMarkup(pasteboardContent.markup);
    PasteboardCustomData customData;
    customData.setOrigin(pasteboardContent.contentOrigin);
    data.setCustomData(customData.createSharedBuffer());
    platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
}

void Pasteboard::clear()
{
    platformStrategies()->pasteboardStrategy()->clearClipboard(m_name);
}

void Pasteboard::clear(const String&)
{
    notImplemented();
}

bool Pasteboard::canSmartReplace()
{
    return false;
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, std::optional<size_t>)
{
    text.text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name, "text/plain;charset=utf-8"_s);
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, std::optional<size_t>)
{
    reader.setContentOrigin(readOrigin());

    auto types = platformStrategies()->pasteboardStrategy()->types(m_name);
    if (types.contains("text/html"_s)) {
        auto text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name, "text/html"_s);
        if (!text.isNull() && reader.readHTML(text))
            return;
    }

    if (policy == WebContentReadingPolicy::OnlyRichTextTypes)
        return;

    static const ASCIILiteral imageTypes[] = { "image/png"_s, "image/jpeg"_s, "image/gif"_s, "image/bmp"_s, "image/vnd.microsoft.icon"_s, "image/x-icon"_s };
    for (const auto& imageType : imageTypes) {
        if (types.contains(imageType)) {
            auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, imageType);
            if (!buffer->isEmpty() && reader.readImage(buffer.releaseNonNull(), imageType))
                return;
        }
    }

    if (types.contains("text/uri-list"_s)) {
        auto filePaths = platformStrategies()->pasteboardStrategy()->readFilePathsFromClipboard(m_name);
        if (reader.readFilePaths(filePaths))
            return;
    }

    if (types.contains("text/plain"_s)) {
        auto text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name, "text/plain"_s);
        if (!text.isNull() && reader.readPlainText(text))
            return;
    }

    if (types.contains("text/plain;charset=utf-8"_s)) {
        auto text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name, "text/plain;charset=utf-8"_s);
        if (!text.isNull() && reader.readPlainText(text))
            return;
    }
}

void Pasteboard::read(PasteboardFileReader& reader, std::optional<size_t> index)
{
    if (!index) {
        auto filePaths = platformStrategies()->pasteboardStrategy()->readFilePathsFromClipboard(m_name);
        for (const auto& filePath : filePaths)
            reader.readFilename(filePath);
        return;
    }

    if (reader.shouldReadBuffer("image/png"_s)) {
        if (auto buffer = readBuffer(index, "image/png"_s))
            reader.readBuffer({ }, { }, buffer.releaseNonNull());
    }
}

bool Pasteboard::hasData()
{
    return !platformStrategies()->pasteboardStrategy()->types(m_name).isEmpty();
}

Vector<String> Pasteboard::typesSafeForBindings(const String& origin)
{
    return platformStrategies()->pasteboardStrategy()->typesSafeForDOMToReadAndWrite(m_name, origin, context());
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    return platformStrategies()->pasteboardStrategy()->types(m_name);
}

String Pasteboard::readOrigin()
{
    // FIXME: cache custom data?
    if (auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, PasteboardCustomData::wpeType()))
        return PasteboardCustomData::fromSharedBuffer(*buffer).origin();

    return { };
}

String Pasteboard::readString(const String& type)
{
    return platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name, type);
}

String Pasteboard::readStringInCustomData(const String& type)
{
    // FIXME: cache custom data?
    if (auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, PasteboardCustomData::wpeType()))
        return PasteboardCustomData::fromSharedBuffer(*buffer).readStringInCustomData(type);

    return { };
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    auto types = platformStrategies()->pasteboardStrategy()->types(m_name);
    if (types.contains("text/uri-list"_s)) {
        auto filePaths = platformStrategies()->pasteboardStrategy()->readFilePathsFromClipboard(m_name);
        if (!filePaths.isEmpty())
            return FileContentState::MayContainFilePaths;
    }

    auto result = types.findIf([](const String& type) {
        return MIMETypeRegistry::isSupportedImageMIMEType(type);
    });
    return result == notFound ? FileContentState::NoFileOrImageData : FileContentState::MayContainFilePaths;
}

void Pasteboard::writeMarkup(const String&)
{
    notImplemented();
}

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>& data)
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->writeCustomData(data, m_name, context());
}

void Pasteboard::write(const Color&)
{
    notImplemented();
}

int64_t Pasteboard::changeCount() const
{
    return platformStrategies()->pasteboardStrategy()->changeCount(m_name);
}

} // namespace WebCore

#endif // PLATFORM(WPE)
