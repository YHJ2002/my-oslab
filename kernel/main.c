#include <kernel/arch.h>
#include <kernel/mem.h>
#include <kernel/print.h>
#include <platform.h>
#include <kernel/string.h>
static int started;
//extern void test_case_1(void);
//extern void test_case_2(void);
//void test_mapping_and_unmapping(void);
void main(void)
{
    int cpuid = (int)arch_cpu_id();

    if (arch_is_boot_cpu()) {
        print_init();
        pmem_init();
        //test_case_1();   // 临时测试
        //test_case_2();   // 临时测试
        kvm_init();
        //test_mapping_and_unmapping();
        kvm_inithart();
        printf("cpu %d is booting!\n", cpuid);
        for (int cpu = 0; cpu < NCPU; cpu++) {
            if (cpu != cpuid && arch_start_cpu((uint64)cpu) != 0)
                panic("start_cpu failed");
        }
        __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    } else {
        kvm_inithart();
        while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0)
            ;
        printf("cpu %d is booting!\n", cpuid);
    }

    arch_park();
}
