#include "ReadWriteLock.hpp"

#if defined(TUNDRA_WIN32)
#if DISABLED(TUNDRA_WIN32_VISTA_APIS)

void t2::ReadWriteLockInit(t2::ReadWriteLock* self)
{
  self->m_ActiveReaders  = 0;
  self->m_ActiveWriters  = 0;
  self->m_WaitingReaders = 0;
  self->m_WaitingWriters = 0;

  MutexInit(&self->m_Mutex);
  CondInit(&self->m_Read);
  CondInit(&self->m_Write);
}

void t2::ReadWriteLockDestroy(t2::ReadWriteLock* self)
{
  CondDestroy(&self->m_Write);
  CondDestroy(&self->m_Read);
  MutexDestroy(&self->m_Mutex);
}

void t2::ReadWriteLockRead(t2::ReadWriteLock* self)
{
  MutexLock(&self->m_Mutex);

  if (self->m_ActiveWriters > 0)
  {
    ++self->m_WaitingReaders;

    while (self->m_ActiveWriters > 0)
    {
      CondWait(&self->m_Read, &self->m_Mutex);
    }

    --self->m_WaitingReaders;
  }

  ++self->m_ActiveReaders;

  MutexUnlock(&self->m_Mutex);
}

void t2::ReadWriteUnlockRead(t2::ReadWriteLock* self)
{
  MutexLock(&self->m_Mutex);

  int reader_count = --self->m_ActiveReaders;

  if (0 == reader_count && self->m_WaitingWriters > 0)
  {
    CondSignal(&self->m_Write);
  }

  MutexUnlock(&self->m_Mutex);
}

void t2::ReadWriteLockWrite(t2::ReadWriteLock* self)
{
  MutexLock(&self->m_Mutex);

  if (self->m_ActiveReaders > 0 || self->m_ActiveWriters > 0)
  {
    ++self->m_WaitingWriters;

    while (self->m_ActiveReaders > 0 || self->m_ActiveWriters > 0)
    {
      CondWait(&self->m_Write, &self->m_Mutex);
    }

    --self->m_WaitingWriters;
  }

  ++self->m_ActiveWriters;

  MutexUnlock(&self->m_Mutex);
}

void t2::ReadWriteUnlockWrite(t2::ReadWriteLock* self)
{
  MutexLock(&self->m_Mutex);

  --self->m_ActiveWriters;

  if (self->m_WaitingReaders)
  {
    CondBroadcast(&self->m_Read);
  }
  else if (self->m_WaitingWriters)
  {
    CondSignal(&self->m_Write);
  }

  MutexUnlock(&self->m_Mutex);

}

#endif
#endif
