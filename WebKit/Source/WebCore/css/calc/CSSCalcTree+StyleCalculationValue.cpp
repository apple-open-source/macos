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

#include "config.h"
#include "CSSCalcTree+StyleCalculationValue.h"

#include "CSSCalcExecutor.h"
#include "CSSCalcRandomCachingKey.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Mappings.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcTree+Traversal.h"
#include "CSSCalcTree.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSUnevaluatedCalc.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"
#include "StyleCalculationTree.h"
#include "StyleCalculationValue.h"
#include "StyleLengthResolution.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSSCalc {

struct FromConversionOptions {
    CanonicalDimension::Dimension canonicalDimension;
    SimplificationOptions simplification;
    const RenderStyle& style;
};

struct ToConversionOptions {
    EvaluationOptions evaluation;
};

static auto fromStyleCalculationValue(const Style::Calculation::Random::Fixed&, const FromConversionOptions&) -> Random::Sharing;
static auto fromStyleCalculationValue(const CSS::Keyword::None&, const FromConversionOptions&) -> CSS::Keyword::None;
static auto fromStyleCalculationValue(const Style::Calculation::ChildOrNone&, const FromConversionOptions&) -> ChildOrNone;
static auto fromStyleCalculationValue(const Style::Calculation::Children&, const FromConversionOptions&) -> Children;
static auto fromStyleCalculationValue(const std::optional<Style::Calculation::Child>&, const FromConversionOptions&) -> std::optional<Child>;
static auto fromStyleCalculationValue(const Style::Calculation::Child&, const FromConversionOptions&) -> Child;
static auto fromStyleCalculationValue(const Style::Calculation::Number&, const FromConversionOptions&) -> Child;
static auto fromStyleCalculationValue(const Style::Calculation::Percentage&, const FromConversionOptions&) -> Child;
static auto fromStyleCalculationValue(const Style::Calculation::Dimension&, const FromConversionOptions&) -> Child;
static auto fromStyleCalculationValue(const Style::Calculation::IndirectNode<Style::Calculation::Blend>&, const FromConversionOptions&) -> Child;
template<typename CalculationOp> auto fromStyleCalculationValue(const Style::Calculation::IndirectNode<CalculationOp>&, const FromConversionOptions&) -> Child;

static auto toStyleCalculationValue(const Random::Sharing&, const ToConversionOptions&) -> Style::Calculation::Random::Fixed;
static auto toStyleCalculationValue(const std::optional<Child>&, const ToConversionOptions&) -> std::optional<Style::Calculation::Child>;
static auto toStyleCalculationValue(const CSS::Keyword::None&, const ToConversionOptions&) -> CSS::Keyword::None;
static auto toStyleCalculationValue(const ChildOrNone&, const ToConversionOptions&) -> Style::Calculation::ChildOrNone;
static auto toStyleCalculationValue(const Children&, const ToConversionOptions&) -> Style::Calculation::Children;
static auto toStyleCalculationValue(const Child&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const Number&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const Percentage&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const CanonicalDimension&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const NonCanonicalDimension&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const Symbol&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const SiblingCount&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const SiblingIndex&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const IndirectNode<Anchor>&, const ToConversionOptions&) -> Style::Calculation::Child;
static auto toStyleCalculationValue(const IndirectNode<AnchorSize>&, const ToConversionOptions&) -> Style::Calculation::Child;
template<typename Op> auto toStyleCalculationValue(const IndirectNode<Op>&, const ToConversionOptions&) -> Style::Calculation::Child;

static CanonicalDimension::Dimension determineCanonicalDimension(CSS::Category category)
{
    switch (category) {
    case CSS::Category::LengthPercentage:
        return CanonicalDimension::Dimension::Length;

    case CSS::Category::AnglePercentage:
        return CanonicalDimension::Dimension::Angle;

    case CSS::Category::Integer:
    case CSS::Category::Number:
    case CSS::Category::Percentage:
    case CSS::Category::Length:
    case CSS::Category::Angle:
    case CSS::Category::Time:
    case CSS::Category::Frequency:
    case CSS::Category::Resolution:
    case CSS::Category::Flex:
        break;
    }

    ASSERT_NOT_REACHED();
    return CanonicalDimension::Dimension::Length;
}

// MARK: - From

Random::Sharing fromStyleCalculationValue(const Style::Calculation::Random::Fixed& randomFixed, const FromConversionOptions&)
{
    return Random::SharingFixed { randomFixed.baseValue };
}

CSS::Keyword::None fromStyleCalculationValue(const CSS::Keyword::None& none, const FromConversionOptions&)
{
    return none;
}

ChildOrNone fromStyleCalculationValue(const Style::Calculation::ChildOrNone& root, const FromConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return ChildOrNone { fromStyleCalculationValue(root, options) }; });
}

Children fromStyleCalculationValue(const Style::Calculation::Children& children, const FromConversionOptions& options)
{
    return WTF::map(children.value, [&](const auto& child) -> Child { return fromStyleCalculationValue(child, options); });
}

std::optional<Child> fromStyleCalculationValue(const std::optional<Style::Calculation::Child>& root, const FromConversionOptions& options)
{
    if (root)
        return fromStyleCalculationValue(*root, options);
    return std::nullopt;
}

Child fromStyleCalculationValue(const Style::Calculation::Child& root, const FromConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return fromStyleCalculationValue(root, options); });
}

Child fromStyleCalculationValue(const Style::Calculation::Number& number, const FromConversionOptions&)
{
    return makeChild(Number { .value = number.value });
}

Child fromStyleCalculationValue(const Style::Calculation::Percentage& percentage, const FromConversionOptions& options)
{
    return makeChild(Percentage { .value = percentage.value, .hint = Type::determinePercentHint(options.simplification.category) });
}

Child fromStyleCalculationValue(const Style::Calculation::Dimension& root, const FromConversionOptions& options)
{
    switch (options.canonicalDimension) {
    case CanonicalDimension::Dimension::Length:
        return makeChild(CanonicalDimension { .value = adjustFloatForAbsoluteZoom(root.value, options.style), .dimension = options.canonicalDimension });

    case CanonicalDimension::Dimension::Angle:
    case CanonicalDimension::Dimension::Time:
    case CanonicalDimension::Dimension::Frequency:
    case CanonicalDimension::Dimension::Resolution:
    case CanonicalDimension::Dimension::Flex:
        break;
    }

    return makeChild(CanonicalDimension { .value = root.value, .dimension = options.canonicalDimension });
}

Child fromStyleCalculationValue(const Style::Calculation::IndirectNode<Style::Calculation::Blend>& root, const FromConversionOptions& options)
{
    // FIXME: (http://webkit.org/b/122036) Create a CSSCalc::Tree equivalent of Style::Calculation::Blend.

    auto createBlendHalf = [](const auto& child, const auto& options, auto progress) -> Child {
        auto product = multiply(
            fromStyleCalculationValue(child, options),
            makeChild(Number { .value = progress })
        );

        if (auto replacement = simplify(product, options.simplification))
            return WTFMove(*replacement);

        auto type = toType(product);
        return makeChild(WTFMove(product), *type);
    };

    auto sum = add(
        createBlendHalf(root->from, options, 1 - root->progress),
        createBlendHalf(root->to, options, root->progress)
    );

    if (auto replacement = simplify(sum, options.simplification))
        return WTFMove(*replacement);

    auto type = toType(sum);
    return makeChild(WTFMove(sum), *type);
}

template<typename CalculationOp> Child fromStyleCalculationValue(const Style::Calculation::IndirectNode<CalculationOp>& root, const FromConversionOptions& options)
{
    using CalcOp = ToCalcTreeOp<CalculationOp>;

    auto op = WTF::apply([&](const auto& ...x) { return CalcOp { fromStyleCalculationValue(x, options)... }; } , *root);

    if (auto replacement = simplify(op, options.simplification))
        return WTFMove(*replacement);

    auto type = toType(op);
    return makeChild(WTFMove(op), *type);
}

// MARK: - To.

auto toStyleCalculationValue(const Random::Sharing& randomSharing, const ToConversionOptions& options) -> Style::Calculation::Random::Fixed
{
    ASSERT(options.evaluation.conversionData);
    ASSERT(options.evaluation.conversionData->styleBuilderState());

    return WTF::switchOn(randomSharing,
        [&](const Random::SharingOptions& sharingOptions) -> Style::Calculation::Random::Fixed {
            if (!sharingOptions.elementShared.has_value()) {
                ASSERT(options.evaluation.conversionData->styleBuilderState()->element());
            }

            auto baseValue = options.evaluation.conversionData->protectedStyleBuilderState()->lookupCSSRandomBaseValue(
                sharingOptions.identifier,
                sharingOptions.elementShared
            );

            return Style::Calculation::Random::Fixed { baseValue };
        },
        [&](const Random::SharingFixed& sharingFixed) -> Style::Calculation::Random::Fixed {
            return WTF::switchOn(sharingFixed.value,
                [&](const CSS::Number<CSS::ClosedUnitRange>::Raw& raw) -> Style::Calculation::Random::Fixed {
                    return Style::Calculation::Random::Fixed { raw.value };
                },
                [&](const CSS::Number<CSS::ClosedUnitRange>::Calc& calc) -> Style::Calculation::Random::Fixed {
                    return Style::Calculation::Random::Fixed { calc.evaluate(CSS::Category::Number, *options.evaluation.conversionData->protectedStyleBuilderState()) };
                }
            );
        }
    );
}

std::optional<Style::Calculation::Child> toStyleCalculationValue(const std::optional<Child>& optionalChild, const ToConversionOptions& options)
{
    if (optionalChild)
        return toStyleCalculationValue(*optionalChild, options);
    return std::nullopt;
}

CSS::Keyword::None toStyleCalculationValue(const CSS::Keyword::None& none, const ToConversionOptions&)
{
    return none;
}

Style::Calculation::ChildOrNone toStyleCalculationValue(const ChildOrNone& root, const ToConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return Style::Calculation::ChildOrNone { toStyleCalculationValue(root, options) }; });
}

Style::Calculation::Children toStyleCalculationValue(const Children& children, const ToConversionOptions& options)
{
    return WTF::map(children, [&](const auto& child) { return toStyleCalculationValue(child, options); });
}

Style::Calculation::Child toStyleCalculationValue(const Child& root, const ToConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return toStyleCalculationValue(root, options); });
}

Style::Calculation::Child toStyleCalculationValue(const Number& root, const ToConversionOptions&)
{
    return Style::Calculation::number(root.value);
}

Style::Calculation::Child toStyleCalculationValue(const Percentage& root, const ToConversionOptions&)
{
    return Style::Calculation::percentage(root.value);
}

Style::Calculation::Child toStyleCalculationValue(const CanonicalDimension& root, const ToConversionOptions& options)
{
    ASSERT(options.evaluation.conversionData);

    switch (root.dimension) {
    case CanonicalDimension::Dimension::Length:
        return Style::Calculation::dimension(Style::computeNonCalcLengthDouble(root.value, CSS::LengthUnit::Px, *options.evaluation.conversionData));

    case CanonicalDimension::Dimension::Angle:
    case CanonicalDimension::Dimension::Time:
    case CanonicalDimension::Dimension::Frequency:
    case CanonicalDimension::Dimension::Resolution:
    case CanonicalDimension::Dimension::Flex:
        break;
    }

    return Style::Calculation::dimension(root.value);
}

Style::Calculation::Child toStyleCalculationValue(const NonCanonicalDimension&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Non-canonical numeric values are not supported in the Style::Calculation::Tree");
    return Style::Calculation::number(0);
}

Style::Calculation::Child toStyleCalculationValue(const Symbol&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated symbols are not supported in the Style::Calculation::Tree");
    return Style::Calculation::number(0);
}

Style::Calculation::Child toStyleCalculationValue(const SiblingCount&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated sibling-count() functions are not supported in the Style::Calculation::Tree");
    return Style::Calculation::number(0);
}

Style::Calculation::Child toStyleCalculationValue(const SiblingIndex&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated sibling-index() functions are not supported in the Style::Calculation::Tree");
    return Style::Calculation::number(0);
}

Style::Calculation::Child toStyleCalculationValue(const IndirectNode<Anchor>&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated anchor() functions are not supported in the Style::Calculation::Tree");
    return Style::Calculation::number(0);
}

Style::Calculation::Child toStyleCalculationValue(const IndirectNode<AnchorSize>&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated anchor-size() functions are not supported in the Style::Calculation::Tree");
    return Style::Calculation::number(0);
}

template<typename Op> Style::Calculation::Child toStyleCalculationValue(const IndirectNode<Op>& root, const ToConversionOptions& options)
{
    using CalculationOp = ToCalculationTreeOp<Op>;

    return Style::Calculation::makeChild(WTF::apply([&](const auto& ...x) { return CalculationOp { toStyleCalculationValue(x, options)... }; } , *root));
}

// MARK: - Exposed functions

Tree fromStyleCalculationValue(const Style::CalculationValue& calculationValue, const RenderStyle& style)
{
    auto category = calculationValue.category();
    auto range = calculationValue.range();

    auto conversionOptions = FromConversionOptions {
        .canonicalDimension = determineCanonicalDimension(category),
        .simplification = SimplificationOptions {
            .category = category,
            .range = { range.min, range.max },
            .conversionData = std::nullopt,
            .symbolTable = { },
            .allowZeroValueLengthRemovalFromSum = true,
        },
        .style = style,
    };

    auto root = fromStyleCalculationValue(calculationValue.tree().root, conversionOptions);
    auto type = getType(root);

    return Tree {
        .root = WTFMove(root),
        .type = type,
        .stage = CSSCalc::Stage::Computed,
    };
}

Ref<Style::CalculationValue> toStyleCalculationValue(const Tree& tree, const EvaluationOptions& options)
{
    ASSERT(options.category == CSS::Category::LengthPercentage || options.category == CSS::Category::AnglePercentage);

    auto category = options.category;
    auto range = options.range;

    auto simplificationOptions = SimplificationOptions {
        .category = category,
        .range = range,
        .conversionData = options.conversionData,
        .symbolTable = options.symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };
    auto simplifiedTree = copyAndSimplify(tree, simplificationOptions);

    auto conversionOptions = ToConversionOptions {
        .evaluation = options
    };
    auto root = toStyleCalculationValue(simplifiedTree.root, conversionOptions);

    return Style::CalculationValue::create(
        category,
        range,
        Style::Calculation::Tree { WTFMove(root) }
    );
}

} // namespace CSSCalc
} // namespace WebCore
