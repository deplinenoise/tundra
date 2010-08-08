#ifndef TUNDRA_PORTABLE_H
#define TUNDRA_PORTABLE_H

/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef _MSC_VER
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed __int64 int64_t;
#else
#include <stdint.h>
#endif

#define TD_UNUSED(var) (void) var

struct td_stat;
struct lua_State;

int td_mkdir(const char *path);
int td_rmdir(const char *path);
int fs_stat_file(const char *filename, struct td_stat *out);
int td_move_file(const char *source, const char *dest);

const char* td_init_homedir(void);

extern const char * const td_platform_string;
void td_init_portable(void);
double td_timestamp(void);

#ifdef _WIN32
#define TD_PATHSEP '\\'
#define TD_PATHSEP_STR "\\"
#else
#define TD_PATHSEP '/'
#define TD_PATHSEP_STR "/"
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
int td_exec(const char* cmd_line, int env_count, const char **env, int *was_signalled_out, const char *prefix);

#if defined(_WIN32)
int td_win32_register_query(struct lua_State *L);
#endif
int td_set_cwd(struct lua_State *L);

#endif
