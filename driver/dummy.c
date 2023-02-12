#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "util.h"
#include "net.h"

#define DUMMY_MTU UINT16_MAX

static int dummy_transmit(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst) {
  debugf("dev=%s, type=%0x04x, len=%zu", dev->name, type, len);
  debugdump(data, len);

  // 送信時に何もしない→データを破棄する
  return 0;
}

static struct net_device_ops dummy_ops = {
  .transmit = dummy_transmit,
};

struct net_device* dummy_init(void) {
  struct net_device *dev;

  dev = net_device_alloc();
  if (!dev) {
    errorf("net_device_alloc() failed");
    return NULL;
  }

  dev->type = NET_DEVICE_TYPE_DUMMY;
  dev->mtu = DUMMY_MTU;
  dev->hlen = 0;
  dev->alen = 0;
  dev->ops = &dummy_ops;
  if (net_device_register(dev) == -1) {
    errorf("net_device_register() failed");
    return NULL;
  }

  debugf("initialized, dev=%s", dev->name);
  return dev;
}