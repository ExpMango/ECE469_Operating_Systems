#include "dlxos.h"
#include "ostraps.h"
#include "process.h"
#include "queue.h"
#include "synch.h"
#include "mbox.h"

static mbox mbox_array[MBOX_NUM_MBOXES];
static mbox_message mbox_message_array[MBOX_NUM_BUFFERS];
static int curr_message_use = 0;
static lock_t global_create_lock = INVALID_LOCK;
static lock_t global_find_empty_buffer_lock = INVALID_LOCK;

//-------------------------------------------------------
//
// void MboxModuleInit();
//
// Initialize all mailboxes.  This process does not need
// to worry about synchronization as it is called at boot
// time.  Only initialize necessary items here: you can
// initialize others in MboxCreate.  In other words,
// don't waste system resources like locks and semaphores
// on unused mailboxes.
//
//-------------------------------------------------------

void MboxModuleInit()
{
  int i;
  for (i = 0; i < MBOX_NUM_BUFFERS; ++i)
  {
    mbox_message_array[i].size = mbox_message_array[i].inuse = 0;
  }
  for (i = 0; i < MBOX_NUM_MBOXES; ++i)
  {
    mbox_array[i].inuse = 0;
    mbox_array[i].mbox_lock = INVALID_LOCK;
    mbox_array[i].blank_cond = mbox_array[i].data_cond = INVALID_COND;
  }
}

//-------------------------------------------------------
//
// mbox_t MboxCreate();
//
// Allocate an available mailbox structure for use.
//
// Returns the mailbox handle on success
// Returns MBOX_FAIL on error.
//
//-------------------------------------------------------
mbox_t MboxCreate()
{
  int mbox_id = -1;
  int i;
  mbox *mbox_ptr;
  int success = 1;

  if (global_create_lock == INVALID_LOCK)
    global_create_lock = LockCreate();
  LockHandleAcquire(global_create_lock);
  for (i = 0; i < MBOX_NUM_MBOXES; ++i)
  {
    if (mbox_array[i].inuse == 0)
    {
      mbox_id = i;
      break;
    }
  }
  if (mbox_id == -1)
  {
    printf("ERROR: In MboxCreate, no empty mbox remain\n");
    LockHandleRelease(global_create_lock);
    return MBOX_FAIL;
  }
  mbox_ptr = &mbox_array[mbox_id];

  if (success && ((mbox_ptr->mbox_lock = LockCreate()) == INVALID_LOCK))
  {
    printf("ERROR: In MboxCreate, create lock faild\n");
    success = 0;
  }

  if (success && ((mbox_ptr->blank_cond =
		   CondCreate(mbox_ptr->mbox_lock)) == INVALID_COND))
  {
    printf("ERROR: In MboxCreate, create blank_cond faild\n");
    success = 0;
  }

  if (success &&
      ((mbox_ptr->data_cond = CondCreate(mbox_ptr->mbox_lock)) == INVALID_COND))
  {
    printf("ERROR: In MboxCreate, create data_cond faild\n");
    success = 0;
  }

  if (success && (AQueueInit(&mbox_ptr->message_queue) == QUEUE_FAIL))
  {
    printf("ERROR: In MboxCreate, create queue faild\n");
    success = 0;
  }

  if (!success)
  {
    mbox_ptr->blank_cond = mbox_ptr->data_cond = INVALID_COND;
    mbox_ptr->mbox_lock = INVALID_LOCK;
    LockHandleRelease(global_create_lock);
    return MBOX_FAIL;
  }

  mbox_ptr->inuse = 1;
  LockHandleRelease(global_create_lock);
  return mbox_id;
}

//-------------------------------------------------------
//
// void MboxOpen(mbox_t);
//
// Open the mailbox for use by the current process.  Note
// that it is assumed that the internal lock/mutex handle
// of the mailbox and the inuse flag will not be changed
// during execution.  This allows us to get the a valid
// lock handle without a need for synchronization.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//-------------------------------------------------------
int MboxOpen(mbox_t handle)
{
  mbox *mbox_ptr;
  unsigned curr_pid;
  int is_in_list, i;

  if (handle < 0 || handle >= MBOX_NUM_MBOXES)
  {
    printf("ERROR: MboxOpen handle out of range\n");
    return MBOX_FAIL;
  }
  mbox_ptr = &mbox_array[handle];
  LockHandleAcquire(mbox_array[handle].mbox_lock);

  if (mbox_ptr->inuse == 0)
  {
    printf("ERROR: MboxOpen handle not in use\n");
    return MBOX_FAIL;
  }

  // int ori_interrupt = DisableIntrs();
  curr_pid = GetCurrentPid();
  is_in_list = 0;
  for (i = 0; i < mbox_ptr->open_process_count; ++i)
  {
    if (mbox_ptr->open_process_list[i] == curr_pid)
      is_in_list = 1;
  }

  if (is_in_list)
  {
    printf("ERROR: MboxOpen handle has in process list\n");
    LockHandleRelease(mbox_array[handle].mbox_lock);
    // RestoreIntrs(ori_interrupt);
    return MBOX_FAIL;
  }

  mbox_ptr->open_process_list[mbox_ptr->open_process_count++] = curr_pid;
  LockHandleRelease(mbox_array[handle].mbox_lock);
  // RestoreIntrs(ori_interrupt);
  return MBOX_SUCCESS;
}

//-------------------------------------------------------
//
// int MboxClose(mbox_t);
//
// Close the mailbox for use to the current process.
// If the number of processes using the given mailbox
// is zero, then disable the mailbox structure and
// return it to the set of available mboxes.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//-------------------------------------------------------
int MboxClose(mbox_t handle)
{
  mbox *mbox_ptr;
  int pid, j;
  if (handle < 0 || handle >= MBOX_NUM_MBOXES)
  {
    printf("ERROR: MboxClose handle out of range\n");
    return MBOX_FAIL;
  }

  mbox_ptr = &mbox_array[handle];
  if (mbox_ptr->inuse == 0)
  {
    printf("ERROR: MboxClose handle not inuse\n");
    return MBOX_FAIL;
  }
  LockHandleAcquire(mbox_ptr->mbox_lock);
  pid = GetCurrentPid();
  for (j = 0; j < mbox_ptr->open_process_count; ++j)
  {
    if (mbox_ptr->open_process_list[j] == pid)
      break;
  }
  if (j != mbox_ptr->open_process_count)
  {
    mbox_ptr->open_process_count--;
    for (; j < mbox_ptr->open_process_count; ++j)
    {
      mbox_ptr->open_process_list[j] = mbox_ptr->open_process_list[j + 1];
    }
  }
  else
  {
    printf("WARNING: MboxClose pid not in process list\n");
  }

  LockHandleRelease(mbox_ptr->mbox_lock);
  return MBOX_SUCCESS;
}

//-------------------------------------------------------
//
// int MboxSend(mbox_t handle,int length, void* message);
//
// Send a message (pointed to by "message") of length
// "length" bytes to the specified mailbox.  Messages of
// length 0 are allowed.  The call
// blocks when there is not enough space in the mailbox.
// Messages cannot be longer than MBOX_MAX_MESSAGE_LENGTH.
// Note that the calling process must have opened the
// mailbox via MboxOpen.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//-------------------------------------------------------
int MboxSend(mbox_t handle, int length, void *message)
{
  mbox *mbox_ptr;
  int pid, j;
  int buffer_index;
  Link *insert_buffer;
  if (handle < 0 || handle >= MBOX_NUM_MBOXES)
  {
    printf("ERROR: MboxSend handle out of range\n");
    return MBOX_FAIL;
  }
  if (length <= 0 || length > MBOX_MAX_MESSAGE_LENGTH || message == NULL)
  {
    printf("ERROR: MboxSend message error\n");
    return MBOX_FAIL;
  }

  mbox_ptr = &mbox_array[handle];
  if (mbox_ptr->inuse == 0)
  {
    printf("ERROR: MboxSend handle not inuse\n");
    return MBOX_FAIL;
  }
  LockHandleAcquire(mbox_ptr->mbox_lock);
  pid = GetCurrentPid();
  for (j = 0; j < mbox_ptr->open_process_count; ++j)
  {
    if (mbox_ptr->open_process_list[j] == pid)
      break;
  }
  if (j == mbox_ptr->open_process_count)
  {
    printf("ERROR: MboxSend pid not in process list\n");
    LockHandleRelease(mbox_ptr->mbox_lock);
    return MBOX_FAIL;
  }

  while (AQueueLength(&mbox_ptr->message_queue) == MBOX_MAX_BUFFERS_PER_MBOX ||
         curr_message_use == MBOX_MAX_MESSAGE_LENGTH)
  {
    CondHandleWait(mbox_ptr->blank_cond);
  }

  buffer_index = -1;
  if (global_find_empty_buffer_lock == INVALID_LOCK)
    global_find_empty_buffer_lock = LockCreate();
  LockHandleAcquire(global_find_empty_buffer_lock);
  for (j = 0; j < MBOX_NUM_BUFFERS; ++j)
  {
    if (!mbox_message_array[j].inuse)
    {
      buffer_index = j;
      mbox_message_array[j].inuse = 1;
      break;
    }
  }
  LockHandleRelease(global_find_empty_buffer_lock);
  if (buffer_index == -1)
  {
    printf("ERROR: MboxSend no empty buffer after get index\n");
    LockHandleRelease(mbox_ptr->mbox_lock);
    return MBOX_FAIL;
  }

  if ((insert_buffer =
       AQueueAllocLink(&mbox_message_array[buffer_index])) == QUEUE_FAIL)
  {
    printf("ERROR: MboxSend AllocLink faild\n");
    LockHandleRelease(mbox_ptr->mbox_lock);
    mbox_message_array[buffer_index].inuse = 0;
    return MBOX_FAIL;
  }

  if (AQueueInsertLast(&mbox_ptr->message_queue, insert_buffer) == QUEUE_FAIL)
  {
    // link mem leak?
    printf("ERROR: MboxSend queue push faild\n");
    LockHandleRelease(mbox_ptr->mbox_lock);
    mbox_message_array[buffer_index].inuse = 0;
    return MBOX_FAIL;
  }

  bcopy(message, mbox_message_array[buffer_index].data, length);
  mbox_message_array[buffer_index].size = length;
  CondHandleSignal(mbox_ptr->data_cond);
  LockHandleRelease(mbox_ptr->mbox_lock);
  printf("MESSAGE: MboxSend Success\n");
  return MBOX_SUCCESS;
}

//-------------------------------------------------------
//
// int MboxRecv(mbox_t handle, int maxlength, void* message);
//
// Receive a message from the specified mailbox.  The call
// blocks when there is no message in the buffer.  Maxlength
// should indicate the maximum number of bytes that can be
// copied from the buffer into the address of "message".
// An error occurs if the message is larger than maxlength.
// Note that the calling process must have opened the mailbox
// via MboxOpen.
//
// Returns MBOX_FAIL on failure.
// Returns number of bytes written into message on success.
//
//-------------------------------------------------------
int MboxRecv(mbox_t handle, int maxlength, void *message)
{
  mbox *mbox_ptr;
  int pid;
  int read_len;
  Link *head, *new_head;
  mbox_message *target_message;
  if (handle < 0 || handle >= MBOX_NUM_MBOXES)
  {
    printf("ERROR: MboxRecv handle out of range\n");
    return MBOX_FAIL;
  }

  mbox_ptr = &mbox_array[handle];
  if (mbox_ptr->inuse == 0)
  {
    printf("ERROR: MboxRecv handle not inuse\n");
    return MBOX_FAIL;
  }
  LockHandleAcquire(mbox_ptr->mbox_lock);
  pid = GetCurrentPid();

  while (AQueueEmpty(&mbox_ptr->message_queue))
  {
    CondHandleWait(mbox_ptr->data_cond);
  }

  if ((target_message = (mbox_message *)AQueueObject(
           mbox_ptr->message_queue.first)) == NULL ||
      target_message->inuse == 0)
  {
    printf("ERROR: MboxRecv cant get message\n");
    LockHandleRelease(mbox_ptr->mbox_lock);
    return MBOX_FAIL;
  }
  read_len = min(maxlength, target_message->size);
  bcopy(target_message->data, message, read_len);

  head = mbox_ptr->message_queue.first;
  new_head = head->next;
  if (AQueueRemove(&head) == QUEUE_FAIL)
  {
    printf("ERROR: MboxRecv cant get message\n");
    LockHandleRelease(mbox_ptr->mbox_lock);
    return MBOX_FAIL;
  }
  target_message->inuse = 0;
  curr_message_use--;

  mbox_ptr->message_queue.first = new_head;
  CondHandleSignal(mbox_ptr->blank_cond);
  LockHandleRelease(mbox_ptr->mbox_lock);
  return read_len;
}

//--------------------------------------------------------------------------------
//
// int MboxCloseAllByPid(int pid);
//
// Scans through all mailboxes and removes this pid from their "open procs"
// list. If this was the only open process, then it makes the mailbox available.
// Call this function in ProcessFreeResources in process.c.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//--------------------------------------------------------------------------------
int MboxCloseAllByPid(int pid)
{
  int i, j;
  for (i = 0; i < MBOX_NUM_MBOXES; ++i)
  {
    if (mbox_array[i].inuse)
    {
      LockHandleAcquire(mbox_array[i].mbox_lock);
      for (j = 0; j < mbox_array[i].open_process_count; ++j)
      {
        if (mbox_array[i].open_process_list[j] == pid)
          break;
      }
      if (j != mbox_array[i].open_process_count)
      {
        mbox_array[i].open_process_count--;
        for (; j < mbox_array[i].open_process_count; ++j)
        {
          mbox_array[i].open_process_list[j] =
              mbox_array[i].open_process_list[j + 1];
        }
      }
      LockHandleRelease(mbox_array[i].mbox_lock);
    }
  }
  return MBOX_SUCCESS;
}
