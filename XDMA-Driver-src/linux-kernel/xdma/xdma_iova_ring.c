#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include "xdma_cdev.h"
#include "cdev_ctrl.h"

#define XDMA_IOVA_RING_BYTES	(256ULL * 1024ULL * 1024ULL)
#define XDMA_IOVA_MMAP_BASE	0x100000000ULL
#define XDMA_IOVA_MAGIC		'x'

struct xdma_iova_ring_request {
	__u32 logical_device, direction, flags, reserved;
	__u64 length, iova;
};

#define XDMA_IOC_IOVA_ALLOC \
	_IOWR(XDMA_IOVA_MAGIC, 0x40, struct xdma_iova_ring_request)
#define XDMA_IOC_IOVA_RELEASE \
	_IOW(XDMA_IOVA_MAGIC, 0x41, struct xdma_iova_ring_request)

struct iova_pool {
	struct list_head node;
	struct xdma_dev *xdev;
	struct file *owner;
	void *cpu;
	dma_addr_t dma;
	u32 ld, dir, flags;
};

static LIST_HEAD(pools);
static DEFINE_MUTEX(pool_lock);

static struct iova_pool *find_pool(struct xdma_dev *x, u32 ld, u32 dir)
{
	struct iova_pool *p;

	list_for_each_entry(p, &pools, node)
		if (p->xdev == x && p->ld == ld && p->dir == dir)
			return p;

	return NULL;
}

static void stop_logical_device(struct xdma_dev *x, u32 ld, u32 flags);

long xdma_iova_ring_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct xdma_cdev *c = f->private_data;
	struct xdma_iova_ring_request r;
	struct iova_pool *p;
	struct xdma_dev *x;

	if (!c || !c->xdev)
		return -ENODEV;
	x = c->xdev;

	if (cmd != XDMA_IOC_IOVA_ALLOC && cmd != XDMA_IOC_IOVA_RELEASE)
		return -ENOTTY;
	if (copy_from_user(&r, (void __user *)arg, sizeof(r)))
		return -EFAULT;
	if (r.logical_device > 1 || r.direction > 1)
		return -EINVAL;

	mutex_lock(&pool_lock);
	p = find_pool(x, r.logical_device, r.direction);

	if (cmd == XDMA_IOC_IOVA_ALLOC) {
		if (r.length && r.length != XDMA_IOVA_RING_BYTES) {
			mutex_unlock(&pool_lock);
			return -EINVAL;
		}

		if (!p) {
			p = kzalloc(sizeof(*p), GFP_KERNEL);
			if (p) {
				p->cpu = dma_alloc_coherent(&x->pdev->dev,
							   XDMA_IOVA_RING_BYTES,
							   &p->dma,
							   GFP_KERNEL | __GFP_NOWARN);
				if (!p->cpu) {
					kfree(p);
					p = NULL;
				} else {
					p->xdev = x;
					p->ld = r.logical_device;
					p->dir = r.direction;
					p->flags = r.flags;
					list_add_tail(&p->node, &pools);
				}
			}
		}

		if (!p) {
			mutex_unlock(&pool_lock);
			return -ENOMEM;
		}
		if (p->owner && p->owner != f) {
			mutex_unlock(&pool_lock);
			return -EBUSY;
		}

		p->owner = f;
		r.length = XDMA_IOVA_RING_BYTES;
		r.iova = p->dma;
		r.flags = 1;
		mutex_unlock(&pool_lock);

		return copy_to_user((void __user *)arg, &r, sizeof(r)) ?
			-EFAULT : 0;
	}

	if (!p || p->owner != f) {
		mutex_unlock(&pool_lock);
		return -ENOENT;
	}

	u32 release_ld = p->ld, release_flags = p->flags;

	p->owner = NULL;
	stop_logical_device(x, release_ld, release_flags);
	mutex_unlock(&pool_lock);

	return 0;
}

int xdma_iova_ring_mmap(struct file *f, struct vm_area_struct *v)
{
	struct xdma_cdev *c = f->private_data;
	struct iova_pool *p;
	u64 k = ((u64)v->vm_pgoff << PAGE_SHIFT) - XDMA_IOVA_MMAP_BASE;
	u32 ld = (k >> 29) & 1, dir = (k >> 28) & 1;
	unsigned long n = v->vm_end - v->vm_start;
	int rc;

	if (!c || !c->xdev || n > XDMA_IOVA_RING_BYTES)
		return -EINVAL;

	mutex_lock(&pool_lock);
	p = find_pool(c->xdev, ld, dir);
	if (!p || p->owner != f) {
		mutex_unlock(&pool_lock);
		return -EPERM;
	}

	v->vm_pgoff = 0;
	vm_flags_set(v, VM_DONTEXPAND | VM_DONTDUMP);
//	v->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	rc = dma_mmap_coherent(&c->xdev->pdev->dev, v, p->cpu, p->dma,
			       XDMA_IOVA_RING_BYTES);
	mutex_unlock(&pool_lock);

	return rc;
}

void xdma_iova_ring_cleanup(struct xdma_dev *x)
{
	struct iova_pool *p, *t;

	mutex_lock(&pool_lock);
	list_for_each_entry_safe(p, t, &pools, node)
		if (p->xdev == x) {
			list_del(&p->node);
			dma_free_coherent(&x->pdev->dev, XDMA_IOVA_RING_BYTES,
					  p->cpu, p->dma);
			kfree(p);
		}
	mutex_unlock(&pool_lock);
}

static void stop_channel(struct xdma_dev *x, unsigned int channel)
{
	void __iomem *bar;
	u32 off, value;

	if (!x || x->config_bar_idx < 0 || x->config_bar_idx >= XDMA_BAR_NUM)
		return;
	bar = x->bar[x->config_bar_idx];
	if (!bar)
		return;

	for (off = 0; off < 2; ++off) {
		u32 reg = (off ? 0x1004U : 0x0004U) + channel * 0x100U;

		value = ioread32(bar + reg);
		iowrite32(value & ~1U, bar + reg);
	}
}

static void stop_logical_device(struct xdma_dev *x, u32 ld, u32 flags)
{
	unsigned int first = (flags & 1U) ? ld : 0;
	unsigned int count = (flags & 1U) ? 1 : 4;
	unsigned int i;

	for (i = 0; i < count; ++i)
		stop_channel(x, first + i);

	/* Ensure RUN is cleared before the next owner programs the ring. */
	for (i = 0; i < count; ++i) {
		void __iomem *bar = (x && x->config_bar_idx >= 0 &&
				    x->config_bar_idx < XDMA_BAR_NUM) ?
				   x->bar[x->config_bar_idx] : NULL;
		unsigned int c = first + i;
		unsigned int tries;

		if (!bar)
			break;

		for (tries = 0; tries < 100; ++tries) {
			u32 h = ioread32(bar + 0x0040U + c * 0x100U);
			u32 r = ioread32(bar + 0x1040U + c * 0x100U);

			if (!(h & 1U) && !(r & 1U))
				break;
			usleep_range(1000, 2000);
		}
	}
}

void xdma_iova_ring_release_file(struct xdma_dev *x, struct file *f)
{
	struct iova_pool *p;
	struct iova_pool *matched[2];
	unsigned int count = 0, i;

	if (!x || !f)
		return;

	mutex_lock(&pool_lock);
	list_for_each_entry(p, &pools, node) {
		if (p->xdev == x && p->owner == f) {
			bool seen = false;

			for (i = 0; i < count; ++i)
				if (matched[i]->ld == p->ld &&
				    matched[i]->flags == p->flags)
					seen = true;
			p->owner = NULL;
			if (!seen && count < ARRAY_SIZE(matched))
				matched[count++] = p;
		}
	}

	for (i = 0; i < count; ++i)
		stop_logical_device(x, matched[i]->ld, matched[i]->flags);
	mutex_unlock(&pool_lock);
}
