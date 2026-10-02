/* 空闲页面的开头存放链表节点，分配后归调用者使用。 */
#include <kernel/mem.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <platform.h>
/* 两个独立的私有池；不能在模块外直接操作链表或计数。 */
static alloc_region_t kern_region __attribute__((unused));
static alloc_region_t user_region __attribute__((unused));
static uint64 page_align_up(uint64 value)
{
    if (value > UINT64_MAX - (PAGE_SIZE - 1))
        panic("pmem address overflow");
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static void region_init(alloc_region_t *region, uint64 begin, uint64 end,
                        const char *name)
{
    if (begin >= end || (begin & (PAGE_SIZE - 1)) ||
        (end & (PAGE_SIZE - 1)))
        panic("invalid physical region");

    region->begin = begin;
    region->end = end;
    region->allocable = 0;
    region->list_head.next = NULL;
    spinlock_init(&region->lk, name);

    for (uint64 pa = begin; pa < end; pa += PAGE_SIZE) {
        page_node_t *node = (page_node_t *)(uintptr_t)pa;
        node->next = region->list_head.next;
        region->list_head.next = node;
        region->allocable++;
    }
}

void pmem_init(void)
{
    uint64 begin = page_align_up((uint64)(uintptr_t)kernel_end);
    uint64 end = DRAM_BASE + DRAM_SIZE;

    if (end < DRAM_BASE || begin >= end)
        panic("invalid physical memory");

    uint64 kernel_end_pa = begin + (uint64)KERNEL_PAGES * PAGE_SIZE;
    if (kernel_end_pa < begin || kernel_end_pa > end)
        panic("not enough physical memory");

    region_init(&kern_region, begin, kernel_end_pa, "kernel memory");
    region_init(&user_region, kernel_end_pa, end, "user memory");
}

void *pmem_alloc(bool kernel)
{
    alloc_region_t *region = kernel ? &kern_region : &user_region;
    spinlock_acquire(&region->lk);
    page_node_t *node = region->list_head.next;
    if (node == NULL) {
        spinlock_release(&region->lk);
        panic("out of physical memory");
    }
    region->list_head.next = node->next;
    if (region->allocable == 0) {
        spinlock_release(&region->lk);
        panic("physical memory counter underflow");
    }
    region->allocable--;
    spinlock_release(&region->lk);
    memset(node, 0, PAGE_SIZE);
    return node;
}

void pmem_free(uint64 pa, bool kernel)
{
    alloc_region_t *region = kernel ? &kern_region : &user_region;
    if ((pa & (PAGE_SIZE - 1)) || pa < region->begin ||
        pa >= region->end || pa > UINT64_MAX - PAGE_SIZE)
        panic("invalid physical page");

    spinlock_acquire(&region->lk);
    for (page_node_t *node = region->list_head.next; node; node = node->next)
        if ((uint64)(uintptr_t)node == pa) {
            spinlock_release(&region->lk);
            panic("physical page already free");
        }

    if (region->allocable >= (region->end - region->begin) / PAGE_SIZE) {
        spinlock_release(&region->lk);
        panic("physical memory counter overflow");
    }
    page_node_t *node = (page_node_t *)(uintptr_t)pa;
    node->next = region->list_head.next;
    region->list_head.next = node;
    region->allocable++;
    spinlock_release(&region->lk);
}

