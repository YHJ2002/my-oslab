#include <kernel/arch.h>
#include <kernel/print.h>
#include <platform.h>

static int started;

void main(void)
{
    int cpuid = (int)arch_cpu_id();

    if (arch_is_boot_cpu()) {
        print_init();
        printf("cpu %d is booting!\n", cpuid);
        for (int cpu = 0; cpu < NCPU; cpu++) {
            if (cpu != cpuid && arch_start_cpu((uint64)cpu) != 0)
                panic("start_cpu failed");
        }
        __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0)
            ;
        printf("cpu %d is booting!\n", cpuid);
    }

    arch_park();
}
