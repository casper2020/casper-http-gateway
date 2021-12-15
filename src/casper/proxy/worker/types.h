/**
 * @file types.h
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
#pragma once
#ifndef CASPER_PROXY_WORKER_TYPES_H_
#define CASPER_PROXY_WORKER_TYPES_H_

#include "casper/job/deferrable/arguments.h"

#include "json/json.h"

#include "cc/easy/http.h"

#include "cc/non-movable.h"

#include <string>
#include <map>

namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {

            // MARK: -
        
            class Config final : public ::cc::NonMovable
            {
            
            public: // Enum(s)
                
                enum class Type : uint8_t {
                    Storage,
                    Storageless,
                };
            
            public: // Data Type(s)
                
                typedef ::ev::curl::Request::Headers                        Headers;
                typedef std::map<std::string, ::ev::curl::Request::Headers> HeadersPerMethod;
                typedef ::ev::curl::Request::Timeouts                       Timeouts;
                
                typedef struct {
                    std::string tokens_;
                } StorageEndpoints;
                
                typedef struct {
                    StorageEndpoints endpoints_;
                    Json::Value      arguments_;
                    Headers          headers_;
                    Timeouts         timeouts_;
                } Storage;
                
                typedef struct {
                    Headers                              headers_;
                    ::cc::easy::OAuth2HTTPClient::Tokens tokens_;
                } Storageless;
                
                typedef Json::Value Signing;
                
            public: // Const Data
                
                const Type                                 type_;               //!< Config type, one of \link Type \link.
                const ::cc::easy::OAuth2HTTPClient::Config http_;               //!< OAuth2 configs
                const Headers                              headers_;            //!< additional headers per request
                const HeadersPerMethod                     headers_per_method_; //!< additional headers per request per method
                const Signing                              signing_;            //!< signing configs
                
            private: // Data
                
                Storage*                                   storage_;        //!< storage config

            private: // Data
                
                Storageless*                               storageless_;    //!< storageless config

            private: // Data
                                
            public: // Constructor(s) / Destructor
                
                Config () = delete;
                
                /**
                 * @brief Default constructor for 'storage' mode.
                 *
                 * @param a_http               OAuth2 HTTP Client config.
                 * @param a_headers            Additional headers per request.
                 * @param a_headers_per_method Additional headers per request per method.
                 * @param a_signing            Signing config.
                 * @param a_storage            Storage config.
                 */
                Config (const ::cc::easy::OAuth2HTTPClient::Config& a_config, const Headers& a_headers, const HeadersPerMethod& a_headers_per_method, const Signing& a_signing, const Storage& a_storage)
                 : type_(Type::Storage), http_(a_config), headers_(a_headers), headers_per_method_(a_headers_per_method), signing_(a_signing)
                {
                    storage_     = new Storage(a_storage);
                    storageless_ = nullptr;
                }

                /**
                 * @brief Default constructor for 'storageless' mode.
                 *
                 * @param a_http               OAuth2 HTTP Client config.
                 * @param a_headers            Additional headers per request.
                 * @param a_headers_per_method Additional headers per request per method.
                 * @param a_signing            Signing config.
                 * @param a_storageless        Storageless config.
                 */
                Config (const ::cc::easy::OAuth2HTTPClient::Config& a_config, const Headers& a_headers, const HeadersPerMethod& a_headers_per_method, const Signing& a_signing, const Storageless& a_storageless)
                 : type_(Type::Storageless), http_(a_config), headers_(a_headers), headers_per_method_(a_headers_per_method), signing_(a_signing)
                {
                    storage_                          = nullptr;
                    storageless_                      = new Storageless(a_storageless);
                    storageless_->tokens_.type_       = "";
                    storageless_->tokens_.access_     = "";
                    storageless_->tokens_.refresh_    = "";
                    storageless_->tokens_.expires_in_ =  0;
                    storageless_->tokens_.scope_      = "";
                    storageless_->tokens_.on_change_  = nullptr;
                }
                
                /**
                 * @brief Copy constructor.
                 *
                 * @param a_config Object to copy.
                 */
                Config (const Config& a_config)
                : type_(a_config.type_), http_(a_config.http_), headers_(a_config.headers_), headers_per_method_(a_config.headers_per_method_), signing_(a_config.signing_)
                {
                    storage_     = ( nullptr != a_config.storage_     ? new Storage(*a_config.storage_)         : nullptr );
                    storageless_ = ( nullptr != a_config.storageless_ ? new Storageless(*a_config.storageless_) : nullptr );
                }

                /**
                 * @brief Default constructor.
                 */
                virtual ~Config ()
                {
                    if ( nullptr != storage_ ) {
                        delete storage_;
                    }
                    if ( nullptr != storageless_ ) {
                        delete storageless_;
                    }
                }
                
            public: // Inline Method(s) / Function(s)
                
                /**
                 * @return R/O access to storage configs.
                 */
                inline const Storage& storage () const
                {
                    if ( nullptr != storage_ ) {
                        return *storage_;
                    }
                    throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                }
                
                /**
                 * @return R/O access to storageless configs.
                 */
                inline const Storageless& storageless () const
                {
                    if ( nullptr != storageless_ ) {
                        return *storageless_;
                    }
                    throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                }
                
                /**
                 * @brief Prepare storageless config, exception for better tracking of variables write acccess.
                 *
                 * @return R/O access to storageless configs.
                 */
                inline const Storageless& storageless (const std::function<void(Storageless&)>& a_callback)
                {
                    if ( nullptr != storageless_ ) {
                        return *storageless_;
                    }
                    throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                }

            public: // Overloaded Operator(s)
                
                void operator = (Config const&)  = delete;  // assignment is not allowed
                
            }; // end of class 'Config'

            // MARK: -
        
            class Parameters final : public ::cc::NonMovable
            {
                
            public: // Data Type(s)
                
                typedef struct {
                    ::ev::curl::Request::HTTPRequestType method_;
                    std::string                          url_;
                    std::string                          body_;
                    ::ev::curl::Request::Headers         headers_;
                    ::ev::curl::Request::Timeouts        timeouts_;
                } Storage;

                typedef struct {
                    ::ev::curl::Request::HTTPRequestType method_;
                    std::string                          url_;
                    std::string                          body_;
                    ::ev::curl::Request::Headers         headers_;
                    ::ev::curl::Request::Timeouts        timeouts_;
                    ::cc::easy::OAuth2HTTPClient::Tokens tokens_;
                } Request;
                
            public: // Const Data
                
                const std::string                           id_;
                const Config::Type                          type_;
                const ::cc::easy::OAuth2HTTPClient::Config& config_;
                const Json::Value&                          data_;
                const bool                                  primitive_;
                const int                                   log_level_;
                
            private: // Data
                
                Storage*                                    storage_; //!< Evaluated storage request data.
                Request                                     request_; //!< Evaluated request data.
                
            public: // Constructor(s) / Destructor
                
                Parameters () = delete;
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_id        Provider ID.
                 * @param a_type      One of \link Config::Type \link.
                 * @param a_config    Ref to R/O config.
                 * @param a_data      JSON object.
                 * @param a_primitive True when response should be done in 'primitive' mode.
                 * @param a_log_level Log level.
                 */
                Parameters (const std::string& a_id,
                            const Config::Type a_type,
                            const ::cc::easy::OAuth2HTTPClient::Config& a_config,
                            const Json::Value& a_data, const bool a_primitive, const int a_log_level)
                 : id_(a_id), type_(a_type), config_(a_config), data_(a_data), primitive_(a_primitive), log_level_(a_log_level),
                   storage_(nullptr),
                   request_({
                       /* method_   */ ::ev::curl::Request::HTTPRequestType::NotSet,
                       /* url_      */ "",
                       /* body_     */ "",
                       /* headers_  */ {},
                       /* timeouts_ */ { -1, -1 },
                       /* tokens_   */ {
                           /* type_       */ "",
                           /* access_     */ "",
                           /* refresh_    */ "",
                           /* expires_in_ */  0,
                           /* scope_      */ "",
                           /* on_change_  */ nullptr
                        }
                   })
                {
                    /* empty */
                }
                
                /**
                 * @brief Copy constructor.
                 *
                 * @param a_parameters Object to copy.
                 */
                Parameters (const Parameters& a_parameters)
                 : id_(a_parameters.id_), type_(a_parameters.type_), config_(a_parameters.config_), data_(a_parameters.data_), primitive_(a_parameters.primitive_), log_level_(a_parameters.log_level_),
                   storage_(nullptr), request_(a_parameters.request_)
                {
                    if ( nullptr != a_parameters.storage_ ) {
                        storage_ = new Storage(*a_parameters.storage_);
                    }
                }
                
                /**
                 * @brief Default constructor.
                 */
                virtual ~Parameters ()
                {
                    if ( nullptr != storage_ ) {
                        delete storage_;
                    }
                }

            public: // Overloaded Operator(s)
                
                void operator = (Parameters const&)  = delete;  // assignment is not allowed

            public: // Inline Method(s) / Function(s)
                
                /**
                 * @return R/O access to storage configs.
                 */
                inline const Storage& storage () const
                {
                    if ( Config::Type::Storage != type_ || nullptr == storage_ ) {
                        throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                    }
                    // ... done ...
                    return *storage_;
                }
                
                /**
                 * @brief Prepare storage config, exception for better tracking of variables write acccess.
                 *
                 * @return R/O access to storage configs.
                 */
                inline const Storage& storage (const std::function<void(Storage&)>& a_callback)
                {
                    if ( Config::Type::Storage != type_ ) {
                        throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                    }
                    // ... if doesn't exists yet ...
                    if ( nullptr == storage_ ) {
                        // ... create it now ...
                        storage_ = new Storage({
                            /* method_   */ ::ev::curl::Request::HTTPRequestType::NotSet,
                            /* url_      */ "",
                            /* body_     */ "",
                            /* headers_  */ {},
                            /* timeouts_ */ { -1, -1 }
                        });
                    }
                    // ... callback ...
                    a_callback(*storage_);
                    // ... done ...
                    return *storage_;
                }
                
                /**
                 * @return R/O access to storage configs.
                 */
                inline const Storage& storage (const ::ev::curl::Request::HTTPRequestType a_type)
                {
                    if ( Config::Type::Storage != type_ || nullptr == storage_ ) {
                        throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                    }
                    storage_->method_ = a_type;
                    storage_->body_   = "";
                    // ... done ...
                    return *storage_;
                }
                
                /**
                 * @return R/O access to storage configs.
                 */
                inline const Storage& storage (const ::ev::curl::Request::HTTPRequestType a_type, const std::string& a_body)
                {
                    if ( Config::Type::Storage != type_ || nullptr == storage_ ) {
                        throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                    }
                    storage_->method_ = a_type;
                    storage_->body_   = a_body;
                    // ... done ...
                    return *storage_;
                }
                
                /**
                 * @return R/O access to request configs.
                 */
                inline const Request& request () const
                {
                    return request_;
                }
                
                /**
                 * @brief Prepare request config, exception for better tracking of variables write acccess.
                 *
                 * @return R/O access to request configs.
                 */
                inline const Request& request (const std::function<void(Request&)>& a_callback)
                {
                    // ... callback ...
                    a_callback(request_);
                    // ... done ...
                    return request_;
                }
                
                /**
                 * @brief Prepare request tokens data, exception for better tracking of variables write acccess.
                 *
                 * @return R/W access to request tokens data.
                 */
                inline ::cc::easy::OAuth2HTTPClient::Tokens& tokens (const std::function<void(::cc::easy::OAuth2HTTPClient::Tokens&)>& a_callback)
                {
                    // ... callback ...
                    a_callback(request_.tokens_);
                    // ... done ...
                    return request_.tokens_;
                }

                /**
                 * @return R/O access to request OAuth2 tokens.
                 */
                inline const ::cc::easy::OAuth2HTTPClient::Tokens& tokens () const
                {
                    return request_.tokens_;
                }

            }; // end of class 'Parameters'
        
            // MARK: -

            class Arguments final : public ::casper::job::deferrable::Arguments<Parameters>
            {
                
            public: // Constructor(s) / Destructor
            
                Arguments() = delete;
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_parameters Parameters.
                 */
                Arguments (const Parameters& a_parameters)
                 : ::casper::job::deferrable::Arguments<Parameters>(a_parameters)
                {
                    /* empty */
                }
                
                /**
                 * @brief Default constructor.
                 */
                virtual ~Arguments ()
                {
                    /* empty */
                }
                
            public: // Method(s) / Function(s)
                
                virtual bool Primitive () const { return parameters_.primitive_; };
                
            }; // end of class 'Arguments'

        } // end of namespace 'proxy'
    
    } // end of namespace 'worker'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_TYPES_H_
