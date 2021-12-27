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

#include "casper/proxy/worker/http/oauth2/client.h"

#include "casper/proxy/worker/http/oauth2/dispatcher.h"

#include "version.h"

#include "cc/ragel.h"

#include "cc/fs/dir.h"

#include "cc/v8/exception.h"

const char* const casper::proxy::worker::http::oauth2::Client::sk_tube_      = "oauth2-http-client";
const Json::Value casper::proxy::worker::http::oauth2::Client::sk_behaviour_ = "default";
const casper::proxy::worker::http::oauth2::Client::RejectedHeadersSet casper::proxy::worker::http::oauth2::Client::sk_rejected_headers_ = {
    "Authorization", "User-Agent", "X-CASPER-ROLE-MASK"
};

/**
 * @brief Default constructor.
 *
 * param a_loggable_data
 * param a_config
 */
casper::proxy::worker::http::oauth2::Client::Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config)
    : ::casper::job::deferrable::Base<Arguments, ClientStep, ClientStep::Done>("OHC", sk_tube_, a_loggable_data, a_config, /* a_sequentiable */ false)
{
    script_ = nullptr;
}

/**
 * @brief Destructor
 */
casper::proxy::worker::http::oauth2::Client::~Client ()
{
    for ( const auto& it : providers_ ) {
        delete it.second;
    }
    providers_.clear();
    if ( nullptr != script_ ) {
        delete script_;
    }
}

/**
 * @brief One-shot setup.
 */
void casper::proxy::worker::http::oauth2::Client::InnerSetup ()
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(0 == providers_.size());
    //
    const ::cc::easy::JSON<::cc::InternalServerError> json;
    // memory managed by base class
    d_.dispatcher_                    = new casper::proxy::worker::http::oauth2::Dispatcher(loggable_data_, CASPER_PROXY_WORKER_NAME "/" CASPER_PROXY_WORKER_VERSION CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));
    d_.on_deferred_request_completed_ = std::bind(&casper::proxy::worker::http::oauth2::Client::OnDeferredRequestCompleted, this, std::placeholders::_1, std::placeholders::_2);
    d_.on_deferred_request_failed_    = std::bind(&casper::proxy::worker::http::oauth2::Client::OnDeferredRequestFailed   , this, std::placeholders::_1, std::placeholders::_2);
    // ...
    const auto object2headers = [&json] (const Json::Value& a_object) -> ::cc::easy::http::oauth2::Client::Headers {
        ::cc::easy::http::oauth2::Client::Headers h;
        if ( false == a_object.isNull() ) {
            for ( auto member : a_object.getMemberNames() ) {
                h[member].push_back(json.Get(a_object, member.c_str(), Json::ValueType::stringValue, nullptr).asString());
            }
        }
        return h;
    };
    const Json::Value& config = config_.other();
    {
        const Json::Value empty_string = "";
        const Json::Value& providers = json.Get(config, "providers", Json::ValueType::objectValue, nullptr);
        for ( auto name : providers.getMemberNames() ) {
            const Json::Value& provider_ref   = json.Get(providers   , name.c_str(), Json::ValueType::objectValue, nullptr);
            const Json::Value& oauth2_ref     = json.Get(provider_ref, "oauth2"    , Json::ValueType::objectValue , nullptr);
            const Json::Value& type_ref       = json.Get(provider_ref, "type"      , Json::ValueType::stringValue , nullptr);
            const Json::Value& grant_type_ref = json.Get(oauth2_ref  , "grant_type", Json::ValueType::stringValue , nullptr);
            const Json::Value& grant_type_obj = json.Get(oauth2_ref  , grant_type_ref.asCString(), Json::ValueType::objectValue , nullptr);
            const Json::Value  k_auto         = Json::Value(false);
            const ::cc::easy::http::oauth2::Client::Config config = ::cc::easy::http::oauth2::Client::Config({
                /* oauth2_ */ {
                    /* grant_   */ {
                      /* name_            */ grant_type_ref.asString(),
                      /* type_            */ TranslatedGrantType(grant_type_ref.asString()),
                      /* rfc_6749_strict_ */ json.Get(grant_type_obj, "rfc6749", Json::ValueType::booleanValue, nullptr).asBool(),
                      /* formpost_        */ json.Get(grant_type_obj, "formpost", Json::ValueType::booleanValue, nullptr).asBool(),
                      /* auto_            */ json.Get(grant_type_obj, "auto", Json::ValueType::booleanValue, &k_auto).asBool()
                    },
                    /* urls_ */ {
                        /* authorization_ */ json.Get(oauth2_ref, "authorization_url", Json::ValueType::stringValue, nullptr).asString(),
                        /* token_         */ json.Get(oauth2_ref, "token_url"        , Json::ValueType::stringValue, nullptr).asString()
                    },
                    /* credentials_ */ {
                        /* client_id_     */ json.Get(oauth2_ref, "client_id"    , Json::ValueType::stringValue, nullptr).asString(),
                        /* client_secret_ */ json.Get(oauth2_ref, "client_secret", Json::ValueType::stringValue, nullptr).asString()
                    },
                    /* redirect_uri_ */ json.Get(oauth2_ref, "redirect_uri", Json::ValueType::stringValue, nullptr).asString(),
                    /* scope_        */ json.Get(oauth2_ref, "scope"       , Json::ValueType::stringValue, &empty_string).asString()
                }
            });
            // ...
            ::cc::easy::http::oauth2::Client::Headers headers;
            const Json::Value& headers_ref = json.Get(provider_ref, "headers", Json::ValueType::objectValue, &Json::Value::null);
            if ( false == headers_ref.isNull() ) {
                headers = object2headers(headers_ref);
            }
            ::cc::easy::http::oauth2::Client::HeadersPerMethod headers_per_method;
            {
                const Json::Value& headers_per_method_ref = json.Get(provider_ref, "headers_per_method", Json::ValueType::objectValue, &Json::Value::null);
                if ( false == headers_per_method_ref.isNull() ) {
                    for ( const auto& member : headers_per_method_ref.getMemberNames() ) {
                        headers_per_method[member] = object2headers(json.Get(headers_per_method_ref, member.c_str(), Json::ValueType::objectValue, nullptr));
                    }
                }
            }
            // ...
            proxy::worker::http::oauth2::Config* p_config = nullptr;
            try {
                // ... fix paths ...
                Json::Value signing = json.Get(provider_ref, "signing", Json::ValueType::objectValue, &Json::Value::null);
                if ( false == signing.isNull() && true == signing.isMember("keys") ) {
                    Json::Value& keys = signing["keys"];
                    if ( true == keys.isObject() ) {
                        for ( const auto m : { "private", "public" } ) {
                            if ( true == keys.isMember(m) && true == keys[m].isString() ) {
                                keys[m] = cc::fs::Dir::RealPath(keys[m].asString());
                            }
                        }
                    }
                }
                // ... storage or storageless?
                if ( 0 == strcasecmp(type_ref.asCString(), "storage") ) {
                    const Json::Value& storage_ref = json.Get(provider_ref, "storage", Json::ValueType::objectValue, nullptr);
                    const Json::Value& endpoints_ref = json.Get(storage_ref, "endpoints", Json::ValueType::objectValue, nullptr);
                    Json::Value timeouts = json.Get(storage_ref, "timeouts", Json::ValueType::objectValue, &Json::Value::null);
                    if ( true == timeouts.isNull() ) {
                        timeouts = Json::Value(Json::ValueType::objectValue);
                        timeouts["connection"] = static_cast<Json::Int>(sk_storage_connection_timeout_);
                        timeouts["operation"]  = static_cast<Json::Int>(sk_storage_operation_timeout_);
                    }
                    p_config = new proxy::worker::http::oauth2::Config( {
                        /* http_               */ config,
                        /* headers             */ headers,
                        /* headers_per_method_ */ headers_per_method,
                        /* signing_            */ signing,
                        /* storage_            */
                        proxy::worker::http::oauth2::Config::Storage({
                            /* endpoints_ */ {
                                /* tokens_  */ json.Get(endpoints_ref, "tokens", Json::ValueType::stringValue, nullptr).asString()
                            },
                            /* arguments_ */ json.Get(storage_ref, "arguments", Json::ValueType::objectValue, &Json::Value::null),
                            /* headers_ */ object2headers(json.Get(storage_ref, "headers", Json::ValueType::objectValue, &Json::Value::null)),
                            /* timeouts_ */ {
                                /* connection_ */ static_cast<long>(json.Get(timeouts, "connection", Json::ValueType::intValue, nullptr).asInt()),
                                /* operation_  */ static_cast<long>(json.Get(timeouts, "operation", Json::ValueType::intValue, nullptr).asInt()),
                            }
                        })
                    });
                } else if ( 0 == strcasecmp(type_ref.asCString(), "storageless") ) {
                    p_config = new proxy::worker::http::oauth2::Config( {
                        /* http_               */ config,
                        /* headers             */ headers,
                        /* headers_per_method_ */ headers_per_method,
                        /* signing_            */ signing,
                        /* storage_            */
                        proxy::worker::http::oauth2::Config::Storageless({
                            /* headers_ */ {},
                        })
                    });
                } else {
                    throw ::cc::InternalServerError("Unknown provider type '%s'!", type_ref.asCString());
                }
                providers_[name] = p_config;
                p_config = nullptr;
            } catch (...) {
                if ( nullptr != p_config ) {
                    delete p_config;
                }
                ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
            }
        }
    }
    // ... prepare v8 simple expression evaluation script ...
    script_ = new casper::proxy::worker::v8::Script(/* a_owner */ tube_, /* a_name */ config_.log_token(),
                                                     /* a_uri */ "thin air", /* a_out_path */ logs_directory()
    );
    // ... load it now ...
    script_->Load(/* a_external_scripts */ Json::Value::null, /* a_expressions */ {});
    // ... for debug purposes only ...
    CC_DEBUG_LOG_PRINT("dump-config", "----\n%s----\n", config.toStyledString().c_str());
}

/**
 * @brief Process a job to this tube.
 *
 * @param a_id      Job ID.
 * @param a_payload Job payload.
 *
 * @param o_response JSON object.
 */
void casper::proxy::worker::http::oauth2::Client::InnerRun (const int64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response)
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
    //    "what": <string> : grant or http
    //    "grant": <object> or "http": <object>
    // }
    bool               broker    = false;
    const Json::Value& payload   = Payload(a_payload, &broker);
    const Json::Value& behaviour = json.Get(payload, "behaviour", Json::ValueType::stringValue, &sk_behaviour_);
    const Json::Value& provider  = json.Get(payload, "provider" , Json::ValueType::stringValue, nullptr);
    const auto& provider_it = providers_.find(provider.asString());
    if ( providers_.end() == provider_it ) {
        throw ::cc::BadRequest("Unknown provider '%s'!", provider.asCString());
    }
    
    const Json::Value& what_ref = json.Get(payload, "what"              , Json::ValueType::stringValue, nullptr);
    const Json::Value& what_obj = json.Get(payload, what_ref.asCString(), Json::ValueType::objectValue, &Json::Value::null);
    
    // ... prepare tracking info ...
    const ::casper::job::deferrable::Tracking tracking = {
        /* bjid_ */ a_id,
        /* rjnr_ */ RJNR(),
        /* rjid_ */ RJID(),
        /* rcid_ */ RCID(),
        /* dpid_ */ "CPW",
        /* ua_   */ dynamic_cast<casper::proxy::worker::http::oauth2::Dispatcher*>(d_.dispatcher_)->user_agent(),
    };

    // ... prepare arguments / parameters ...
    casper::proxy::worker::http::oauth2::Arguments arguments = casper::proxy::worker::http::oauth2::Arguments(
        {
            /* a_id        */ provider_it->first,
            /* a_type      */ provider_it->second->type_,
            /* a_config    */ provider_it->second->http_,
            /* a_data      */ what_obj,
            /* a_primitive */ ( true == broker && 0 == strcasecmp("gateway", behaviour.asCString()) ),
            /* a_log_level */ config_.log_level()
        }
    );
    
    const auto& provider_cfg = *provider_it->second;
    // ... set v8 data ...
    Json::Value v8_data = payload;
    //
    // COMMON
    //
    const auto set_timeouts = [&json] (const Json::Value& a_timeouts, ::cc::easy::http::oauth2::Client::Timeouts& o_timeouts) {
        // ... timeouts ...
        if ( false == a_timeouts.isNull() ) {
            const Json::Value connection_ref = json.Get(a_timeouts, "connection", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == connection_ref.isNull() ) {
                o_timeouts.connection_ = static_cast<long>(connection_ref.asInt64());
            }
            const Json::Value operation_ref = json.Get(a_timeouts, "operation", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == operation_ref.isNull() ) {
                o_timeouts.operation_ = static_cast<long>(operation_ref.asInt64());
            }
        }
    };
    //
    // STORAGE
    //
    const auto set_storage = [this, &tracking, &arguments, &provider_cfg, &v8_data] (::cc::easy::http::oauth2::Client::Tokens* o_tokens) {
        // ... storageless?
        if ( proxy::worker::http::oauth2::Config::Type::Storageless == arguments.parameters().type_ ) {
            // ... copy latest tokens available?..
            if ( nullptr != o_tokens ) {
                (*o_tokens) = provider_cfg.storageless().tokens_;
            }
        } else if ( proxy::worker::http::oauth2::Config::Type::Storage == arguments.parameters().type_ ) {
            // ... prepare load / save tokens ...
            const auto& storage_cfg = provider_cfg.storage();
            // ... merge default values for 'arguments' ...
            if ( false == storage_cfg.arguments_.isNull() ) {
                const auto& args = storage_cfg.arguments_;
                for ( auto member : args.getMemberNames() ) {
                    if ( false == v8_data.isMember(member) ) {
                        v8_data[member] = args[member];
                    }
                }
            }
            // ... setup load/save tokens ...
            arguments.parameters().storage([&, this](proxy::worker::http::oauth2::Parameters::Storage& a_storage){
                // ... copy config headers ...
                a_storage.headers_ = storage_cfg.headers_;
                // ... override and / or merge headers?
                for ( const auto& header : a_storage.headers_ ) {
                    auto& vector = a_storage.headers_[header.first];
                    for ( size_t idx = 0 ; idx < header.second.size() ; ++idx ) {
                        Evaluate(tracking.bjid_, header.second[idx], v8_data, vector[idx]);
                    }
                }
                a_storage.headers_["Content-Type"] = { "application/json; charset=utf-8" };
                // ... URL V8(ing) ?
                const std::string url = storage_cfg.endpoints_.tokens_;
                if ( nullptr != strchr(url.c_str(), '$') ) {
                    Evaluate(tracking.bjid_, url, v8_data, a_storage.url_);
                } else {
                    a_storage.url_ = url;
                }
            });
        }
    };
    //
    // GRANT OR HTTP REQUEST
    //
    if ( 0 == strcasecmp(what_ref.asCString(), "grant") ) {
        // ... set 'grant' operation arguments ...
        (void)arguments.parameters().auth_code_request([this, &set_timeouts, &set_storage, &json, &tracking, &provider_cfg, &arguments, &v8_data](proxy::worker::http::oauth2::Parameters::GrantAuthCodeRequest& auth_code) {
            // ... set timeouts ...
            set_timeouts(json.Get(arguments.parameters().data_, "timeouts", Json::ValueType::objectValue, &Json::Value::null), auth_code.timeouts_);
            // ... set storage ...
            set_storage(nullptr);
            // ... set request ....
            SetupGrantRequest(tracking, provider_cfg, arguments, auth_code, v8_data);
        });
    } else if ( 0 == strcasecmp(what_ref.asCString(), "http") ) {
        (void)arguments.parameters().http_request([this, &set_timeouts, &set_storage, &json, tracking, &provider_cfg, &arguments, &v8_data](proxy::worker::http::oauth2::Parameters::HTTPRequest& request) {
            // ... set timeouts ...
            set_timeouts(json.Get(arguments.parameters().data_, "timeouts", Json::ValueType::objectValue, &Json::Value::null), request.timeouts_);
            // ... set storage ...
            set_storage(&request.tokens_);
            // .. set request ...
            SetupHTTPRequest(tracking, provider_cfg, arguments, request, v8_data);
        });
    } else { // ... WTF?
        throw ::cc::BadRequest("Don't know how to process '%s' - unknown operation!", what_ref.asCString());
    }
    // ... schedule deferred HTTP request ...
    dynamic_cast<casper::proxy::worker::http::oauth2::Dispatcher*>(d_.dispatcher_)->Push(tracking, arguments);
    // ... publish progress ...
    ::casper::job::deferrable::Base<Arguments, ClientStep, ClientStep::Done>::Publish(tracking.bjid_, tracking.rcid_, tracking.rjid_,
                                                                                                  ClientStep::DoingIt,
                                                                                                  ::casper::job::deferrable::Base<Arguments, ClientStep, ClientStep::Done>::Status::InProgress,
                                                                                                  sk_i18n_in_progress_.key_, sk_i18n_in_progress_.arguments_
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
uint16_t casper::proxy::worker::http::oauth2::Client::OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<casper::proxy::worker::http::oauth2::Arguments>* a_deferred, Json::Value& o_payload)
{
    const auto& params = a_deferred->arguments().parameters();
    // ...
    {
        const auto& provider = providers_.find(params.id_);
        if ( proxy::worker::http::oauth2::Config::Type::Storageless == provider->second->type_ ) {
            provider->second->storageless([&params](proxy::worker::http::oauth2::Config::Storageless& a_storageless){
                a_storageless.tokens_ = params.tokens();
            });
        }
    }
    // ...
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
uint16_t casper::proxy::worker::http::oauth2::Client::OnDeferredRequestFailed (const ::casper::job::deferrable::Deferred<casper::proxy::worker::http::oauth2::Arguments>* a_deferred, Json::Value& o_payload)
{
    const auto& params = a_deferred->arguments().parameters();
    // ...
    {
        const auto& provider = providers_.find(params.id_);
        if ( proxy::worker::http::oauth2::Config::Type::Storageless == provider->second->type_ ) {
            provider->second->storageless([&params](proxy::worker::http::oauth2::Config::Storageless& a_storageless){
                a_storageless.tokens_ = params.tokens();
            });
        }
    }
    // ...
    const auto     response = a_deferred->response();
    const uint16_t code     = response.code();
    // ... set payload ...
    o_payload = Json::Value(Json::ValueType::objectValue);
    // ... primitive or default?
    if ( true == a_deferred->arguments().parameters().primitive_ ) {
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

// MARK:  - Method(s) / Function(s) - Schedule Helper(s)

::cc::easy::http::oauth2::Client::GrantType casper::proxy::worker::http::oauth2::Client::TranslatedGrantType (const std::string& a_name)
{
    const ::cc::easy::JSON<::cc::BadRequest> json;
    ::cc::easy::http::oauth2::Client::GrantType  type = ::cc::easy::http::oauth2::Client::GrantType::NotSet;
    {
        CC_RAGEL_DECLARE_VARS(grant_type, a_name.c_str(), a_name.length());
        CC_DIAGNOSTIC_PUSH();
        CC_DIAGNOSTIC_IGNORED("-Wunreachable-code");
        %%{
            machine GrantTypeMachine;
            main := |*
                /authorization_code/i => { type = ::cc::easy::http::oauth2::Client::GrantType::AuthorizationCode; };
                /client_credentials/i => { type = ::cc::easy::http::oauth2::Client::GrantType::ClientCredentials; };
            *|;
            write data;
            write init;
            write exec;
        }%%
        CC_RAGEL_SILENCE_VARS(GrantType)
        CC_DIAGNOSTIC_POP();
    }
    // ... process ...
    switch(type) {
        case ::cc::easy::http::oauth2::Client::GrantType::AuthorizationCode:
        case ::cc::easy::http::oauth2::Client::GrantType::ClientCredentials:
            break;
        default:
            throw ::cc::NotImplemented("OAuth2 grant type '%s' not implemented // not supported!", a_name.c_str());
    }
    // ... done ...
    return type;
}

/**
 * @brief Setup a 'grant_type' operation.
 *
 * @param a_tracking  Request tracking info.
 * @param a_payload   Job payload.
 * @param a_arguments HTTP args.
 */
void casper::proxy::worker::http::oauth2::Client::SetupGrantRequest (const ::casper::job::deferrable::Tracking& a_tracking,
                                                             const casper::proxy::worker::http::oauth2::Config& a_provider, casper::proxy::worker::http::oauth2::Arguments& a_arguments, casper::proxy::worker::http::oauth2::Parameters::GrantAuthCodeRequest& a_request,
                                                             Json::Value& o_v8_data)
{
    const ::cc::easy::JSON<::cc::BadRequest> json;
    // ... process ...
    const auto type_str = json.Get(a_arguments.parameters().data_, "type", Json::ValueType::stringValue, nullptr).asString();
    const auto type     = TranslatedGrantType(type_str);
    switch(type) {
        case ::cc::easy::http::oauth2::Client::GrantType::AuthorizationCode:
            break;
        default:
            throw ::cc::NotImplemented("OAuth2 grant type '%s' not implemented // not supported!", type_str.c_str());
    }
    //
    // ... assuming 'authorization_code' grant type ...
    //
    if ( type != a_provider.http_.oauth2_.grant_.type_ ) {
        throw ::cc::NotImplemented("OAuth2 grant type '%s' not supported for this provider!", type_str.c_str());
    }
    // ... setup ...
    const Json::Value& scope = json.Get(a_arguments.parameters().data_, "scope", Json::ValueType::stringValue, &Json::Value::null);
    if ( false == scope.isNull() ) {
        a_request.scope_ = scope.asString();
    }
    const Json::Value& state = json.Get(a_arguments.parameters().data_, "state", Json::ValueType::stringValue, &Json::Value::null);
    if ( false == state.isNull() ) {
        a_request.state_ = state.asString();
    }
    a_request.value_ = json.Get(a_arguments.parameters().data_, "code", Json::ValueType::stringValue, nullptr).asString();
}

/**
 * @brief Process a 'http' operation.
 *
 * @param a_tracking  Request tracking info.
 * @param a_payload   Job payload.
 * @param a_arguments HTTP args.
 */
void casper::proxy::worker::http::oauth2::Client::SetupHTTPRequest (const ::casper::job::deferrable::Tracking& a_tracking,
                                                            const casper::proxy::worker::http::oauth2::Config& a_provider, casper::proxy::worker::http::oauth2::Arguments& a_arguments, casper::proxy::worker::http::oauth2::Parameters::HTTPRequest& a_request,
                                                            Json::Value& o_v8_data)
{
    const ::cc::easy::JSON<::cc::BadRequest> json;

    // ... prepare request ...
    const auto& http = a_arguments.parameters().data_;
    const Json::Value& url_ref    = json.Get(http, "url"   , Json::ValueType::stringValue, nullptr);
    const Json::Value& method_ref = json.Get(http, "method", Json::ValueType::stringValue, nullptr);
    const Json::Value& body_ref   = json.Get(http, "body"  , Json::ValueType::objectValue, &Json::Value::null);
    //
    // REQUEST
    //
    // ... V8 data ...
    o_v8_data["signing"]  = a_provider.signing_;
    // ... method ...
    const std::string method = method_ref.asString();
    {
        CC_RAGEL_DECLARE_VARS(method, method.c_str(), method.length());
        CC_DIAGNOSTIC_PUSH();
        CC_DIAGNOSTIC_IGNORED("-Wunreachable-code");
        %%{
            machine ClientMachine;
            main := |*
                /get/i    => { a_request.method_ = ::cc::easy::http::Client::Method::GET;    };
                /put/i    => { a_request.method_ = ::cc::easy::http::Client::Method::PUT;    };
                /delete/i => { a_request.method_ = ::cc::easy::http::Client::Method::DELETE; };
                /post/i   => { a_request.method_ = ::cc::easy::http::Client::Method::POST;   };
                /patch/i  => { a_request.method_ = ::cc::easy::http::Client::Method::PATCH;  };
                /head/i   => { a_request.method_ = ::cc::easy::http::Client::Method::HEAD;   };
            *|;
            write data;
            write init;
            write exec;
        }%%
        CC_RAGEL_SILENCE_VARS(Client)
        CC_DIAGNOSTIC_POP();
    }
    const Json::Value& req_headers_ref  = json.Get(http           , "headers"     , Json::ValueType::objectValue, nullptr);
    const Json::Value& content_type_ref = json.Get(req_headers_ref, "Content-Type", Json::ValueType::stringValue, nullptr);
    // ... body ...
    if ( false == body_ref.isNull() ) {
        if ( 0 == strncasecmp(content_type_ref.asCString(), "application/json", sizeof(char) * 16) ) {
            a_request.body_ = json.Write(body_ref);
        } else {
            a_request.body_ = body_ref.asString();
        }
    }
    o_v8_data["body"] = a_request.body_;
    // ... headers ...
    {
        if ( false == http.isMember("headers") ) {
            o_v8_data["headers"] = Json::Value(Json::ValueType::objectValue);
        } else {
            o_v8_data["headers"] = http["headers"];
        }
        for ( auto key : req_headers_ref.getMemberNames() ) {
            const Json::Value& header = json.Get(req_headers_ref, key.c_str(), Json::ValueType::stringValue, nullptr);
            o_v8_data["headers"][key] = header.asString();
            a_request.headers_[key] = { header.asString() };
        }
        // ... reject OAuth2 header(s) ...
        {
            for ( const auto& header : sk_rejected_headers_ ) {
                const auto it = std::find_if(a_request.headers_.begin(), a_request.headers_.end(), ::cc::easy::http::Client::HeaderMapKeyComparator(header));
                if ( a_request.headers_.end() != it ) {
                    a_request.headers_.erase(it);
                }
            }
        }
        // ... append or override headers ...
        for ( auto header : a_provider.headers_ ) {
            for ( auto& v : header.second ) {
                const auto it = a_request.headers_.find(header.first);
                if ( a_request.headers_.end() == it ) {
                    a_request.headers_[header.first].push_back("");
                }
                auto& last = a_request.headers_[header.first][a_request.headers_[header.first].size() - 1];
                Evaluate(a_tracking.bjid_, v, o_v8_data, last);
                o_v8_data["headers"][header.first] = last;
            }
        }
        // ... append or override headers per method ...
        const auto hpm = a_provider.headers_per_method_.find(method);
        if ( a_provider.headers_per_method_.end() != hpm ) {
            for ( auto header : hpm->second ) {
                for ( auto& v : header.second ) {
                    const auto it = a_request.headers_.find(header.first);
                    if ( a_request.headers_.end() == it ) {
                        a_request.headers_[header.first].push_back("");
                    }
                    auto& last = a_request.headers_[header.first][a_request.headers_[header.first].size() - 1];
                    Evaluate(a_tracking.bjid_, v, o_v8_data, last);
                    o_v8_data["headers"][header.first] = last;
                }
            }
        }
        // ... double check, prevent injected OAuth2 header(s) ...
        {
            for ( const auto& header : sk_rejected_headers_ ) {
                const auto it = std::find_if(a_request.headers_.begin(), a_request.headers_.end(), ::cc::easy::http::Client::HeaderMapKeyComparator(header));
                if ( a_request.headers_.end() != it ) {
                    a_request.headers_.erase(it);
                }
            }
        }
    }
    // ... URL V8(ing)?
    const std::string url = url_ref.asString();
    if ( nullptr != strchr(url.c_str(), '$') ) {
        Evaluate(a_tracking.bjid_, url, o_v8_data, a_request.url_);
    } else {
        a_request.url_ = url;
    }
}

// MARK: - Method(s) / Function(s) - V8 Helper(s)

/**
 * @brief Evaluate a V8 expression.
 *
 * @param a_id         JOB beanstalkd id.
 * @param a_expression Expression to evaluate.
 * @param a_data       Data to load ( to be used during expression evaluation ).
 * @param o_value      Evaluation result.
 */
void casper::proxy::worker::http::oauth2::Client::Evaluate (const uint64_t& a_id, const std::string& a_expression, const Json::Value& a_data, std::string& o_value) const
{
    const std::set<std::string> k_evaluation_map_ = {
        "$.", "NowUTCISO8601(", "RSASignSHA256("
    };
    bool evaluate = false;
    for ( auto it : k_evaluation_map_ ) {
        if ( strstr(a_expression.c_str(), it.c_str()) ) {
            evaluate = true;
            break;
        }
    }
    // ... evaluate?
    if ( false == evaluate)  {
        // ... no ...
        o_value = a_expression;
        // ... done ..
        return;
    }
    // yes ....
    try {
        // ... is addressable ?
        std::string expression;
        {
            const std::set<std::string> k_addressable_func_map_ = {
                "NowUTCISO8601(", "RSASignSHA256("
            };
            for ( const auto& it : k_addressable_func_map_ ) {
                if ( a_expression.length() >= it.length() && 0 == strncmp(a_expression.c_str(), it.c_str(), it.length()) ) {
                    if ( ')' != a_expression.c_str()[a_expression.length() - 1] ) {
                        expression = it + "$.__instance__, " + ( a_expression.c_str() + it.length() );
                    } else {
                        expression = it + "$.__instance__" + ( a_expression.c_str() + it.length() );
                    }
                    break;
                }
            }
        }
        // ... evaluate it now ...
        if ( 0 != expression.length() ) {
            // ... patch data ...
            Json::Value data = a_data; data["__instance__"] = CC_OBJECT_HEX_ADDR(script_);
            // ... evaluate it now ...
            Evaluate((std::to_string(a_id) + "-v8-data"), expression, data, o_value);
        } else {
            // ... no patch needed, evaluate it now ...
            Evaluate((std::to_string(a_id) + "-v8-data"), a_expression, a_data, o_value);
        }
    } catch (const ::cc::v8::Exception& a_v8_exception) {
        if ( nullptr == strstr(a_v8_exception.what(), a_expression.c_str()) ) {
            throw ::cc::BadRequest("Un error occured while evaluation '%s': %s", a_expression.c_str(), a_v8_exception.what());
        } else {
            throw ::cc::BadRequest("%s", a_v8_exception.what());
        }
    }
}

/**
 * @brief Evaluate a V8 expression.
 *
 * @param a_id         ID.
 * @param a_expression Expression to evaluate.
 * @param a_data       Data to load ( to be used during expression evaluation ).
 * @param o_value      Evaluation result.
 */
void casper::proxy::worker::http::oauth2::Client::Evaluate (const std::string& a_id, const std::string& a_expression, const Json::Value& a_data, std::string& o_value) const
{
    const ::cc::easy::JSON<::cc::BadRequest> json;
    
    ::v8::Persistent<::v8::Value> v8_value; ::cc::v8::Value cc_value;
    script_->SetData(/* a_name  */ a_id.c_str(),
                     /* a_data   */ json.Write(a_data).c_str(),
                     /* o_object */ nullptr,
                     /* o_value  */ &v8_value,
                     /* a_key    */ nullptr
    );
    script_->Evaluate(v8_value, a_expression, cc_value);
    if ( true == script_->IsExceptionSet() ) {
        throw ::cc::BadRequest("%s", script_->exception().what());
    }
    switch(cc_value.type()) {
        case ::cc::v8::Value::Type::Int32:
        case ::cc::v8::Value::Type::UInt32:
        case ::cc::v8::Value::Type::Double:
        case ::cc::v8::Value::Type::String:
            o_value = cc_value.AsString();
            break;
        default:
            throw ::cc::BadRequest("Unexpected // unsupported v8 evaluation ( of %s ), result type %u ", a_expression.c_str(), cc_value.type());
    }
}
