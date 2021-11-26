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
    : ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>("OHC", sk_tube_, a_loggable_data, a_config)
{
    script_ = nullptr;
}

/**
 * @brief Destructor
 */
casper::proxy::worker::OAuth2Client::~OAuth2Client ()
{
    for ( const auto& it : providers_ ) {
        if ( nullptr != it.second->storage_ ) {
            delete it.second->storage_;
        }
        if ( nullptr != it.second->storageless_ ) {
            delete it.second->storageless_;
        }
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
    const ::cc::easy::JSON<::casper::job::InternalServerException> json;
    // memory managed by base class
    d_.dispatcher_                    = new casper::proxy::worker::Dispatcher(loggable_data_ CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));
    d_.on_deferred_request_completed_ = std::bind(&casper::proxy::worker::OAuth2Client::OnDeferredRequestCompleted, this, std::placeholders::_1, std::placeholders::_2);;
    // ...
    const auto headers = [&json] (const Json::Value& a_object) -> std::map<std::string, std::vector<std::string>> {
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
            proxy::worker::Config* p_config = nullptr;
            try {
                if ( 0 == strcasecmp(type_ref.asCString(), "storage") ) {
                    const Json::Value& storage_ref = json.Get(provider_ref, "storage", Json::ValueType::objectValue, nullptr);
                    const Json::Value& endpoints_ref = json.Get(storage_ref, "endpoints", Json::ValueType::objectValue, nullptr);
                    p_config = new proxy::worker::Config( {
                        /* http_    */ config,
                        /* storage_ */
                        proxy::worker::Config::Storage({
                            /* headers_ */ headers(json.Get(storage_ref, "headers", Json::ValueType::objectValue, &Json::Value::null)),
                            /* endpoints_ */ {
                                /* tokens_  */ json.Get(endpoints_ref, "tokens", Json::ValueType::stringValue, nullptr).asString()
                            }
                        })
                    });
                } else if ( 0 == strcasecmp(type_ref.asCString(), "storageless") ) {
                    p_config = new proxy::worker::Config( {
                        /* http_ */ config,
                        /* storage_ */
                        proxy::worker::Config::Storageless({
                            /* headers_ */ {}
                        })
                    });
                } else {
                    throw ::casper::job::InternalServerException("Unknown provider type '%s'!", type_ref.asCString());
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
    bool               broker    = false;
    const Json::Value& payload   = Payload(a_payload, &broker);
    const Json::Value& http      = json.Get(payload, "http"     , Json::ValueType::objectValue, nullptr);
    const Json::Value& behaviour = json.Get(payload, "behaviour", Json::ValueType::stringValue, &sk_behaviour_);
    const Json::Value& provider  = json.Get(payload, "provider" , Json::ValueType::stringValue, nullptr);
    const auto& provider_it = providers_.find(provider.asString());
    if ( providers_.end() == provider_it ) {
        throw ::casper::job::BadRequestException("Unknown provider '%s'!", provider.asCString());
    }
    // ... prepare tracking info ...
    const ::casper::job::deferrable::Tracking tracking = {
        /* bjid_ */ a_id,
        /* rjnr_ */ RJNR(),
        /* rjid_ */ RJID(),
        /* rcid_ */ RCID(),
        /* dpi_  */ "CPW",
    };
    // TODO: v8 here!
    casper::proxy::worker::Arguments arguments({provider_it->second->type_, provider_it->second->http_, http, ( true == broker && 0 == strcasecmp("gateway",  behaviour.asCString()) )});
    // TODO: v8 all
    // ... prepare request ...
    {
        const ::cc::easy::JSON<::casper::job::BadRequestException> json;
        
        const auto& http = arguments.parameters().data_;
        const Json::Value& url_ref      = json.Get(http, "url"     , Json::ValueType::stringValue, nullptr);
        const Json::Value& method_ref   = json.Get(http, "method"  , Json::ValueType::stringValue, nullptr);
        const Json::Value& body_ref     = json.Get(http, "body"    , Json::ValueType::objectValue, &Json::Value::null);
        const Json::Value& timeouts_ref = json.Get(http, "timeouts", Json::ValueType::objectValue, &Json::Value::null);
        //
        auto& request = arguments.parameters().request_;
        // ... method ...
        const std::string method = method_ref.asString();
        {
            CC_RAGEL_DECLARE_VARS(method, method.c_str(), method.length());
            CC_DIAGNOSTIC_PUSH();
            CC_DIAGNOSTIC_IGNORED("-Wunreachable-code");
            %%{
                machine ClientMachine;
                main := |*
                    /get/i    => { request.method_ = ::ev::curl::Request::HTTPRequestType::GET;    };
                    /put/i    => { request.method_ = ::ev::curl::Request::HTTPRequestType::PUT;    };
                    /delete/i => { request.method_ = ::ev::curl::Request::HTTPRequestType::DELETE; };
                    /post/i   => { request.method_ = ::ev::curl::Request::HTTPRequestType::POST;   };
                    /patch/i  => { request.method_ = ::ev::curl::Request::HTTPRequestType::PATCH;  };
                    /head/i   => { request.method_ = ::ev::curl::Request::HTTPRequestType::HEAD;   };
                *|;
                write data;
                write init;
                write exec;
            }%%
            CC_RAGEL_SILENCE_VARS(Client)
            CC_DIAGNOSTIC_POP();
        }
        // ... URL ...
        request.url_ = url_ref.asString();
        // ... headers ...
        const Json::Value& headers_ref = json.Get(http, "headers", Json::ValueType::objectValue, &Json::Value::null);
        if ( false == headers_ref.isNull() ) {
            for ( auto key : headers_ref.getMemberNames() ) {
                const Json::Value& header = json.Get(headers_ref, key.c_str(), Json::ValueType::stringValue, nullptr);
                request.headers_[key] = { header.asString() };
            }
        }
        // TODO: override with config headers
        // ... body ...
        if ( false == body_ref.isNull() ) {
            request.body_ = json.Write(body_ref);
        }
        // ... timeouts ...
        if ( false == timeouts_ref.isNull() ) {
            const Json::Value connection_ref = json.Get(timeouts_ref, "connection", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == connection_ref.isNull() ) {
                request.timeouts_.connection_ = static_cast<long>(connection_ref.asInt64());
            }
            const Json::Value operation_ref = json.Get(timeouts_ref, "operation", Json::ValueType::uintValue, &Json::Value::null);
            if ( false == operation_ref.isNull() ) {
                request.timeouts_.operation_ = static_cast<long>(operation_ref.asInt64());
            }
        }
        // ... prepare load / save tokens ...
        if ( proxy::worker::Config::Type::Storage == arguments.parameters().type_ ) {
            // ... load tokens ...
            auto& params = arguments.parameters();
            params.storage_.url_     = provider_it->second->storage_->endpoints_.tokens_;
            params.storage_.headers_ = provider_it->second->storage_->headers_;
            try {
                ::v8::Persistent<::v8::Value> data; ::cc::v8::Value value;
                script_->SetData(/* a_name  */ ( std::to_string(a_id) + "-v8-data" ).c_str(),
                                 /* a_data   */ json.Write(payload).c_str(),
                                 /* o_object */ nullptr,
                                 /* o_value  */ &data,
                                 /* a_key    */ nullptr
                );
                script_->Evaluate(data, params.storage_.url_, value);
                switch(value.type()) {
                    case ::cc::v8::Value::Type::String:
                        params.storage_.url_ = value.AsString();
                        break;
                    default:
                        throw ::casper::job::BadRequestException("Unexpected // unsupported v8 evaluation result type of %u ", value.type());
                }
            } catch (const ::cc::v8::Exception& a_v8_exception) {
                fprintf(stderr, "%s\n", a_v8_exception.what());
                fflush(stderr);
                throw ::casper::job::BadRequestException("%s", a_v8_exception.what());
            }
        }
    }
    // ... schedule deferred HTTP request ...
    dynamic_cast<casper::proxy::worker::Dispatcher*>(d_.dispatcher_)->Push(tracking, arguments);
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
uint16_t casper::proxy::worker::OAuth2Client::OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>* a_deferred, Json::Value& o_payload)
{
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
        if ( 0 == strncasecmp(response.content_type().c_str(), "content-type: application/json", sizeof(char) * 30) ) {
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
