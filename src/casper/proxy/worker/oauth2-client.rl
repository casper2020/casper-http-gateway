/**
 * @file oauth2-client.h
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

#include "casper/proxy/worker/oauth2-client.h"

#include "casper/proxy/worker/dispatcher.h"

#include "cc/ragel.h"

#include "cc/v8/exception.h"

const Json::Value casper::proxy::worker::OAuth2Client::sk_behaviour_ = "default";

/**
 * @brief Default constructor.
 *
 * param a_loggable_data
 * param a_config
 */
casper::proxy::worker::OAuth2Client::OAuth2Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config)
    : ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>("OHC", sk_tube_, a_loggable_data, a_config, /* a_sequentiable */ false)
{
    script_ = nullptr;
}

/**
 * @brief Destructor
 */
casper::proxy::worker::OAuth2Client::~OAuth2Client ()
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
void casper::proxy::worker::OAuth2Client::InnerSetup ()
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(0 == providers_.size());
    //
    const ::cc::easy::JSON<::cc::InternalServerError> json;
    // memory managed by base class
    d_.dispatcher_                    = new casper::proxy::worker::Dispatcher(loggable_data_ CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));
    d_.on_deferred_request_completed_ = std::bind(&casper::proxy::worker::OAuth2Client::OnDeferredRequestCompleted, this, std::placeholders::_1, std::placeholders::_2);
    d_.on_deferred_request_failed_    = std::bind(&casper::proxy::worker::OAuth2Client::OnDeferredRequestFailed   , this, std::placeholders::_1, std::placeholders::_2);
    // ...
    const auto object2headers = [&json] (const Json::Value& a_object) -> std::map<std::string, std::vector<std::string>> {
        std::map<std::string, std::vector<std::string>> h;
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
            const Json::Value& provider_ref = json.Get(providers   , name.c_str(), Json::ValueType::objectValue, nullptr);
            const Json::Value& oauth2_ref   = json.Get(provider_ref, "oauth2"    , Json::ValueType::objectValue , nullptr);
            const Json::Value& type_ref     = json.Get(provider_ref, "type"      , Json::ValueType::stringValue , nullptr);
            const Json::Value& m2m_ref      = json.Get(oauth2_ref  , "m2m"       , Json::ValueType::booleanValue, nullptr);
            const ::cc::easy::OAuth2HTTPClient::Config config = ::cc::easy::OAuth2HTTPClient::Config({
                /* oauth2_ */ {
                    /* m2m_ */ m2m_ref.asBool(),
                    /* urls_ */ {
                        /* authorization_ */ json.Get(oauth2_ref, "authorization_url", Json::ValueType::stringValue, nullptr).asString(),
                        /* token_         */ json.Get(oauth2_ref, "token_url"        , Json::ValueType::stringValue, nullptr).asString()
                    },
                    /* credentials_ */ {
                        /* client_id_     */ json.Get(oauth2_ref, "client_id"    , Json::ValueType::stringValue, nullptr).asString(),
                        /* client_secret_ */ json.Get(oauth2_ref, "client_secret", Json::ValueType::stringValue, nullptr).asString()
                    },
                    /* redirect_uri_ */ json.Get(oauth2_ref, "redirect_uri", Json::ValueType::stringValue, nullptr).asString(),
                    /* scope_        */ json.Get(oauth2_ref, "scope"       , Json::ValueType::stringValue, &empty_string).asString(),
                }
            });
            // ...
            proxy::worker::Config::Headers headers;
            const Json::Value& headers_ref = json.Get(provider_ref, "headers", Json::ValueType::objectValue, nullptr);
            if ( false == headers_ref.isNull() ) {
                headers = object2headers(headers_ref);
            }
            // ...
            proxy::worker::Config* p_config = nullptr;
            try {
                if ( 0 == strcasecmp(type_ref.asCString(), "storage") ) {
                    const Json::Value& storage_ref = json.Get(provider_ref, "storage", Json::ValueType::objectValue, nullptr);
                    const Json::Value& endpoints_ref = json.Get(storage_ref, "endpoints", Json::ValueType::objectValue, nullptr);                    
                    Json::Value timeouts = json.Get(storage_ref, "timeouts", Json::ValueType::objectValue, &Json::Value::null);
                    if ( true == timeouts.isNull() ) {
                        timeouts = Json::Value(Json::ValueType::objectValue);
                        timeouts["connection"] = static_cast<Json::Int>(sk_storage_connection_timeout_);
                        timeouts["operation"]  = static_cast<Json::Int>(sk_storage_operation_timeout_);
                    }
                    p_config = new proxy::worker::Config( {
                        /* http_    */ config,
                        /* headers  */ headers,
                        /* signing_ */ json.Get(provider_ref, "signing", Json::ValueType::objectValue, &Json::Value::null),
                        /* storage_ */
                        proxy::worker::Config::Storage({
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
                    p_config = new proxy::worker::Config( {
                        /* http_    */ config,
                        /* headers  */ headers,
                        /* signing_ */ json.Get(provider_ref, "signing", Json::ValueType::objectValue, &Json::Value::null),
                        /* storage_ */
                        proxy::worker::Config::Storageless({
                            /* headers_ */ {}
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
    // ... for debug proposes only ...
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
void casper::proxy::worker::OAuth2Client::InnerRun (const int64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response)
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
    // }

    //
    // Payload
    //
    bool               broker    = false;
    const Json::Value& payload   = Payload(a_payload, &broker);
    const Json::Value& http      = json.Get(payload, "http"     , Json::ValueType::objectValue, nullptr);
    const Json::Value& behaviour = json.Get(payload, "behaviour", Json::ValueType::stringValue, &sk_behaviour_);
    const Json::Value& provider  = json.Get(payload, "provider" , Json::ValueType::stringValue, nullptr);
    const auto& provider_it = providers_.find(provider.asString());
    if ( providers_.end() == provider_it ) {
        throw ::cc::BadRequest("Unknown provider '%s'!", provider.asCString());
    }
    // ... prepare tracking info ...
    const ::casper::job::deferrable::Tracking tracking = {
        /* bjid_ */ a_id,
        /* rjnr_ */ RJNR(),
        /* rjid_ */ RJID(),
        /* rcid_ */ RCID(),
        /* dpi_  */ "CPW",
    };
    // ... prepare arguments / parameters ...
    casper::proxy::worker::Arguments arguments = casper::proxy::worker::Arguments(
        {
            /* a_id        */ provider_it->first,
            /* a_type      */ provider_it->second->type_,
            /* a_config    */ provider_it->second->http_,
            /* a_data      */ http,
            /* a_primitive */ ( true == broker && 0 == strcasecmp("gateway",  behaviour.asCString()) ),
            /* a_log_level */ config_.log_level()
        }
    );
    // ... prepare request ...
    {
        const ::cc::easy::JSON<::cc::BadRequest> json;
        
        const auto& http = arguments.parameters().data_;
        const Json::Value& url_ref      = json.Get(http, "url"     , Json::ValueType::stringValue, nullptr);
        const Json::Value& method_ref   = json.Get(http, "method"  , Json::ValueType::stringValue, nullptr);
        const Json::Value& body_ref     = json.Get(http, "body"    , Json::ValueType::objectValue, &Json::Value::null);
        const Json::Value& timeouts_ref = json.Get(http, "timeouts", Json::ValueType::objectValue, &Json::Value::null);
        //
        Json::Value merged = payload;
        //
        // REQUEST
        //
        (void)arguments.parameters().request([&, this](proxy::worker::Parameters::Request& a_request) {
            // ... V8 data ...
            merged["signing"] = provider_it->second->signing_;
            // ... method ...
            const std::string method = method_ref.asString();
            {
                CC_RAGEL_DECLARE_VARS(method, method.c_str(), method.length());
                CC_DIAGNOSTIC_PUSH();
                CC_DIAGNOSTIC_IGNORED("-Wunreachable-code");
                %%{
                    machine ClientMachine;
                    main := |*
                        /get/i    => { a_request.method_ = ::ev::curl::Request::HTTPRequestType::GET;    };
                        /put/i    => { a_request.method_ = ::ev::curl::Request::HTTPRequestType::PUT;    };
                        /delete/i => { a_request.method_ = ::ev::curl::Request::HTTPRequestType::DELETE; };
                        /post/i   => { a_request.method_ = ::ev::curl::Request::HTTPRequestType::POST;   };
                        /patch/i  => { a_request.method_ = ::ev::curl::Request::HTTPRequestType::PATCH;  };
                        /head/i   => { a_request.method_ = ::ev::curl::Request::HTTPRequestType::HEAD;   };
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
            merged["body"] = a_request.body_;
            // ... headers ...
            {
                if ( false == http.isMember("headers") ) {
                    merged["headers"] = Json::Value(Json::ValueType::objectValue);
                } else {
                    merged["headers"] = http["headers"];
                }
                for ( auto key : req_headers_ref.getMemberNames() ) {
                    const Json::Value& header = json.Get(req_headers_ref, key.c_str(), Json::ValueType::stringValue, nullptr);
                    merged["headers"][key] = header.asString();
                    a_request.headers_[key] = { header.asString() };
                }
                // ... append or override headers ...
                for ( auto header : provider_it->second->headers_ ) {
                    for ( auto& v : header.second ) {
                        a_request.headers_[header.first].push_back("");
                        auto& last = a_request.headers_[header.first][a_request.headers_[header.first].size() - 1];
                        Evaluate(a_id, v, merged, last);
                        merged["headers"][header.first] = last;
                    }
                }
            }
            // ... URL V8(ing)?
            const std::string url = url_ref.asString();
            if ( nullptr != strchr(url.c_str(), '$') ) {
                Evaluate(a_id, url, merged, a_request.url_);
            } else {
                a_request.url_ = url;
            }
            // ... timeouts ...
            if ( false == timeouts_ref.isNull() ) {
                const Json::Value connection_ref = json.Get(timeouts_ref, "connection", Json::ValueType::uintValue, &Json::Value::null);
                if ( false == connection_ref.isNull() ) {
                    a_request.timeouts_.connection_ = static_cast<long>(connection_ref.asInt64());
                }
                const Json::Value operation_ref = json.Get(timeouts_ref, "operation", Json::ValueType::uintValue, &Json::Value::null);
                if ( false == operation_ref.isNull() ) {
                    a_request.timeouts_.operation_ = static_cast<long>(operation_ref.asInt64());
                }
            }
            // ... storageless?
            if ( proxy::worker::Config::Type::Storageless == arguments.parameters().type_ ) {
                // ... copy latest tokens available?..
                a_request.tokens_ = provider_it->second->storageless().tokens_;
            }
        });
        //
        // STORAGE
        //
        {
            // ... prepare load / save tokens ...
            if ( proxy::worker::Config::Type::Storage == arguments.parameters().type_ ) {
                const auto& storage_cfg = provider_it->second->storage();
                // ... merge default values for 'arguments' ...
                if ( false == storage_cfg.arguments_.isNull() ) {
                    const auto& args = storage_cfg.arguments_;
                    for ( auto member : args.getMemberNames() ) {
                        if ( false == merged.isMember(member) ) {
                            merged[member] = args[member];
                        }
                    }
                }
                // ... setup load/save tokens ...
                arguments.parameters().storage([&, this](proxy::worker::Parameters::Storage& a_storage){
                    // ... copy config headers ...
                    a_storage.headers_ = storage_cfg.headers_;
                    // ... override and / or merge headers?
                    for ( const auto& header : a_storage.headers_ ) {
                        auto& vector = a_storage.headers_[header.first];
                        for ( size_t idx = 0 ; idx < header.second.size() ; ++idx ) {
                            Evaluate(a_id, header.second[idx], merged, vector[idx]);
                        }
                    }
                    a_storage.headers_["Content-Type"] = { "application/json; charset=utf-8" };
                    // ... URL V8(ing) ?
                    const std::string url = storage_cfg.endpoints_.tokens_;
                    if ( nullptr != strchr(url.c_str(), '$') ) {
                        Evaluate(a_id, url, merged, a_storage.url_);
                    } else {
                        a_storage.url_ = url;
                    }
                });
            }
        }
    }
    // ... schedule deferred HTTP request ...
    dynamic_cast<casper::proxy::worker::Dispatcher*>(d_.dispatcher_)->Push(tracking, arguments);
    // ... publish progress ...
    ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>::Publish(tracking.bjid_, tracking.rcid_, tracking.rjid_,
                                                                                                  OAuth2ClientStep::DoingIt,
                                                                                                  ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>::Status::InProgress,
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
uint16_t casper::proxy::worker::OAuth2Client::OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>* a_deferred, Json::Value& o_payload)
{
    const auto& params = a_deferred->arguments().parameters();
    // ...
    {
        const auto& provider = providers_.find(params.id_);
        if ( proxy::worker::Config::Type::Storageless == provider->second->type_ ) {
            provider->second->storageless([&params](proxy::worker::Config::Storageless& a_storageless){
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
uint16_t casper::proxy::worker::OAuth2Client::OnDeferredRequestFailed (const ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>* a_deferred, Json::Value& o_payload)
{
    const auto& params = a_deferred->arguments().parameters();
    // ...
    {
        const auto& provider = providers_.find(params.id_);
        if ( proxy::worker::Config::Type::Storageless == provider->second->type_ ) {
            provider->second->storageless([&params](proxy::worker::Config::Storageless& a_storageless){
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

// MARK: - Method(s) / Function(s) -  V8 Helper(s)

/**
 * @brief Evaluate a V8 expression.
 *
 * @param a_id         JOB beanstalkd id.
 * @param a_expression Expression to evaluate.
 * @param a_data       Data to load ( to be used during expression evaluation ).
 * @param o_value      Evaluation result.
 */
void casper::proxy::worker::OAuth2Client::Evaluate (const uint64_t& a_id, const std::string& a_expression, const Json::Value& a_data, std::string& o_value) const
{
    const ::cc::easy::JSON<::cc::BadRequest> json;
    try {
        ::v8::Persistent<::v8::Value> data; ::cc::v8::Value value;
        script_->SetData(/* a_name  */ ( std::to_string(a_id) + "-v8-data" ).c_str(),
                         /* a_data   */ json.Write(a_data).c_str(),
                         /* o_object */ nullptr,
                         /* o_value  */ &data,
                         /* a_key    */ nullptr
        );
        script_->Evaluate(data, a_expression, value);
        switch(value.type()) {
            case ::cc::v8::Value::Type::Int32:
            case ::cc::v8::Value::Type::UInt32:
            case ::cc::v8::Value::Type::Double:
            case ::cc::v8::Value::Type::String:
                o_value = value.AsString();
                break;
            default:
                throw ::cc::BadRequest("Unexpected // unsupported v8 evaluation result type of %u ", value.type());
        }
    } catch (const ::cc::v8::Exception& a_v8_exception) {
        throw ::cc::BadRequest("%s", a_v8_exception.what());
    }
}
