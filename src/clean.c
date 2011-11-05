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

#include "clean.h"
#include "engine.h"
#include "files.h"

#include <stdio.h>
#include <stdlib.h>

/* clean.c -- support code to delete output files and automatically
   created output directories */

enum {
	TD_MAX_CLEAN_DIRS = 4096
};

static void
clean_file(td_engine *engine, td_node *node, td_file **dirs, int *dir_count, td_file *file)
{
	int k;
	const td_stat *stat;
	td_file *dir = td_parent_dir(engine, file);

	/* scan for this directory */
	if (dir)
	{
		for (k = *dir_count - 1; k >= 0; --k)
		{
			if (dirs[k] == dir)
				break;
		}

		if (k < 0)
		{
			int index = *dir_count;
			if (index >= TD_MAX_CLEAN_DIRS)
				td_croak("too many dirs to clean! limit is %d", TD_MAX_CLEAN_DIRS);
			*dir_count = index + 1;
			dirs[index] = dir;
		}
	}

	/* Don't delete the output of precious nodes. */
	if (TD_NODE_PRECIOUS & node->flags)
		return;

	stat = td_stat_file(engine, file);

	if (TD_STAT_EXISTS & stat->flags)
	{
		if (td_verbosity_check(engine, 1))
			printf("Clean %s\n", file->path);

		if (0 != remove(file->path))
			fprintf(stderr, "error: couldn't remove %s\n", file->path);

		td_touch_file(engine, file);
	}
}

static void
clean_output_files(td_engine *engine, td_node *root, td_file **dirs, int *dir_count)
{
	int i, count;

	root->job.flags |= TD_JOBF_CLEANED;

	for (i = 0, count = root->output_count; i < count; ++i)
		clean_file(engine, root, dirs, dir_count, root->outputs[i]);

	for (i = 0, count = root->aux_output_count; i < count; ++i)
		clean_file(engine, root, dirs, dir_count, root->aux_outputs[i]);

	for (i = 0, count = root->dep_count; i < count; ++i)
	{
		td_node *dep = root->deps[i];
		if (0 == (dep->job.flags & TD_JOBF_CLEANED))
		{
			clean_output_files(engine, dep, dirs, dir_count);
		}
	}
}

static int
path_separator_count(const char *fn, int len)
{
	int i, result = 0;
	for (i = 0; i < len; ++i)
	{
		char ch = fn[i];
		if ('/' == ch || '\\' == ch)
			++result;
	}
	return result;
}

static int
directory_depth_compare(const void *l, const void *r)
{
	int lc, rc;
	const td_file *lf = *(const td_file **)l;
	const td_file *rf = *(const td_file **)r;

	/* Just count the number of path separators in the path, as it is normalized. */
	lc = path_separator_count(lf->path, lf->path_len);
	rc = path_separator_count(rf->path, rf->path_len);

	return rc - lc;
}


void
td_clean_files(td_engine *engine, td_node *root)
{
	int i;
	int dir_clean_count = 0;
	td_file *dirs_to_clean[TD_MAX_CLEAN_DIRS];

	/* First remove as many files as possible. As we pass through directories
	 * with generated files, keep track of those. We assume that directories
	 * where output files are placed are OK to remove when cleaning provided
	 * they are empty.*/
	clean_output_files(engine, root, dirs_to_clean, &dir_clean_count);

	/* Now sort the list of dirs in depth order so that leaves will be removed
	 * first. */
	qsort(&dirs_to_clean, dir_clean_count, sizeof(td_file *), directory_depth_compare);

	/* Now we can try to remove directories if they are empty. */
	for (i = 0; i < dir_clean_count; ++i)
	{
		if (0 == td_rmdir(dirs_to_clean[i]->path))
		{
			if (td_verbosity_check(engine, 1))
				printf("RmDir %s\n", dirs_to_clean[i]->path);
		}
	}
}

