#include "misc.h"
#include "usertraps.h"

#include "target.h"

int main(int argc, char *argv[])
{
  sem_t process_remain;
  int num_S2, num_CO;
  int inject_S2_proc, inject_CO_proc, reaction_S2_proc, reaction_CO_proc, reaction_SO4_proc, max_proc;
  char mode1[] = "1";
  char mode2[] = "2";
  char mode3[] = "3";
  char mbox_S2_str[SYNC_LENGTH], mbox_CO_str[SYNC_LENGTH],
      mbox_S_str[SYNC_LENGTH], mbox_O2_str[SYNC_LENGTH],
      mbox_C2_str[SYNC_LENGTH], mbox_SO4_str[SYNC_LENGTH],
      sem_process_remain_str[SYNC_LENGTH];
  mbox_t mbox_S2, mbox_CO, mbox_S, mbox_O2, mbox_C2, mbox_SO4;
  int i;

  if (argc != 3)
  {
    Printf("ERROR: makeprocs {number of S2} {number of CO}\n");
    return 0;
  }
  num_S2 = dstrtol(argv[1], NULL, 10);
  num_CO = dstrtol(argv[2], NULL, 10);

  if ((mbox_S2 = mbox_create()) == MBOX_FAIL ||
      (mbox_CO = mbox_create()) == MBOX_FAIL ||
      (mbox_S = mbox_create()) == MBOX_FAIL ||
      (mbox_O2 = mbox_create()) == MBOX_FAIL ||
      (mbox_C2 = mbox_create()) == MBOX_FAIL ||
      (mbox_SO4 = mbox_create()) == MBOX_FAIL)
  {
    Printf("FATAL ERROR: %s mbox create faild\n", argv[0]);
    Exit();
  }

  if (mbox_open(mbox_S2) == MBOX_FAIL || mbox_open(mbox_CO) == MBOX_FAIL ||
      mbox_open(mbox_S) == MBOX_FAIL || mbox_open(mbox_O2) == MBOX_FAIL ||
      mbox_open(mbox_C2) == MBOX_FAIL || mbox_open(mbox_SO4) == MBOX_FAIL)
  {
    Printf("FATAL ERROR: %s makeprocs mbox open faild\n", argv[0]);
    Exit();
  }

  inject_S2_proc = num_S2;
  inject_CO_proc = num_CO / 4;
  reaction_S2_proc = num_S2;
  reaction_CO_proc = num_CO / 4;
  reaction_SO4_proc = min(reaction_CO_proc, reaction_S2_proc * 2);
  max_proc = max(reaction_S2_proc, inject_CO_proc);
  max_proc = max(max_proc, reaction_SO4_proc);

  Printf("MMMMMMMMMMMMMM: max_proc = %d, reac S2 = %d, reac CO = %d, SO4 = %d\n", max_proc, reaction_S2_proc, reaction_CO_proc, reaction_SO4_proc);

  if ((process_remain =
           sem_create(-inject_S2_proc - inject_CO_proc - reaction_S2_proc -
                      reaction_CO_proc - reaction_SO4_proc + 1)) == SYNC_FAIL)
  {
    Printf("FATAL ERROR: makeprocs create sem faild\n");
    Exit();
  }

  ditoa(mbox_S2, mbox_S2_str);
  ditoa(mbox_CO, mbox_CO_str);
  ditoa(mbox_S, mbox_S_str);
  ditoa(mbox_O2, mbox_O2_str);
  ditoa(mbox_C2, mbox_C2_str);
  ditoa(mbox_SO4, mbox_SO4_str);
  ditoa(process_remain, sem_process_remain_str);

  for (i = 0; i < max_proc; ++i)
  {
    if (i < inject_S2_proc)
    {
      process_create(INJECT_OBJ, 0, 0, mode1, mbox_S2_str,
                     sem_process_remain_str, NULL);
    }
    if (i < inject_CO_proc)
    {
      process_create(INJECT_OBJ, 0, 0, mode2, mbox_CO_str,
                     sem_process_remain_str, NULL);
    }
    if (i < reaction_S2_proc)
    {
      process_create(REACTION_OBJ, 0, 0, mode1, mbox_S2_str, mbox_S_str,
                     mbox_S_str, sem_process_remain_str, NULL);
    }
    if (i < reaction_CO_proc)
    {
      process_create(REACTION_OBJ, 0, 0, mode2, mbox_CO_str, mbox_O2_str,
                     mbox_C2_str, sem_process_remain_str, NULL);
    }
    if (i < reaction_SO4_proc)
    {
      process_create(REACTION_OBJ, 0, 0, mode3, mbox_S_str, mbox_O2_str,
                     mbox_SO4_str, sem_process_remain_str, NULL);
    }
  }

  if (sem_wait(process_remain) != SYNC_SUCCESS)
  {
    Printf("FATAL ERROR: makeprocs wait sem process remain faild\n");
    Exit();
  }

  if (mbox_close(mbox_S2) == MBOX_FAIL || mbox_close(mbox_CO) == MBOX_FAIL ||
      mbox_close(mbox_S) == MBOX_FAIL || mbox_close(mbox_O2) == MBOX_FAIL ||
      mbox_close(mbox_C2) == MBOX_FAIL || mbox_close(mbox_SO4) == MBOX_FAIL)
  {
    Printf("ERROR: makeprocs some mbox close faild\n");
  }
  else
  {
    Printf("Reaction completed");
  }
  return 0;
}
