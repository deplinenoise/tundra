#ifndef READWRITELOCK_HPP
#define READWRITELOCK_HPP

#include "Common.hpp"
#include "ConditionVar.hpp"
#include "Mutex.hpp"

#if defined(TUNDRA_UNIX)
#include <pthread.h>
#endif

#if defined(TUNDRA_WIN32)
#include <windows.h>
#endif

namespace t2
{
  struct ReadWriteLock;

  void ReadWriteLockInit(ReadWriteLock* self);
  void ReadWriteLockDestroy(ReadWriteLock* self);

  void ReadWriteLockRead(ReadWriteLock* self);
  void ReadWriteUnlockRead(ReadWriteLock* self);

  void ReadWriteLockWrite(ReadWriteLock* self);
  void ReadWriteUnlockWrite(ReadWriteLock* self);

#if defined(TUNDRA_UNIX)

struct ReadWriteLock
{
  pthread_rwlock_t m_Impl;
};

inline void ReadWriteLockInit(ReadWriteLock* self)
{
  if (0 != pthread_rwlock_init(&self->m_Impl, nullptr))
    CroakErrno("pthread_rwlock_init() failed");
}

inline void ReadWriteLockDestroy(ReadWriteLock* self)
{
  if (0 != pthread_rwlock_destroy(&self->m_Impl))
    CroakErrno("pthread_rwlock_destroy() failed");
}

inline void ReadWriteLockRead(ReadWriteLock* self)
{
  if (0 != pthread_rwlock_rdlock(&self->m_Impl))
    CroakErrno("pthread_rwlock_rdlock() failed");
}

inline void ReadWriteUnlockRead(ReadWriteLock* self)
{
  if (0 != pthread_rwlock_unlock(&self->m_Impl))
    CroakErrno("pthread_rwlock_unlock() failed");
}

inline void ReadWriteLockWrite(ReadWriteLock* self)
{
  if (0 != pthread_rwlock_wrlock(&self->m_Impl))
    CroakErrno("pthread_rwlock_rdlock() failed");
}

inline void ReadWriteUnlockWrite(ReadWriteLock* self)
{
  if (0 != pthread_rwlock_unlock(&self->m_Impl))
    CroakErrno("pthread_rwlock_unlock() failed");
}
#endif

#if defined(TUNDRA_WIN32)

struct ReadWriteLock
{
#if ENABLED(TUNDRA_WIN32_VISTA_APIS)
  SRWLOCK m_Impl;
#else
  // Emulation modeled after Buthenhof
  Mutex               m_Mutex;
  ConditionVariable   m_Read;
  ConditionVariable   m_Write;
  int                 m_ActiveReaders;
  int                 m_ActiveWriters;
  int                 m_WaitingReaders;
  int                 m_WaitingWriters;
#endif
};

#if ENABLED(TUNDRA_WIN32_VISTA_APIS)
inline void ReadWriteLockInit(ReadWriteLock* self)
{
  InitializeSRWLock(&self->m_Impl);
}

inline void ReadWriteLockDestroy(ReadWriteLock* self)
{
  // nop
}

inline void ReadWriteLockRead(ReadWriteLock* self)
{
  AcquireSRWLockShared(&self->m_Impl);
}

inline void ReadWriteUnlockRead(ReadWriteLock* self)
{
  ReleaseSRWLockShared(&self->m_Impl);
}

inline void ReadWriteLockWrite(ReadWriteLock* self)
{
  AcquireSRWLockExclusive(&self->m_Impl);
}

inline void ReadWriteUnlockWrite(ReadWriteLock* self)
{
  ReleaseSRWLockExclusive(&self->m_Impl);
}
#endif

#endif

}

#endif
