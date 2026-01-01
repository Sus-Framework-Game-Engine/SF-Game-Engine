/******************************************************************************/
/* TypeTraits.hpp                                                             */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Monsieur Martin.                                */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
namespace SFTL
{
    template <typename T>
    T &&declval() noexcept; // no definition on purpose

    template <typename T, T val>
    struct integral_constant
    {
        static constexpr T value = val;
        using value_type = T;
        using type = integral_constant<T, val>;
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };
    template <bool b>
    using bool_constant = integral_constant<bool, b>;

    /// The type used as a compile-time boolean with true value.
    using true_type = bool_constant<true>;

    /// The type used as a compile-time boolean with false value.
    using false_type = bool_constant<false>;

    template <typename T>
    struct is_nothrow_move_constructible : bool_constant<noexcept(T(declval<T &&>()))>
    {
    };

    template <typename T>
    constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible_impl<T>();

    template <typename T>
    struct is_nothrow_move_assignable : bool_constant<noexcept(declval<T &>() = declval<T &&>())>
    {
    };

    template <typename T>
    inline constexpr bool is_nothrow_move_assignable_v =
        is_nothrow_move_assignable<T>::value;
}