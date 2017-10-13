// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

// [Dependencies]
#include "../base/algorithm.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

// ============================================================================
// [asmjit::Algorithm]
// ============================================================================

#if defined(ASMJIT_TEST)
template<typename T>
static void testArrays(const T* a, const T* b, size_t len) noexcept {
  for (size_t i = 0; i < len; i++)
    EXPECT(a[i] == b[i], "Mismatch at %u", unsigned(i));
}

UNIT(base_algorithm) {
  INFO("Testing qsort and isort of predefined arrays");
  {
    constexpr size_t kArraySize = 11;

    int ref_[kArraySize] = { -4, -2, -1, 0, 1, 9, 12, 13, 14, 19, 22 };
    int arr1[kArraySize] = { 0, 1, -1, 19, 22, 14, -4, 9, 12, 13, -2 };
    int arr2[kArraySize];

    std::memcpy(arr2, arr1, kArraySize * sizeof(int));

    Algorithm::iSortT(arr1, kArraySize);
    Algorithm::qSortT(arr2, kArraySize);
    testArrays(arr1, ref_, kArraySize);
    testArrays(arr2, ref_, kArraySize);
  }

  INFO("Testing qsort and isort of artificial arrays");
  {
    constexpr size_t kArraySize = 200;

    int arr1[kArraySize];
    int arr2[kArraySize];
    int ref_[kArraySize];

    for (size_t len = 2; len < kArraySize; len++) {
      for (size_t i = 0; i < len; i++) {
        arr1[i] = int(len - 1 - i);
        arr2[i] = int(len - 1 - i);
        ref_[i] = int(i);
      }

      Algorithm::iSortT(arr1, len);
      Algorithm::qSortT(arr2, len);
      testArrays(arr1, ref_, len);
      testArrays(arr2, ref_, len);
    }
  }

  INFO("Testing qsort and isort having unstable compare function");
  {
    constexpr size_t kArraySize = 5;

    float arr1[kArraySize] = { 1.0f, 0.0f, 3.0f, -1.0f, std::numeric_limits<float>::quiet_NaN() };
    float arr2[kArraySize] = { };

    std::memcpy(arr2, arr1, kArraySize * sizeof(float));

    // We don't test as it's undefined where the NaN would be.
    Algorithm::iSortT(arr1, kArraySize);
    Algorithm::qSortT(arr2, kArraySize);
  }
}

#endif // ASMJIT_TEST

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"
