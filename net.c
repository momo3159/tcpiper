#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "platform.h"
#include "util.h"
#include "net.h"

static struct net_device *devices;

struct net_device *net_device_alloc(void) {
  struct net_device *dev;

  dev = memory_alloc(sizeof(*dev));
  if (!dev) {
    errorf("memory_alloc() failed");
    return NULL;
  }

  return dev;
}

int net_device_register(struct net_device *dev) {
  static unsigned int index = 0;
  dev->index = index++;
  snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);

  dev->next = devices;
  devices = dev;

  infof("registered, dev=%s, type=0x%04x", dev->name, dev->type);
  return 0;
}

static int net_device_open(struct net_device *dev) {
  if (NET_DEVICE_IS_UP(dev)) {
    // すでに UP 状態なのでエラーとする
    errorf("already opened, dev=%s", dev->name);
    return -1;
  }

  if (dev->ops->open) {
    if (dev->ops->close(dev) == -1) {
      errorf("failure, dev=%s", dev->name);
      return -1;
    }
  }

  dev->flags |= NET_DEVICE_FLAG_UP;
  infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
  return 0;
}

static int net_device_close(struct net_device *dev) {
  if (!NET_DEVICE_IS_UP(dev)) {
    errorf("not opened, dev=%s", dev->name);
    return -1;
  }

  if (dev->ops->close) {
    if (dev->ops->close(dev) == -1) {
      errorf("failure, dev=%s", dev->name);
      return -1;
    }
  }

  dev->flags &= ~NET_DEVICE_FLAG_UP;
  infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
  return 0;
}