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

#include "casper/proxy/worker/deferred.h"

#include "casper/job/exceptions.h"

/**
 * @brief Default constructor.
 *
 * @param a_tracking      Request tracking info.
 * @param a_loggable_data
 */
casper::proxy::worker::Deferred::Deferred (const casper::job::deferrable::Tracking& a_tracking, const ev::Loggable::Data& a_loggable_data
                                           CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id))
: ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>(casper::proxy::worker::MakeID(a_tracking), a_tracking CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(a_thread_id)),
    loggable_data_(a_loggable_data),
    http_(nullptr),
    http_oauth2_(nullptr),
    tokens_({
        /* type_       */ "",
        /* access_     */ "",
        /* refresh_    */ "",
        /* expires_in_ */  0,
        /* scope_      */ "",
        /* on_change_  */ nullptr
    })
{
    current_              = Deferred::Operation::NotSet;
    allow_oauth2_restart_ = false;
}

/**
 * @brief Destructor.
 */
casper::proxy::worker::Deferred::~Deferred()
{
    if ( nullptr != http_ ) {
        delete http_;
    }
    if ( nullptr != http_oauth2_ ) {
        delete http_oauth2_;
    }
}

/**
 * @brief Async execute request.
 *
 * @param a_args      This deferred request arguments.
 * @param a_callback  Functions to call by this object when needed.
 */
void casper::proxy::worker::Deferred::Run (const casper::proxy::worker::Arguments& a_args, Callbacks a_callbacks)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(nullptr == http_ && nullptr == http_oauth2_ && nullptr == arguments_);
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... keep track of arguments and callbacks ...
    arguments_ = new casper::proxy::worker::Arguments(a_args);
    callbacks_ = a_callbacks;    
    // ... prepare HTTP client ...
    http_oauth2_ = new ::cc::easy::OAuth2HTTPClient(loggable_data_, arguments_->parameters().config_, tokens_);
    tokens_.on_change_ = std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestTokensChanged, this);
    // ... first, load tokens from DB ...
    ScheduleLoadTokens(true, nullptr, 0);
}

// MARK: -

/**
 * @brief Schedule a 'load tokens' operation.
 *
 * @param a_track  When true request will be tracked.
 * @param a_origin Caller function name.
 * @param a_delay  Delay in ms.
 */
void casper::proxy::worker::Deferred::ScheduleLoadTokens (const bool /* a_track */, const char* const a_origin, const size_t /* a_delay */)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    CC_DEBUG_ASSERT(nullptr == http_ && nullptr != arguments_);
    CC_DEBUG_ASSERT(true == Tracked());
    // ... mark ...
    current_       = Deferred::Operation::LoadTokens;
    operation_str_ = "db/" + std::string(nullptr != a_origin ? a_origin : __FUNCTION__);
    // ... log ...
    callbacks_.on_log_deferred_step_(this, operation_str_ + "...");
    // ... track it now ...
    Track();
    // ... load tokens and then perform request, or just perform request?
    switch (arguments().parameters().type_) {
        case proxy::worker::Config::Type::Storage:
        {
            // ... allow oauth2 restart?
            allow_oauth2_restart_ = arguments().parameters().config_.oauth2_.m2m_;
            // ... then, perform request ...
            operations_.push_back(Deferred::Operation::PerformRequest);
            // ... but first, perform obtain tokens ...
            arguments().parameters().storage_.method_ = ::ev::curl::Request::HTTPRequestType::GET;
            // ... prepare HTTP client ...
            http_ = new ::cc::easy::HTTPClient(loggable_data_);
            // ... HTTP requests must be performed @ MAIN thread ...
            callbacks_.on_main_thread_([this]() {
                const auto& params  = arguments().parameters();
                // ... first load tokens from db ...
                http_->GET(params.storage_.url_, params.storage_.headers_, ::cc::easy::HTTPClient::RawCallbacks({
                                        /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                                        /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                                        /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
                                    })
                );
            });
        }
            break;
        case proxy::worker::Config::Type::Storageless:
        {
            allow_oauth2_restart_ = true;
            // ... just perform request ...
            if ( 0 == tokens_.access_.size() ) {
                ScheduleAuthorization(false, __FUNCTION__, 0);
            } else {
                // TODO: handle with access and refresh expiration retry
                SchedulePerformRequest(false, __FUNCTION__, 0);
            }
        }
            break;
        default:
            throw ::casper::job::InternalServerException("@ %s : Method " UINT8_FMT " - not implemented yet!",
                                                         __FUNCTION__, static_cast<uint8_t>(arguments().parameters().type_)
            );
    }
}

/**
 * @brief Schedule a 'save tokens' operation.
 *
 * @param a_track  When true request will be tracked.
 * @param a_origin Caller function name.
 * @param a_delay  Delay in ms.
 */
void casper::proxy::worker::Deferred::ScheduleSaveTokens (const bool /* a_track */, const char* const a_origin, const size_t /* a_delay */)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    CC_DEBUG_ASSERT(nullptr != arguments_);
    CC_DEBUG_ASSERT(true == Tracked());
    // ... mark ...
    current_       = Deferred::Operation::SaveTokens;
    operation_str_ = "db/" + std::string(nullptr != a_origin ? a_origin : __FUNCTION__);
    // ... log ...
    callbacks_.on_log_deferred_step_(this, operation_str_ + "...");
    // ... save tokens ...
    switch (arguments().parameters().type_) {
        case proxy::worker::Config::Type::Storage:
        {
            // ... but first, perform obtain tokens ...
            arguments().parameters().storage_.method_ = ::ev::curl::Request::HTTPRequestType::POST;
            // ... prepare HTTP client ...
            if ( nullptr != http_ ) {
                http_ = new ::cc::easy::HTTPClient(loggable_data_);
            }
            // ... set body ...
            const ::cc::easy::JSON<::casper::job::InternalServerException> json;
            Json::Value body = Json::Value(Json::ValueType::objectValue);
            body["access_token"]  = tokens_.access_;
            body["refresh_token"] = tokens_.refresh_;
            arguments().parameters().storage_.body_ = json.Write(body);
            // ... HTTP requests must be performed @ MAIN thread ...
            callbacks_.on_main_thread_([this]() {
                auto& params = arguments().parameters();
                // ... save tokens from db ...
                http_->POST(params.storage_.url_, params.storage_.headers_, params.storage_.body_,
                            ::cc::easy::HTTPClient::RawCallbacks({
                                    /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                                    /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                                    /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
                            })
                );
            });
        }
            break;
        case proxy::worker::Config::Type::Storageless:
            // ... nop - tokens already stored in memory ..
            break;
        default:
            throw ::casper::job::InternalServerException("@ %s : Method " UINT8_FMT " - not implemented yet!",
                                                         __FUNCTION__, static_cast<uint8_t>(arguments().parameters().type_)
            );
    }
}

/**
 * @brief Schedule an 'auhtorization request' operation.
 *
 * @param a_track  When true request will be tracked.
 * @param a_origin Caller function name.
 * @param a_delay  Delay in ms.
 */
void casper::proxy::worker::Deferred::ScheduleAuthorization (const bool a_track, const char* const a_origin, const size_t a_delay)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    CC_DEBUG_ASSERT(nullptr != arguments_);
    CC_DEBUG_ASSERT(true == arguments().parameters().config_.oauth2_.m2m_);
    // ... HTTP requests must be performed @ MAIN thread ...
    callbacks_.on_main_thread_([this]() {
        http_oauth2_->AuthorizationRequest({
            /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestCompleted, this, std::placeholders::_1),
            /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestError    , this, std::placeholders::_1),
            /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestFailure  , this, std::placeholders::_1)
        });
    });
}

/**
 * @brief Schedule a 'perform request' operation.
 *
 * @param a_track  When true request will be tracked.
 * @param a_origin Caller function name.
 * @param a_delay  Delay in ms.
 */
void casper::proxy::worker::Deferred::SchedulePerformRequest (const bool a_track, const char* const a_origin, const size_t a_delay)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_ASSERT(nullptr != arguments_);
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

    current_       = Deferred::Operation::PerformRequest;
    operation_str_ = "http/" + std::string(nullptr != a_origin ? a_origin : __FUNCTION__);

    // ... log ...
    callbacks_.on_log_deferred_step_(this, operation_str_ + "...");
    
    // ... track it now?
    if ( true == a_track ) {
        Track();
    }
    // ... HTTP requests must be performed @ MAIN thread ...
    callbacks_.on_main_thread_([this]() {
        const auto& request = arguments().parameters().request_;
        // ... async perform HTTP request ...
        const ::cc::easy::HTTPClient::RawCallbacks callbacks = {
            /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestCompleted, this, std::placeholders::_1),
            /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestError, this, std::placeholders::_1),
            /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPOAuth2RequestFailure, this, std::placeholders::_1)
        };
        switch(request.method_) {
            case ::ev::curl::Request::HTTPRequestType::HEAD:
                http_oauth2_->HEAD(request.url_, request.headers_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::GET:
                http_oauth2_->GET(request.url_, request.headers_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::DELETE:
                http_oauth2_->DELETE(request.url_, request.headers_, ( 0 != request.body_.length() ? &request.body_ : nullptr ), callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::POST:
                http_oauth2_->POST(request.url_, request.headers_, request.body_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::PUT:
                http_oauth2_->PUT(request.url_, request.headers_, request.body_, callbacks, &request.timeouts_);
                break;
            case ::ev::curl::Request::HTTPRequestType::PATCH:
                http_oauth2_->PATCH(request.url_, request.headers_, request.body_, callbacks, &request.timeouts_);
                break;
            default:
                OnHTTPOAuth2RequestFailure(casper::job::InternalServerException("Method '" UINT8_FMT "' not implemented!", static_cast<uint8_t>(request.method_)));
                break;
        }
    });
}

// MARK: - HTTP Client

/**
 * @brief Called by HTTP client to report when an API request was performed.
 *
 * @param a_value RAW value.
 */
void casper::proxy::worker::Deferred::OnHTTPRequestCompleted (const ::cc::easy::HTTPClient::RawValue& a_value)
{
    OnHTTPOAuth2RequestCompleted(a_value);
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an internal error ( NOT server error ).
 *
 * @param a_exception Exception ocurred.
 */
void casper::proxy::worker::Deferred::OnHTTPRequestError (const ::cc::easy::HTTPClient::RawError& a_error)
{
    OnHTTPOAuth2RequestError(a_error);
}

/**
 * @brief
 *
 * @param a_exception
 */
void casper::proxy::worker::Deferred::OnHTTPRequestFailure (const ::cc::Exception& a_exception)
{
    OnHTTPOAuth2RequestFailure(a_exception);
}



// MARK: - HTTP OAuth2 Client

/**
 * @brief Called by HTTP client to report when OAuth2 tokens changed.
 */
void casper::proxy::worker::Deferred::OnHTTPOAuth2RequestTokensChanged ()
{
    operations_.push_back(Deferred::Operation::SaveTokens);
    // TODO: SAVE @ CDB
    // ScheduleSaveTokens(<#const bool a_track#>, <#const char *const a_origin#>, <#const size_t a_delay#>)
}

/**
 * @brief Called by HTTP client to report when an API request was performed.
 *
 * @param a_value RAW value.
 */
void casper::proxy::worker::Deferred::OnHTTPOAuth2RequestCompleted (const ::cc::easy::HTTPClient::RawValue& a_value)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... save response ...
    const std::string content_type = a_value.header_value("Content-Type");
    {
        std::map<std::string, std::string> headers;
        response_.Set(a_value.code(), content_type, a_value.headers_as_map(headers), a_value.body(), a_value.rtt());
    }
    const std::string tag  = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + operation_str_ + ( 200 == response_.code() ? "-succeeded-" : "-failed-" );
    // ... parse response?
    if ( std::string::npos != content_type.find("application/json") ) {
        switch(current_) {
            case Deferred::Operation::LoadTokens:
                response_.Parse();
            case Deferred::Operation::SaveTokens:
            {
                // TODO: easy json
                tokens_.access_  = response_.json()["access_token"].asString();
                tokens_.refresh_ = response_.json()["refresh_token"].asString();
                tokens_.type_    = response_.json()["token_type"].asString();
                response_.Parse();
            }
                break;
            default:
                throw cc::Exception("Don't know how to parse operation " UINT8_FMT " response - not implemented!", static_cast<uint8_t>(current_));
        }
    }
    // ... save response ...
    responses_[current_] = response_;
    // ... finalize now or still work to do?
    bool finalize = ( 200 != response().code() || 0 == operations_.size() );
    if ( finalize == false ) {
        // ... exception ...
        if ( Deferred::Operation::LoadTokens == current_ && 0 == tokens_.access_.size()  && true == allow_oauth2_restart_ ) {
            operations_.push_front(Deferred::Operation::RestartOAuth2);
        }
        std::string tag2 = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-";
        // ... no, more work to do ...
        const auto next = operations_.front();
        operations_.pop_front(); // TODO: FIX THIS - why it make deque empty instead of removing the front element?
        switch(next) {
            case Deferred::Operation::RestartOAuth2:
                callbacks_.on_looper_thread_(tag2 + "-restart-oauth2", [this](const std::string&) {
                    allow_oauth2_restart_ = false;
                    ScheduleAuthorization(false, nullptr, 0);
                });
                break;
            case Deferred::Operation::PerformRequest:
                callbacks_.on_looper_thread_(tag2 + "-perform-request", [this](const std::string&) {
                    SchedulePerformRequest(false, nullptr, 0);
                });
                break;
            case Deferred::Operation::SaveTokens:
                callbacks_.on_looper_thread_(tag2 + "-perform-request", [this](const std::string&) {
                    ScheduleSaveTokens(false, nullptr, 0);
                });
                break;
            default:
                throw cc::Exception("Don't know how to schedule next operation " UINT8_FMT " - not implemented!", static_cast<uint8_t>(next));
        }
    }
    // ... finalize?
    if ( true == finalize ) {
        // ... exception: override 303 responses ...
        if ( 302 == a_value.code() && Deferred::Operation::RestartOAuth2 == current_ ) {
            // TODO: check error string
            response_.Set(500, "application/json", "{\error\":\"TODO\"}", a_value.rtt());
        }
        // ... must be done on 'looper' thread ...
        callbacks_.on_looper_thread_(tag, [this, a_value] (const std::string&) {
            // ... debug log ...
            callbacks_.on_log_deferred_debug_(this, "response/CODE: "  + std::to_string(a_value.code()));
            callbacks_.on_log_deferred_debug_(this, "response/BODY: "  + a_value.body());
            callbacks_.on_log_deferred_debug_(this, "response/BODY/EXPECTED: JSON");
            // ... log error ...
            if ( 200 != response_.code() ) {
                callbacks_.on_log_deferred_error_(this, a_value.body().c_str());
            }
            // ... notify ...
            callbacks_.on_completed_(this);
            // ... done ...
            Untrack();
        });
    }
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an cURL error ( or maybe server error ).
 *
 * @param a_error Error ocurred.
 */
void casper::proxy::worker::Deferred::OnHTTPOAuth2RequestError (const ::cc::easy::HTTPClient::RawError& a_value)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    switch (a_value.code_) {
        case CURLE_OPERATION_TIMEOUTED:
            // 504 Gateway Timeout
            response_.Set(504, "cURL: " + a_value.message());
            break;
        default:
            // 500 Internal Server Error
            response_.Set(500, a_value.message());
            break;
    }
    callbacks_.on_looper_thread_(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + operation_str_ + "-error-", [this](const std::string&) {
        callbacks_.on_completed_(this);
        Untrack();
    });
}


/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an internal error ( NOT server error ).
 *
 * @param a_exception Exception ocurred.
 */
void casper::proxy::worker::Deferred::OnHTTPOAuth2RequestFailure (const ::cc::Exception& a_exception)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    response_.Set(500, a_exception);
    callbacks_.on_looper_thread_(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + operation_str_ + "-failure-", [this](const std::string&) {
        callbacks_.on_completed_(this);
        Untrack();
    });
}
