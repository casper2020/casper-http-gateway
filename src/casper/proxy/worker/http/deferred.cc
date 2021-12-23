/**
 * @file deferred.cc
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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

#include "casper/proxy/worker/http/deferred.h"

#include "cc/easy/job/types.h"

/**
 * @brief Default constructor.
 *
 * @param a_tracking      Request tracking info.
 * @param a_loggable_data
 */
casper::proxy::worker::http::Deferred::Deferred (const casper::job::deferrable::Tracking& a_tracking, const ev::Loggable::Data& a_loggable_data
                                                 CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id))
: ::casper::job::deferrable::Deferred<casper::proxy::worker::http::Arguments>(MakeID(a_tracking), a_tracking CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(a_thread_id)),
    loggable_data_(a_loggable_data),
    http_(nullptr)
{
    http_options_ = HTTPOptions::Trace | HTTPOptions::Redact;
}

/**
 * @brief Destructor.
 */
casper::proxy::worker::http::Deferred::~Deferred()
{
    if ( nullptr != http_ ) {
        delete http_;
    }
}

/**
 * @brief Async execute request.
 *
 * @param a_args      This deferred request arguments.
 * @param a_callback  Functions to call by this object when needed.
 */
void casper::proxy::worker::http::Deferred::Run (const casper::proxy::worker::http::Arguments& a_args, Callbacks a_callbacks)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_ASSERT(nullptr == http_ && nullptr == arguments_);
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... update http options ...
    if ( a_args.parameters().log_level_ >= CC_JOB_LOG_LEVEL_VBS ) {
        http_options_ |= HTTPOptions::Log;
        // ... log storage related requests?
        if ( a_args.parameters().log_level_ >= CC_JOB_LOG_LEVEL_DBG ) {
            http_options_ &= ~HTTPOptions::Redact;
        }
    }
    // ... keep track of arguments and callbacks ...
    arguments_ = new casper::proxy::worker::http::Arguments(a_args);
    // ... bind callbacks ...
    Bind(a_callbacks);
    // ... prepare HTTP client ...
    http_ = new ::cc::easy::HTTPClient(loggable_data_, tracking_.ua_.c_str());
    if ( HTTPOptions::NotSet != ( ( HTTPOptions::Log | HTTPOptions::Trace ) & http_options_ ) ) {
        http_->SetcURLedCallbacks({
            /* log_request_  */ std::bind(&casper::proxy::worker::http::Deferred::OnLogHTTPRequest , this, std::placeholders::_1, std::placeholders::_2),
            /* log_response_ */ std::bind(&casper::proxy::worker::http::Deferred::OnLogHTTPValue   , this, std::placeholders::_1, std::placeholders::_2)
            CC_IF_DEBUG(
                ,
                /* progress_     */ nullptr,
                /* debug_        */ nullptr
            )
        }, HTTPOptions::Redact == ( HTTPOptions::Redact & http_options_ ));
    }
    // ... track it ...
    Track();
    // ... log ...
    OnLogDeferredStep(this, "http/...");
    // ... HTTP requests must be performed @ MAIN thread ...
    CallOnMainThread([this]() {
        //
        const auto& request = arguments_->parameters().http_request();
        // ... set additional properties ...
        if ( true == request.follow_location_ ) {
            http_->SetFollowLocation();
        }
        // ... set callbacks ...
        const ::cc::easy::HTTPClient::RawCallbacks callbacks = {
            /* on_success_ */ std::bind(&casper::proxy::worker::http::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
            /* on_error_   */ std::bind(&casper::proxy::worker::http::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
            /* on_failure_ */ std::bind(&casper::proxy::worker::http::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
        };
        // ... async perform HTTP request ...
        switch(request.method_) {
            case ::ev::curl::Request::HTTPRequestType::HEAD:
                http_->HEAD(request.url_, request.headers_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::GET:
                http_->GET(request.url_, request.headers_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::DELETE:
                http_->DELETE(request.url_, request.headers_, ( 0 != request.body_.length() ? &request.body_ : nullptr ), callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::POST:
                http_->POST(request.url_, request.headers_, request.body_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::PUT:
                http_->PUT(request.url_, request.headers_, request.body_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::PATCH:
                http_->PATCH(request.url_, request.headers_, request.body_, callbacks, &request.timeouts_);
                break;
            default:
                throw ::cc::NotImplemented("Method '" UINT8_FMT "' not implemented!", static_cast<uint8_t>(request.method_));
        }
    });
}

// MARK: -

/**
 * @brief Call this method when it's time to signal that this request is now completed.
 *
 * @param a_tag Callback tag.
 */
void casper::proxy::worker::http::Deferred::Finalize (const std::string& a_tag)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... must be done on 'looper' thread ...
    CallOnLooperThread(a_tag, [this] (const std::string&) {
        // ... if request failed, and if we're tracing and did not log HTTP calls, should we do it now?
        if ( CC_EASY_HTTP_OK != response_.code() && HTTPOptions::Trace == ( HTTPOptions::Trace & http_options_ ) && not ( HTTPOptions::Log == ( HTTPOptions::Log & http_options_ ) ) ) {
            for ( const auto& trace : http_trace_ ) {
                OnLogDeferred(this, CC_JOB_LOG_LEVEL_VBS ,CC_JOB_LOG_STEP_HTTP, trace.data_);
            }
        }
        // ... notify ...
        OnCompleted(this);
        // ... done ...
        Untrack();
    }, /* a_daredevil */ true);
}

/**
 * @brief Called by HTTP client to report when an API request was performed.
 *
 * @param a_value RAW value.
 */
void casper::proxy::worker::http::Deferred::OnHTTPRequestCompleted (const ::cc::easy::HTTPClient::RawValue& a_value)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... save response ...
    const std::string content_type = a_value.header_value("Content-Type");
    {
        std::map<std::string, std::string> headers;
        response_.Set(a_value.code(), content_type, a_value.headers_as_map(headers), a_value.body(), a_value.rtt());
    }
    const std::string tag = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + '-' + CC_OBJECT_HEX_ADDR(&a_value) + "-http-" + ( CC_EASY_HTTP_OK == response_.code() ? "-succeeded-" : "-failed-" );
    // ... finalize ...
    Finalize(tag);
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an cURL error ( or maybe server error ).
 *
 * @param a_error Error ocurred.
 */
void casper::proxy::worker::http::Deferred::OnHTTPRequestError (const ::cc::easy::HTTPClient::RawError& a_value)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... set response ...
    switch (a_value.code_) {
        case CURLE_OPERATION_TIMEOUTED:
            // 504 Gateway Timeout
            response_.Set(CC_EASY_HTTP_GATEWAY_TIMEOUT, "cURL: " + a_value.message());
            break;
        default:
            // 500 Internal Server Error
            response_.Set(CC_EASY_HTTP_INTERNAL_SERVER_ERROR, a_value.message());
            break;
    }
    // ... finalize ...
    Finalize(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + '-' + CC_OBJECT_HEX_ADDR(&a_value) + "-http-error-");
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an internal error ( NOT server error ).
 *
 * @param a_exception Exception ocurred.
 */
void casper::proxy::worker::http::Deferred::OnHTTPRequestFailure (const ::cc::Exception& a_exception)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... set response ...
    response_.Set(CC_EASY_HTTP_INTERNAL_SERVER_ERROR, a_exception);
    // ... finalize ...
    Finalize(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + '-' + CC_OBJECT_HEX_ADDR(&a_exception) + "-http-failure-");
}

// MARK: - HTTP Client Callbacks

/**
 * @brief Called by an HTTP client when it's time to log a request.
 *
 * @param a_request Request that will be running.
 * @param a_data    cURL(ed) style command ( for log proposes only ).
 */
void casper::proxy::worker::http::Deferred::OnLogHTTPRequest (const ::ev::curl::Request& a_request, const std::string& a_data)
{
    OnHTTPRequestWillRunLogIt(a_request, a_data, http_options_);
}

/**
 * @brief Called by an HTTP client when it's time to log a request.
 *
 * @param a_value Post request execution, result data.
 * @param a_data    cURL(ed) style response data ( for log proposes only ).
 */
void casper::proxy::worker::http::Deferred::OnLogHTTPValue (const ::ev::curl::Value& a_value, const std::string& a_data)
{
    OnHTTPRequestSteppedLogIt(a_value, a_data, http_options_);
}

// MARK: -
/**
 * @brief Called by an HTTP client when a request will run and it's time to log data ( ⚠️ for logging proposes only, request has not started yet ! )
 *
 * @param a_request Request that will be running.
 * @param a_data    cURL(ed) style command ( for log proposes only ).
 * @param a_options Adjusted options for this request, for more info See \link proxy::worker::Deferred::HTTPOptions \link.
 */
void casper::proxy::worker::http::Deferred::OnHTTPRequestWillRunLogIt (const ::ev::curl::Request& a_request, const std::string& a_data, const proxy::worker::http::Deferred::HTTPOptions a_options)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    //... make sure that we're tracing or logging if it was requested to do it so ..
    if (
        ( ( HTTPOptions::Trace == ( HTTPOptions::Trace & a_options ) || ( HTTPOptions::Trace == ( HTTPOptions::Trace & http_options_ ) ) ) )
    ) {
        // ... must be done on 'looper' thread ...
        const std::string tag = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + '-' + CC_OBJECT_HEX_ADDR(&a_request) + "-log-http-oauth2-client-response";
        CallOnLooperThread(tag, [this, a_data, a_options] (const std::string&) {
            // ... log?
            if ( HTTPOptions::Log == ( HTTPOptions::Log & a_options ) ) {
                OnLogDeferred(this, CC_JOB_LOG_LEVEL_VBS ,CC_JOB_LOG_STEP_HTTP, a_data);
            } else { // assuming trace
                http_trace_.push_back({
                    /* code_ */ 0,
                    /* data_ */ a_data
                });
            }
        });
    }
}

/**
 * @brief Called by an HTTP client when a request did run and it's time to log data ( ⚠️ for logging proposes only, request is it's not completed ! )
 *
 * @param a_value   Request post-execution, result data.
 * @param a_data    cURL(ed) style response data ( for log proposes only ).
 * @param a_options Adjusted options for this value, for more info See \link proxy::worker::Deferred::HTTPOptions \link.
 */
void casper::proxy::worker::http::Deferred::OnHTTPRequestSteppedLogIt (const ::ev::curl::Value& a_value, const std::string& a_data, const proxy::worker::http::Deferred::HTTPOptions a_options)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    //... make sure that we're tracing or logging if it was requested to do it so ..
    if (
        ( ( HTTPOptions::Trace == ( HTTPOptions::Trace & a_options ) || ( HTTPOptions::Trace == ( HTTPOptions::Trace & http_options_ ) ) ) )
    ) {
        const uint16_t code = a_value.code();
        // ... must be done on 'looper' thread ...
        const std::string tag = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + '-' + CC_OBJECT_HEX_ADDR(&a_value) + "-log-http-oauth2-step";
        CallOnLooperThread(tag, [this, a_data, a_options, code] (const std::string&) {
            // ... log?
            if ( HTTPOptions::Log == ( HTTPOptions::Log & a_options ) ) {
                OnLogDeferred(this, CC_JOB_LOG_LEVEL_VBS, CC_JOB_LOG_STEP_HTTP, a_data);
            } else { // assuming trace
                http_trace_.push_back({
                    /* code_ */ code,
                    /* data_ */ a_data
                });
            }
        });
    }
}
