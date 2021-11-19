/**
 * @file oauth2-client.h
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
 * along with casper-http-gateway.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "casper/http/gateway/oauth2-client.h"

#include "casper/http/gateway/dispatcher.h"

/**
 * @brief Default constructor.
 *
 * param a_loggable_data
 * param a_config
 */
casper::http::gateway::OAuth2Client::OAuth2Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config)
    : ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>("OHC", sk_tube_, a_loggable_data, a_config)
{
    Set(Job::Mode::Gateway);
}

/**
 * @brief Destructor
 */
casper::http::gateway::OAuth2Client::~OAuth2Client ()
{
    /* empty */
}

/**
 * @brief One-shot setup.
 */
void casper::http::gateway::OAuth2Client::InnerSetup ()
{
    // memory managed by base class
    d_.dispatcher_                    = new gateway::Dispatcher(loggable_data_ CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));
    d_.on_deferred_request_completed_ = std::bind(&gateway::OAuth2Client::OnDeferredRequestCompleted, this, std::placeholders::_1, std::placeholders::_2);;
}

/**
 * @brief Process a job to this tube.
 *
 * @param a_id      Job ID.
 * @param a_payload Job payload.
 *
 * @param o_response JSON object.
 */
void casper::http::gateway::OAuth2Client::InnerRun (const int64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response)
{
    const ::cc::easy::JSON<::casper::job::BadRequestException> json;
    // ... assuming BAD REQUEST ...
    o_response.code_ = 400;

    //
    // IN payload:
    //
    // {
    //    "id": <numeric>,
    //    "tube": <string>,
    //    "ttr": <numeric>,
    //    "validity": <validity>,
    // }

    //
    // Payload
    //
    const Json::Value& payload = Payload(a_payload);
    const Json::Value& http    = json.Get(payload, "http", Json::ValueType::objectValue, nullptr);

    // ... prepare tracking info ...
    const ::casper::job::deferrable::Tracking tracking = {
        /* bjid_ */ a_id,
        /* rjnr_ */ RJNR(),
        /* rjid_ */ RJID(),
        /* rcid_ */ RCID(),
        /* dpi_  */ "GW",
    };        
    // TODO: NOW perform requested method, not only POST    
    const gateway::Arguments arguments(http);
    // ... schedule deferred HTTP request ...
    dynamic_cast<gateway::Dispatcher*>(d_.dispatcher_)->Push(tracking, arguments);
    // ... publish progress ...
    ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>::Publish(tracking.bjid_, tracking.rcid_, tracking.rjid_,
                                                                                                  OAuth2ClientStep::DoingIt,
                                                                                                  ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>::Status::InProgress,
                                                                                                  sk_i18n_in_progress_.key_, sk_i18n_in_progress_.arguments_);
    // ... accepted ...
    o_response.code_ = 200;
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
 *         - if no 0, or if an exception is catched finalize job immediatley.
 */
uint16_t casper::http::gateway::OAuth2Client::OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<gateway::Arguments>* a_deferred, Json::Value& o_payload)
{
    // ... finalize ...
    const auto response = a_deferred->response();
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
    // ... set payload ...
    o_payload         = Json::Value(Json::ValueType::objectValue);
    o_payload["data"] = ss.str();
    // ... done ...
    return a_deferred->response().code();
}
