#include <lkl_host.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <lkl/linux/virtio_ring.h>
#include "../iomem.h"
#include "../virtio.h"
#include "../endian.h"

#define DMA_TO_CPU(x) ((uint64_t)x + 0x700000000000ULL)
#define CPU_TO_DMA(x) ((uint64_t)x - 0x700000000000ULL)

uint32_t virtio_fuzz_get_num_bootdevs(void);

#define VIRTIO_DEV_MAGIC		0x74726976
#define VIRTIO_DEV_VERSION		2

#define VIRTIO_MMIO_MAGIC_VALUE		0x000
#define VIRTIO_MMIO_VERSION		0x004
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_VENDOR_ID		0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL		0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define VIRTIO_MMIO_QUEUE_NUM		0x038
#define VIRTIO_MMIO_QUEUE_READY		0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064
#define VIRTIO_MMIO_STATUS		0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW	0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH	0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW	0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH	0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW	0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH	0x0a4
#define VIRTIO_MMIO_CONFIG_GENERATION	0x0fc
#define VIRTIO_MMIO_CONFIG		0x100
#define VIRTIO_MMIO_INT_VRING		0x01
#define VIRTIO_MMIO_INT_CONFIG		0x02

#define BIT(x) (1ULL << x)

#define virtio_panic(msg, ...) do {					\
  lkl_printf("LKL virtio error" msg, ##__VA_ARGS__);	\
  lkl_host_ops.panic();					\
} while (0)

struct virtio_queue {
  struct lkl_mutex *mu;
  uint32_t num_max;
  uint32_t num;
  uint32_t ready;
  uint32_t max_merge_len;
  int32_t eio;

  struct lkl_vring_desc *desc_tmp;
  struct lkl_vring_avail *avail_tmp;
  struct lkl_vring_used *used_tmp;
  struct lkl_vring_desc *desc;
  struct lkl_vring_avail *avail;
  struct lkl_vring_used *used;
  uint16_t last_avail_idx;
  uint16_t last_used_idx_signaled;
};

struct _virtio_req {
  struct virtio_req req;
  struct virtio_dev *dev;
  struct virtio_queue *q;
  uint16_t idx;
};


static inline void virtio_set_avail_event(struct virtio_queue *q, uint16_t val)
{
  *((uint16_t *)&q->used->ring[q->num]) = val;
}

static inline void virtio_deliver_irq(struct virtio_dev *dev)
{
  dev->int_status |= VIRTIO_MMIO_INT_VRING;
  /* Make sure all memory writes before are visible to the driver. */
  __sync_synchronize();
  lkl_trigger_irq(dev->irq);
}

static inline uint16_t virtio_get_used_idx(struct virtio_queue *q)
{
  return le16toh(q->used->idx);
}

static inline void virtio_add_used(struct virtio_queue *q, uint16_t used_idx,
    uint16_t avail_idx, uint16_t len)
{
  uint16_t desc_idx = q->avail->ring[avail_idx & (q->num - 1)];
  used_idx = used_idx & (q->num - 1);
  q->used->ring[used_idx].id = desc_idx;
  q->used->ring[used_idx].len = htole16(len);
}

/*
 * Make sure all memory writes before are visible to the driver before updating
 * the idx.  We need it here even we already have one in virtio_deliver_irq()
 * because there might already be an driver thread reading the idx and dequeuing
 * used buffers.
 */
static inline void virtio_sync_used_idx(struct virtio_queue *q, uint16_t idx)
{
  __sync_synchronize();
  q->used->idx = htole16(idx);
}

#define min_len(a, b) (a < b ? a : b)

void virtio_fuzz_req_complete(struct virtio_req *req, uint32_t len)
{
  struct _virtio_req *_req = container_of(req, struct _virtio_req, req);
  struct virtio_queue *q = _req->q;
  uint16_t avail_idx = _req->idx;
  uint16_t used_idx = virtio_get_used_idx(_req->q);
  int i;

  /*
   * We've potentially used up multiple (non-chained) descriptors and have
   * to create one "used" entry for each descriptor we've consumed.
   */
  for (i = 0; i < req->buf_count; i++) {
    uint16_t used_len;

    if (!q->max_merge_len)
      used_len = len;
    else
      used_len = min_len(len,  req->buf[i].iov_len);

    virtio_add_used(q, used_idx++, avail_idx++, used_len);

    len -= used_len;
    if (!len)
      break;
  }
  virtio_sync_used_idx(q, used_idx);
  q->last_avail_idx = avail_idx;

  //if(_req->dev->drain_irqs || _req->dev->extra_io || lkl_host_ops.fuzz_ops->get_rem() > 0) {
    virtio_deliver_irq(_req->dev);
  //}
}

  static inline
struct lkl_vring_desc *vring_desc_at_le_idx(struct virtio_queue *q,
    __lkl__virtio16 le_idx)
{
  uint64_t ret = (uint64_t)&q->desc[le16toh(le_idx) & (q->num -1)];
  if(ret < 0x700000000000ULL) {
     //lkl_printf("%s adjust %llx + %llx -> %llx\n", __FUNCTION__, (uint64_t)ret, 0x7f0000000000ULL, (uint64_t)DMA_TO_CPU((uint64_t)ret));
     ret = (uint64_t)DMA_TO_CPU(ret);
  }
  return (struct lkl_vring_desc *)ret;

}

  static inline
struct lkl_vring_desc *vring_desc_at_avail_idx(struct virtio_queue *q,
    uint16_t idx)
{
  uint16_t desc_idx = q->avail->ring[idx & (q->num - 1)];
  uint64_t ret = (uint64_t)vring_desc_at_le_idx(q, desc_idx);
  if(ret < 0x700000000000ULL) {
     //lkl_printf("%s adjust %llx + %llx -> %llx\n", __FUNCTION__, (uint64_t)ret, 0x7f0000000000ULL, (uint64_t)DMA_TO_CPU((uint64_t)ret));
     ret = (uint64_t)DMA_TO_CPU(ret);
  }
  return (struct lkl_vring_desc *)ret;
}

static int add_dev_buf_from_vring_desc(struct virtio_req *req,
    struct lkl_vring_desc *vring_desc)
{
  struct iovec *buf = &req->buf[req->buf_count++];
  uint64_t iov_base;

  iov_base = (uint64_t)le64toh(vring_desc->addr);
  if(iov_base < 0x700000000000ULL) {
     //lkl_printf("%s adjust %llx + %llx -> %llx\n", __FUNCTION__, (uint64_t)iov_base, 0x7f0000000000ULL, (uint64_t)DMA_TO_CPU((uint64_t)iov_base));
     iov_base = (uint64_t)DMA_TO_CPU(iov_base);
  }
  buf->iov_base = (void *)(uintptr_t)iov_base; //(void *)(uintptr_t)le64toh(vring_desc->addr);
  buf->iov_len = le32toh(vring_desc->len);

  if (!(buf->iov_base && buf->iov_len)) {
    return -1;
  }

  req->total_len += buf->iov_len;
  return 0;
}

static struct lkl_vring_desc *get_next_desc(struct virtio_queue *q,
    struct lkl_vring_desc *desc,
    uint16_t *idx)
{
  uint16_t desc_idx;

  if (q->max_merge_len) {
    if (++(*idx) == le16toh(q->avail->idx))
      return NULL;
    desc_idx = q->avail->ring[*idx & (q->num - 1)];
    return vring_desc_at_le_idx(q, desc_idx);
  }

  if (!(le16toh(desc->flags) & LKL_VRING_DESC_F_NEXT))
    return NULL;
  return vring_desc_at_le_idx(q, desc->next);
}

static int virtio_process_one(struct virtio_dev *dev, int qidx)
{
  struct virtio_queue *q = &dev->queue[qidx];
  uint16_t idx = q->last_avail_idx;
  struct _virtio_req _req = {
    .dev = dev,
    .q = q,
    .idx = idx,
  };
  struct virtio_req *req = &_req.req;
  struct lkl_vring_desc *desc = vring_desc_at_avail_idx(q, _req.idx);

  do {
    if(add_dev_buf_from_vring_desc(req, desc)!=0)
      return -1;
    if (q->max_merge_len && req->total_len > q->max_merge_len)
      break;
    desc = get_next_desc(q, desc, &idx);
    //if(dev->extra_io==0)
    //   break;
  } while (desc && req->buf_count < VIRTIO_REQ_MAX_BUFS);

  if (desc && le16toh(desc->flags) & LKL_VRING_DESC_F_NEXT)
    lkl_printf("Warning: too many chained bufs\n");
  return dev->ops->enqueue(dev, qidx, req);
}

void virtio_fuzz_process_queue(struct virtio_dev *dev, uint32_t qidx)
{
  struct virtio_queue *q = NULL;

  // in simulation mode we just trigger an IRQ,
  // the values are injected directly into the shared memory areas
  if(dev->fuzz_dma == 1) {
    virtio_deliver_irq(dev);
    return;
  }

  if(qidx >= dev->nqueues) {
     //lkl_printf("Error: invalid queue index %x (max %x) in %s\n", qidx, dev->nqueues, __FUNCTION__);
     return;
  }

  q = &dev->queue[qidx];
  lkl_host_ops.mutex_lock(q->mu);

  if(q->avail == NULL) {
    goto out;
  }
  if(q->used == NULL) {
    goto out;
  }
  if(q->desc == NULL) {
    goto out;
  }
  if (!q->ready) {
    goto out;
  }

  if(dev->extra_io==0 && lkl_host_ops.fuzz_ops->get_rem() <=0 && q->eio--<=0) {
      goto out;
  }
  if (dev->ops->acquire_queue)
     dev->ops->acquire_queue(dev, qidx);

  while (q->last_avail_idx != le16toh(q->avail->idx)) {
     /*
      * Make sure following loads happens after loading
      * q->avail->idx.
      */
     __sync_synchronize();
     if (virtio_process_one(dev, qidx) < 0)
        break;
     if (q->last_avail_idx == le16toh(q->avail->idx))
        virtio_set_avail_event(q, q->avail->idx);
     if(dev->extra_io==0)
        break;
  }

  if (dev->ops->release_queue)
    dev->ops->release_queue(dev, qidx);
out:
  lkl_host_ops.mutex_unlock(q->mu);
}

static inline uint32_t virtio_read_device_features(struct virtio_dev *dev)
{
  uint32_t val;
  lkl_host_ops.fuzz_ops->get_n(&val, sizeof(val));
  if (dev->device_features_sel) {
    val |= (dev->features_set_mask)>>32;
    val &= (~dev->features_unset_mask)>>32;
  } else {
    val |= dev->features_set_mask;
    val &= ~dev->features_unset_mask;
  }
  return val;
}

static inline void virtio_write_driver_features(struct virtio_dev *dev,
    uint32_t val)
{
   // nothing to see here
}

static bool is_nofuzz_offset(struct virtio_dev *dev, int offset) {
   for(unsigned int i=0; i<dev->n_nofuzz; i++) {
      if(dev->nofuzz[i] == offset) return true;
   }
   return false;
}
static int virtio_read(void *data, int offset, void *res, int size)
{
  uint32_t val;
  struct virtio_dev *dev = (struct virtio_dev *)data;

  //if(offset == VIRTIO_MMIO_INTERRUPT_STATUS) {
  //   val = dev->int_status;
  //   //lkl_printf("VIRTIO_MMIO_INTERRUPT_STATUS ret %x\n", val);
  //   *(uint32_t *)res = htole32(val);
  //   return 0;
  //}
  //if(offset == VIRTIO_MMIO_STATUS) {
  //   val = dev->status;
  //   *(uint32_t *)res = htole32(val);
  //   return 0;
  //}
  //if(offset == VIRTIO_MMIO_QUEUE_READY) {
  //   val = dev->queue[dev->queue_sel].ready;
  //   *(uint32_t *)res = htole32(val);
  //   return 0;
  //}
  if(lkl_host_ops.fuzz_ops->is_active() && !is_nofuzz_offset(dev, offset)) {
    lkl_host_ops.fuzz_ops->get_n(res, size);
    return 0;
  }
  if (offset >= VIRTIO_MMIO_CONFIG) {
    offset -= VIRTIO_MMIO_CONFIG;
    if (offset + size > dev->config_len)
      return -LKL_EINVAL;
    memcpy(res, dev->config_data + offset, size);
    return 0;
  }

  if (size != sizeof(uint32_t))
    return -LKL_EINVAL;

  switch (offset) {
    case VIRTIO_MMIO_MAGIC_VALUE:
      val = VIRTIO_DEV_MAGIC;
      break;
    case VIRTIO_MMIO_VERSION:
      val = VIRTIO_DEV_VERSION;
      break;
    case VIRTIO_MMIO_DEVICE_ID:
      val = dev->device_id;
      break;
    case VIRTIO_MMIO_VENDOR_ID:
      val = dev->vendor_id;
      break;
    case VIRTIO_MMIO_DEVICE_FEATURES:
      val = virtio_read_device_features(dev);
      break;
    case VIRTIO_MMIO_QUEUE_NUM_MAX:
      val = dev->queue[dev->queue_sel].num_max;
      break;
    case VIRTIO_MMIO_QUEUE_READY:
      val = dev->queue[dev->queue_sel].ready;
      break;
    case VIRTIO_MMIO_INTERRUPT_STATUS:
      val = dev->int_status;
      break;
    case VIRTIO_MMIO_STATUS:
      val = dev->status;
      break;
    case VIRTIO_MMIO_CONFIG_GENERATION:
      val = dev->config_gen;
      break;
    default:
      return -1;
  }

  *(uint32_t *)res = htole32(val);

  return 0;
}

static inline void set_ptr_low(void **ptr, uint32_t val)
{
  uint64_t tmp = (uintptr_t)*ptr;

  tmp = (tmp & 0xFFFFFFFF00000000) | val;
  *ptr = (void *)(long)tmp;
}

static inline void set_ptr_high(void **ptr, uint32_t val)
{
  uint64_t tmp = (uintptr_t)*ptr;

  tmp = (tmp & 0x00000000FFFFFFFF) | ((uint64_t)val << 32);
  *ptr = (void *)(long)tmp;
}

static inline void set_status(struct virtio_dev *dev, uint32_t val)
{
  dev->status = val;
}

static int virtio_write(void *data, int offset, void *res, int size)
{
  struct virtio_dev *dev = (struct virtio_dev *)data;
  struct virtio_queue *q = &dev->queue[dev->queue_sel];
  uint32_t val;
  int ret = 0;

  if (offset >= VIRTIO_MMIO_CONFIG) {
    offset -= VIRTIO_MMIO_CONFIG;

    if (offset + size >= dev->config_len)
      return -LKL_EINVAL;
    memcpy(dev->config_data + offset, res, size);
    return 0;
  }


  if (size != sizeof(uint32_t))
    return -LKL_EINVAL;

  val = le32toh(*(uint32_t *)res);

  switch (offset) {
    case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
      if (val > 1)
	return -LKL_EINVAL;
      dev->device_features_sel = val;
      break;
    case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
      if (val > 1)
	return -LKL_EINVAL;
      dev->driver_features_sel = val;
      break;
    case VIRTIO_MMIO_DRIVER_FEATURES:
      virtio_write_driver_features(dev, val);
      break;
    case VIRTIO_MMIO_QUEUE_SEL:
      if(val >= dev->nqueues) {
         //lkl_printf("Error: invalid queue index %x (max %x) in %s:%d\n", val, dev->nqueues, __FUNCTION__, __LINE__);
         dev->queue_sel = 0;
         break;
      }
      dev->queue_sel = val;
      break;
    case VIRTIO_MMIO_QUEUE_NUM:
      dev->queue[dev->queue_sel].num = val;
      break;
    case VIRTIO_MMIO_QUEUE_READY:
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      dev->queue[dev->queue_sel].ready = val;
      dev->queue[dev->queue_sel].last_avail_idx = 0;
      if(val==0) {
         dev->queue[dev->queue_sel].avail = NULL;
         dev->queue[dev->queue_sel].used = NULL;
         dev->queue[dev->queue_sel].desc = NULL;
         dev->queue[dev->queue_sel].avail_tmp = NULL;
         dev->queue[dev->queue_sel].used_tmp = NULL;
         dev->queue[dev->queue_sel].desc_tmp = NULL;
         dev->queue[dev->queue_sel].eio = 0;
      } else {
         dev->queue[dev->queue_sel].eio = 64;
      }
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      if(lkl_fuzz_get_fast_irqs()==1 && val!=0) {
         virtio_fuzz_process_queue(dev, dev->queue_sel);
      }
      break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:
      if(val >= dev->nqueues) {
         //lkl_printf("Error: invalid queue index %x (max %x) %s:%d\n", val, dev->nqueues, __FUNCTION__, __LINE__);
         break;
      }
      virtio_fuzz_process_queue(dev, val);
      break;
    case VIRTIO_MMIO_INTERRUPT_ACK:
      dev->int_status = 0;
      break;
    case VIRTIO_MMIO_STATUS:
      set_status(dev, val);
      break;
    case VIRTIO_MMIO_QUEUE_DESC_LOW:
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      set_ptr_low((void **)&q->desc_tmp, val);
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      break;
    case VIRTIO_MMIO_QUEUE_DESC_HIGH:
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      set_ptr_high((void **)&q->desc_tmp, val);
      if((uint64_t)q->desc_tmp < 0x700000000000ULL) {
         //lkl_printf("VIRTIO_MMIO_QUEUE_desc_HIGH %llx + %llx -> %llx\n", (uint64_t)q->desc, 0x7f0000000000ULL, (uint64_t)DMA_TO_CPU((uint64_t)q->desc));
         q->desc_tmp = (struct lkl_vring_desc *)DMA_TO_CPU((uint64_t)q->desc_tmp);
      }
      q->desc = q->desc_tmp;
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      break;
    case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      set_ptr_low((void **)&q->avail_tmp, val);
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      break;
    case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
      //if((int64_t)val <= 0) {
      //   lkl_printf("WARNING: avail dma ring alloc failed val %llx\n", (uint64_t)val);
      //   break;
      //}
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      set_ptr_high((void **)&q->avail_tmp, val);
      if((uint64_t)q->avail_tmp < 0x700000000000ULL) {
         //lkl_printf("VIRTIO_MMIO_QUEUE_avail_HIGH %llx + %llx -> %llx\n", (uint64_t)q->avail_tmp, 0x7f0000000000ULL, (uint64_t)DMA_TO_CPU((uint64_t)q->avail_tmp));
         q->avail_tmp = (struct lkl_vring_avail *)DMA_TO_CPU((uint64_t)q->avail_tmp);
      }
      q->avail = q->avail_tmp;
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      break;
    case VIRTIO_MMIO_QUEUE_USED_LOW:
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      set_ptr_low((void **)&q->used_tmp, val);
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      break;
    case VIRTIO_MMIO_QUEUE_USED_HIGH:
      lkl_host_ops.mutex_lock(dev->queue[dev->queue_sel].mu);
      set_ptr_high((void **)&q->used_tmp, val);
      if((uint64_t)q->used_tmp < 0x700000000000ULL) {
         //lkl_printf("VIRTIO_MMIO_QUEUE_used_HIGH %llx + %llx -> %llx\n", (uint64_t)q->used_tmp, 0x7f0000000000ULL, (uint64_t)DMA_TO_CPU((uint64_t)q->used_tmp));
         q->used_tmp = (struct lkl_vring_used *)DMA_TO_CPU((uint64_t)q->used_tmp);
      }
      q->used = q->used_tmp;
      lkl_host_ops.mutex_unlock(dev->queue[dev->queue_sel].mu);
      break;
    default:
      ret = -1;
  }

  return ret;
}

static const struct lkl_iomem_ops virtio_ops = {
  .read = virtio_read,
  .write = virtio_write,
};

static uint32_t lkl_num_virtio_boot_devs;

#define IORESOURCE_MEM		0x00000200
int virtio_fuzz_dev_setup(struct virtio_dev *dev, int queues, int num_max, int fuzz_dma)
{
  int qsize = VIO_MAX_QUEUES * sizeof(*dev->queue);
  int mmio_size;
  int i;
  int ret;
  struct lkl_fuzz_platform_dev_config platform_conf;
  dev->irq = lkl_get_free_irq("virtio");
  if (dev->irq < 0)
    return dev->irq;

  dev->int_status = 0;
  dev->device_features |= BIT(LKL_VIRTIO_F_VERSION_1) |
    BIT(LKL_VIRTIO_RING_F_EVENT_IDX);
  dev->queue = lkl_host_ops.mem_alloc(qsize);
  if (!dev->queue)
    return -LKL_ENOMEM;

  memset(dev->queue, 0, qsize);
  for (i = 0; i < queues; i++) {
    dev->queue[i].num_max = num_max;
    dev->queue[i].max_merge_len = 65536;
    dev->queue[i].mu = lkl_host_ops.mutex_alloc(0);
  }

  mmio_size = VIRTIO_MMIO_CONFIG + dev->config_len;
  dev->base = register_iomem(dev, mmio_size, &virtio_ops);

  memset(&platform_conf, 0, sizeof(platform_conf));
  strncpy(platform_conf.name, "virtio-mmio", 256);
  platform_conf.n_mmio = 1;
  platform_conf.mmio_regions[0].remapped = 1;
  platform_conf.mmio_regions[0].start = (uint64_t)dev->base;
  platform_conf.mmio_regions[0].end = (uint64_t)dev->base + mmio_size;
  platform_conf.mmio_regions[0].flags = IORESOURCE_MEM;
  platform_conf.irq = dev->irq;
  platform_conf.fuzz_dma = fuzz_dma;

  if(fuzz_dma == 0) {
    lkl_printf("Mode: Simulation\n");
    ret = lkl_sys_virtio_mmio_device_add((long)dev->base, mmio_size,
	dev->irq);
  } else {
    lkl_printf("Mode: Passthrough\n");
    ret =
      lkl_sys_fuzz_configure_dev(LKL_FDEV_TYPE_PLATFORM, &platform_conf);
  }
  if (ret < 0) {
    lkl_printf("can't register mmio device\n");
    return -1;
  }
  lkl_printf("%s register mmio dev %d\n", __FUNCTION__, ret);
  dev->virtio_mmio_id = lkl_num_virtio_boot_devs + ret;

  return 0;
}

int virtio_fuzz_dev_cleanup(struct virtio_dev *dev)
{
  char devname[100];
  long fd, ret;
  long mount_ret;

  if (!lkl_is_running())
    goto skip_unbind;

  mount_ret = lkl_mount_fs("sysfs");
  if (mount_ret < 0)
    return mount_ret;

  if (dev->virtio_mmio_id >= virtio_fuzz_get_num_bootdevs())
    ret = snprintf(devname, sizeof(devname), "virtio-mmio.%d.auto",
	dev->virtio_mmio_id - virtio_fuzz_get_num_bootdevs());
  else
    ret = snprintf(devname, sizeof(devname), "virtio-mmio.%d",
	dev->virtio_mmio_id);
  if (ret < 0 || (size_t) ret >= sizeof(devname))
    return -LKL_ENOMEM;

  fd = lkl_sys_open("/sysfs/bus/platform/drivers/virtio-mmio/unbind",
      LKL_O_WRONLY, 0);
  if (fd < 0)
    return fd;

  ret = lkl_sys_write(fd, devname, strlen(devname));
  if (ret < 0)
    return ret;

  ret = lkl_sys_close(fd);
  if (ret < 0)
    return ret;

  if (mount_ret == 0) {
    ret = lkl_sys_umount("/sysfs", 0);
    if (ret < 0)
      return ret;
  }

skip_unbind:
  lkl_put_irq(dev->irq, "virtio");
  unregister_iomem(dev->base);
  lkl_host_ops.mem_free(dev->queue);

  return 0;
}

uint32_t virtio_fuzz_get_num_bootdevs(void)
{
  return lkl_num_virtio_boot_devs;
}

struct lkl_virtio_fuzz_dev {
  struct virtio_dev dev;
  uint8_t config[1024];
};

static struct lkl_virtio_fuzz_dev *fuzz_dev = NULL;

// Note(feli): act as if we support everything
static int fuzz_check_features(struct virtio_dev *dev)
{
  return 0;
}

static int fuzz_enqueue(struct virtio_dev *dev, int q, struct virtio_req *req)
{
  int i, cnt, bc=0, n=0;
  struct iovec *iov;
  iov = req->buf;
  cnt = req->buf_count;
  int rem = lkl_host_ops.fuzz_ops->get_rem();
  //if(dev->extra_io && rem <= 0) {
  //  req->buf_count = bc;
  //  virtio_fuzz_req_complete(req, n);
  //  return -1;
  //}
  for(i=0; i<cnt; i++) {
    if(iov->iov_base == NULL) continue;
    n+= iov->iov_len;
    rem = lkl_host_ops.fuzz_ops->get_n(iov->iov_base, iov->iov_len);
    bc++;
    //if(!dev->extra_io && rem <= 0)
    //  break;
  }
  req->buf_count = bc;
  virtio_fuzz_req_complete(req, n);
  return 0;
}

static struct virtio_dev_ops fuzz_ops = {
  .check_features = fuzz_check_features,
  .enqueue = fuzz_enqueue,
};

static void fuzz_virtio_reset_dev(void *dev_handle) {
  unsigned int i;
  struct lkl_virtio_fuzz_dev *dev;
  dev = (struct lkl_virtio_fuzz_dev*) dev_handle;
  for (i = 0; i < dev->dev.nqueues; i++) {
     dev->dev.queue[i].avail = NULL;
     dev->dev.queue[i].used = NULL;
     dev->dev.queue[i].desc = NULL;
     dev->dev.queue[i].avail_tmp = NULL;
     dev->dev.queue[i].used_tmp = NULL;
     dev->dev.queue[i].desc_tmp = NULL;
     dev->dev.queue[i].eio = 0;
  }
}

uint64_t lkl_add_virtio_fuzz_dev(struct lkl_fuzz_virtio_dev_config *conf) {
  int ret;
  struct lkl_virtio_fuzz_dev *dev;

  if(conf->nqueues > VIO_MAX_QUEUES) {
    fprintf(stderr, "Error: too many queues (max: %d)\n", VIO_MAX_QUEUES);
    return -1;
  }
  dev = lkl_host_ops.mem_alloc(sizeof(*dev));
  if (!dev) {
    fprintf(stderr, "fdnet: failed to allocate memory\n");
    /* TODO: propagate the error state, maybe use errno for that? */
    return -1;
  }

  memset(dev, 0, sizeof(*dev));

  dev->dev.device_id = conf->device_id;
  dev->dev.vendor_id = conf->vendor_id;
  dev->dev.ops = &fuzz_ops;
  dev->dev.config_len = sizeof(dev->config);
  dev->dev.config_data = dev->config;
  dev->dev.features_set_mask = conf->features_set_mask;
  dev->dev.features_unset_mask = conf->features_unset_mask;
  dev->dev.drain_irqs = conf->drain_irqs;
  dev->dev.extra_io = conf->extra_io;
  dev->dev.fuzz_dma = conf->fuzz_dma;
  dev->dev.nqueues = conf->nqueues;
  for(unsigned int i=0; i<conf->n_nofuzz; i++) {
     dev->dev.nofuzz[i] = conf->nofuzz[i];
  }
  dev->dev.n_nofuzz = conf->n_nofuzz;
  ret = virtio_fuzz_dev_setup(&dev->dev, conf->nqueues, conf->num_max, conf->fuzz_dma);
  lkl_host_ops.fuzz_ops->set_dev_data((void*)dev, fuzz_virtio_reset_dev, LKL_FDEV_TYPE_VIRTIO, dev->dev.irq);
  fuzz_dev = dev;
  return (uint64_t)dev;
}

int lkl_virtio_send_to_queue(uint64_t handle, int qidx) {
  virtio_fuzz_process_queue(&fuzz_dev->dev, qidx);
  //virtio_deliver_irq(&fuzz_dev->dev);
  return 0;
}

int lkl_virtio_trigger_irq(void) {
   virtio_deliver_irq(&fuzz_dev->dev);
   return 0;
}
