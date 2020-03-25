#include "misc.h"
#include "usertraps.h"

#include "target.h"

int main(int argc, char *argv[])
{
  char message1[] = "S";
  char message2[] = "2O2";
  char message3[] = "2C2";
  char message4[] = "SO4";

  int pid = getpid();
  int mode;
  sem_t remain;
  mbox_t reaction_mbox_array[3];
  int success;
  int i;
  char recv_message[SYNC_LENGTH], recv_message2[SYNC_LENGTH];

  if (argc != 6)
  {
    Printf("FATAL ERROR: pid: %d, reaction param size error\n", pid);
    Exit();
  }
  mode = dstrtol(argv[1], NULL, 10);
  remain = dstrtol(argv[5], NULL, 10);

  Printf("Enter reaction %d, pid = %d\n", mode, pid);

  for (i = 2; i <= 4; ++i)
    reaction_mbox_array[i - 2] = dstrtol(argv[i], NULL, 10);
  success = 1;
  if (mode == 1)
  { // S2 -> S + S
    for (i = 0; i < 2; ++i)
    {
      if (mbox_open(reaction_mbox_array[i]) == MBOX_FAIL)
      {
        success = 0;
        break;
      }
    }
  }
  else
  { // 4CO -> 2O2 + 2C2 or S + 2O2 -> SO4
    for (i = 0; i < 3; ++i)
    {
      if (mbox_open(reaction_mbox_array[i]) == MBOX_FAIL)
      {
        success = 0;
        break;
      }
    }
  }
  if (!success)
  {
    Printf("FATAL ERROR: pid: %d, reaction open mbox error\n", pid);
    Exit();
  }

  Printf("reac opend in pid : %d\n", pid);

  success = 1;
  if (mode == 1)
  {
    if (mbox_recv(reaction_mbox_array[0], SYNC_LENGTH, recv_message) ==
        MBOX_FAIL)
    {
      Printf(
          "FATAL ERROR: pid: %d, reaction 1 (S2 -> S + S) recv S2 mbox error\n",
          pid);
      Exit();
    }

    if (mbox_send(reaction_mbox_array[1], 2, message1) == MBOX_FAIL ||
        mbox_send(reaction_mbox_array[1], 2, message1) == MBOX_FAIL)
    {
      Printf(
          "FATAL ERROR: pid: %d, reaction 1 (S2 -> S + S) send S mbox error\n",
          pid);
      Exit();
    }
    Printf("MESSAGE: pid %d, reaction 1 (S2 -> S + S) successful\n", pid);
  }
  else if (mode == 2)
  {
    if (mbox_recv(reaction_mbox_array[0], SYNC_LENGTH, recv_message) ==
        MBOX_FAIL)
    {
      Printf(
          "FATAL ERROR: pid: %d, reaction 2 (4CO -> 2O2 + 2C2) recv 4CO mbox "
          "error\n",
          pid);
      Exit();
    }
    if (mbox_send(reaction_mbox_array[1], 4, message2) == MBOX_FAIL ||
        mbox_send(reaction_mbox_array[2], 4, message3) == MBOX_FAIL)
    {
      Printf("FATAL ERROR: pid: %d, reaction 2 (4CO -> 2O2 + 2C2) send 2O2 or "
             "2C2 mbox error\n",
             pid);
      Exit();
    }
    Printf("MESSAGE: pid %d, reaction 2 (4CO -> 2O2 + 2C2) successful\n", pid);
  }
  else
  {
    if (mbox_recv(reaction_mbox_array[0], SYNC_LENGTH, recv_message) ==
            MBOX_FAIL ||
        mbox_recv(reaction_mbox_array[1], SYNC_LENGTH, recv_message2) ==
            MBOX_FAIL)
    {
      Printf("FATAL ERROR: pid: %d, reaction 3 (S + 2O2 -> SO4) recv S or 2O2 "
             "mbox error\n",
             pid);
      Exit();
    }
    if (mbox_send(reaction_mbox_array[2], 4, message4) == MBOX_FAIL)
    {
      Printf("FATAL ERROR: pid: %d, reaction 3 (S + 2O2 -> SO4) send SO4 mbox "
             "error\n",
             pid);
      Exit();
    }
    Printf("MESSAGE: pid %d, reaction 3 (S + 2O2 -> SO4) successful\n", pid);
  }

  Printf("reac send in pid %d\n", pid);

  success = 1;
  if (mode == 1)
  { // S2 -> S + S
    for (i = 0; i < 2; ++i)
    {
      if (mbox_close(reaction_mbox_array[i]) == MBOX_FAIL)
      {
        success = 0;
        break;
      }
    }
  }
  else
  { // 4CO -> 2O2 + 2C2 or S + 2O2 -> SO4
    for (i = 0; i < 3; ++i)
    {
      if (mbox_close(reaction_mbox_array[i]) == MBOX_FAIL)
      {
        success = 0;
        break;
      }
    }
  }
  if (!success)
  {
    Printf("FATAL ERROR: pid: %d, reaction close mbox error\n", pid);
    Exit();
  }

  Printf("reac closed in pid %d\n", pid);

  if (sem_signal(remain) == SYNC_FAIL)
  {
    Printf("FATAL ERROR: pid: %d, reaction sem signal error\n", pid);
    Exit();
  }
  return 0;
}
