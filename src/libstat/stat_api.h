/*-
 * Copyright 2016 Vsevolod Stakhov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef STAT_API_H_
#define STAT_API_H_

#include "config.h"
#include "task.h"
#include <lua.h>
#include <event.h>

/**
 * @file stat_api.h
 * High level statistics API
 */

#define RSPAMD_STAT_TOKEN_FLAG_TEXT (1 << 0)
#define RSPAMD_STAT_TOKEN_FLAG_META (1 << 1)
#define RSPAMD_STAT_TOKEN_FLAG_LUA_META (1 << 2)
#define RSPAMD_STAT_TOKEN_FLAG_EXCEPTION (1 << 3)
#define RSPAMD_STAT_TOKEN_FLAG_SUBJECT (1 << 4)
#define RSPAMD_STAT_TOKEN_FLAG_UNIGRAM (1 << 5)

typedef struct rspamd_stat_token_s {
	const gchar *begin;
	gsize len;
	guint flags;
} rspamd_stat_token_t;

struct rspamd_stat_ctx;

/**
 * The results of statistics processing:
 * - error
 * - need to do additional job for processing
 * - all processed
 */
typedef enum rspamd_stat_result_e {
	RSPAMD_STAT_PROCESS_ERROR = 0,
	RSPAMD_STAT_PROCESS_DELAYED = 1,
	RSPAMD_STAT_PROCESS_OK
} rspamd_stat_result_t;

/**
 * Initialise statistics modules
 * @param cfg
 */
void rspamd_stat_init (struct rspamd_config *cfg, struct event_base *ev_base);

/**
 * Finalize statistics
 */
void rspamd_stat_close (void);

/**
 * Tokenize task
 * @param st_ctx
 * @param task
 */
void rspamd_stat_process_tokenize (struct rspamd_stat_ctx *st_ctx,
							  struct rspamd_task *task);

/**
 * Classify the task specified and insert symbols if needed
 * @param task
 * @param L lua state
 * @param err error returned
 * @return TRUE if task has been classified
 */
rspamd_stat_result_t rspamd_stat_classify (struct rspamd_task *task,
		lua_State *L, guint stage, GError **err);


/**
 * Check if a task should be learned and set the appropriate flags for it
 * @param task
 * @return
 */
gboolean rspamd_stat_check_autolearn (struct rspamd_task *task);

/**
 * Learn task as spam or ham, task must be processed prior to this call
 * @param task task to learn
 * @param spam if TRUE learn spam, otherwise learn ham
 * @param L lua state
 * @param classifier NULL to learn all classifiers, name to learn a specific one
 * @param err error returned
 * @return TRUE if task has been learned
 */
rspamd_stat_result_t rspamd_stat_learn (struct rspamd_task *task,
		gboolean spam, lua_State *L, const gchar *classifier,
		guint stage,
		GError **err);

/**
 * Get the overall statistics for all statfile backends
 * @param cfg configuration
 * @param total_learns the total number of learns is stored here
 * @return array of statistical information
 */
rspamd_stat_result_t rspamd_stat_statistics (struct rspamd_task *task,
		struct rspamd_config *cfg,
		guint64 *total_learns,
		ucl_object_t **res);

void rspamd_stat_unload (void);

#endif /* STAT_API_H_ */
