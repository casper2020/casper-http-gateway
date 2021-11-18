/**
 * @file deferred.cc
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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

#include "casper/http/gateway/deferred.h"

/**
 * @brief Default constructor.
 *
 * @param a_tracking      Request tracking info.
 * @param a_loggable_data
 */
casper::http::gateway::Deferred::Deferred (const casper::job::deferrable::Tracking& a_tracking, const ev::Loggable::Data& a_loggable_data
                                           CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id))
: ::casper::job::deferrable::Deferred<gateway::Arguments>(gateway::MakeID(a_tracking), a_tracking CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(a_thread_id)),
    loggable_data_(a_loggable_data),
    http_(loggable_data_)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
casper::http::gateway::Deferred::~Deferred()
{
   /* empty */
}

/**
 * @brief Async execute request.
 *
 * @param a_args      This deferred request arguments.
 * @param a_callback  Functions to call by this object when needed.
 */
void casper::http::gateway::Deferred::Run (const gateway::Arguments& a_args, Callbacks a_callbacks)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);        
    arguments_  = new gateway::Arguments(a_args);
    callbacks_  = a_callbacks;
    method_str_ = a_args.parameters().method_;
    // ... log ...
    callbacks_.on_log_deferred_step_(this, method_str_ + "...");
    // ... track ...
    Track();
    // ... HTTP requests must be performed @ MAIN thread ...
    callbacks_.on_main_thread_([this]() {
        const auto& params = arguments().parameters();
        // TODO: NOW perform requested method, not only POST
        // ... async perform HTTP request ...
        http_.POST(params.url_,
                   /* a_headers */
                   params.headers_,
                   /* a_body */ params.body_,
                   /* a_callbacks */ {
                        /* on_success_ */ std::bind(&gateway::Deferred::OnCompleted, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
                        /* on_failure_ */ std::bind(&gateway::Deferred::OnFailure, this, std::placeholders::_1)
                   }
        );
    });
}

// MARK: -

/**
 * @brief Called by HTTP to report when an API request was performed .
 *
 * @param a_code     Status code.
 * @param a_header   Response headers.
 * @param a_body     Body.
 * @param a_rtt      RTT.
 * @param a_callback Function to call, return 100 if another request will run if not notify done.
 */
void casper::http::gateway::Deferred::OnCompleted (const uint16_t& a_code, const std::string& a_content_type, const std::string& a_body, const size_t& a_rtt)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... save response ...
    response_.Set(a_code, a_content_type,a_body, a_rtt, /* a_parse */ false);
    // ... parse response?
    if ( std::string::npos != a_content_type.find("application/json") ) {
        response_.Parse();
    }
    // ... finalize now ...
    callbacks_.on_looper_thread_(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + method_str_ + "-success-", [this](const std::string&) {
        callbacks_.on_completed_(this);
        Untrack();
    });
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an internal error ( NOT server error ).
 *
 * @param a_exception Exception ocurred.
 */
void casper::http::gateway::Deferred::OnFailure (const cc::Exception& a_exception)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    response_.Set(500, a_exception);
    callbacks_.on_looper_thread_(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + method_str_ + "-failure-", [this](const std::string&) {
        callbacks_.on_completed_(this);
        Untrack();
    });
}
