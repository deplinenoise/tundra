#ifndef READWRITELOCK_HPP
#define READWRITELOCK_HPP

#include "Common.hpp"

#if defined(TUNDRA_UNIX)
#include <pthread.h>
#endif

#if defined(TUNDRA_WIN32)
#include <windows.h>
#endif

namespace t2
{
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
  SRWLOCK m_Impl;
};

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

}

#endif
