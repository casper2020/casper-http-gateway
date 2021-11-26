/**
 * @file script.cc
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-proxy-worker.
 *
 * casper-proxy-worker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-proxy-worker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-proxy-worker. If not, see <http://www.gnu.org/licenses/>.
 */

#include "casper/proxy/worker/v8/script.h"

/**
 * @brief Default constructor.
 *
 * @param a_owner    Script owner.
 * @param a_name     Script name
 * @param a_uri
 * @param a_out_path Writable directory.
 */
casper::proxy::worker::v8::Script::Script (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                                           const std::string& a_out_path)
: ::cc::v8::basic::Evaluator(a_owner, a_name, a_uri, a_out_path)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
casper::proxy::worker::v8::Script::~Script ()
{
    /* empty */
}
