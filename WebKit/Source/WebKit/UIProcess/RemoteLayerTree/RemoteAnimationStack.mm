/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteAnimationStack.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RemoteAnimationStack);

Ref<RemoteAnimationStack> RemoteAnimationStack::create(RemoteAnimations&& animations, WebCore::AcceleratedEffectValues&& baseValues, WebCore::FloatRect bounds)
{
    return adoptRef(*new RemoteAnimationStack(WTFMove(animations), WTFMove(baseValues), bounds));
}

RemoteAnimationStack::RemoteAnimationStack(RemoteAnimations&& animations, WebCore::AcceleratedEffectValues&& baseValues, WebCore::FloatRect bounds)
    : m_animations(WTFMove(animations))
    , m_baseValues(WTFMove(baseValues))
    , m_bounds(bounds)
{
    bool affectsFilter = false;
    bool affectsOpacity = false;
    bool affectsTransform = false;

    for (auto& animation : m_animations) {
        auto& properties = animation->animatedProperties();
        affectsFilter = affectsFilter || properties.containsAny({ WebCore::AcceleratedEffectProperty::Filter, WebCore::AcceleratedEffectProperty::BackdropFilter });
        affectsOpacity = affectsOpacity || properties.contains(WebCore::AcceleratedEffectProperty::Opacity);
        affectsTransform = affectsTransform || properties.containsAny(WebCore::transformRelatedAcceleratedProperties);
        if (affectsFilter && affectsOpacity && affectsTransform)
            break;
    }

    ASSERT(affectsFilter || affectsOpacity || affectsTransform);

    if (affectsFilter)
        m_affectedLayerProperties.add(LayerProperty::Filter);
    if (affectsOpacity)
        m_affectedLayerProperties.add(LayerProperty::Opacity);
    if (affectsTransform)
        m_affectedLayerProperties.add(LayerProperty::Transform);
}

#if PLATFORM(MAC)
const WebCore::FilterOperations* RemoteAnimationStack::longestFilterList() const
{
    // FIXME: this assumes there is no WebCore::AcceleratedEffectProperty::BackdropFilter anymore, only Filter.
    if (!m_affectedLayerProperties.contains(LayerProperty::Filter))
        return nullptr;

    const WebCore::FilterOperations* longestFilterList = nullptr;
    for (auto& animation : m_animations) {
        if (!animation->animatedProperties().contains(WebCore::AcceleratedEffectProperty::Filter))
            continue;
        for (auto& keyframe : animation->keyframes()) {
            if (!keyframe.animatedProperties().contains(WebCore::AcceleratedEffectProperty::Filter))
                continue;
            auto& filter = keyframe.values().filter;
            if (!longestFilterList || longestFilterList->size() < filter.size())
                longestFilterList = &filter;
        }
    }

    if (longestFilterList) {
        auto& baseFilter = m_baseValues.filter;
        if (longestFilterList->size() < baseFilter.size())
            longestFilterList = &baseFilter;
    }

    return longestFilterList && !longestFilterList->isEmpty() ? longestFilterList : nullptr;
}

void RemoteAnimationStack::initEffectsFromMainThread(PlatformLayer *layer)
{
    ASSERT(m_filterPresentationModifiers.isEmpty());
    ASSERT(!m_opacityPresentationModifier);
    ASSERT(!m_transformPresentationModifier);
    ASSERT(!m_presentationModifierGroup);

    auto computedValues = computeValues();

    auto* canonicalFilters = longestFilterList();

    auto numberOfPresentationModifiers = [&]() {
        size_t count = 0;
        if (m_affectedLayerProperties.contains(LayerProperty::Filter)) {
            ASSERT(canonicalFilters);
            count += WebCore::PlatformCAFilters::presentationModifierCount(*canonicalFilters);
        }
        if (m_affectedLayerProperties.contains(LayerProperty::Opacity))
            count++;
        if (m_affectedLayerProperties.contains(LayerProperty::Transform))
            count++;
        return count;
    }();

    m_presentationModifierGroup = [CAPresentationModifierGroup groupWithCapacity:numberOfPresentationModifiers];

    if (m_affectedLayerProperties.contains(LayerProperty::Filter)) {
        WebCore::PlatformCAFilters::presentationModifiers(computedValues.filter, longestFilterList(), m_filterPresentationModifiers, m_presentationModifierGroup);
        for (auto& filterPresentationModifier : m_filterPresentationModifiers)
            [layer addPresentationModifier:filterPresentationModifier.second.get()];
    }

    if (m_affectedLayerProperties.contains(LayerProperty::Opacity)) {
        RetainPtr opacity = @(computedValues.opacity.value);
        m_opacityPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"opacity" initialValue:opacity.get() additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_opacityPresentationModifier.get()];
    }

    if (m_affectedLayerProperties.contains(LayerProperty::Transform)) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        RetainPtr transform = [NSValue valueWithCATransform3D:computedTransform];
        m_transformPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"transform" initialValue:transform.get() additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_transformPresentationModifier.get()];
    }

    [m_presentationModifierGroup flushWithTransaction];
}

void RemoteAnimationStack::applyEffectsFromScrollingThread() const
{
    ASSERT(m_presentationModifierGroup);

    auto computedValues = computeValues();

    if (!m_filterPresentationModifiers.isEmpty())
        WebCore::PlatformCAFilters::updatePresentationModifiers(computedValues.filter, m_filterPresentationModifiers);

    if (m_opacityPresentationModifier) {
        RetainPtr opacity = @(computedValues.opacity.value);
        [m_opacityPresentationModifier setValue:opacity.get()];
    }

    if (m_transformPresentationModifier) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        RetainPtr transform = [NSValue valueWithCATransform3D:computedTransform];
        [m_transformPresentationModifier setValue:transform.get()];
    }

    [m_presentationModifierGroup flush];
}
#endif

void RemoteAnimationStack::applyEffectsFromMainThread(PlatformLayer *layer, bool backdropRootIsOpaque) const
{
    auto computedValues = computeValues();

    if (m_affectedLayerProperties.contains(LayerProperty::Filter))
        WebCore::PlatformCAFilters::setFiltersOnLayer(layer, computedValues.filter, backdropRootIsOpaque);

    if (m_affectedLayerProperties.contains(LayerProperty::Opacity))
        [layer setOpacity:computedValues.opacity.value];

    if (m_affectedLayerProperties.contains(LayerProperty::Transform)) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        [layer setTransform:computedTransform];
    }
}

WebCore::AcceleratedEffectValues RemoteAnimationStack::computeValues() const
{
    auto values = m_baseValues;
    for (auto& animation : m_animations)
        animation->apply(values);
    return values;
}

void RemoteAnimationStack::clear(PlatformLayer *layer)
{
#if PLATFORM(MAC)
    ASSERT(m_presentationModifierGroup);

    for (auto& filterPresentationModifier : m_filterPresentationModifiers)
        [layer removePresentationModifier:filterPresentationModifier.second.get()];
    if (m_opacityPresentationModifier)
        [layer removePresentationModifier:m_opacityPresentationModifier.get()];
    if (m_transformPresentationModifier)
        [layer removePresentationModifier:m_transformPresentationModifier.get()];

    [m_presentationModifierGroup flushWithTransaction];

    m_filterPresentationModifiers.clear();
    m_opacityPresentationModifier = nil;
    m_transformPresentationModifier = nil;
    m_presentationModifierGroup = nil;
#endif
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
