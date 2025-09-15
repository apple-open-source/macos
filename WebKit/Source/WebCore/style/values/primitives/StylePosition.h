/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPosition.h"
#include "FloatPoint.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

struct Length;
struct LengthPoint;

namespace Style {

struct TwoComponentPositionHorizontal {
    LengthPercentage<> offset;

    bool operator==(const TwoComponentPositionHorizontal&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionHorizontal, offset);

struct TwoComponentPositionVertical {
    LengthPercentage<> offset;

    bool operator==(const TwoComponentPositionVertical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionVertical, offset);

struct Position {
    Position(TwoComponentPositionHorizontal&& x, TwoComponentPositionVertical&& y)
        : value { WTFMove(x.offset), WTFMove(y.offset) }
    {
    }

    Position(LengthPercentage<>&& x, LengthPercentage<>&& y)
        : value { WTFMove(x), WTFMove(y) }
    {
    }

    Position(SpaceSeparatedPoint<LengthPercentage<>>&& point)
        : value { WTFMove(point) }
    {
    }

    Position(FloatPoint point)
        : value { LengthPercentage<>::Dimension { point.x() }, LengthPercentage<>::Dimension { point.y() } }
    {
    }

    Position(const WebCore::LengthPoint&);

    bool operator==(const Position&) const = default;

    LengthPercentage<> x() const { return value.x(); }
    LengthPercentage<> y() const { return value.y(); }

    SpaceSeparatedPoint<LengthPercentage<>> value;
};

template<size_t I> const auto& get(const Position& position)
{
    return get<I>(position.value);
}

struct PositionX {
    LengthPercentage<> value;

    bool operator==(const PositionX&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(PositionX, value);

struct PositionY {
    LengthPercentage<> value;

    bool operator==(const PositionY&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(PositionY, value);

// MARK: - Conversion

// Specialization is needed for ToStyle to implement resolution of keyword value to <length-percentage>.
template<> struct ToCSSMapping<TwoComponentPositionHorizontal> { using type = CSS::TwoComponentPositionHorizontal; };
template<> struct ToStyle<CSS::TwoComponentPositionHorizontal> { auto operator()(const CSS::TwoComponentPositionHorizontal&, const BuilderState&) -> TwoComponentPositionHorizontal; };
template<> struct ToCSSMapping<TwoComponentPositionVertical> { using type = CSS::TwoComponentPositionVertical; };
template<> struct ToStyle<CSS::TwoComponentPositionVertical> { auto operator()(const CSS::TwoComponentPositionVertical&, const BuilderState&) -> TwoComponentPositionVertical; };

template<> struct ToCSS<Position> { auto operator()(const Position&, const RenderStyle&) -> CSS::Position; };
template<> struct ToStyle<CSS::Position> { auto operator()(const CSS::Position&, const BuilderState&) -> Position; };

template<> struct ToCSS<PositionX> { auto operator()(const PositionX&, const RenderStyle&) -> CSS::PositionX; };
template<> struct ToStyle<CSS::PositionX> { auto operator()(const CSS::PositionX&, const BuilderState&) -> PositionX; };

template<> struct ToCSS<PositionY> { auto operator()(const PositionY&, const RenderStyle&) -> CSS::PositionY; };
template<> struct ToStyle<CSS::PositionY> { auto operator()(const CSS::PositionY&, const BuilderState&) -> PositionY; };

// MARK: - Evaluation

template<> struct Evaluation<Position> { auto operator()(const Position&, FloatSize) -> FloatPoint; };

// MARK: - Platform

template<> struct ToPlatform<Position> { auto operator()(const Position&) -> WebCore::LengthPoint; };
template<> struct ToPlatform<PositionX> { auto operator()(const PositionX&) -> WebCore::Length; };
template<> struct ToPlatform<PositionY> { auto operator()(const PositionY&) -> WebCore::Length; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::TwoComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::TwoComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::PositionX, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::PositionY, 1)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Position, 2)
