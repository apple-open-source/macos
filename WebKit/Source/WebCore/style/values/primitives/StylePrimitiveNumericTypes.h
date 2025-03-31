/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericTypes.h"
#include "StylePrimitiveNumericConcepts.h"
#include "StyleUnevaluatedCalculation.h"
#include "StyleValueTypes.h"
#include <wtf/CompactVariant.h>

namespace WebCore {
namespace Style {

// Default implementation of `PrimitiveNumeric` for non-composite numeric types.
template<CSS::Numeric CSSType> struct PrimitiveNumeric {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    static constexpr auto unit = UnitTraits::canonical;
    ResolvedValueType value { 0 };

    constexpr PrimitiveNumeric(ResolvedValueType value)
        : value { value }
    {
    }

    constexpr bool isZero() const { return !value; }

    constexpr bool operator==(const PrimitiveNumeric&) const = default;
    constexpr bool operator==(ResolvedValueType other) const { return value == other; };
};

template<typename> struct DimensionPercentageMapping;

// Specialization of `PrimitiveNumeric` for composite dimension-percentage types.
template<CSS::DimensionPercentageNumeric CSSType> struct PrimitiveNumeric<CSSType> {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    // Composite types only currently support float as the `ResolvedValueType`, allowing unconditional use of `CompactVariant`.
    static_assert(std::same_as<ResolvedValueType, float>);

    using Dimension = typename DimensionPercentageMapping<CSS>::Dimension;
    using Percentage = typename DimensionPercentageMapping<CSS>::Percentage;
    using Calc = UnevaluatedCalculation<CSS>;
    using Representation = CompactVariant<Dimension, Percentage, Calc>;

    PrimitiveNumeric(Dimension dimension)
        : m_value { WTFMove(dimension) }
    {
    }

    PrimitiveNumeric(Percentage percentage)
        : m_value { WTFMove(percentage) }
    {
    }

    PrimitiveNumeric(Calc calc)
        : m_value { WTFMove(calc) }
    {
    }

    // NOTE: CalculatedValue is intentionally not part of IPCData.
    using IPCData = std::variant<Dimension, Percentage>;
    PrimitiveNumeric(IPCData&& data)
        : m_value { WTF::switchOn(WTFMove(data), [&](auto&& data) -> Representation { return { WTFMove(data) }; }) }
    {
    }

    IPCData ipcData() const
    {
        return WTF::switchOn(m_value,
            [](const Dimension& dimension) -> IPCData { return dimension; },
            [](const Percentage& percentage) -> IPCData { return percentage; },
            [](const Calc&) -> IPCData { ASSERT_NOT_REACHED(); return Dimension { 0 }; }
        );
    }

    constexpr size_t index() const { return m_value.index(); }

    template<typename T> constexpr bool holdsAlternative() const { return WTF::holdsAlternative<T>(m_value); }
    template<size_t I> constexpr bool holdsAlternative() const { return WTF::holdsAlternative<I>(m_value); }

    template<typename T> T get() const
    {
        return WTF::switchOn(m_value,
            []<std::same_as<T> U>(const U& alternative) -> T { return alternative; },
            [](const auto&) -> T { RELEASE_ASSERT_NOT_REACHED(); }
        );
    }

    template<typename... F> decltype(auto) switchOn(F&&... functors) const
    {
        return WTF::switchOn(m_value, std::forward<F>(functors)...);
    }

    constexpr bool isZero() const
    {
        return WTF::switchOn(m_value,
            []<HasIsZero T>(const T& alternative) { return alternative.isZero(); },
            [](const auto&) { return false; }
        );
    }

    bool operator==(const PrimitiveNumeric&) const = default;

private:
    Representation m_value;
};

// MARK: Integer Primitive

template<CSS::Range R = CSS::All, typename V = int> struct Integer : PrimitiveNumeric<CSS::Integer<R, V>> {
    using Base = PrimitiveNumeric<CSS::Integer<R, V>>;
};

// MARK: Number Primitive

template<CSS::Range R = CSS::All, typename V = double> struct Number : PrimitiveNumeric<CSS::Number<R, V>> {
    using Base = PrimitiveNumeric<CSS::Number<R, V>>;
    using Base::Base;
};

// MARK: Percentage Primitive

template<CSS::Range R = CSS::All, typename V = double> struct Percentage : PrimitiveNumeric<CSS::Percentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::Percentage<R, V>>;
    using Base::Base;
};

// MARK: Dimension Primitives

template<CSS::Range R = CSS::All, typename V = double> struct Angle : PrimitiveNumeric<CSS::Angle<R, V>> {
    using Base = PrimitiveNumeric<CSS::Angle<R, V>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All, typename V = float> struct Length : PrimitiveNumeric<CSS::Length<R, V>> {
    using Base = PrimitiveNumeric<CSS::Length<R, V>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All, typename V = double> struct Time : PrimitiveNumeric<CSS::Time<R, V>> {
    using Base = PrimitiveNumeric<CSS::Time<R, V>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All, typename V = double> struct Frequency : PrimitiveNumeric<CSS::Frequency<R, V>> {
    using Base = PrimitiveNumeric<CSS::Frequency<R, V>>;
    using Base::Base;
};
template<CSS::Range R = CSS::Nonnegative, typename V = double> struct Resolution : PrimitiveNumeric<CSS::Resolution<R, V>> {
    using Base = PrimitiveNumeric<CSS::Resolution<R, V>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All, typename V = double> struct Flex : PrimitiveNumeric<CSS::Flex<R, V>> {
    using Base = PrimitiveNumeric<CSS::Flex<R, V>>;
    using Base::Base;
};

// MARK: Dimension + Percentage Primitives

template<CSS::Range R = CSS::All, typename V = float> struct AnglePercentage : PrimitiveNumeric<CSS::AnglePercentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::AnglePercentage<R, V>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All, typename V = float> struct LengthPercentage : PrimitiveNumeric<CSS::LengthPercentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::LengthPercentage<R, V>>;
    using Base::Base;
};

template<auto R, typename V> struct DimensionPercentageMapping<CSS::AnglePercentage<R, V>> {
    using Dimension = Style::Angle<R, V>;
    using Percentage = Style::Percentage<R, V>;
};
template<auto R, typename V> struct DimensionPercentageMapping<CSS::LengthPercentage<R, V>> {
    using Dimension = Style::Length<R, V>;
    using Percentage = Style::Percentage<R, V>;
};

template<typename T> T get(DimensionPercentageNumeric auto const& dimensionPercentage)
{
    return dimensionPercentage.template get<T>();
}

// MARK: Additional Common Types and Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
template<CSS::Range nR = CSS::All, CSS::Range pR = nR, typename V = double> struct NumberOrPercentage {
    using Number = Style::Number<nR, V>;
    using Percentage = Style::Percentage<pR, V>;

    NumberOrPercentage(std::variant<Number, Percentage>&& value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentage(Number value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentage(Percentage value)
        : value { WTFMove(value) }
    {
    }

    bool operator==(const NumberOrPercentage&) const = default;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<Number>()));

        return WTF::switchOn(value,
            [](EmptyToken) -> ResultType {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const Number& number) -> ResultType {
                return visitor(number);
            },
            [&](const Percentage& percentage) -> ResultType {
                return visitor(percentage);
            }
        );
    }

    struct MarkableTraits {
        static bool isEmptyValue(const NumberOrPercentage& value) { return value.isEmpty(); }
        static NumberOrPercentage emptyValue() { return NumberOrPercentage(EmptyToken()); }
    };

private:
    struct EmptyToken { constexpr bool operator==(const EmptyToken&) const = default; };
    NumberOrPercentage(EmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<EmptyToken>(value); }

    std::variant<EmptyToken, Number, Percentage> value;
};

template<CSS::Range nR = CSS::All, CSS::Range pR = nR, typename V = double> struct NumberOrPercentageResolvedToNumber {
    using Number = Style::Number<nR, V>;
    using Percentage = Style::Percentage<pR, V>;

    Number value { 0 };

    constexpr NumberOrPercentageResolvedToNumber(typename Number::ResolvedValueType value)
        : value { value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Number number)
        : value { number }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Percentage percentage)
        : value { percentage.value / 100.0 }
    {
    }

    constexpr bool isZero() const { return value.isZero(); }

    constexpr bool operator==(const NumberOrPercentageResolvedToNumber&) const = default;
    constexpr bool operator==(typename Number::ResolvedValueType other) const { return value.value == other; };
};

// Standard Numbers
using NumberAll = Number<CSS::All>;
using NumberNonnegative = Number<CSS::Nonnegative>;

// Standard Angles
using AngleAll = Length<CSS::All>;

// Standard Lengths
using LengthAll = Length<CSS::All>;
using LengthNonnegative = Length<CSS::Nonnegative>;

// Standard LengthPercentages
using LengthPercentageAll = LengthPercentage<CSS::All>;
using LengthPercentageNonnegative = LengthPercentage<CSS::Nonnegative>;

// Standard Percentages
using PercentageAll = Percentage<CSS::All>;
using PercentageNonnegative = Percentage<CSS::Nonnegative>;
using PercentageAllFloat = Percentage<CSS::All, float>;
using PercentageNonnegativeFloat = Percentage<CSS::Nonnegative, float>;
using Percentage0To100 = LengthPercentage<CSS::Range{0,100}>;

// Standard Points
using LengthPercentageSpaceSeparatedPointAll = SpaceSeparatedPoint<LengthPercentageAll>;
using LengthPercentageSpaceSeparatedPointNonnegative = SpaceSeparatedPoint<LengthPercentageNonnegative>;

// Standard Sizes
using LengthPercentageSpaceSeparatedSizeAll = SpaceSeparatedSize<LengthPercentageAll>;
using LengthPercentageSpaceSeparatedSizeNonnegative = SpaceSeparatedSize<LengthPercentageNonnegative>;

// MARK: CSS -> Style

template<auto R, typename V> struct ToStyleMapping<CSS::Integer<R, V>>          { using type = Integer<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Number<R, V>>           { using type = Number<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Percentage<R, V>>       { using type = Percentage<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Angle<R, V>>            { using type = Angle<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Length<R, V>>           { using type = Length<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Time<R, V>>             { using type = Time<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Frequency<R, V>>        { using type = Frequency<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Resolution<R, V>>       { using type = Resolution<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Flex<R, V>>             { using type = Flex<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::AnglePercentage<R, V>>  { using type = AnglePercentage<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::LengthPercentage<R, V>> { using type = LengthPercentage<R, V>; };

template<auto nR, auto pR, typename V> struct ToStyleMapping<CSS::NumberOrPercentage<nR, pR, V>> {
    using type = NumberOrPercentage<nR, pR>;
};

template<auto nR, auto pR, typename V> struct ToStyleMapping<CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using type = NumberOrPercentageResolvedToNumber<nR, pR, V>;
};

// MARK: Style -> CSS

template<Numeric T> struct ToCSSMapping<T> {
    using type = typename T::CSS;
};

template<auto nR, auto pR, typename V> struct ToCSSMapping<NumberOrPercentage<nR, pR, V>> {
    using type = CSS::NumberOrPercentage<nR, pR, V>;
};

template<auto nR, auto pR, typename V> struct ToCSSMapping<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using type = CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>;
};

// MARK: Utility Concepts

template<typename T> concept IsPercentageOrCalc =
       std::same_as<T, Percentage<T::range, typename T::ResolvedValueType>>
    || std::same_as<T, UnevaluatedCalculation<typename T::CSS>>;

} // namespace Style
} // namespace WebCore

template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::AnglePercentage<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthPercentage<R, V>> = true;
template<auto nR, auto pR, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::NumberOrPercentage<nR, pR, V>> = true;
