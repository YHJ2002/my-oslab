/* 汇编已建立栈并保存 hartid。 */
#include <kernel/arch.h>
#include <kernel/print.h>
extern void main(void);

void start(uint64 hartid, uint64 opaque)
{
    (void)hartid;
    (void)opaque;
    arch_early_init();
    main();
    panic("main returned");
}
