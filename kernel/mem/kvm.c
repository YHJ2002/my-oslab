#include <kernel/mem.h>
#include <kernel/print.h>
#include <asm/csr.h>
#include <platform.h>
pgtbl_t kernel_pgtbl;
static const uint64 pte_perm_mask = PTE_R | PTE_W | PTE_X | PTE_U |
                                    PTE_G | PTE_A | PTE_D;
static const uint64 physical_limit = 1UL << 56;

static uint64 vm_range_end(uint64 va, uint64 len)
{
    if ((va & (PAGE_SIZE - 1)) || len == 0 || va >= VA_MAX ||
        va > UINT64_MAX - len)
        panic("invalid virtual range");

    uint64 end = va + len;
    if (end > VA_MAX || end > UINT64_MAX - (PAGE_SIZE - 1))
        panic("virtual range overflow");
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (end > VA_MAX)
        panic("virtual range overflow");
    return end;
}

pte_t *vm_getpte(pgtbl_t root, uint64 va, bool alloc)
{
    if (va >= VA_MAX)
        panic("invalid virtual address");

    pgtbl_t table = root;
    for (int level = 2; level > 0; --level) {
        pte_t *pte = &table[VPN(va, level)];
        if (!(*pte & PTE_V)) {
            if (!alloc)
                return NULL;
            pgtbl_t next = (pgtbl_t)pmem_alloc(true);
            *pte = PA_TO_PTE(next) | PTE_V;
            table = next;
            continue;
        }
        if (*pte & (PTE_R | PTE_W | PTE_X))
            panic("large page in vm_getpte");
        table = (pgtbl_t)(uintptr_t)PTE_TO_PA(*pte);
    }
    return &table[VPN(va, 0)];
}

void vm_mappages(pgtbl_t root, uint64 va, uint64 pa, uint64 len, uint64 perm)
{
    uint64 end = vm_range_end(va, len);
    uint64 mapped_len = end - va;
    if ((pa & (PAGE_SIZE - 1)) || (perm & ~pte_perm_mask) ||
        !(perm & (PTE_R | PTE_X)) || ((perm & PTE_W) && !(perm & PTE_R)) ||
        pa >= physical_limit || mapped_len > physical_limit ||
        pa > physical_limit - mapped_len)
        panic("invalid page mapping");

    for (uint64 current_va = va, current_pa = pa; current_va < end;
         current_va += PAGE_SIZE, current_pa += PAGE_SIZE) {
        pte_t *pte = vm_getpte(root, current_va, true);
        if (*pte & PTE_V) {
            if (!(*pte & (PTE_R | PTE_W | PTE_X)) ||
                PTE_TO_PA(*pte) != current_pa)
                panic("remapping page");
        }
        *pte = PA_TO_PTE(current_pa) | perm | PTE_V | PTE_A | PTE_D;
    }
}

void vm_unmappages(pgtbl_t root, uint64 va, uint64 len, bool free_pages)
{
    uint64 end = vm_range_end(va, len);
    for (uint64 current_va = va; current_va < end; current_va += PAGE_SIZE) {
        pte_t *pte = vm_getpte(root, current_va, false);
        if (pte == NULL || !(*pte & PTE_V) ||
            !(*pte & (PTE_R | PTE_W | PTE_X)))
            panic("unmapping absent page");
        uint64 pa = PTE_TO_PA(*pte);
        *pte = 0;
        if (free_pages)
            pmem_free(pa, false);
    }
}

void kvm_init(void)
{
    extern char kernel_start[];
    uint64 dram_end = DRAM_BASE + DRAM_SIZE;
    kernel_pgtbl = (pgtbl_t)pmem_alloc(true);

    vm_mappages(kernel_pgtbl, (uint64)(uintptr_t)kernel_start,
                (uint64)(uintptr_t)kernel_start,
                (uint64)(uintptr_t)text_end - (uint64)(uintptr_t)kernel_start,
                PTE_R | PTE_X);
    vm_mappages(kernel_pgtbl, (uint64)(uintptr_t)text_end,
                (uint64)(uintptr_t)text_end,
                (uint64)(uintptr_t)rodata_end - (uint64)(uintptr_t)text_end,
                PTE_R);
    vm_mappages(kernel_pgtbl, (uint64)(uintptr_t)rodata_end,
                (uint64)(uintptr_t)rodata_end,
                dram_end - (uint64)(uintptr_t)rodata_end,
                PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, PAGE_SIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE, PLIC_SIZE, PTE_R | PTE_W);
}
void kvm_inithart(void)
{
    csr_write(satp, (8UL << 60) | ((uint64)kernel_pgtbl >> 12));
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");
}
static void print_level(pgtbl_t root, unsigned level)
{
    for (unsigned i = 0; i < 512; i++) {
        pte_t pte = root[i];
        if (!(pte & PTE_V)) continue;
        for (unsigned n = level; n < 3; ++n) printf(".. ");
        printf("level=%d index=%d pa=%p flags=%x\n", (int)level, (int)i,
               (void *)PTE_TO_PA(pte), (unsigned)(pte & 1023));
        if (level && !(pte & (PTE_R | PTE_W | PTE_X))) print_level((pgtbl_t)PTE_TO_PA(pte), level - 1);
    }
}
/* 教师诊断：root 及所有中间表须有效，遍历期间不允许修改。 */
void vm_print(pgtbl_t root)
{ printf("root pgtbl: pa=%p\n", (void *)root); print_level(root, 2); }
