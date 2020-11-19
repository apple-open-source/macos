//
//  SecTranslocateEnumUtils.h
//  Security
//
//

#ifndef SecTranslocateEnumUtils_h
#define SecTranslocateEnumUtils_h

#include <type_traits>

template<typename Enum>
Enum operator |(Enum lhs, Enum rhs)
{
    static_assert(std::is_enum<Enum>::value,
                  "template parameter is not an enum type");

    using underlying = typename std::underlying_type<Enum>::type;

    return static_cast<Enum> (
        static_cast<underlying>(lhs) |
        static_cast<underlying>(rhs)
    );
}

template<typename Enum>
Enum operator &(Enum lhs, Enum rhs)
{
    static_assert(std::is_enum<Enum>::value,
                  "template parameter is not an enum type");

    using underlying = typename std::underlying_type<Enum>::type;

    return static_cast<Enum> (
        static_cast<underlying>(lhs) &
        static_cast<underlying>(rhs)
    );
}

#endif /* SecTranslocateEnumUtils_h */
