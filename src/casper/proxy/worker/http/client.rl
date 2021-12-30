/**
 * @file client.rl
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
 * along with casper-proxy-worker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "casper/proxy/worker/http/client.h"

#include "casper/proxy/worker/http/dispatcher.h"

#include "version.h"

#include "cc/ragel.h"

#include "cc/fs/dir.h"

#include "cc/v8/exception.h"

#include "version.h"

const char* const casper::proxy::worker::http::Client::sk_tube_      = "http-client";
const Json::Value casper::proxy::worker::http::Client::sk_behaviour_ = "default";

/**
 * @brief Default constructor.
 *
 * param a_loggable_data
 * param a_config
 */
casper::proxy::worker::http::Client::Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config)
    : ClientBaseClass("HC", sk_tube_, a_loggable_data, a_config, /* a_sequentiable */ false)
{
    /* empty */
}

/**
 * @brief Destructor
 */
casper::proxy::worker::http::Client::~Client ()
{
    /* empty */
}

/**
 * @brief One-shot setup.
 */
void casper::proxy::worker::http::Client::InnerSetup ()
{
    // memory managed by base class
    d_.dispatcher_                    = new casper::proxy::worker::http::Dispatcher(loggable_data_, CASPER_PROXY_WORKER_NAME "/" CASPER_PROXY_WORKER_VERSION CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));
    d_.on_deferred_request_completed_ = std::bind(&casper::proxy::worker::http::Client::OnDeferredRequestCompleted, this, std::placeholders::_1, std::placeholders::_2);
    d_.on_deferred_request_failed_    = std::bind(&casper::proxy::worker::http::Client::OnDeferredRequestFailed   , this, std::placeholders::_1, std::placeholders::_2);
}

/**
 * @brief Process a job to this tube.
 *
 * @param a_id      Job ID.
 * @param a_payload Job payload.
 *
 * @param o_response JSON object.
 */
void casper::proxy::worker::http::Client::InnerRun (const int64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response)
{
    const ::cc::easy::JSON<::cc::BadRequest> json;
    // ... assuming BAD REQUEST ...
    o_response.code_ = CC_STATUS_CODE_BAD_REQUEST;
    //
    // IN payload:
    //
    // {
    //    "id": <numeric>,
    //    "tube": <string>,
    //    "ttr": <numeric>,
    //    "validity": <validity>,
    //    "http": <object>
    // }
    bool               broker    = false;
    const Json::Value& payload   = Payload(a_payload, &broker);
    const Json::Value& i18n     = json.Get(payload, "i18n", Json::ValueType::objectValue, &Json::Value::null);
    if ( false == i18n.isNull() ) {
        OverrideI18N(i18n);
    }
    const Json::Value& behaviour = json.Get(payload, "behaviour", Json::ValueType::stringValue, &sk_behaviour_);
    const Json::Value& http      = json.Get(payload, "http"     , Json::ValueType::objectValue, nullptr);
    // ... prepare tracking info ...
    const ::casper::job::deferrable::Tracking tracking = {
        /* bjid_ */ a_id,
        /* rjnr_ */ RJNR(),
        /* rjid_ */ RJID(),
        /* rcid_ */ RCID(),
        /* dpid_ */ "CPW",
        /* ua_   */ dynamic_cast<http::Dispatcher*>(d_.dispatcher_)->user_agent(),
    };

    // ... prepare arguments / parameters ...
    http::Arguments arguments = http::Arguments(
        {
            /* a_data       */ http,
            /* a_primitive  */ ( true == broker && 0 == strcasecmp("gateway", behaviour.asCString()) ),
            /* a_log_level  */ config_.log_level(),
            /* a_log_redact */ config_.log_redact()
        }
    );
    // ... set request properties ...
    (void)arguments.parameters().http_request([&json, &http, &arguments](http::Parameters::HTTPRequest& request){
        // ... prepare request ...
        const Json::Value& url             = json.Get(http   , "url"            , Json::ValueType::stringValue, nullptr);
        const Json::Value& method          = json.Get(http   , "method"         , Json::ValueType::stringValue, nullptr);
        const Json::Value& params          = json.Get(http   , "params"         , Json::ValueType::objectValue, nullptr);
        const Json::Value& body            = json.Get(http   , "body"           , Json::ValueType::objectValue, &Json::Value::null);
        const Json::Value& headers         = json.Get(http   , "headers"        , Json::ValueType::objectValue, nullptr);
        const Json::Value& follow_location = json.Get(http   , "follow_location", Json::ValueType::booleanValue, &Json::Value::null);
        const Json::Value& content_type    = json.Get(headers, "Content-Type"   , Json::ValueType::stringValue, nullptr);
        // ... method ...
        const std::string method_str = method.asString();
        {
            CC_RAGEL_DECLARE_VARS(method, method_str.c_str(), method_str.length());
            CC_DIAGNOSTIC_PUSH();
            CC_DIAGNOSTIC_IGNORED("-Wunreachable-code");
            %%{
                machine ClientMachine;
                main := |*
                    /get/i    => { request.method_ = ::cc::easy::http::Client::Method::GET;    };
                    /put/i    => { request.method_ = ::cc::easy::http::Client::Method::PUT;    };
                    /delete/i => { request.method_ = ::cc::easy::http::Client::Method::DELETE; };
                    /post/i   => { request.method_ = ::cc::easy::http::Client::Method::POST;   };
                    /patch/i  => { request.method_ = ::cc::easy::http::Client::Method::PATCH;  };
                    /head/i   => { request.method_ = ::cc::easy::http::Client::Method::HEAD;   };
                *|;
                write data;
                write init;
                write exec;
            }%%
            CC_RAGEL_SILENCE_VARS(Client)
            CC_DIAGNOSTIC_POP();
        }
        // ... url ...
        request.url_ = url.asString();
        // ... params ...
        if ( false == params.isNull() ) {
            std::map<std::string, std::string> map;
            for ( auto key : params.getMemberNames() ) {
                map[key] = json.Get(params, key.c_str(), Json::ValueType::stringValue, nullptr).asString();
            }
            ::cc::easy::http::Client::SetURLQuery(request.url_, map, request.url_);
        }
        // ... body ...
        if ( false == body.isNull() ) {
            if ( 0 == strncasecmp(content_type.asCString(), "application/json", sizeof(char) * 16) ) {
                request.body_ = json.Write(body);
            } else {
                request.body_ = body.asString();
            }
        }
        // ... headers ...
        for ( auto key : headers.getMemberNames() ) {
            const Json::Value& header = json.Get(headers, key.c_str(), Json::ValueType::stringValue, nullptr);
            request.headers_[key] = { header.asString() };
        }
        // ... follow location?
        if ( false == follow_location.isNull() ) {
            request.follow_location_ = follow_location.asBool();
        }
        // ... timeouts ...
        const Json::Value& timeouts = json.Get(arguments.parameters().data_, "timeouts", Json::ValueType::objectValue, &Json::Value::null);
        if ( false == timeouts.isNull() ) {
            const Json::Value connection_ref = json.Get(timeouts, "connection", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == connection_ref.isNull() ) {
                request.timeouts_.connection_ = static_cast<long>(connection_ref.asInt64());
            }
            const Json::Value operation_ref = json.Get(timeouts, "operation", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == operation_ref.isNull() ) {
                request.timeouts_.operation_ = static_cast<long>(operation_ref.asInt64());
            }
        }
    });
    // ... schedule deferred HTTP request ...
    dynamic_cast<http::Dispatcher*>(d_.dispatcher_)->Push(tracking, arguments);
    // ... publish progress ...
    ClientBaseClass::Publish(tracking.bjid_, tracking.rcid_, tracking.rjid_, ClientStep::DoingIt, ClientBaseClass::Status::InProgress,
                             I18NInProgress()
    );
    // ... accepted ...
    o_response.code_ = CC_STATUS_CODE_OK;
    // ... but it will be deferred ...
    SetDeferred();
}

// MARK: - Method(s) / Function(s) - deferrable::Dispatcher Callbacks

/**
 * @brief Called by 'deferred' request when it's completed.
 *
 * @param a_deferred Deferred request data.
 * @param o_payload  JSON response to fill.
 *
 * @return HTTP Status code:
 *         - if returns 0 don't finalize job now ( still work to do );
 *         - if 0, or if an exception is catched finalize job immediatley.
 */
uint16_t casper::proxy::worker::http::Client::OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<casper::proxy::worker::http::Arguments>* a_deferred, Json::Value& o_payload)
{
    const auto&    params   = a_deferred->arguments().parameters();
    const auto     response = a_deferred->response();
    const uint16_t code     = response.code();
    // ... set payload ...
    o_payload = Json::Value(Json::ValueType::objectValue);
    // ... primitive or default?
    if ( true == params.primitive_ ) {
        // ... gateway response mode ....
        // !<status-code-int-value>,<content-type-length-in-bytes>,<content-type-string-value>,<headers-length-bytes>,<headers>,<body-length-bytes>,<body>
        std::stringstream ss;
        ss << '!' << response.code() << ',' << response.content_type().length() << ',' << response.content_type();
        ss << ',' << response.body().length() << ',' << response.body();
        {
            std::string v = "";
            for ( auto header : a_deferred->response().headers() ) {
                v = header.first + ":" + header.second;
                ss << ',' << v.length() << ',' << v;
            }
        }
        // ... data ...
        o_payload["data"] = ss.str();
    } else {
        // ... body ...
        if ( true == ::cc::easy::JSON<::cc::Exception>::IsJSON(response.content_type()) ) {
            const ::cc::easy::JSON<::cc::Exception> json;
            json.Parse(response.body(), o_payload["body"]);
        } else {
            o_payload["body"] = response.body();
        }
        // ... headers ...
        o_payload["headers"] = Json::Value(Json::ValueType::objectValue);
        for ( auto header : response.headers() ) {
            o_payload["headers"][header.first] = header.second;
        }
        // ... code ...
        o_payload["code"] = response.code();
    }
    // ... done ...
    return code;
}

/**
 * @brief Called by 'deferred' request when it's completed but status code is NOT 200.
 *
 * @param a_deferred Deferred request data.
 * @param o_payload  JSON response to fill.
 *
 * @return HTTP Status code:
 *         - if returns 0 don't finalize job now ( still work to do );
 *         - if 0, or if an exception is catched finalize job immediatley.
 */
uint16_t casper::proxy::worker::http::Client::OnDeferredRequestFailed (const ::casper::job::deferrable::Deferred<casper::proxy::worker::http::Arguments>* a_deferred, Json::Value& o_payload)
{
    const auto&    params   = a_deferred->arguments().parameters();
    const auto     response = a_deferred->response();
    const uint16_t code     = response.code();
    // ... set payload ...
    o_payload = Json::Value(Json::ValueType::objectValue);
    // ... primitive or default?
    if ( true == params.primitive_ ) {
        // ... gateway response mode ....
        // !<status-code-int-value>,<content-type-length-in-bytes>,<content-type-string-value>,<headers-length-bytes>,<headers>,<body-length-bytes>,<body>
        std::stringstream ss;
        ss << '!' << response.code() << ',' << response.content_type().length() << ',' << response.content_type();
        // ... body ...
        const auto exception = a_deferred->response().exception();
        if ( nullptr != exception ) {
            ss << ',' << strlen(exception->what()) << ',' << exception->what();
        } else {
            ss << ',' << response.body().length() << ',' << response.body();
        }
        // ... headers ...
        {
            std::string v = "";
            for ( auto header : a_deferred->response().headers() ) {
                v = header.first + ":" + header.second;
                ss << ',' << v.length() << ',' << v;
            }
        }
        // ... data ...
        o_payload["data"] = ss.str();
    } else {
        // ... body ...
        if ( true == ::cc::easy::JSON<::cc::Exception>::IsJSON(response.content_type()) ) {
            const ::cc::easy::JSON<::cc::Exception> json;
            json.Parse(response.body(), o_payload["body"]);
        } else {
            o_payload["body"] = response.body();
        }
        // ... headers ...
        o_payload["headers"] = Json::Value(Json::ValueType::objectValue);
        for ( auto header : response.headers() ) {
            o_payload["headers"][header.first] = header.second;
        }
        // ... code ...
        o_payload["code"] = response.code();
    }
    // ... done ...
    return code;
}
