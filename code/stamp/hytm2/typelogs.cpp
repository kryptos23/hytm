#include "vlock.h"
#include "log.h"
#include "typelogs.h"

TypeLogs::TypeLogs() {
}

TypeLogs::TypeLogs(Thread* Self, long _sz) {
    sz = _sz;
    l = Log(sz, Self);
    f = Log(sz, Self);
}

__INLINE__ void TypeLogs::clear() {
    l.clear();
    f.clear();
}

__INLINE__ bool TypeLogs::lockAll(Thread* Self) {
    return l.lockAll(Self) && f.lockAll(Self);
}

__INLINE__ void TypeLogs::releaseAll(Thread* Self) {
    l.releaseAll(Self);
    f.releaseAll(Self);
}

__INLINE__ bool TypeLogs::validate(Thread* Self) {
    return l.validate<long>(Self) && f.validate<float>(Self);
}

__INLINE__ void TypeLogs::writeForward() {
    l.writeForward<long>();
    f.writeForward<float>();
}

__INLINE__ void TypeLogs::writeReverse() {
    l.writeReverse<long>();
    f.writeReverse<float>();
}

template <typename T>
__INLINE__ void TypeLogs::append(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv) {
    Log *_log = getTypedLog<T>(this);
    _log->append(Addr, Valu, _LockFor, _rdv);
}

template <typename T>
__INLINE__ AVPair* TypeLogs::find(vLock* Lock) {
    Log *_log = getTypedLog<T>(this);
    return _log->findFirst(Lock);
}

template <typename T>
__INLINE__ bool TypeLogs::contains(vLock* Lock) {
    Log *_log = getTypedLog<T>(this);
    return _log->contains(Lock);
}

template <>
inline Log * getTypedLog<long>(TypeLogs * typelogs) {
    return &typelogs->l;
}
template <>
inline Log * getTypedLog<float>(TypeLogs * typelogs) {
    return &typelogs->f;
}