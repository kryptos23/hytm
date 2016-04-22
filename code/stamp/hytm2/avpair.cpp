#include "avpair.h"
#include "vlock.h"

AVPair::AVPair() {}

AVPair::AVPair(AVPair* _Next, AVPair* _Prev, Thread* _Owner, long _Ordinal)
: Next(_Next), Prev(_Prev), LockFor(0), rdv(0), Owner(_Owner), Ordinal(_Ordinal) {
    addr.l = 0;
    value.l = 0;
}

template <>
inline void assignAV<long>(AVPair* e, volatile long* addr, long value) {
    e->addr.l = addr;
    e->value.l = value;
}
template <>
inline void assignAV<float>(AVPair* e, volatile float* addr, float value) {
    e->addr.f = addr;
    e->value.f[0] = value;
}

template <>
inline void replayStore<long>(AVPair* e) {
    *(e->addr.l) = e->value.l;
}
template <>
inline void replayStore<float>(AVPair* e) {
    *(e->addr.f) = e->value.f[0];
}

template <>
inline volatile long* unpackAddr<long>(AVPair* e) {
    return e->addr.l;
}
template <>
inline volatile float* unpackAddr<float>(AVPair* e) {
    return e->addr.f;
}

template <>
inline long unpackValue<long>(AVPair* e) {
    return e->value.l;
}
template <>
inline float unpackValue<float>(AVPair* e) {
    return e->value.f[0];
}
