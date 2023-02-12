#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "platform.h"
#include "util.h"
#include "net.h"

#define LOOPBACK_MTU UINT16_MAX
#define LOOPBACK_IRQ (INTR_IRQ_BASE+1)

// ループバックデバイスのドライバで使用するプライベートなデータ
struct loopback {
  int irq;
  mutex_t mutex;
  struct queue_head queue;
};

static int loopback_transmit(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst) {
  debugf("dev=%s, type~%0x04x, len=%zu", dev->name, type, len);
  debugdump(data, len);

  intr_raise_irq(LOOPBACK_IRQ);
  return 0;
}

static int loopback_isr(unsigned int irq, void *id) {
  debugf("irq=%u, dev=%s", irq, ((struct net_device*)id)->name);
  return 0;
}

static struct net_device_ops loopback_ops = {
  .transmit = loopback_transmit,
};

struct net_device *loopback_init(void) {
  struct net_device *dev;
  struct loopback *lo;

  dev = net_device_alloc();
  if (!dev) {
    errorf("net_device_alloc() failed");
    return NULL;
  }


  lo = memory_alloc(sizeof(*lo));
  if (!lo) {
    errorf("memory_alloc() failure");
    return NULL;
  }
  lo->irq = LOOPBACK_IRQ;
  mutex_init(&lo->mutex);
  queue_init(&lo->queue);

  dev->type = NET_DEVICE_TYPE_LOOPBACK;
  dev->mtu = LOOPBACK_MTU;
  dev->flags = NET_DEVICE_FLAG_LOOPBACK;
  dev->hlen = 0;
  dev->alen = 0;
  dev->priv = lo;

  if (net_device_register(dev) == -1) {
    errorf("net_device_register() failed");
    return NULL;
  }

  intr_request_irq(LOOPBACK_IRQ, loopback_isr, dev->flags, dev->name, dev);

  debugf("initialized, dev=%s", dev->name);
  return dev;
}