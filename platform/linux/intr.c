#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "platform.h"
#include "util.h"
#include "net.h"

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

int intr_raise_irq(unsigned int irq) {
  return pthread_kill(tid, (int)irq); // 割り込み処理用のスレッドにシグナルを送信
}

static void* intr_thread(void *arg) {
  int terminate = 0, sig, err;
  struct irq_entry *entry;

  debugf("start...");
  pthread_barrier_wait(&barrier);

  while(!terminate) {
    // sigmaskに登録されているシグナル（今回では，割り込みとしてみなすもの）が発生するまで待機．
    // cf) https://docs.oracle.com/cd/E19455-01/806-2732/gen-75415/index.html
    err = sigwait(&sigmask, &sig);
    if (err) {
      errorf("sigwait() %s", strerror(err));
      break;
    }

    switch (sig) {
      case SIGHUP:
        terminate = 1;
        break;
      case SIGUSR1:
        net_softirq_handler();
        break;

      default:
        for (entry=irqs;entry;entry=entry->next) {
          if (entry->irq == (unsigned int)sig) {
            debugf("ifq=%d, name=%s", entry->irq, entry->name);
            entry->handler(entry->irq, entry->dev);
          }
        }
        break;
    }
  }
  debugf("terminated");
  return NULL;
}

int intr_run(void) {
  int err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
  if (err) {
    errorf("pthread_sigmask() %s", strerror(err));
    return -1;
  }
  err = pthread_create(&tid, NULL, intr_thread, NULL);
  if (err) {
    errorf("pthread_create() %s", strerror(err));
    return -1;
  }

  pthread_barrier_wait(&barrier);
  return 0;
}

void intr_shutdown(void) {
  if (pthread_equal(tid, pthread_self()) != 0) {
    return;
  }

  pthread_kill(tid, SIGHUP);
  pthread_join(tid, NULL);
}

int intr_init(void) {
  tid = pthread_self();
  pthread_barrier_init(&barrier, NULL, 2);
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGHUP); // 割り込み処理スレッドに終了を通知する
  sigaddset(&sigmask, SIGUSR1);

  return 0;
}