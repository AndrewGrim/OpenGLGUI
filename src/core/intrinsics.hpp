#ifndef AGRO_INTRINSICS_HPP
    #define AGRO_INTRINSICS_HPP

    #include <immintrin.h>

    #ifdef _MSC_VER
        #include <intrin.h>

        static inline u32 __builtin_ctz(u32 n) {
            assert(sizeof(u32) == sizeof(unsigned long) && "These should have the same size!");
            u32 value;
            _BitScanForward((unsigned long*)&value, n);
            return value;
        }

        static inline u16 __builtin_popcount(u16 n) {
            return __popcnt16(n);
        }

        static inline u32 __builtin_popcount(u32 n) {
            return __popcnt(n);
        }

        static inline u64 __builtin_popcount(u64 n) {
            return __popcnt64(n);
        }
    #endif

    // TODO would be good to do ARM NEON as well
    #ifdef AVX2
        #define SIMD_WIDTH 32
    #elif defined SSE2
        #define SIMD_WIDTH 16
    #else
        // Fallback to non-intrinsic code so we dont need a separate non-simd code path.
        #define SIMD_WIDTH 8
    #endif

    namespace simd {
        template <typename T, u8 size> struct Vector {};

        template <> struct Vector<u8, 8> {
            u8 _data[8];

            Vector(u8 data) { memset(_data, data, 8); }
            Vector(const u8 *data) { load(data); }
            void load(const u8 *data) { memcpy(_data, data, 8); }

            u8 mask() const {
                u8 m = 0;
                for (u8 i = 0; i < 8; i++) {
                    m = (m >> 1) + (_data[i] & 0b1000'0000);
                }
                return m;
            }

            Vector equal(const Vector &rhs) const {
                Vector result((u8)0);
                for (u8 i = 0; i < 8; i++) {
                    result._data[i] = (*this)._data[i] == rhs._data[i] ? 255 : 0;
                }
                return result;
            }

            Vector notEqual(const Vector &rhs) const {
                Vector result((u8)0);
                for (u8 i = 0; i < 8; i++) {
                    result._data[i] = (*this)._data[i] != rhs._data[i] ? 255 : 0;
                }
                return result;
            }

            Vector lessThan(const Vector &rhs) const {
                Vector result((u8)0);
                for (u8 i = 0; i < 8; i++) {
                    result._data[i] = (*this)._data[i] < rhs._data[i] ? 255 : 0;
                }
                return result;
            }

            Vector greaterThan(const Vector &rhs) const {
                Vector result((u8)0);
                for (u8 i = 0; i < 8; i++) {
                    result._data[i] = (*this)._data[i] > rhs._data[i] ? 255 : 0;
                }
                return result;
            }

            friend u8 operator==(const Vector &lhs, const Vector &rhs) { return lhs.equal(rhs).mask(); }
            friend u8 operator!=(const Vector &lhs, const Vector &rhs) { return lhs.notEqual(rhs).mask(); }
            friend u8 operator<(const Vector &lhs, const Vector &rhs) { return lhs.lessThan(rhs).mask(); }
            friend u8 operator>(const Vector &lhs, const Vector &rhs) { return lhs.greaterThan(rhs).mask(); }
        };

        template <> struct Vector<u8, 16> {
            __m128i _data;
            Vector(__m128i data) { _data = data; }
            Vector(u8 data) { _data = _mm_set1_epi8(data); }
            Vector(const u8 *data) { load(data); }
            void load(const u8 *data) { _data = _mm_loadu_si128((const __m128i*)data); }
            u16 mask() const { return _mm_movemask_epi8(_data); }
            Vector equal(const Vector &rhs) const { return Vector(_mm_cmpeq_epi8(_data, rhs._data)); }
            Vector notEqual(const Vector &rhs) const { return Vector(_mm_andnot_si128(_mm_cmpeq_epi8(_data, rhs._data), _mm_set1_epi8(-1))); }
            Vector lessThan(const Vector &rhs) const { return Vector(_mm_cmplt_epi8(_data, rhs._data)); }
            Vector greaterThan(const Vector &rhs) const { return Vector(_mm_cmpgt_epi8(_data, rhs._data)); }
            friend u16 operator==(const Vector &lhs, const Vector &rhs) { return lhs.equal(rhs).mask(); }
            // Note: We are using ~mask rather than directly notEqual because of extra instructions.
            friend u16 operator!=(const Vector &lhs, const Vector &rhs) { return ~lhs.equal(rhs).mask(); }
            friend u16 operator<(const Vector &lhs, const Vector &rhs) { return lhs.lessThan(rhs).mask(); }
            friend u16 operator>(const Vector &lhs, const Vector &rhs) { return lhs.greaterThan(rhs).mask(); }
        };

        template <> struct Vector<u8, 32> {
            __m256i _data;
            Vector(__m256i data) { _data = data; }
            Vector(u8 data) { _data = _mm256_set1_epi8(data); }
            Vector(const u8 *data) { load(data); }
            void load(const u8 *data) { _data = _mm256_loadu_si256((const __m256i*)data); }
            u32 mask() const { return _mm256_movemask_epi8(_data); }
            Vector equal(const Vector &rhs) const { return Vector(_mm256_cmpeq_epi8(_data, rhs._data)); }
            Vector notEqual(const Vector &rhs) const { return Vector(_mm256_andnot_si256(_mm256_cmpeq_epi8(_data, rhs._data), _mm256_set1_epi8(-1))); }
            Vector lessThan(const Vector &rhs) const { return Vector(_mm256_cmpgt_epi8(rhs._data, _data)); }
            Vector greaterThan(const Vector &rhs) const { return Vector(_mm256_cmpgt_epi8(_data, rhs._data)); }
            friend u32 operator==(const Vector &lhs, const Vector &rhs) { return lhs.equal(rhs).mask(); }
            // Note: We are using ~mask rather than directly notEqual because of extra instructions.
            friend u32 operator!=(const Vector &lhs, const Vector &rhs) { return ~lhs.equal(rhs).mask(); }
            friend u32 operator<(const Vector &lhs, const Vector &rhs) { return lhs.lessThan(rhs).mask(); }
            friend u32 operator>(const Vector &lhs, const Vector &rhs) { return lhs.greaterThan(rhs).mask(); }
        };
    }
#endif
