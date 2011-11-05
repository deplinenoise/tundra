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

#include "files.h"
#include "engine.h"

#include <assert.h>
#include <string.h>

static pthread_mutex_t *
get_object_lock(td_engine *engine, uint32_t hash)
{
	return &engine->object_locks[hash % TD_OBJECT_LOCK_COUNT];
}

const td_stat *
td_stat_file(td_engine *engine, td_file *f)
{
	double t1 = 0.0;
	int did_stat = 0;
	int collect_stats = 0;
	pthread_mutex_t *objlock;

	collect_stats = td_debug_check(engine, TD_DEBUG_STATS);

	if (collect_stats)
		t1 = td_timestamp();

	objlock = get_object_lock(engine, f->hash);
	td_mutex_lock_or_die(objlock);

	if (f->stat_dirty)
	{
		if (0 != fs_stat_file(f->path, &f->stat))
		{
			f->stat.flags = 0;
			f->stat.size = 0;
			f->stat.timestamp = 0;
		}
		f->stat_dirty = 0;
		did_stat = 1;
	}

	td_mutex_unlock_or_die(objlock);

	if (collect_stats)
	{
		double t2 = td_timestamp();
		td_mutex_lock_or_die(engine->stats_lock);
		++engine->stats.stat_calls;
		engine->stats.stat_time += t2 - t1;
		engine->stats.stat_checks += did_stat;
		td_mutex_unlock_or_die(engine->stats_lock);
	}

	return &f->stat;
}

void
td_touch_file(td_engine *engine, td_file *f)
{
	pthread_mutex_t *obj_lock;
	obj_lock = get_object_lock(engine, f->hash);

	td_mutex_lock_or_die(obj_lock);
	f->stat_dirty = 1;
	f->signature_dirty = 1;
	td_mutex_unlock_or_die(obj_lock);
}

td_digest *
td_get_signature(td_engine *engine, td_file *f)
{
	const int collect_stats = td_debug_check(engine, TD_DEBUG_STATS);
	int count_bump = 0;
	const int dry_run = engine->settings.dry_run;
	double t1 = 0.0, t2 = 0.0;
	pthread_mutex_t *object_lock;

	if (collect_stats)
		t1 = td_timestamp();

	object_lock = get_object_lock(engine, f->hash);
	td_mutex_lock_or_die(object_lock);

	if (f->signature_dirty)
	{
		if (!dry_run)
		{
			assert(f->signer);

			if (f->signer->is_lua)
				td_croak("lua signers not implemented yet");
			else
				(*f->signer->function.function)(engine, f, &f->signature);

			count_bump = 1;
		}
		else
		{
			memset(&f->signature, 0, sizeof(f->signature));
		}

		f->signature_dirty = 0;
	}

	td_mutex_unlock_or_die(object_lock);

	if (collect_stats)
	{
		td_mutex_lock_or_die(engine->stats_lock);
		if (count_bump && !f->signer->is_lua)
		{
			td_sign_fn function = f->signer->function.function;
			if (td_sign_timestamp == function)
				++engine->stats.timestamp_sign_count;
			else if (td_sign_digest == function)
				++engine->stats.md5_sign_count;
		}

		t2 = td_timestamp();
		engine->stats.file_signing_time += t2 - t1;
		td_mutex_unlock_or_die(engine->stats_lock);
	}

	return &f->signature;
}

td_file *td_parent_dir(td_engine *engine, td_file *f)
{
	int i;
	char path_buf[512];

	if (f->path_len >= sizeof(path_buf) - 1)
		td_croak("path too long: %s", f->path);

	/* root directory has no parent */
	if (f->path_len == 1 && f->path[0] == TD_PATHSEP)
		return NULL;

#if defined(TUNDRA_WIN32)
	/* device root directory has no parent */
	if (f->path_len == 3 &&
		f->path[1] == ':' &&
		f->path[2] == TD_PATHSEP)
		return NULL;
#endif

	strncpy(path_buf, f->path, sizeof(path_buf));

	for (i = f->path_len - 1; i >= 0; --i)
	{
		char ch = path_buf[i];

		if (TD_PATHSEP == ch)
		{
			if (i > 0)
				path_buf[i] = '\0';
			else
			{
				/* if we get here the path looks like /foo or \foo which means
				 * we have to return the root directory as the parent */
				path_buf[0] = TD_PATHSEP;
				path_buf[1] = '\0';
			}

			return td_engine_get_file(engine, path_buf, TD_COPY_STRING);
		}
	}

	return NULL;
}
