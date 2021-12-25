#include "Common/Units.hpp"

namespace time {

nanosecond::operator microsecond() {
    return microsecond(value / 1000ull);
}
nanosecond::operator millisecond() {
    return millisecond(value / 1000000ull);
}
nanosecond::operator second() {
    return second(value / 1000000000ull);
}
nanosecond::operator minute() {
    return minute(value / 60000000000ull);
}
nanosecond::operator hour() {
    return hour(value / 3600000000000ull);
}
nanosecond::operator day() {
    return day(value / 86400000000000ull);
}

microsecond::operator nanosecond() {
    return nanosecond(value * 1000ull);
}
microsecond::operator millisecond() {
    return millisecond(value / 1000ull);
}
microsecond::operator second() {
    return second(value / 1000000ull);
}
microsecond::operator minute() {
    return minute(value / 60000000ull);
}
microsecond::operator hour() {
    return hour(value / 3600000000ull);
}
microsecond::operator day() {
    return day(value / 86400000000ull);
}

millisecond::operator nanosecond() {
    return nanosecond(value * 1000000ull);
}
millisecond::operator microsecond() {
    return microsecond(value * 1000ull);
}
millisecond::operator second() {
    return second(value / 1000ull);
}
millisecond::operator minute() {
    return minute(value / 60000ull);
}
millisecond::operator hour() {
    return hour(value / 3600000ull);
}
millisecond::operator day() {
    return day(value / 86400000ull);
}

second::operator nanosecond() {
    return nanosecond(value * 1000000000ull);
}
second::operator microsecond() {
    return microsecond(value * 1000000ull);
}
second::operator millisecond() {
    return millisecond(value * 1000ull);
}
second::operator minute() {
    return minute(value / 60ull);
}
second::operator hour() {
    return hour(value / 3600ull);
}
second::operator day() {
    return day(value / 86400ull);
}

minute::operator nanosecond() {
    return nanosecond(value * 60000000000ull);
}
minute::operator microsecond() {
    return microsecond(value * 60000000ull);
}
minute::operator millisecond() {
    return millisecond(value * 60000ull);
}
minute::operator second() {
    return second(value * 60ull);
}
minute::operator hour() {
    return hour(value / 60ull);
}
minute::operator day() {
    return day(value / 1440ull);
}

hour::operator nanosecond() {
    return nanosecond(value * 3600000000000ull);
}
hour::operator microsecond() {
    return microsecond(value * 360000000ull);
}
hour::operator millisecond() {
    return millisecond(value * 3600000ull);
}
hour::operator second() {
    return second(value * 3600ull);
}
hour::operator minute() {
    return minute(value * 60ull);
}
hour::operator day() {
    return day(value / 24ull);
}

day::operator nanosecond() {
    return nanosecond(value * 86400000000000ull);
}
day::operator microsecond() {
    return microsecond(value * 8640000000ull);
}
day::operator millisecond() {
    return millisecond(value * 86400000ull);
}
day::operator second() {
    return second(value * 86400ull);
}
day::operator minute() {
    return minute(value * 1440ull);
}
day::operator hour() {
    return hour(value * 24ull);
}

}// namespace time