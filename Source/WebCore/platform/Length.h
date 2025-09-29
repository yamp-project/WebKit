/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006-2024 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include <WebCore/LayoutUnit.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueArray.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class LengthType : uint8_t {
    Auto,
    Normal,
    Relative,
    Percent,
    Fixed,
    Intrinsic,
    MinIntrinsic,
    MinContent,
    MaxContent,
    FillAvailable,
    FitContent,
    Calculated,
    Content,
    Undefined
};

enum class ValueRange : uint8_t {
    All,
    NonNegative
};

struct BlendingContext;
class CalculationValue;

struct Length {
    WTF_MAKE_TZONE_ALLOCATED(Length);
public:
    Length(LengthType = LengthType::Auto);

    using FloatOrInt = Variant<float, int>;
    struct AutoData { };
    struct NormalData { };
    struct RelativeData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct PercentData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct FixedData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct IntrinsicData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct MinIntrinsicData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct MinContentData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct MaxContentData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct FillAvailableData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct FitContentData {
        FloatOrInt value;
        bool hasQuirk;
    };
    struct ContentData { };
    struct UndefinedData { };
    using IPCData = Variant<
        AutoData,
        NormalData,
        RelativeData,
        PercentData,
        FixedData,
        IntrinsicData,
        MinIntrinsicData,
        MinContentData,
        MaxContentData,
        FillAvailableData,
        FitContentData,
        ContentData,
        UndefinedData
        // LengthType::Calculated is intentionally not serialized.
    >;

    WEBCORE_EXPORT Length(IPCData&&);
    Length(int value, LengthType, bool hasQuirk = false);
    Length(LayoutUnit value, LengthType, bool hasQuirk = false);
    Length(float value, LengthType, bool hasQuirk = false);
    Length(double value, LengthType, bool hasQuirk = false);

    WEBCORE_EXPORT explicit Length(Ref<CalculationValue>&&);

    explicit Length(WTF::HashTableEmptyValueType);

    Length(const Length&);
    Length(Length&&);
    Length& operator=(const Length&);
    Length& operator=(Length&&);

    ~Length();

    bool operator==(const Length&) const;

    float value() const;
    int intValue() const;
    float percent() const;
    CalculationValue& calculationValue() const;
    Ref<CalculationValue> protectedCalculationValue() const;

    struct Fixed {
        constexpr Fixed(float value) : value(value) { }
        constexpr auto evaluate(float zoom) const { return value * zoom; }
        private:
            float value;
    };
    std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { value() }) : std::nullopt; }

    struct Percentage { float value; };
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { value() }) : std::nullopt; }

    LengthType type() const;
    bool isFloat() const;

    WEBCORE_EXPORT IPCData ipcData() const;

    bool isFixed() const;
    bool isCalculated() const;
    bool isPercent() const;
    bool isPercentOrCalculated() const; // Returns true for both Percent and Calculated.
    bool isSpecified() const;

    bool isRelative() const;

    bool isAuto() const;
    bool isNormal() const;
    bool isUndefined() const;

    bool isEmptyValue() const { return m_isEmptyValue; }

    bool hasQuirk() const;
    void setHasQuirk(bool);

    // FIXME calc: https://bugs.webkit.org/show_bug.cgi?id=80357. A calculated Length
    // always contains a percentage, and without a maxValue passed to these functions
    // it's impossible to determine the sign or zero-ness. The following three functions
    // act as if all calculated values are positive.
    bool isZero() const;
    bool isPositive() const;
    bool isNegative() const;

    WEBCORE_EXPORT float nonNanCalculatedValue(float maxValue) const;

private:
    friend struct MarkableTraits<WebCore::Length>;

    static Length createEmptyValue()
    {
        auto result = Length(LengthType::Undefined);
        result.m_isEmptyValue = true;
        return result;
    }

    bool isCalculatedEqual(const Length&) const;

    void initialize(const Length&);
    void initialize(Length&&);

    WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;
    FloatOrInt floatOrInt() const;
    static LengthType typeFromIndex(const IPCData&);

    union {
        int m_intValue { 0 };
        float m_floatValue;
        unsigned m_calculationValueHandle;
    };
    LengthType m_type;
    bool m_hasQuirk { false };
    bool m_isFloat { false };
    bool m_isEmptyValue { false };
};

// Blend two lengths to produce a new length that is in between them. Used for animation.
Length blend(const Length& from, const Length& to, const BlendingContext&);
Length blend(const Length& from, const Length& to, const BlendingContext&, ValueRange);

inline Length::Length(LengthType type)
    : m_type(type)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(int value, LengthType type, bool hasQuirk)
    : m_intValue(value)
    , m_type(type)
    , m_hasQuirk(hasQuirk)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(LayoutUnit value, LengthType type, bool hasQuirk)
    : m_floatValue(value.toFloat())
    , m_type(type)
    , m_hasQuirk(hasQuirk)
    , m_isFloat(true)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(float value, LengthType type, bool hasQuirk)
    : m_floatValue(value)
    , m_type(type)
    , m_hasQuirk(hasQuirk)
    , m_isFloat(true)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(double value, LengthType type, bool hasQuirk)
    : m_floatValue(static_cast<float>(value))
    , m_type(type)
    , m_hasQuirk(hasQuirk)
    , m_isFloat(true)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(WTF::HashTableEmptyValueType)
    : m_type(LengthType::Undefined)
    , m_isEmptyValue(true)
{
}

inline Length::Length(const Length& other)
{
    initialize(other);
}

inline Length::Length(Length&& other)
{
    initialize(WTFMove(other));
}

inline Length& Length::operator=(const Length& other)
{
    if (this == &other)
        return *this;

    if (isCalculated())
        deref();

    initialize(other);
    return *this;
}

inline Length& Length::operator=(Length&& other)
{
    if (this == &other)
        return *this;

    if (isCalculated())
        deref();

    initialize(WTFMove(other));
    return *this;
}

inline void Length::initialize(const Length& other)
{
    m_type = other.m_type;
    m_hasQuirk = other.m_hasQuirk;
    m_isEmptyValue = other.m_isEmptyValue;

    switch (m_type) {
    case LengthType::Auto:
    case LengthType::Normal:
    case LengthType::Content:
    case LengthType::Undefined:
        m_intValue = 0;
        break;
    case LengthType::Fixed:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent:
        m_isFloat = other.m_isFloat;
        if (m_isFloat)
            m_floatValue = other.m_floatValue;
        else
            m_intValue = other.m_intValue;
        break;
    case LengthType::Calculated:
        m_calculationValueHandle = other.m_calculationValueHandle;
        ref();
        break;
    }
}

inline void Length::initialize(Length&& other)
{
    m_type = other.m_type;
    m_hasQuirk = other.m_hasQuirk;
    m_isEmptyValue = other.m_isEmptyValue;

    switch (m_type) {
    case LengthType::Auto:
    case LengthType::Normal:
    case LengthType::Content:
    case LengthType::Undefined:
        m_intValue = 0;
        break;
    case LengthType::Fixed:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent:
        m_isFloat = other.m_isFloat;
        if (m_isFloat)
            m_floatValue = other.m_floatValue;
        else
            m_intValue = other.m_intValue;
        break;
    case LengthType::Calculated:
        m_calculationValueHandle = std::exchange(other.m_calculationValueHandle, 0);
        break;
    }

    other.m_type = LengthType::Auto;
}

inline Length::~Length()
{
    if (isCalculated())
        deref();
}

inline bool Length::operator==(const Length& other) const
{
    // FIXME: This might be too long to be inline.
    if (type() != other.type() || hasQuirk() != other.hasQuirk())
        return false;
    if (isEmptyValue() || other.isEmptyValue())
        return isEmptyValue() && other.isEmptyValue();
    if (isUndefined())
        return true;
    if (isCalculated())
        return isCalculatedEqual(other);
    return value() == other.value();
}

inline float Length::value() const
{
    ASSERT(!isUndefined());
    ASSERT(!isEmptyValue());
    ASSERT(!isCalculated());
    return m_isFloat ? m_floatValue : m_intValue;
}

inline int Length::intValue() const
{
    ASSERT(!isUndefined());
    ASSERT(!isCalculated());
    // FIXME: Makes no sense to return 0 here but not in the value() function above.
    if (isCalculated())
        return 0;
    return m_isFloat ? static_cast<int>(m_floatValue) : m_intValue;
}

inline float Length::percent() const
{
    ASSERT(isPercent());
    return value();
}

inline LengthType Length::type() const
{
    return static_cast<LengthType>(m_type);
}

inline bool Length::isFloat() const
{
    return m_isFloat;
}

inline bool Length::hasQuirk() const
{
    return m_hasQuirk;
}

inline void Length::setHasQuirk(bool hasQuirk)
{
    m_hasQuirk = hasQuirk;
}

inline bool Length::isFixed() const
{
    return type() == LengthType::Fixed;
}

inline bool Length::isPercent() const
{
    return type() == LengthType::Percent;
}

inline bool Length::isCalculated() const
{
    return type() == LengthType::Calculated;
}

inline bool Length::isPercentOrCalculated() const
{
    return isPercent() || isCalculated();
}

inline bool Length::isSpecified() const
{
    return isFixed() || isPercentOrCalculated();
}

inline bool Length::isRelative() const
{
    return type() == LengthType::Relative;
}

inline bool Length::isNormal() const
{
    return type() == LengthType::Normal;
}

inline bool Length::isAuto() const
{
    return type() == LengthType::Auto;
}

inline bool Length::isUndefined() const
{
    return type() == LengthType::Undefined;
}

inline bool Length::isPositive() const
{
    ASSERT(!isEmptyValue());
    if (isUndefined())
        return false;
    if (isCalculated())
        return true;
    return m_isFloat ? (m_floatValue > 0) : (m_intValue > 0);
}

inline bool Length::isNegative() const
{
    ASSERT(!isEmptyValue());
    if (isUndefined() || isCalculated())
        return false;
    return m_isFloat ? (m_floatValue < 0) : (m_intValue < 0);
}

inline bool Length::isZero() const
{
    ASSERT(!isUndefined());
    ASSERT(!isEmptyValue());
    if (isCalculated() || isAuto())
        return false;
    return m_isFloat ? !m_floatValue : !m_intValue;
}

Length convertTo100PercentMinusLength(const Length&);
Length convertTo100PercentMinusLengthSum(const Length&, const Length&);

inline bool canInterpolateLengths(const Length& from, const Length& to, bool isLengthPercentage)
{
    if (from.type() == to.type())
        return true;

    // Some properties allow for <length-percentage> and <number> values. We must allow animating
    // between a <length> and a <percentage>, but exclude animating between a <number> and either
    // a <length> or <percentage>. We can use Length::isRelative() to determine whether we are
    // dealing with a <number> as opposed to a <length> or <percentage>.
    if (isLengthPercentage) {
        return (from.isFixed() || from.isPercentOrCalculated() || from.isRelative())
            && (to.isFixed() || to.isPercentOrCalculated() || to.isRelative())
            && from.isRelative() == to.isRelative();
    }

    if (from.isCalculated())
        return to.isFixed() || to.isPercentOrCalculated();
    if (to.isCalculated())
        return from.isFixed() || from.isPercentOrCalculated();

    return false;
}

inline bool lengthsRequireInterpolationForAccumulativeIteration(const Length& from, const Length& to)
{
    // If interpolating the values can yield a calc() value, we must go through the interpolation code for iterationComposite.
    return from.isCalculated() || to.isCalculated() || from.type() != to.type();
}

WTF::TextStream& operator<<(WTF::TextStream&, Length);

} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::Length> {
    static bool isEmptyValue(const WebCore::Length& length) { return length.isEmptyValue(); }
    static WebCore::Length emptyValue() { return WebCore::Length::createEmptyValue(); }
};

}
