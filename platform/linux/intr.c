#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "platform.h"
#include "util.h"

// IDT の各行を表す
struct irq_entry {
  struct irq_entry *next;
  unsigned int irq; // 割り込み番号
  int (*handler)(unsigned int irq, void *dev); // 割り込みハンドラ
  int flags;
  char name[16]; // デバッグ出力のために用いる
  void *dev; // 割り込みの発生元となるデバイス
};

static struct irq_entry *irqs;
static sigset_t sigmask;

static pthread_t tid;
static pthread_barrier_t barrier;

int intr_request_irq(unsigned int irq, int (*handler)(unsigned int irq, void *dev), int flags, const char *name, void *dev) {
  struct irq_entry *entry;

  debugf("irq=%u, flags=%d, name=%s", irq, flags, name);
  for (entry=irqs;entry;entry=entry->next) {
    if (entry->irq == irq) {
      if (entry->flags ^ INTR_IRQ_SHARED || flags ^ INTR_IRQ_SHARED) {
        // IRQ番号がすでに登録されており，かつIRQ番号の共有が許可されていない→エラー
        errorf("conflicts with already registered IRQs");
        return -1;
      }
    }
  }

  entry = memory_alloc(sizeof(*entry));
  if (!entry) {
    errorf("memory_alloc() failed");
    return -1;
  }

  entry->irq = irq;
  entry->handler = handler;
  entry->flags = flags;
  strncpy(entry->name, name, sizeof(entry->name)-1);
  entry->dev = dev;
  entry->next = irqs;
  irqs = entry;

  sigaddset(&sigmask, irq);
  debugf("registered: irq=%u, name=%s", irq, name);

  return 0;
}
