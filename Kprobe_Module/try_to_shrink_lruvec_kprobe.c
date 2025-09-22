#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/mmzone.h>
#include <linux/mm_inline.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/page-flags.h>
#include <linux/pgtable.h>


static struct kprobe kp;

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct lruvec *lruvec = NULL;
    struct lru_gen_folio *lrugen = NULL;
    int gen, type, zone, page_refcount;

    lruvec = (struct lruvec *)regs->di;

    if (!lruvec) {
        pr_warn("lruvec is NULL\n");
        return 0;
    }

    lrugen = &lruvec->lrugen;
    if (!lrugen) {
        pr_warn("lrugen is NULL\n");
        return 0;
    }

    pr_info("[KPROBE] try_to_shrink_lruvec called\n");
    pr_info("[KPROBE] max_seq: %lu\n", lrugen->max_seq);

    for (gen = 0; gen < MAX_NR_GENS; gen++) {
        for (type = 0; type < ANON_AND_FILE; type++) {
            for (zone = 0; zone < MAX_NR_ZONES; zone++) {
                struct list_head *head;
                struct folio *folio;
                struct page *page;
                long count = 0;

                head = &lrugen->folios[gen][type][zone];

                if (!head || list_empty(head))
                    continue;

                pr_info("Generation %d, type %s, zone %d:\n", gen,
                        (type == 0 ? "anon" : "file"), zone);

                list_for_each_entry(folio, head, lru) {
                    if (!folio) {
                        pr_warn("NULL folio in list\n");
                        break;
                    }
                    
                    page = &folio->page;
                    page_refcount = page_ref_count(page);
                    unsigned long pfn = page_to_pfn(page);
                    unsigned long long page_phys_addr = pfn << PAGE_SHIFT;

                    // bool active = PageActive(page);
                    // bool referenced = PageReferenced(page);
                    bool dirty = PageDirty(page);
                    // bool swapbacked = PageSwapBacked(page);
                    bool writeback = PageWriteback(page);
                    // bool unevictable = PageUnevictable(page);

                    pr_info("(folio: %px) (page addr: %px) (page phys addr: %px) (page ref count: %d)\n", folio, page, page_phys_addr, page_refcount);
                    count++;
                    if (count > 50) {
                        pr_info("  ... more pages omitted\n");
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

static int __init mglru_monitor_init(void)
{
    int ret;

    kp.symbol_name = "try_to_shrink_lruvec";
    kp.pre_handler = handler_pre;

    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed, returned %d\n", ret);
        return ret;
    }

    pr_info("MGLRU monitor module loaded.\n");
    return 0;
}

static void __exit mglru_monitor_exit(void)
{
    unregister_kprobe(&kp);
    pr_info("MGLRU monitor module unloaded.\n");
}

module_init(mglru_monitor_init);
module_exit(mglru_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WuTa");
MODULE_DESCRIPTION("Print folio addresses in MGLRU generations");
