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

#include "cc/easy/job/types.h"

extern std::string ede (const std::string&);
extern std::string edd (const std::string&);

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
    http_oauth2_(nullptr)
{
    http_options_         = HTTPOptions::OAuth2 | HTTPOptions::Trace | HTTPOptions::Redact;
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
    // ... (in)sanity checkpoint ...
    CC_DEBUG_ASSERT(nullptr == http_ && nullptr == http_oauth2_ && nullptr == arguments_);
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... update http options ...
    if ( a_args.parameters().log_level_ >= CC_JOB_LOG_LEVEL_VBS ) {
        http_options_ |= HTTPOptions::Log;
        // ... log storage related requests?
        if ( a_args.parameters().log_level_ >= CC_JOB_LOG_LEVEL_DBG ) {
            http_options_ |= HTTPOptions::NonOAuth2;
            http_options_ &= ~HTTPOptions::Redact;
        }
    }
    // ... keep track of arguments and callbacks ...
    arguments_ = new casper::proxy::worker::Arguments(a_args);
    callbacks_ = a_callbacks;
    // ... prepare HTTP client ...
    http_oauth2_ = new ::cc::easy::OAuth2HTTPClient(loggable_data_, arguments_->parameters().config_,
                                                    arguments_->parameters().tokens([this](::cc::easy::OAuth2HTTPClient::Tokens& a_tokens){
                                                        a_tokens.on_change_ = std::bind(&casper::proxy::worker::Deferred::OnOAuth2TokensChanged, this);
                                                    })
    );
    if ( HTTPOptions::NotSet != ( ( HTTPOptions::Log | HTTPOptions::Trace ) & http_options_ ) ) {
        http_oauth2_->SetcURLedCallbacks({
            /* log_request_  */ std::bind(&casper::proxy::worker::Deferred::LogHTTPOAuth2ClientRequest, this, std::placeholders::_1, std::placeholders::_2),
            /* log_response_ */ std::bind(&casper::proxy::worker::Deferred::LogHTTPOAuth2ClientValue  , this, std::placeholders::_1, std::placeholders::_2)
        }, HTTPOptions::Redact == ( HTTPOptions::Redact & http_options_ ));
    }
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
    CC_DEBUG_ASSERT(false == Tracked());
    // ... mark ...
    current_       = Deferred::Operation::LoadTokens;
    operation_str_ = "db/" + std::string(nullptr != a_origin ? a_origin : __FUNCTION__);
    // ... log ...
    callbacks_.on_log_deferred_step_(this, operation_str_ + "...");
    // ... track it now ...
    Track();
    // ... load tokens and then perform request, or just perform request?
    switch (arguments_->parameters().type_) {
        case proxy::worker::Config::Type::Storage:
        {
            // ... allow oauth2 restart?
            allow_oauth2_restart_ = false;
            // ... then, perform request ...
            operations_.push_back(Deferred::Operation::PerformRequest);
            // ... but first, perform obtain tokens ...
            (void)arguments_->parameters().storage(::ev::curl::Request::HTTPRequestType::GET);
            // ... prepare HTTP client ...
            http_ = new ::cc::easy::HTTPClient(loggable_data_);
            if ( HTTPOptions::NotSet != ( ( HTTPOptions::Log | HTTPOptions::Trace ) & http_options_ ) ) {
                http_->SetcURLedCallbacks({
                    /* log_request_  */ std::bind(&casper::proxy::worker::Deferred::LogHTTPRequest, this, std::placeholders::_1, std::placeholders::_2),
                    /* log_response_ */ std::bind(&casper::proxy::worker::Deferred::LogHTTPValue  , this, std::placeholders::_1, std::placeholders::_2)
                }, HTTPOptions::Redact == ( HTTPOptions::Redact & http_options_ ));
            }
            // ... HTTP requests must be performed @ MAIN thread ...
            callbacks_.on_main_thread_([this]() {
                // ... first load tokens from db ...
                const auto& storage = arguments_->parameters().storage();
                http_->GET(storage.url_, storage.headers_,
                           ::cc::easy::HTTPClient::RawCallbacks({
                              /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                              /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                              /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
                           }),
                           &storage.timeouts_
                );
            });
        }
            break;
        case proxy::worker::Config::Type::Storageless:
        {
            // ... since we're storageless, we're m2m ....
            allow_oauth2_restart_ = true;
            // ... just perform request ...
            if ( 0 == arguments_->parameters().tokens().access_.size() ) {
                // ... then, perform request ...
                operations_.push_back(Deferred::Operation::PerformRequest);
                // .... but first, schedule authorization ....
                ScheduleAuthorization(false, __FUNCTION__, 0);
            } else {
                // ... since we have tokens, use them and perform the request ...
                SchedulePerformRequest(false, __FUNCTION__, 0);
            }
        }
            break;
        default:
            throw ::cc::NotImplemented("@ %s : Method " UINT8_FMT " - not implemented yet!",
                                            __FUNCTION__, static_cast<uint8_t>(arguments_->parameters().type_)
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
    switch (arguments_->parameters().type_) {
        case proxy::worker::Config::Type::Storage:
        {
            // ... prepare HTTP client ...
            if ( nullptr == http_ ) {
                http_ = new ::cc::easy::HTTPClient(loggable_data_);
                if ( HTTPOptions::NotSet != ( ( HTTPOptions::Log | HTTPOptions::Trace ) & http_options_ ) ) {
                    http_->SetcURLedCallbacks({
                        /* log_request_  */ std::bind(&casper::proxy::worker::Deferred::LogHTTPRequest, this, std::placeholders::_1, std::placeholders::_2),
                        /* log_response_ */ std::bind(&casper::proxy::worker::Deferred::LogHTTPValue  , this, std::placeholders::_1, std::placeholders::_2)
                    }, HTTPOptions::Redact == ( HTTPOptions::Redact & http_options_ ));
                }
            }
            // ... perform save tokens ...
            const auto& tokens = arguments_->parameters().tokens();
            // ... set body ...
            const ::cc::easy::JSON<::cc::InternalServerError> json;
            Json::Value body = Json::Value(Json::ValueType::objectValue);
            body["access_token"]  = ede(tokens.access_);
            body["refresh_token"] = ede(tokens.refresh_);
            body["expires_in"]    = static_cast<Json::UInt64>(tokens.expires_in_);
            body["scope"]         = tokens.scope_;
            (void)arguments_->parameters().storage(::ev::curl::Request::HTTPRequestType::POST, json.Write(body));
            // ... HTTP requests must be performed @ MAIN thread ...
            callbacks_.on_main_thread_([this]() {
                const auto& storage = arguments_->parameters().storage();
                // ... save tokens from db ...
                http_->POST(storage.url_, storage.headers_, storage.body_,
                            ::cc::easy::HTTPClient::RawCallbacks({
                                    /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                                    /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                                    /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
                            }),
                            &storage.timeouts_
                );
            });
        }
            break;
        case proxy::worker::Config::Type::Storageless:
            // ... nop - tokens already stored in memory ..
            break;
        default:
            throw ::cc::NotImplemented("@ %s : Method " UINT8_FMT " - not implemented yet!",
                                       __FUNCTION__, static_cast<uint8_t>(arguments_->parameters().type_)
            );
    }
}

/**
 * @brief Schedule an 'authorization request' operation.
 *
 * @param a_track  When true request will be tracked.
 * @param a_origin Caller function name.
 * @param a_delay  Delay in ms.
 */
void casper::proxy::worker::Deferred::ScheduleAuthorization (const bool a_track, const char* const a_origin, const size_t a_delay)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    CC_DEBUG_ASSERT(true == Tracked());
    CC_DEBUG_ASSERT(nullptr != arguments_);
    // ... mark ...
    current_       = Deferred::Operation::RestartOAuth2;
    operation_str_ = "http/" + std::string(nullptr != a_origin ? a_origin : __FUNCTION__);
    
    auto grant_type = arguments().parameters().config_.oauth2_.grant_type_;
    // ... HTTP requests must be performed @ MAIN thread ...
    callbacks_.on_main_thread_([this, grant_type]() {
        // TODO: grant_type_ @ config http_oauth2_->AuthorizationRequest({
        if ( 0 == strcasecmp(grant_type.id_.c_str(), "client_credentials") ) {
            http_oauth2_->ClientCredentialsGrant({
                /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
            }, /* a_rfc_6749 */ grant_type.rfc_6749_strict_);
        } else if ( 0 == strcasecmp(grant_type.id_.c_str(), "authorization_code") ) {
            // TODO: read authorization code from params
            http_oauth2_->AuthorizationCodeGrant("<TODO>", {
                /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
            }, /* a_rfc_6749 */ grant_type.rfc_6749_strict_);
        } else if ( 0 == strcasecmp(grant_type.id_.c_str(), "authorization_code-auto") ) {
            http_oauth2_->AuthorizationCodeGrant({
                /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
                 /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
                /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
            });
        }
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
    // ... mark ...
    current_       = Deferred::Operation::PerformRequest;
    operation_str_ = "http/" + std::string(nullptr != a_origin ? a_origin : __FUNCTION__);
    // ... log ...
    callbacks_.on_log_deferred_step_(this, operation_str_ + "...");
    // ... track it now?
    if ( true == a_track ) {
        Track();
    }
    CC_DEBUG_ASSERT(true == Tracked());
    // ... HTTP requests must be performed @ MAIN thread ...
    callbacks_.on_main_thread_([this]() {
        const auto& request = arguments_->parameters().request();
        // ... async perform HTTP request ...
        const ::cc::easy::HTTPClient::RawCallbacks callbacks = {
            /* on_success_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestCompleted, this, std::placeholders::_1),
            /* on_error_   */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestError    , this, std::placeholders::_1),
            /* on_failure_ */ std::bind(&casper::proxy::worker::Deferred::OnHTTPRequestFailure  , this, std::placeholders::_1)
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
                throw ::cc::NotImplemented("Method '" UINT8_FMT "' not implemented!", static_cast<uint8_t>(request.method_));
        }
    });
}

/**
 * @brief Call this method when it's time to signal that this request is now completed.
 *
 * @param a_tag Callback tag.
 */
void casper::proxy::worker::Deferred::Finalize (const std::string& a_tag)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... must be done on 'looper' thread ...
    callbacks_.on_looper_thread_(a_tag, [this] (const std::string&) {
        // ... if request failed, and if we're tracing and did not log HTTP calls, should we do it now?
        if ( CC_EASY_HTTP_OK != response_.code() && HTTPOptions::Trace == ( HTTPOptions::Trace & http_options_ ) && not ( HTTPOptions::Log == ( HTTPOptions::Log & http_options_ ) ) ) {
            for ( const auto& trace : http_trace_ ) {
                callbacks_.on_log_deferred_(this, CC_JOB_LOG_LEVEL_VBS ,CC_JOB_LOG_STEP_HTTP, trace.data_);
            }
        }
        // ... notify ...
        callbacks_.on_completed_(this);
        // ... done ...
        Untrack();
    });
}

// MARK: - HTTP && OAuth2 HTTP Clients

/**
 * @brief Called by HTTP client to report when OAuth2 tokens changed.
 */
void casper::proxy::worker::Deferred::OnOAuth2TokensChanged ()
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... push next operation to run after this one is successfully completed ...
    if ( proxy::worker::Config::Type::Storage == arguments_->parameters().type_ ) {
        operations_.push_back(Deferred::Operation::SaveTokens);
    }
}

/**
 * @brief Called by HTTP client to report when an API request was performed.
 *
 * @param a_value RAW value.
 */
void casper::proxy::worker::Deferred::OnHTTPRequestCompleted (const ::cc::easy::HTTPClient::RawValue& a_value)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... save response ...
    const std::string content_type = a_value.header_value("Content-Type");
    {
        std::map<std::string, std::string> headers;
        response_.Set(a_value.code(), content_type, a_value.headers_as_map(headers), a_value.body(), a_value.rtt());
    }
    const std::string tag = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + operation_str_ + ( CC_EASY_HTTP_OK == response_.code() ? "-succeeded-" : "-failed-" );
    // ... parse response?
    bool acceptable = ( CC_EASY_HTTP_OK == response_.code() );
    if ( ::cc::easy::JSON<::cc::InternalServerError>::IsJSON(content_type) ) {
        const ::cc::easy::JSON<::cc::InternalServerError> json;
        switch(current_) {
            case Deferred::Operation::LoadTokens:
            {
                // ... parse response and read tokens ...
                response_.Parse();
                if ( CC_EASY_HTTP_OK == response_.code() ) {
                    const Json::Value& data = response_.json();
                    (void)arguments_->parameters().tokens([&json, &data](::cc::easy::OAuth2HTTPClient::Tokens& a_tokens) {
                        a_tokens.type_    = json.Get(data, "token_type", Json::ValueType::stringValue, nullptr).asString();
                        a_tokens.access_  = edd(json.Get(data, "access_token", Json::ValueType::stringValue, nullptr).asString());
                        a_tokens.refresh_ = edd(json.Get(data, "refresh_token", Json::ValueType::stringValue, nullptr).asString());
                        const Json::Value& scope = json.Get(data, "token_type", Json::ValueType::stringValue, &Json::Value::null);
                        if ( false == scope.isNull() ) {
                            a_tokens.scope_ = data["scope"].asString();
                        } else {
                            a_tokens.scope_ = "";
                        }
                        const Json::Value& expires_in = json.Get(data, "expires_in", Json::ValueType::uintValue, &Json::Value::null);
                        if ( false == expires_in.isNull() ) {
                            a_tokens.expires_in_ = static_cast<size_t>(expires_in.asUInt64());
                        } else {
                            a_tokens.expires_in_ = 0;
                        }
                    });
                }
            }
                break;
            case Deferred::Operation::SaveTokens:
                response_.Parse();
                break;
            case Deferred::Operation::PerformRequest:
                break;
            case Deferred::Operation::RestartOAuth2:
            {
                // ... parse response and read tokens ...
                response_.Parse();
                if ( CC_EASY_HTTP_OK == response_.code() ) {
                    const Json::Value& data = response_.json();
                    (void)arguments_->parameters().tokens([&json, &data](::cc::easy::OAuth2HTTPClient::Tokens& a_tokens) {
                        a_tokens.access_ = json.Get(data, "access_token", Json::ValueType::stringValue, nullptr).asString();
                        const Json::Value& refresh_token = json.Get(data, "refresh_token", Json::ValueType::stringValue, &Json::Value::null);
                        if ( false == refresh_token.isNull() ) {
                            a_tokens.refresh_ = refresh_token.asString();
                        }
                        const Json::Value& token_type = json.Get(data, "token_type", Json::ValueType::stringValue, &Json::Value::null);
                        if ( false == token_type.isNull() ) {
                            a_tokens.type_ = token_type.asString();
                        }
                        const Json::Value& expires_in = json.Get(data, "expires_in", Json::ValueType::uintValue, &Json::Value::null);
                        if ( false == expires_in.isNull() ) {
                            a_tokens.expires_in_ = static_cast<size_t>(expires_in.asUInt64());
                        } else {
                            a_tokens.expires_in_ = 0;
                        }
                    });
                }
                // ... next, save tokens?
                if ( proxy::worker::Config::Type::Storage == arguments_->parameters().type_ ) {
                    operations_.insert(operations_.begin(), Deferred::Operation::SaveTokens);
                }
            }
                break;
            default:
                throw cc::Exception("Don't know how to parse operation " UINT8_FMT " response - not implemented!", static_cast<uint8_t>(current_));
        }
    }
    // ... override 'acceptable' flag ...
    // ... and OAuth2 process should be restarted?
    if ( false == acceptable ) {
        switch(current_) {
            case Deferred::Operation::LoadTokens:
                // ... no tokens available ...
                acceptable = ( CC_EASY_HTTP_NOT_FOUND == response_.code() );
                // ... obtain new pair? ( no need to add save tokens operation - it will be added upon success )
                if ( 0 == arguments_->parameters().tokens().access_.size() && true == acceptable && true == allow_oauth2_restart_ ) {
                    operations_.insert(operations_.begin(), Deferred::Operation::RestartOAuth2);
                }
                break;
            case Deferred::Operation::PerformRequest:
                // ... tokens renewal problem ( refresh absent or expired ) ...
                acceptable = ( CC_EASY_HTTP_UNAUTHORIZED == response_.code() );
                if ( true == acceptable && true == allow_oauth2_restart_ ) {
                    // ... forget all operations ...
                    operations_.clear();
                    // ... obtain new pair ( no need to add save tokens operation - it will be added upon success ) ...
                    operations_.push_back(Deferred::Operation::RestartOAuth2);
                    // ... replay failed request ...
                    operations_.push_back(Deferred::Operation::PerformRequest);
                }
                break;
            default:
                // ... nothing to do here ...
                break;
        }
    }
    // ... failed to renew tokens exception ...
    if ( proxy::worker::Config::Type::Storage == arguments_->parameters().type_ ) {
        if ( false == acceptable && current_ != Deferred::Operation::SaveTokens ) {
            // ... if there's a pending operation to 'store' tokens ...
            const auto it = std::find_if(operations_.begin(), operations_.end(), [](const Deferred::Operation& a_operation) {
                return ( Deferred::Operation::SaveTokens == a_operation );
            });
            if ( operations_.end() != it ) {
                // ... forget all others, except this one ...
                operations_.clear();
                operations_.push_back(Deferred::Operation::SaveTokens);
                // ... and override 'acceptable' flag ...
                acceptable = true;
            }
        }
    }
    // ... save response ...
    responses_[current_] = response_;
    // ... finalize now or still work to do?
    bool finalize = ( false == acceptable || 0 == operations_.size() );
    if ( finalize == false ) {
        const std::string tag2 = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-";
        // ... no, more work to do ...
        const auto next = operations_.front();
        operations_.erase(operations_.begin());
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
                callbacks_.on_looper_thread_(tag2 + "-save-tokens", [this](const std::string&) {
                    ScheduleSaveTokens(false, nullptr, 0);
                });
                break;
            default:
                throw cc::Exception("Don't know how to schedule next operation " UINT8_FMT " - not implemented!", static_cast<uint8_t>(next));
        }
    }
    // ... finalize?
    if ( true == finalize ) {
        // ... exception: override 302 responses ...
        if ( CC_EASY_HTTP_MOVED_TEMPORARILY == a_value.code() && Deferred::Operation::RestartOAuth2 == current_ ) {
            // TODO: check error string
            response_.Set(CC_EASY_HTTP_INTERNAL_SERVER_ERROR, "application/json", "{\error\":\"TODO\"}", a_value.rtt());
        } else {
            // ... 'main' target is 'PerformRequest' operation response ...
            const std::vector<Deferred::Operation> priority = {
                Deferred::Operation::PerformRequest, Deferred::Operation::LoadTokens, Deferred::Operation::RestartOAuth2, Deferred::Operation::SaveTokens
            };
            for ( const auto& p : priority ) {
                const auto it = std::find_if(responses_.begin(), responses_.end(), [&p](const std::pair<Operation, job::deferrable::Response>& a_result) {
                    return ( p == a_result.first );
                });
                if ( it != responses_.end() ) {
                    response_ = it->second;
                    break;
                }
            }
        }
        // ... finalize ...
        Finalize(tag);
    }
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an cURL error ( or maybe server error ).
 *
 * @param a_error Error ocurred.
 */
void casper::proxy::worker::Deferred::OnHTTPRequestError (const ::cc::easy::HTTPClient::RawError& a_value)
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
    Finalize(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + operation_str_ + "-error-");
}

/**
 * @brief Called by HTTP client to report when an API call to the provided endpoint was not performed - usually due to an internal error ( NOT server error ).
 *
 * @param a_exception Exception ocurred.
 */
void casper::proxy::worker::Deferred::OnHTTPRequestFailure (const ::cc::Exception& a_exception)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... set response ...
    response_.Set(CC_EASY_HTTP_INTERNAL_SERVER_ERROR, a_exception);
    // ... finalize ...
    Finalize(std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-" + operation_str_ + "-failure-");
}

// MARK: - HTTP Client Callbacks

/**
 * @brief Called by an HTTP client when it's time to log a request.
 *
 * @param a_request Request that will be running.
 * @param a_data    cURL(ed) style command ( for log proposes only ).
 */
void casper::proxy::worker::Deferred::LogHTTPRequest (const ::ev::curl::Request& a_request, const std::string& a_data)
{
    OnHTTPRequestWillRunLogIt(a_request, a_data, ( ( http_options_ &~ HTTPOptions::OAuth2 ) | HTTPOptions::NonOAuth2 ));
}

/**
 * @brief Called by an HTTP client when it's time to log a request.
 *
 * @param a_value Post request execution, result data.
 * @param a_data    cURL(ed) style response data ( for log proposes only ).
 */
void casper::proxy::worker::Deferred::LogHTTPValue (const ::ev::curl::Value& a_value, const std::string& a_data)
{
    OnHTTPRequestSteppedLogIt(a_value, a_data, ( ( http_options_ &~ HTTPOptions::OAuth2 ) | HTTPOptions::NonOAuth2 ));
}

// MARK: - HTTP OAuth2 Client Callbacks

/**
 * @brief Called by an HTTP client when it's time to log a request.
 *
 * @param a_request Request that will be running.
 * @param a_data    cURL(ed) style command ( for log proposes only ).
 */
void casper::proxy::worker::Deferred::LogHTTPOAuth2ClientRequest (const ::ev::curl::Request& a_request, const std::string& a_data)
{
    OnHTTPRequestWillRunLogIt(a_request, a_data, ( ( http_options_ &~ HTTPOptions::NonOAuth2 ) | HTTPOptions::OAuth2 ));
}

/**
 * @brief Called by an HTTP client when it's time to log a request.
 *
 * @param a_value Post request execution, result data.
 * @param a_data  cURL(ed) style response data ( for log proposes only ).
 */
void casper::proxy::worker::Deferred::LogHTTPOAuth2ClientValue (const ::ev::curl::Value& a_value, const std::string& a_data)
{
    OnHTTPRequestSteppedLogIt(a_value, a_data, ( ( http_options_ &~ HTTPOptions::NonOAuth2 ) | HTTPOptions::OAuth2 ));
}

/**
 * @brief Called by an HTTP client when a request will run and it's time to log data ( ⚠️ for logging proposes only, request has not started yet ! )
 *
 * @param a_request Request that will be running.
 * @param a_data    cURL(ed) style command ( for log proposes only ).
 * @param a_options Adjusted options for this request, for more info See \link proxy::worker::Deferred::HTTPOptions \link.

 */
void casper::proxy::worker::Deferred::OnHTTPRequestWillRunLogIt (const ::ev::curl::Request& /* a_request */, const std::string& a_data, const proxy::worker::Deferred::HTTPOptions a_options)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    //... make sure that we're tracing or logging if it was requested to do it so ..
    if (
        ( ( HTTPOptions::Trace == ( HTTPOptions::Trace & a_options ) || ( HTTPOptions::Trace == ( HTTPOptions::Log & a_options ) ) ) )
                &&
        (
            ( HTTPOptions::OAuth2 == ( HTTPOptions::OAuth2 & a_options ) && HTTPOptions::OAuth2 == ( HTTPOptions::OAuth2 & http_options_ ) )
                ||
            ( HTTPOptions::NonOAuth2 == ( HTTPOptions::NonOAuth2 & a_options ) && HTTPOptions::NonOAuth2 == ( HTTPOptions::NonOAuth2 & http_options_ ) )
        )
    ) {
        // ... must be done on 'looper' thread ...
        const std::string tag = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-log-http-oauth2-client-response";
        callbacks_.on_looper_thread_(tag, [this, a_data, a_options] (const std::string&) {
            // ... log?
            if ( HTTPOptions::Log == ( HTTPOptions::Log & a_options ) ) {
                callbacks_.on_log_deferred_(this, CC_JOB_LOG_LEVEL_VBS ,CC_JOB_LOG_STEP_HTTP, a_data);
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
void casper::proxy::worker::Deferred::OnHTTPRequestSteppedLogIt (const ::ev::curl::Value& a_value, const std::string& a_data, const proxy::worker::Deferred::HTTPOptions a_options)
{
    // ... (in)sanity checkpoint ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    //... make sure that we're tracing or logging if it was requested to do it so ..
    if (
        ( ( HTTPOptions::Trace == ( HTTPOptions::Trace & a_options ) || ( HTTPOptions::Trace == ( HTTPOptions::Log & a_options ) ) ) )
                &&
        (
            ( HTTPOptions::OAuth2 == ( HTTPOptions::OAuth2 & a_options ) && HTTPOptions::OAuth2 == ( HTTPOptions::OAuth2 & http_options_ ) )
                ||
            ( HTTPOptions::NonOAuth2 == ( HTTPOptions::NonOAuth2 & a_options ) && HTTPOptions::NonOAuth2 == ( HTTPOptions::NonOAuth2 & http_options_ ) )
        )
    ) {
        const uint16_t code = a_value.code();
        // ... must be done on 'looper' thread ...
        const std::string tag = std::to_string(tracking_.bjid_) + "-" + tracking_.rjid_ + "-log-http-oauth2-client-response";
        callbacks_.on_looper_thread_(tag, [this, a_data, a_options, code] (const std::string&) {
            // ... log?
            if ( HTTPOptions::Log == ( HTTPOptions::Log & a_options ) ) {
                callbacks_.on_log_deferred_(this, CC_JOB_LOG_LEVEL_VBS, CC_JOB_LOG_STEP_HTTP, a_data);
            } else { // assuming trace
                http_trace_.push_back({
                    /* code_ */ code,
                    /* data_ */ a_data
                });
            }
        });
    }
}
