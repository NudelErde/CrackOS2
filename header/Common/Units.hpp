#pragma once

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