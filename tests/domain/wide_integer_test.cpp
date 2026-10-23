#include <pulsar/domain/support/wide_integer.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using pulsar::domain::WideInteger;

int fail(std::string_view message) {
  std::cerr << "wide integer test failed: " << message << '\n';
  return 1;
}

bool construction_tests() {
  constexpr auto int64_min = std::numeric_limits<std::int64_t>::min();
  constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();

  if (WideInteger{int64_max}.to_int64() != int64_max) return false;
  if (WideInteger{int64_min}.to_int64() != int64_min) return false;
  if (!WideInteger{int64_min}.is_negative()) return false;
  if (WideInteger{int64_min}.high_word() != 0xFFFFFFFFFFFFFFFFULL) return false;
  if (WideInteger{int64_min}.low_word() != 0x8000000000000000ULL) return false;

  if (!WideInteger{}.is_zero()) return false;
  if (!WideInteger{0}.is_zero()) return false;
  if (WideInteger{1}.is_zero()) return false;

  const auto unsigned_max = std::numeric_limits<std::uint64_t>::max();
  const WideInteger wide_unsigned = WideInteger::from_unsigned(unsigned_max);
  if (wide_unsigned.is_negative()) return false;
  if (wide_unsigned.high_word() != 0 || wide_unsigned.low_word() != unsigned_max) return false;

  const WideInteger words = WideInteger::from_words(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL);
  if (words.high_word() != 0x0123456789ABCDEFULL) return false;
  if (words.low_word() != 0xFEDCBA9876543210ULL) return false;

  return true;
}

bool addition_and_subtraction_tests() {
  if (WideInteger{5} + WideInteger{7} != WideInteger{12}) return false;
  if (WideInteger{5} - WideInteger{7} != WideInteger{-2}) return false;
  if (WideInteger{-5} + WideInteger{-7} != WideInteger{-12}) return false;
  if (WideInteger{-5} - WideInteger{-7} != WideInteger{2}) return false;

  // The result of 2^64 is carried into the high word.
  const WideInteger two_to_64 = (WideInteger{1} << 64);
  if (two_to_64.high_word() != 1 || two_to_64.low_word() != 0) return false;
  if (two_to_64 + WideInteger{-1} !=
      WideInteger::from_unsigned(std::numeric_limits<std::uint64_t>::max())) {
    return false;
  }

  const WideInteger big = WideInteger::from_words(1, 0);
  const WideInteger also_big = WideInteger::from_words(0, 0xFFFFFFFFFFFFFFFFULL);
  if (big - also_big != WideInteger{1}) return false;

  WideInteger accumulator{10};
  accumulator += WideInteger{5};
  accumulator -= WideInteger{20};
  accumulator *= WideInteger{-2};
  if (accumulator != WideInteger{10}) return false;

  return true;
}

bool multiplication_tests() {
  constexpr auto int64_min = std::numeric_limits<std::int64_t>::min();
  constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();
  constexpr auto int32_max = std::numeric_limits<std::int32_t>::max();

  if (WideInteger{0} * WideInteger{int64_max} != WideInteger{0}) return false;
  if (WideInteger{-1} * WideInteger{int64_max} != WideInteger{-int64_max}) return false;
  // Negating INT64_MIN widens to +2^63, which is representable in 128 bits.
  if (WideInteger{int64_min} * WideInteger{-1} !=
      WideInteger::from_words(0, 0x8000000000000000ULL)) {
    return false;
  }

  const WideInteger product = WideInteger{int64_max} * WideInteger{int64_max};
  // (2^63 - 1)^2 = 2^126 - 2^64 + 1
  const WideInteger expected = WideInteger::from_words(0x3FFFFFFFFFFFFFFFULL, 1);
  if (product != expected) return false;

  if (WideInteger{int32_max} * WideInteger{int32_max} != WideInteger{4611686014132420609LL}) {
    return false;
  }

  const WideInteger negative_product = WideInteger{-int32_max} * WideInteger{int32_max};
  if (negative_product != WideInteger{-4611686014132420609LL}) return false;

  return true;
}

bool division_tests() {
  if ((WideInteger{7} / WideInteger{2}) != WideInteger{3}) return false;
  if ((WideInteger{-7} / WideInteger{2}) != WideInteger{-3}) return false;
  if ((WideInteger{7} / WideInteger{-2}) != WideInteger{-3}) return false;
  if ((WideInteger{-7} / WideInteger{-2}) != WideInteger{3}) return false;

  if ((WideInteger{7} % WideInteger{2}) != WideInteger{1}) return false;
  if ((WideInteger{-7} % WideInteger{2}) != WideInteger{-1}) return false;
  if ((WideInteger{7} % WideInteger{-2}) != WideInteger{1}) return false;
  if ((WideInteger{-7} % WideInteger{-2}) != WideInteger{-1}) return false;

  // Division by zero is defined as zero so the type stays noexcept.
  if (!(WideInteger{7} / WideInteger{0}).is_zero()) return false;
  if (!(WideInteger{7} % WideInteger{0}).is_zero()) return false;

  // 2^126 / 2^63 == 2^63
  const WideInteger dividend = WideInteger{1} << 126;
  const WideInteger quotient = dividend / (WideInteger{1} << 63);
  if (quotient != (WideInteger{1} << 63)) return false;

  // The 64-bit fast path must agree with the 128-bit loop it replaces.
  const WideInteger unsigned_max = WideInteger::from_unsigned(
      std::numeric_limits<std::uint64_t>::max());
  if (unsigned_max / WideInteger{2} != WideInteger::from_words(0, 0x7FFFFFFFFFFFFFFFULL)) {
    return false;
  }
  if (unsigned_max % WideInteger{10} != WideInteger{5}) return false;
  if ((WideInteger{1} << 100) / WideInteger{1} != (WideInteger{1} << 100)) return false;

  // Magnitudes of 2^127 (INT128_MIN) are not representable as positive values.
  const WideInteger int128_min = WideInteger::from_words(0x8000000000000000ULL, 0);
  const WideInteger int128_max = WideInteger::from_words(0x7FFFFFFFFFFFFFFFULL,
                                                         std::numeric_limits<std::uint64_t>::max());
  if (int128_min / WideInteger{1} != int128_min) return false;
  if (int128_min % WideInteger{1} != WideInteger{0}) return false;
  if (int128_min / WideInteger{-1} != int128_min) return false;
  if (int128_min / int128_max != WideInteger{-1}) return false;
  if (int128_min % int128_max != WideInteger{-1}) return false;
  if (WideInteger{0} / int128_min != WideInteger{0}) return false;
  if (WideInteger{-5} % int128_min != WideInteger{-5}) return false;

  WideInteger compound{100};
  compound /= WideInteger{7};
  compound %= WideInteger{4};
  if (compound != WideInteger{2}) return false;

  return true;
}

bool comparison_tests() {
  if (!(WideInteger{-1} < WideInteger{0})) return false;
  if (!(WideInteger{0} < WideInteger{1})) return false;
  if (!(WideInteger{-2} < WideInteger{-1})) return false;

  // Values that share the same low word must be ordered by the high word.
  const WideInteger low_only = WideInteger::from_words(0, 0xFFFFFFFFFFFFFFFFULL);
  const WideInteger carry = WideInteger::from_words(1, 0);
  if (!(low_only < carry)) return false;
  if (!(carry > low_only)) return false;
  if (!(low_only <= carry) || !(carry >= low_only)) return false;
  if (low_only == carry || !(low_only != carry)) return false;

  const WideInteger negative_words = WideInteger::from_words(0xFFFFFFFFFFFFFFFFULL, 0);
  if (!(negative_words < WideInteger{0})) return false;
  if (!(negative_words.is_negative())) return false;

  return true;
}

bool shift_tests() {
  const WideInteger one{1};
  if ((one << 63).high_word() != 0 || (one << 63).low_word() != 0x8000000000000000ULL) return false;
  if ((one << 64).high_word() != 1 || (one << 64).low_word() != 0) return false;
  if ((one << 65).high_word() != 2 || (one << 65).low_word() != 0) return false;
  if ((one << 127).high_word() != 0x8000000000000000ULL) return false;

  if (!(one << 128).is_zero()) return false;
  if (!(one << 200).is_zero()) return false;
  if ((one << 0) != one) return false;
  if ((one << -3) != one) return false;

  const WideInteger minus_one{-1};
  if ((minus_one << 64).high_word() != 0xFFFFFFFFFFFFFFFFULL) return false;
  if ((minus_one << 64).low_word() != 0) return false;

  return true;
}

bool native_comparison_tests() {
#if defined(__SIZEOF_INT128__)
  using Native = __int128;
  using NativeUnsigned = unsigned __int128;

  std::uint64_t state = 0x243F6A8885A308D3ULL;
  const auto next = [&state]() noexcept {
    state ^= state << 13U;
    state ^= state >> 7U;
    state ^= state << 17U;
    return state;
  };

  for (int iteration = 0; iteration < 2000; ++iteration) {
    const auto left = static_cast<std::int64_t>(next());
    const auto right = static_cast<std::int64_t>(next());

    const Native product = static_cast<Native>(left) * static_cast<Native>(right);
    const auto product_bits = static_cast<NativeUnsigned>(product);
    const WideInteger wide_product = WideInteger{left} * WideInteger{right};
    if (wide_product.high_word() != static_cast<std::uint64_t>(product_bits >> 64U) ||
        wide_product.low_word() != static_cast<std::uint64_t>(product_bits)) {
      return false;
    }

    // INT64_MIN / -1 is undefined for native integers; the wide type wraps.
    if (right != 0 && !(left == std::numeric_limits<std::int64_t>::min() && right == -1)) {
      const Native quotient = static_cast<Native>(left) / static_cast<Native>(right);
      const Native remainder = static_cast<Native>(left) % static_cast<Native>(right);
      if (static_cast<std::int64_t>(quotient) !=
          static_cast<std::int64_t>(WideInteger{left} / WideInteger{right})) {
        return false;
      }
      if (static_cast<std::int64_t>(remainder) !=
          static_cast<std::int64_t>(WideInteger{left} % WideInteger{right})) {
        return false;
      }
    }
  }

  // Full 128-bit differential for the restoring loop and its edge cases.
  const auto wide_from_native = [](Native value) {
    const auto bits = static_cast<NativeUnsigned>(value);
    return WideInteger::from_words(static_cast<std::uint64_t>(bits >> 64U),
                                   static_cast<std::uint64_t>(bits));
  };
  const auto same_as_native = [](WideInteger wide, Native native) {
    const auto bits = static_cast<NativeUnsigned>(native);
    return wide.high_word() == static_cast<std::uint64_t>(bits >> 64U) &&
           wide.low_word() == static_cast<std::uint64_t>(bits);
  };
  for (int iteration = 0; iteration < 2000; ++iteration) {
    const Native wide_left =
        static_cast<Native>((static_cast<NativeUnsigned>(next()) << 64U) | next());
    Native wide_right = static_cast<Native>((static_cast<NativeUnsigned>(next()) << 64U) | next());
    if (wide_right == 0) wide_right = 1;
    // INT128_MIN / -1 overflows natively; the wide type wraps instead.
    if (wide_left == std::numeric_limits<Native>::min() && wide_right == -1) continue;
    if (!same_as_native(wide_from_native(wide_left) / wide_from_native(wide_right),
                        wide_left / wide_right)) {
      return false;
    }
    if (!same_as_native(wide_from_native(wide_left) % wide_from_native(wide_right),
                        wide_left % wide_right)) {
      return false;
    }
  }
#endif

  return true;
}

} // namespace

int main() {
  if (!construction_tests()) {
    return fail("construction tests");
  }
  if (!addition_and_subtraction_tests()) {
    return fail("addition and subtraction tests");
  }
  if (!multiplication_tests()) {
    return fail("multiplication tests");
  }
  if (!division_tests()) {
    return fail("division tests");
  }
  if (!comparison_tests()) {
    return fail("comparison tests");
  }
  if (!shift_tests()) {
    return fail("shift tests");
  }
  if (!native_comparison_tests()) {
    return fail("native differential tests");
  }

  std::cout << "wide integer tests passed\n";
  return 0;
}
