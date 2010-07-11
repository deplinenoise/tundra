#ifndef TUNDRA_PORTABLE_H
#define TUNDRA_PORTABLE_H

struct td_stat;

int td_mkdir(const char *path);
int fs_stat_file(const char *filename, struct td_stat *out);
int td_move_file(const char *source, const char *dest);

void td_init_timer(void);
double td_timestamp(void);

#ifdef _WIN32
#define TD_PATHSEP '\\'
#else
#define TD_PATHSEP '/'
#endif

#ifndef _WIN32
#include <pthread.h>
#else

struct w32_pthread { int index; };
struct w32_pthread_mutex { void *handle; };
struct w32_pthread_cond { void *handle; };

typedef struct w32_pthread pthread_t;
typedef struct w32_pthread_mutex pthread_mutex_t;
typedef struct w32_pthread_cond pthread_cond_t;

int pthread_mutex_lock(pthread_mutex_t *lock);
int pthread_mutex_unlock(pthread_mutex_t *lock);
int pthread_mutex_init(pthread_mutex_t *mutex, void *args);
int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, void *args);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t* lock);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond);

typedef void *(*pthread_thread_routine)(void *arg);

int pthread_create(pthread_t *result, void* options, pthread_thread_routine, void *routine_arg);
int pthread_join(pthread_t thread, void **result_out);

#endif

#endif
