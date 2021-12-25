#pragma once
#include <stdint.h>

constexpr inline unsigned long long int operator"" Ki(unsigned long long int t) {
    return t * 1024;
}

constexpr inline unsigned long long int operator"" Mi(unsigned long long int t) {
    return t * 1024Ki;
}

constexpr inline unsigned long long int operator"" Gi(unsigned long long int t) {
    return t * 1024Mi;
}

constexpr inline unsigned long long int operator"" Ti(unsigned long long int t) {
    return t * 1024Gi;
}

namespace time {

struct nanosecond;
struct microsecond;
struct millisecond;
struct second;
struct minute;
struct hour;
struct day;

struct nanosecond {
    uint64_t value;

    nanosecond(uint64_t value) : value(value) {}
    operator microsecond();
    operator millisecond();
    operator second();
    operator minute();
    operator hour();
    operator day();
};

struct microsecond {
    uint64_t value;

    microsecond(uint64_t value) : value(value) {}
    operator nanosecond();
    operator millisecond();
    operator second();
    operator minute();
    operator hour();
    operator day();
};

struct millisecond {
    uint64_t value;

    millisecond(uint64_t value) : value(value) {}
    operator nanosecond();
    operator microsecond();
    operator second();
    operator minute();
    operator hour();
    operator day();
};

struct second {
    uint64_t value;

    second(uint64_t value) : value(value) {}
    operator nanosecond();
    operator microsecond();
    operator millisecond();
    operator minute();
    operator hour();
    operator day();
};

struct minute {
    uint64_t value;

    minute(uint64_t value) : value(value) {}
    operator nanosecond();
    operator microsecond();
    operator millisecond();
    operator second();
    operator hour();
    operator day();
};


struct hour {
    uint64_t value;

    hour(uint64_t value) : value(value) {}
    operator nanosecond();
    operator microsecond();
    operator millisecond();
    operator second();
    operator minute();
    operator day();
};

struct day {
    uint64_t value;

    day(uint64_t value) : value(value) {}
    operator nanosecond();
    operator microsecond();
    operator millisecond();
    operator second();
    operator minute();
    operator hour();
};


inline nanosecond operator"" ns(unsigned long long int t) {
    return nanosecond(t);
}

inline microsecond operator"" us(unsigned long long int t) {
    return microsecond(t);
}

inline millisecond operator"" ms(unsigned long long int t) {
    return millisecond(t);
}

inline second operator"" s(unsigned long long int t) {
    return second(t);
}

inline minute operator"" m(unsigned long long int t) {
    return minute(t);
}

inline hour operator"" h(unsigned long long int t) {
    return hour(t);
}

inline day operator"" d(unsigned long long int t) {
    return day(t);
}

}