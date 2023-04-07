#ifndef _CPR_H_
#define _CPR_H_

#include <linux/aio_abi.h>
#include <sys/syscall.h>
#include <unistd.h>

long io_setup(unsigned int nr_events, aio_context_t *ctx_idp) {
  return syscall(SYS_io_setup, nr_events, ctx_idp);
}

int io_destroy(aio_context_t ctx_id) { return syscall(SYS_io_destroy, ctx_id); }

int io_submit(aio_context_t ctx_id, long nr, struct iocb **iocbpp) {
  return syscall(SYS_io_submit, ctx_id, nr, iocbpp);
}

int io_getevents(aio_context_t ctx_id, long min_nr, long nr,
                 struct io_event *events, struct timespec *timeout) {
  return syscall(SYS_io_getevents, ctx_id, min_nr, nr, events, timeout);
}

int io_cancel(aio_context_t ctx_id, struct iocb *iocb,
              struct io_event *result) {
  return syscall(SYS_io_cancel, ctx_id, iocb, result);
}

#endif
