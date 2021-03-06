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

#include "css_property.hxx"


namespace rspamd::css {

auto css_property::from_bytes (const char *input, size_t inlen) -> tl::expected<css_property,css_parse_error>
{
	return tl::unexpected{css_parse_error(css_parse_error_type::PARSE_ERROR_NYI)};
}

}
