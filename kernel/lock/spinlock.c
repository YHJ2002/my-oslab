#include <kernel/lock.h>
#include <kernel/arch.h>
#include <kernel/print.h>

void spinlock_init(spinlock_t *lk, const char *name)
{
    lk->locked = 0;
    lk->owner = -1;
    lk->name = name;
}
bool spinlock_holding(spinlock_t *lk)
{
    return __atomic_load_n(&lk->locked, __ATOMIC_ACQUIRE) != 0 &&
           __atomic_load_n(&lk->owner, __ATOMIC_RELAXED) == (int)arch_cpu_id();
}
void spinlock_acquire(spinlock_t *lk)
{
    push_off();
    if (spinlock_holding(lk)) panic("acquire");
    while (__atomic_exchange_n(&lk->locked, 1, __ATOMIC_ACQUIRE) != 0)
        ;
    __atomic_store_n(&lk->owner, (int)arch_cpu_id(), __ATOMIC_RELEASE);
}
void spinlock_release(spinlock_t *lk)
{
    if (!spinlock_holding(lk)) panic("release");
    __atomic_store_n(&lk->owner, -1, __ATOMIC_RELEASE);
    __atomic_store_n(&lk->locked, 0, __ATOMIC_RELEASE);
    pop_off();
}
