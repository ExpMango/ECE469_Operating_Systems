#include "misc.h"
#include "usertraps.h"

#include "target.h"

int main(int argc, char *argv[])
{
  char message1[] = "S2";
  char message2[] = "CO";
  int pid = getpid();
  int mode;
  mbox_t inject_box;
  sem_t remain;

  if (argc != 4)
  {
    Printf("FATAL ERROR: pid: %d, inject param size error\n", pid);
    Exit();
  }

  mode = dstrtol(argv[1], NULL, 10);
  inject_box = dstrtol(argv[2], NULL, 10);
  remain = dstrtol(argv[3], NULL, 10);

  if (mbox_open(inject_box) == MBOX_FAIL)
  {
    Printf("FATAL ERROR: pid: %d, open mbox error", pid);
    Exit();
  }

  if (mode == 1)
  {
    if (mbox_send(inject_box, 3, message1) == MBOX_FAIL ||
        mbox_send(inject_box, 3, message1) == MBOX_FAIL)
    {
      Printf("FATAL ERROR: pid: %d, inject S2 send message error\n", pid);
      Exit();
    }
    else
    {
      Printf("Message: pid: %d, inject S2 successful", pid);
    }
  }
  else
  {
    if (mbox_send(inject_box, 3, message1) == MBOX_FAIL ||
        mbox_send(inject_box, 3, message1) == MBOX_FAIL)
    {
      Printf("FATAL ERROR: pid: %d, inject CO send message error\n", pid);
      Exit();
    }
    else
    {
      Printf("Message: pid: %d, inject CO successful", pid);
    }
  }

  if (mbox_close(inject_box) == MBOX_FAIL)
  {
    Printf("FATAL ERROR: pid: %d, inject mbox close error\n", pid);
    Exit();
  }

  if (sem_signal(remain) == SYNC_FAIL)
  {
    Printf("FATAL ERROR: pid: %d, inject sem signal error\n", pid);
    Exit();
  }

  return 0;
}