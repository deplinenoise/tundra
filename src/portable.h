#ifndef TUNDRA_PORTABLE_H
#define TUNDRA_PORTABLE_H

#define TD_UNUSED(var) (void) var

struct td_stat;
struct lua_State;

int td_mkdir(const char *path);
int td_rmdir(const char *path);
int fs_stat_file(const char *filename, struct td_stat *out);
int td_move_file(const char *source, const char *dest);

const char* td_init_homedir(void);

extern const char * const td_platform_string;
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
struct w32_mutexattr { char dummy; };

typedef struct w32_pthread pthread_t;
typedef struct w32_pthread_mutex pthread_mutex_t;
typedef struct w32_pthread_cond pthread_cond_t;
typedef struct w32_mutexattr pthread_mutexattr_t;

#define pthread_mutexattr_init(attr) do {} while(0)
#define pthread_mutexattr_settype(attr, type) do {} while(0)

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

typedef struct {
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	int flag;
	const char *reason;
} td_sighandler_info;

void td_block_signals(int block);
void td_install_sighandler(td_sighandler_info *info);
void td_remove_sighandler(void);
int td_exec(const char* cmd_line, int *was_signalled_out);

#if defined(_WIN32)
int td_win32_register_query(struct lua_State *L);
#endif

#endif
