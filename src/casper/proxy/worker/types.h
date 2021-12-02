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
                
                typedef std::map<std::string, std::vector<std::string>> Headers;
                
                typedef struct {
                    std::string tokens_;
                } StorageEndpoints;
                
                typedef struct {
                    Headers          headers_;
                    StorageEndpoints endpoints_;
                    Json::Value      arguments_;
                } Storage;
                
                typedef struct {
                    Headers headers_;
                } Storageless;
                
                typedef Json::Value Signing;
                
            public: // Const Data
                
                const Type                                 type_;
                const ::cc::easy::OAuth2HTTPClient::Config http_;
                const Headers                              headers_;        //!< additional headers per request
                const Signing                              signing_;        //!< signing configs
                const Storage*                             storage_;
                const Storageless*                         storageless_;
                
            public: // Constructor(s) / Destructor
                
                Config () = delete;
                
                /**
                 * @brief Default constructor for 'storage' mode.
                 *
                 * @param a_http    OAuth2 HTTP Client config.
                 * @param a_headers Additional headers per request.
                 * @param a_signing Signing config.
                 * @param a_storage Storage config.
                 */
                Config (const ::cc::easy::OAuth2HTTPClient::Config& a_config, const Headers& a_headers, const Signing& a_signing, const Storage& a_storage)
                 : type_(Type::Storage), http_(a_config), headers_(a_headers), signing_(a_signing)
                {
                    storage_     = new Storage(a_storage);
                    storageless_ = nullptr;
                }

                /**
                 * @brief Default constructor for 'storageless' mode.
                 *
                 * @param a_http        OAuth2 HTTP Client config.
                 * @param a_headers     Additional headers per request.
                 * @param a_signing     Signing config.
                 * @param a_storageless Storageless config.
                 */
                Config (const ::cc::easy::OAuth2HTTPClient::Config& a_config, const Headers& a_headers, const Signing& a_signing, const Storageless& a_storageless)
                 : type_(Type::Storageless), http_(a_config), headers_(a_headers), signing_(a_signing)
                {
                    storage_     = nullptr;
                    storageless_ = new Storageless(a_storageless);
                }
                
                /**
                 * @brief Copy constructor.
                 *
                 * @param a_config Object to copy.
                 */
                Config (const Config& a_config)
                : type_(a_config.type_), http_(a_config.http_), headers_(a_config.headers_), signing_(a_config.signing_)
                {
                    storage_     = ( nullptr != a_config.storage_     ? new Storage(*a_config.storage_)         : nullptr );
                    storageless_ = ( nullptr != a_config.storageless_ ? new Storageless(*a_config.storageless_) : nullptr );
                }

                /**
                 * @brief Default constructor.
                 */
                virtual ~Config ()
                {
                    /* empty */
                }
                
            public: // Overloaded Operator(s)
                
                void operator = (Config const&)  = delete;  // assignment is not allowed

                
            }; // end of class 'Config'

            class Parameters final : public ::cc::NonMovable
            {
                
            public: // Data Type(s)
                
                typedef struct {
                    ::ev::curl::Request::HTTPRequestType method_;
                    std::string                          url_;
                    std::string                          body_;
                    ::ev::curl::Request::Headers         headers_;
                    ::ev::curl::Request::Timeouts        timeouts_;
                } Request;

            public: // Const Data
                
                const Config::Type                          type_;
                const ::cc::easy::OAuth2HTTPClient::Config& config_;
                const Json::Value&                          data_;
                const bool                                  primitive_;
                const int                                   log_level_;
                
            public: // Data
                
                Request                                     storage_; //!< Evaluated storage request data.
                Request                                     request_; //!< Evaluated request data.
                ::cc::easy::OAuth2HTTPClient::Tokens        tokens_;
                
            public: // Constructor(s) / Destructor
                
                Parameters () = delete;
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_type      One of \link Config::Type \link.
                 * @param a_config    Ref to R/O config.
                 * @param a_data     JSON object.
                 * @param a_primitive True when response should be done in 'primitive' mode.
                 * @param a_log_level Log level.
                 */
                Parameters (const Config::Type a_type,
                            const ::cc::easy::OAuth2HTTPClient::Config& a_config,
                            const Json::Value& a_data, const bool a_primitive, const int a_log_level)
                 : type_(a_type), config_(a_config), data_(a_data), primitive_(a_primitive), log_level_(a_log_level),
                   storage_({
                       /* method_   */ ::ev::curl::Request::HTTPRequestType::NotSet,
                       /* url_      */ "",
                       /* body_     */ "",
                       /* headers_  */ {},
                       /* timeouts_ */ { -1, -1 }
                   }),
                   request_({
                       /* method_   */ ::ev::curl::Request::HTTPRequestType::NotSet,
                       /* url_      */ "",
                       /* body_     */ "",
                       /* headers_  */ {},
                       /* timeouts_ */ { -1, -1 }
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
                 : type_(a_parameters.type_), config_(a_parameters.config_), data_(a_parameters.data_), primitive_(a_parameters.primitive_), log_level_(a_parameters.log_level_),
                   storage_(a_parameters.storage_), request_(a_parameters.request_),
                   tokens_(a_parameters.tokens_)
                {
                    /* empty */
                }
                
                /**
                 * @brief Default constructor.
                 */
                virtual ~Parameters ()
                {
                    /* empty */
                }

            public: // Overloaded Operator(s)
                
                void operator = (Parameters const&)  = delete;  // assignment is not allowed

                
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

        } // end of namespace 'worker'
    
    } // end of namespace 'worker'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_TYPES_H_
