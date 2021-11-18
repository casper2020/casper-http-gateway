/**
 * @file dispatcher.cc
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-http-gateway.
 *
 * casper-http-gateway is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-http-gateway  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-http-gateway. If not, see <http://www.gnu.org/licenses/>.
 */

#include "casper/http/gateway/dispatcher.h"
#include "casper/http/gateway/deferred.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data Logging data params.
 * param a_thread_id      For debug proposes only
 */
casper::http::gateway::Dispatcher::Dispatcher (const ev::Loggable::Data& a_loggable_data
                                               CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id))
: ::casper::job::deferrable::Dispatcher<gateway::Arguments>(CC_IF_DEBUG(a_thread_id)),
    loggable_data_(a_loggable_data)
{
    /* empty */
}

/**
 * @brief Destructor
 */
casper::http::gateway::Dispatcher::~Dispatcher ()
{
    /* empty */
}

/**
 * @brief One-shot call setup.
 *
 * @param a_config JSON object with required config.
 */
void casper::http::gateway::Dispatcher::Setup (const Json::Value& /* a_config */)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(gateway::Dispatcher::thread_id_);
}

/**
 * Dispatch an HTTP request.
 *
 * @param a_tracking Request tracking info.
 * @param a_args     HTTP args.
 */
void casper::http::gateway::Dispatcher::Push (const casper::job::deferrable::Tracking& a_tracking, const gateway::Arguments& a_args)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    Dispatch(a_args, new gateway::Deferred(a_tracking, loggable_data_ CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_)));
}
