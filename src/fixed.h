
#pragma once
#include <cstdint>
#include <cmath>

template<unsigned frac_bits>
class fixed {

    template<unsigned frac_bits2 = frac_bits>
    static fixed<frac_bits2> fromFixed(int v) {
        fixed<frac_bits2> rv;
        rv.val = v;
        return rv;
    }
    template<unsigned frac_bits2 = frac_bits>
    static fixed<frac_bits2> fromFixed(int64_t v) {
        fixed<frac_bits2> rv;
        rv.val = v;
        return rv;
    }
    
    public:

    static constexpr int frac_mul_i = 1 << frac_bits;
    static constexpr double frac_mul_f = 1 << frac_bits;
    int64_t val;

    fixed() {
        val = 0;
    }

    fixed(float v) {
        val = int64_t(v * frac_mul_f);
    }

    template <unsigned other_frac_bits>
    fixed<frac_bits + other_frac_bits> operator*(fixed<other_frac_bits> rhs) const {
        return fromFixed<frac_bits + other_frac_bits>(val * rhs.val);
    }


    bool operator>=(int rhs) const {
        return toInt() >= rhs;
    }

    bool operator>(int rhs) const {
        return toInt() > rhs;
    }

    bool operator>(int64_t rhs) const {
        return toInt64() > rhs;
    }

    bool operator>(fixed<frac_bits> rhs) const {
        return val > rhs.val;
    }

    bool operator<(int rhs) const {
        return toInt() < rhs;
    }

    bool operator<(int64_t rhs) const {
        return toInt64() < rhs;
    }

    bool operator<(const fixed<frac_bits> rhs) const  {
        return val < rhs.val;
    }

    fixed<frac_bits> operator-(const fixed<frac_bits> rhs) const {
        return fromFixed(val - rhs.val);
    }

    fixed<frac_bits> operator+(const fixed<frac_bits> rhs) const {
        return fromFixed(val + rhs.val);
    }

    template <unsigned bits>
    fixed<frac_bits - bits> unfrac() {
        return fromFixed<frac_bits - bits>(val >> bits);
    }

    template <unsigned bits>
    fixed<frac_bits - bits> round() {
        return fromFixed(val + (1<<bits)) >> bits;
    }

    fixed<frac_bits> mul(int rhs) {
        return fromFixed(val * rhs);
    }

    fixed<frac_bits> mul(int64_t rhs) {
        return fromFixed(val * rhs);
    }

    template <unsigned other_frac_bits>
    fixed<frac_bits> mul(fixed<other_frac_bits> rhs) {
        return fromFixed((val * rhs) >> other_frac_bits);
    }

    fixed<frac_bits> abs() {
        return fromFixed(std::abs(val));
    }

    fixed<frac_bits> min(fixed<frac_bits> rhs) {
        return fromFixed(std::min(val, rhs.val));
    }

    fixed<frac_bits> max(fixed<frac_bits> rhs) {
        return fromFixed(std::max(val, rhs.val));
    }

    static fixed<frac_bits> fromInt(int v) {
        return fromFixed(v << frac_bits);
    }

    static fixed<frac_bits> fromInt(int64_t v) {
        return fromFixed(v << frac_bits);
    }

    int toInt() const {
        return val >> frac_bits;
    }

    int64_t toInt64() const {
        return val >> frac_bits;
    }


};

template <unsigned frac_bits>
fixed<frac_bits> operator*(const int lhs, const fixed<frac_bits> rhs) {
    return lhs * rhs.val;
}