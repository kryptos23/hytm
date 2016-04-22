#include "vlock.h"

vLockSnapshot::vLockSnapshot() {
}

vLockSnapshot::vLockSnapshot(uintptr_t _lockstate) {
    lockstate = _lockstate;
}

bool vLockSnapshot::isLocked() {
    return lockstate & LOCKBIT;
}

bool vLockSnapshot::version() {
    return lockstate & ~LOCKBIT;
}

vLock::vLock(uintptr_t lockstate) {
    lock = lockstate;
}

vLock::vLock() {
    lock = 0;
}

vLockSnapshot vLock::getSnapshot() {
    return vLockSnapshot(lock);
}

bool vLock::tryAcquire() {
    uintptr_t val = lock & ~LOCKBIT;
    return __sync_bool_compare_and_swap(&lock, val, val + 1);
}

void vLock::release() {
    ++lock;
}

void vLock::htmIncrementVersion() {
    lock += LOCKBIT * 2;
}