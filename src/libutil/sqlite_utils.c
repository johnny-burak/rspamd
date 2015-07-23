/*
 * Copyright (c) 2015, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "libutil/logger.h"
#include "libutil/sqlite_utils.h"


static GQuark
rspamd_sqlite3_quark (void)
{
	return g_quark_from_static_string ("rspamd-sqlite3");
}

GArray*
rspamd_sqlite3_init_prstmt (sqlite3 *db,
		struct rspamd_sqlite3_prstmt *init_stmt,
		gint max_idx,
		GError **err)
{
	gint i;
	GArray *res;
	struct rspamd_sqlite3_prstmt *nst;

	res = g_array_sized_new (FALSE, TRUE, sizeof (struct rspamd_sqlite3_prstmt),
			max_idx);
	g_array_set_size (res, max_idx);

	for (i = 0; i < max_idx; i ++) {
		nst = &g_array_index (res, struct rspamd_sqlite3_prstmt, i);
		memcpy (nst, &init_stmt[i], sizeof (*nst));

		if (sqlite3_prepare_v2 (db, init_stmt[i].sql, -1,
				&nst->stmt, NULL) != SQLITE_OK) {
			g_set_error (err, rspamd_sqlite3_quark (),
				-1, "Cannot initialize prepared sql `%s`: %s",
				nst->sql, sqlite3_errmsg (db));
			rspamd_sqlite3_close_prstmt (db, res);

			return NULL;
		}
	}

	return res;
}

int
rspamd_sqlite3_run_prstmt (sqlite3 *db, GArray *stmts,
		gint idx, ...)
{
	gint retcode;
	va_list ap;
	sqlite3_stmt *stmt;
	gint i, rowid, nargs, j;
	gint64 len;
	struct rspamd_sqlite3_prstmt *nst;
	const char *argtypes;

	if (idx < 0 || idx >= (gint)stmts->len) {

		return -1;
	}

	nst = &g_array_index (stmts, struct rspamd_sqlite3_prstmt, idx);
	stmt = nst->stmt;

	g_assert (nst != NULL);

	msg_debug ("executing `%s`", nst->sql);
	argtypes = nst->args;
	sqlite3_reset (stmt);
	va_start (ap, idx);
	nargs = 1;

	for (i = 0, rowid = 1; argtypes[i] != '\0'; i ++) {
		switch (argtypes[i]) {
		case 'T':

			for (j = 0; j < nargs; j ++, rowid ++) {
				sqlite3_bind_text (stmt, rowid, va_arg (ap, const char*), -1,
					SQLITE_STATIC);
			}

			nargs = 1;
			break;
		case 'V':

			for (j = 0; j < nargs; j ++, rowid ++) {
				len = va_arg (ap, gint64);
				sqlite3_bind_text (stmt, rowid, va_arg (ap, const char*), len,
						SQLITE_STATIC);
			}

			nargs = 1;
			break;
		case 'I':

			for (j = 0; j < nargs; j ++, rowid ++) {
				sqlite3_bind_int64 (stmt, rowid, va_arg (ap, gint64));
			}

			nargs = 1;
			break;
		case 'S':

			for (j = 0; j < nargs; j ++, rowid ++) {
				sqlite3_bind_int (stmt, rowid, va_arg (ap, gint));
			}

			nargs = 1;
			break;
		case '*':
			nargs = va_arg (ap, gint);
			break;
		}
	}

	va_end (ap);
	retcode = sqlite3_step (stmt);

	if (retcode == nst->result) {
		argtypes = nst->ret;

		for (i = 0; argtypes != NULL && argtypes[i] != '\0'; i ++) {
			switch (argtypes[i]) {
			case 'T':
				*va_arg (ap, char**) = g_strdup (sqlite3_column_text (stmt, i));
				break;
			case 'I':
				*va_arg (ap, gint64*) = sqlite3_column_int64 (stmt, i);
				break;
			case 'S':
				*va_arg (ap, int*) = sqlite3_column_int (stmt, i);
				break;
			case 'L':
				*va_arg (ap, gint64*) = sqlite3_last_insert_rowid (db);
				break;
			}
		}

		if (!(nst->flags & RSPAMD_SQLITE3_STMT_MULTIPLE)) {
			sqlite3_reset (stmt);
		}

		return SQLITE_OK;
	}
	else if (retcode != SQLITE_DONE) {
		msg_debug ("failed to execute query %s: %d, %s", nst->sql,
				retcode, sqlite3_errmsg (db));
	}

	if (!(nst->flags & RSPAMD_SQLITE3_STMT_MULTIPLE)) {
		sqlite3_reset (stmt);
	}

	return retcode;
}

void
rspamd_sqlite3_close_prstmt (sqlite3 *db, GArray *stmts)
{
	guint i;
	struct rspamd_sqlite3_prstmt *nst;

	for (i = 0; i < stmts->len; i++) {
		nst = &g_array_index (stmts, struct rspamd_sqlite3_prstmt, i);
		if (nst->stmt != NULL) {
			sqlite3_finalize (nst->stmt);
		}
	}

	g_array_free (stmts, TRUE);

	return;
}

static gboolean
rspamd_sqlite3_wait (const gchar *lock)
{
	gint fd;
	struct timespec sleep_ts = {
		.tv_sec = 0,
		.tv_nsec = 1000000
	};

	fd = open (lock, O_RDONLY);

	if (fd == -1) {
		msg_err ("cannot open lock file %s: %s", lock, strerror (errno));

		return FALSE;
	}

	while (!rspamd_file_lock (fd, TRUE)) {
		if (nanosleep (&sleep_ts, NULL) == -1 && errno != EINTR) {
			close (fd);
			msg_err ("cannot sleep open lock file %s: %s", lock, strerror (errno));

			return FALSE;
		}
	}

	rspamd_file_unlock (fd, FALSE);

	close (fd);

	return TRUE;
}



sqlite3 *
rspamd_sqlite3_open_or_create (const gchar *path, const
		gchar *create_sql, GError **err)
{
	sqlite3 *sqlite;
	gint rc, flags, lock_fd;
	gchar lock_path[PATH_MAX], dbdir[PATH_MAX], *pdir;
	static const char sqlite_wal[] = "PRAGMA journal_mode=\"wal\";",
			exclusive_lock_sql[] = "PRAGMA locking_mode=\"exclusive\";";
	gboolean create = FALSE;

	flags = SQLITE_OPEN_READWRITE;
#ifdef SQLITE_OPEN_SHAREDCACHE
	flags |= SQLITE_OPEN_SHAREDCACHE;
#endif
#ifdef SQLITE_OPEN_WAL
	flags |= SQLITE_OPEN_WAL;
#endif

	rspamd_strlcpy (dbdir, path, sizeof (dbdir));
	pdir = dirname (dbdir);

	if (access (pdir, W_OK) == -1) {
		g_set_error (err, rspamd_sqlite3_quark (),
				errno, "cannot open sqlite directory %s: %s",
				pdir, strerror (errno));

		return NULL;
	}

	rspamd_snprintf (lock_path, sizeof (lock_path), "%s.lock", path);

	if (access (path, R_OK) == -1 || access (lock_path, R_OK) != -1) {
		flags |= SQLITE_OPEN_CREATE;

		rspamd_snprintf (lock_path, sizeof (lock_path), "%s.lock", path);
		lock_fd = open (lock_path, O_WRONLY|O_CREAT|O_EXCL, 00600);

		if (lock_fd == -1 && (errno == EEXIST || errno == EBUSY)) {
			msg_debug ("checking %s to wait for db being created", lock_path);

			if (!rspamd_sqlite3_wait (lock_path)) {
				g_set_error (err, rspamd_sqlite3_quark (),
						errno, "cannot create sqlite file %s: %s",
						path, strerror (errno));

				return NULL;
			}

			/* At this point we have database created */
			create = FALSE;
		}
		else {
			msg_debug ("locking %s to block creating", lock_path);

			g_assert (rspamd_file_lock (lock_fd, FALSE));
			create = TRUE;
		}
	}

	if ((rc = sqlite3_open_v2 (path, &sqlite,
			flags, NULL)) != SQLITE_OK) {
#if SQLITE_VERSION_NUMBER >= 3008000
		g_set_error (err, rspamd_sqlite3_quark (),
				rc, "cannot open sqlite db %s: %s",
				path, sqlite3_errstr (rc));
#else
		g_set_error (err, rspamd_sqlite3_quark (),
				rc, "cannot open sqlite db %s: %d",
				path, rc);
#endif

		return NULL;
	}

	if (create) {
		if (sqlite3_exec (sqlite, sqlite_wal, NULL, NULL, NULL) != SQLITE_OK) {
			msg_warn ("WAL mode is not supported (%s), locking issues might occur",
					sqlite3_errmsg (sqlite));
		}

		if (sqlite3_exec (sqlite, exclusive_lock_sql, NULL, NULL, NULL) != SQLITE_OK) {
			msg_warn ("cannot exclusively lock database to create schema: %s",
					sqlite3_errmsg (sqlite));
		}

		if (sqlite3_exec (sqlite, create_sql, NULL, NULL, NULL) != SQLITE_OK) {
			g_set_error (err, rspamd_sqlite3_quark (),
					-1, "cannot execute create sql `%s`: %s",
					create_sql, sqlite3_errmsg (sqlite));
			sqlite3_close (sqlite);
			rspamd_file_unlock (lock_fd, FALSE);
			unlink (lock_path);
			close (lock_fd);

			return NULL;
		}

		msg_debug ("removing lock from %s", lock_path);

		sqlite3_close (sqlite);
		rspamd_file_unlock (lock_fd, FALSE);
		unlink (lock_path);
		close (lock_fd);

		/* Reopen in normal mode */
		msg_debug ("reopening %s in normal mode", path);
		flags &= ~SQLITE_OPEN_CREATE;

		if ((rc = sqlite3_open_v2 (path, &sqlite,
				flags, NULL)) != SQLITE_OK) {
	#if SQLITE_VERSION_NUMBER >= 3008000
			g_set_error (err, rspamd_sqlite3_quark (),
					rc, "cannot open sqlite db after creation %s: %s",
					path, sqlite3_errstr (rc));
	#else
			g_set_error (err, rspamd_sqlite3_quark (),
					rc, "cannot open sqlite db after creation %s: %d",
					path, rc);
	#endif

			return NULL;
		}
	}

	if (sqlite3_exec (sqlite, sqlite_wal, NULL, NULL, NULL) != SQLITE_OK) {
		msg_warn ("WAL mode is not supported (%s), locking issues might occur",
				sqlite3_errmsg (sqlite));
	}

	return sqlite;
}
