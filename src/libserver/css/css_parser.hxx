/*-
 * Copyright 2021 Vsevolod Stakhov
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

#pragma once

#ifndef RSPAMD_CSS_PARSER_HXX
#define RSPAMD_CSS_PARSER_HXX

#include "css.hxx"
#include "parse_error.hxx"
#include "contrib/expected/expected.hpp"
#include "logger.h"

namespace rspamd::css {

INIT_LOG_MODULE(chartable)

auto parse_css (rspamd_mempool_t *pool, const std::string_view &st) ->
		tl::expected<std::unique_ptr<css_style_sheet>,css_parse_error>;

}

#endif //RSPAMD_CSS_PARSER_HXX
