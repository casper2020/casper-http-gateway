/**
 * @file dispatcher.cc
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

#include "casper/proxy/worker/http/oauth2/dispatcher.h"
#include "casper/proxy/worker/http/oauth2/deferred.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data Logging data params.
 * @param a_user_agent    HTTP User-Agent header value.
 * param a_thread_id      For debug purposes only
 */
casper::proxy::worker::http::oauth2::Dispatcher::Dispatcher (const ev::Loggable::Data& a_loggable_data,
                                                             const std::string& a_user_agent
                                                             CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id))
: ::casper::job::deferrable::Dispatcher<casper::proxy::worker::http::oauth2::Arguments>(CC_IF_DEBUG(a_thread_id)),
    loggable_data_(a_loggable_data), user_agent_(a_user_agent)
{
    /* empty */
}

/**
 * @brief Destructor
 */
casper::proxy::worker::http::oauth2::Dispatcher::~Dispatcher ()
{
    /* empty */
}

/**
 * @brief One-shot call setup.
 *
 * param a_config JSON object with required config.
 */
void casper::proxy::worker::http::oauth2::Dispatcher::Setup (const Json::Value& /* a_config */)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(casper::proxy::worker::http::oauth2::Dispatcher::thread_id_);
}

/**
 * Dispatch an HTTP request.
 *
 * @param a_tracking Request tracking info.
 * @param a_args     HTTP args.
 */
void casper::proxy::worker::http::oauth2::Dispatcher::Push (const casper::job::deferrable::Tracking& a_tracking, const casper::proxy::worker::http::oauth2::Arguments& a_args)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    Dispatch(a_args, new casper::proxy::worker::http::oauth2::Deferred(a_tracking, loggable_data_ CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_)));
}
