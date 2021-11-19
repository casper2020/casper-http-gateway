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

#include "casper/job/exceptions.h"

#include "cc/ragel.h"

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
    // ... log ...
    callbacks_.on_log_deferred_step_(this, method_str_ + "...");
    // ... ...
    typedef struct {
        ::ev::curl::Request::HTTPRequestType method_;
        std::string                          url_;
        std::string                          body_;
        ::ev::curl::Request::Headers         headers_;
        ::ev::curl::Request::Timeouts        timeouts_;
    } Data;
    Data data = {
        /* method_   */ ::ev::curl::Request::HTTPRequestType::NotSet,
        /* url_      */ "",
        /* body_     */ "",
        /* headers_  */ {},
        /* timeouts_ */ { -1, -1 }
    };

    {
        const ::cc::easy::JSON<::casper::job::BadRequestException> json;
        
        const auto& http = arguments().parameters().request_;
        const Json::Value& url_ref      = json.Get(http   , "url"     , Json::ValueType::stringValue, nullptr);
        const Json::Value& method_ref   = json.Get(http   , "method"  , Json::ValueType::stringValue, nullptr);
        const Json::Value& body_ref     = json.Get(http   , "body"    , Json::ValueType::objectValue, &Json::Value::null);
        const Json::Value& timeouts_ref = json.Get(http   , "timeouts", Json::ValueType::objectValue, &Json::Value::null);
        // ... method ...
        method_str_ = method_ref.asString();
        {
            CC_RAGEL_DECLARE_VARS(method, method_str_.c_str(), method_str_.length());
            CC_DIAGNOSTIC_PUSH();
            CC_DIAGNOSTIC_IGNORED("-Wunreachable-code");
            %%{
                machine DeferredMachine;
                main := |*
                    /get/i    => { data.method_ = ::ev::curl::Request::HTTPRequestType::GET;    };
                    /put/i    => { data.method_ = ::ev::curl::Request::HTTPRequestType::PUT;    };
                    /delete/i => { data.method_ = ::ev::curl::Request::HTTPRequestType::DELETE; };
                    /post/i   => { data.method_ = ::ev::curl::Request::HTTPRequestType::POST;   };
                    /patch/i  => { data.method_ = ::ev::curl::Request::HTTPRequestType::PATCH;  };
                    /head/i   => { data.method_ = ::ev::curl::Request::HTTPRequestType::HEAD;   };
                *|;
                write data;
                write init;
                write exec;
            }%%
            CC_RAGEL_SILENCE_VARS(Deferred)
            CC_DIAGNOSTIC_POP();
        }
        // ... URL ...
        data.url_ = url_ref.asString();
        // ... headers ...
        const Json::Value& headers_ref = json.Get(http, "headers", Json::ValueType::objectValue, &Json::Value::null);
        if ( false == headers_ref.isNull() ) {
            for ( auto key : headers_ref.getMemberNames() ) {
                const Json::Value& header = json.Get(headers_ref, key.c_str(), Json::ValueType::stringValue, nullptr);
                data.headers_[key] = { header.asString() };
            }
        }
        // ... body ...
        if ( false == body_ref.isNull() ) {
            data.body_ = json.Write(body_ref);
        }
        // ... timeouts ...
        if ( false == timeouts_ref.isNull() ) {
            const Json::Value connection_ref = json.Get(timeouts_ref, "connection", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == connection_ref.isNull() ) {
                data.timeouts_.connection_ = static_cast<long>(connection_ref.asInt64());
            }
            const Json::Value operation_ref = json.Get(timeouts_ref, "operation", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == operation_ref.isNull() ) {
                data.timeouts_.operation_ = static_cast<long>(operation_ref.asInt64());
            }
        }
    }
    // ... track ...
    Track();
    // ... HTTP requests must be performed @ MAIN thread ...
    callbacks_.on_main_thread_([this, data]() {
        // ... async perform HTTP request ...
        const ::cc::easy::HTTPClient::RawCallbacks callbacks = {
            /* on_success_ */ std::bind(&gateway::Deferred::OnCompleted, this, std::placeholders::_1),
            /* on_failure_ */ std::bind(&gateway::Deferred::OnFailure, this, std::placeholders::_1)
        };
        switch(data.method_) {
            case ::ev::curl::Request::HTTPRequestType::GET:
                http_.GET(data.url_, data.headers_, callbacks, &data.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::POST:
                http_.POST(data.url_, data.headers_, data.body_, callbacks, &data.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::PUT:
            case ::ev::curl::Request::HTTPRequestType::DELETE:
            case ::ev::curl::Request::HTTPRequestType::PATCH:
            case ::ev::curl::Request::HTTPRequestType::HEAD:
            default:
                OnFailure(casper::job::InternalServerException("Method '%s' not implemented!", method_str_.c_str()));
                break;
        }
    });
}

// MARK: -

/**
 * @brief Called by HTTP to report when an API request was performed .
 *
 * @param a_value RAW value.
 */
void casper::http::gateway::Deferred::OnCompleted (const ::cc::easy::HTTPClient::RawValue& a_value)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... save response ...
    std::map<std::string, std::string> headers;
    response_.Set(a_value.code(), a_value.header("Content-Type"), a_value.headers_as_map(headers), a_value.body(), a_value.rtt());
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
void casper::http::gateway::Deferred::OnFailure (const ::cc::Exception& a_exception)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    response_.Set(500, a_exception);
    callbacks_.on_looper_thread_(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + method_str_ + "-failure-", [this](const std::string&) {
        callbacks_.on_completed_(this);
        Untrack();
    });
}
