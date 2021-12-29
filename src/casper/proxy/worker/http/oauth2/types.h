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
#ifndef CASPER_PROXY_WORKER_HTTP_OAUTH2_TYPES_H_
#define CASPER_PROXY_WORKER_HTTP_OAUTH2_TYPES_H_

#include "casper/job/deferrable/arguments.h"

#include "json/json.h"

#include "cc/non-movable.h"

#include "cc/easy/http/oauth2/client.h"

#include <string>
#include <map>

namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {
        
            namespace http
            {

                namespace oauth2
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
                                                
                        typedef struct {
                            std::string tokens_;
                        } StorageEndpoints;
                        
                        typedef struct {
                            StorageEndpoints                           endpoints_;
                            Json::Value                                arguments_;
                            ::cc::easy::http::oauth2::Client::Headers  headers_;
                            ::cc::easy::http::oauth2::Client::Timeouts timeouts_;
                        } Storage;
                        
                        typedef struct {
                            ::cc::easy::http::oauth2::Client::Headers headers_;
                            ::cc::easy::http::oauth2::Client::Tokens  tokens_;
                        } Storageless;
                        
                        typedef Json::Value Signing;
                        
                    public: // Const Data
                        
                        const Type                                               type_;               //!< Config type, one of \link Type \link.
                        const ::cc::easy::http::oauth2::Client::Config           http_;               //!< OAuth2 configs
                        const ::cc::easy::http::oauth2::Client::Headers          headers_;            //!< additional headers per request
                        const ::cc::easy::http::oauth2::Client::HeadersPerMethod headers_per_method_; //!< additional headers per request per method
                        const Signing                                            signing_;            //!< signing configs
                        
                    private: // Data
                        
                        Storage* storage_;         //!< storage config
                        Storageless* storageless_; //!< storageless config

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
                        Config (const ::cc::easy::http::oauth2::Client::Config& a_config, const ::cc::easy::http::oauth2::Client::Headers& a_headers,
                                const ::cc::easy::http::oauth2::Client::HeadersPerMethod& a_headers_per_method, const Signing& a_signing, const Storage& a_storage)
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
                        Config (const ::cc::easy::http::oauth2::Client::Config& a_config, const ::cc::easy::http::oauth2::Client::Headers& a_headers,
                                const ::cc::easy::http::oauth2::Client::HeadersPerMethod& a_headers_per_method, const Signing& a_signing, const Storageless& a_storageless)
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
                            ::cc::easy::http::oauth2::Client::Method   method_;
                            std::string                                url_;
                            std::string                                body_;
                            ::cc::easy::http::oauth2::Client::Headers  headers_;
                            ::cc::easy::http::oauth2::Client::Timeouts timeouts_;
                        } Storage;

                        enum class RequestType : uint8_t {
                            OAuth2Grant = 0x01,
                            HTTP
                        };
                        
                        typedef struct {
                            ::cc::easy::http::oauth2::Client::Method   method_;
                            std::string                                url_;
                            std::string                                body_;
                            ::cc::easy::http::oauth2::Client::Headers  headers_;
                            ::cc::easy::http::oauth2::Client::Timeouts timeouts_;
                            ::cc::easy::http::oauth2::Client::Tokens   tokens_;
                        } HTTPRequest;
                        
                        typedef struct {
                            std::string                                value_;    //!< authorization code value
                            std::string                                scope_;    //!< scope
                            std::string                                state_;    //!< state
                            ::cc::easy::http::oauth2::Client::Timeouts timeouts_; //!< http timeouts
                            ::cc::easy::http::oauth2::Client::Tokens   tokens_;   //!< oauth2 tokens
                        } GrantAuthCodeRequest;
                        
                    public: // Const Data
                        
                        const std::string                               id_;
                        const Config::Type                              type_;
                        const ::cc::easy::http::oauth2::Client::Config& config_;
                        const Json::Value&                              data_;
                        const bool                                      primitive_;
                        const int                                       log_level_;
                        
                    private: // Data
                        
                        Storage*                                    storage_;       //!< Evaluated storage request data.
                        HTTPRequest*                                http_req_;      //!< Evaluated HTTP request data.
                        GrantAuthCodeRequest*                       auth_code_req_; //!< Evaludated authorization code request
                        
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
                                    const ::cc::easy::http::oauth2::Client::Config& a_config,
                                    const Json::Value& a_data, const bool a_primitive, const int a_log_level)
                         : id_(a_id), type_(a_type), config_(a_config), data_(a_data), primitive_(a_primitive), log_level_(a_log_level),
                           storage_(nullptr), http_req_(nullptr), auth_code_req_(nullptr)
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
                           storage_(nullptr), http_req_(nullptr), auth_code_req_(nullptr)
                        {
                            if ( nullptr != a_parameters.storage_ ) {
                                storage_ = new Storage(*a_parameters.storage_);
                            }
                            if ( nullptr != a_parameters.http_req_ ) {
                                http_req_ = new HTTPRequest(*a_parameters.http_req_);
                            }
                            if ( nullptr != a_parameters.auth_code_req_ ) {
                                auth_code_req_ = new GrantAuthCodeRequest(*a_parameters.auth_code_req_);
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
                            if ( nullptr != http_req_ ) {
                                delete http_req_;
                            }
                            if ( nullptr != auth_code_req_ ) {
                                delete auth_code_req_;
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
                                    /* method_   */ ::cc::easy::http::oauth2::Client::Method::NotSet,
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
                        inline const Storage& storage (const ::cc::easy::http::oauth2::Client::Method a_type)
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
                        inline const Storage& storage (const ::cc::easy::http::oauth2::Client::Method a_type, const std::string& a_body)
                        {
                            if ( Config::Type::Storage != type_ || nullptr == storage_ ) {
                                throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                            }
                            storage_->method_ = a_type;
                            storage_->body_   = a_body;
                            // ... done ...
                            return *storage_;
                        }
                        
                        inline RequestType request_type () const
                        {
                            if ( nullptr != http_req_ ) {
                                return RequestType::HTTP;
                            } else if ( nullptr != auth_code_req_ ) {
                                return RequestType::OAuth2Grant;
                            }
                            throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                        }
                        
                        /**
                         * @return R/O access to HTTP request data.
                         */
                        inline const HTTPRequest& http_request () const
                        {
                            if ( nullptr != http_req_ ) {
                                return *http_req_;
                            }
                            throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                        }
                        
                        /**
                         * @brief Prepare request data, exception for better tracking of variables write acccess.
                         *
                         * @return R/O access to request data.
                         */
                        inline const HTTPRequest& http_request (const std::function<void(HTTPRequest&)>& a_callback)
                        {
                            CC_DEBUG_ASSERT(nullptr == auth_code_req_);
                            // ... if doesn't exists yet ...
                            if ( nullptr == http_req_ ) {
                                http_req_ = new HTTPRequest({
                                    /* method_   */ ::cc::easy::http::oauth2::Client::Method::NotSet,
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
                                });
                            }
                            // ... callback ...
                            a_callback(*http_req_);
                            // ... done ...
                            return *http_req_;
                        }
                        
                        /**
                         * @brief Prepare request tokens data, exception for better tracking of variables write acccess.
                         *
                         * @return R/W access to request tokens data.
                         */
                        inline ::cc::easy::http::oauth2::Client::Tokens& tokens (const std::function<void(::cc::easy::http::oauth2::Client::Tokens&)>& a_callback)
                        {
                            if ( nullptr != auth_code_req_ ) {
                                // ... callback ...
                                a_callback(auth_code_req_->tokens_);
                                // ... done ...
                                return auth_code_req_->tokens_;
                            } else if ( nullptr != http_req_ ) {
                                // ... callback ...
                                a_callback(http_req_->tokens_);
                                // ... done ...
                                return http_req_->tokens_;
                            }
                            throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                        }

                        /**
                         * @return R/O access to request OAuth2 tokens.
                         */
                        inline const ::cc::easy::http::oauth2::Client::Tokens& tokens () const
                        {
                            if ( nullptr != auth_code_req_ ) {
                                // ... done ...
                                return auth_code_req_->tokens_;
                            } else if ( nullptr != http_req_ ) {
                                // ... done ...
                                return http_req_->tokens_;
                            }
                            throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                        }
                        
                        /**
                         * @return R/O access to authorization code data.
                         */
                        inline const GrantAuthCodeRequest& auth_code_request () const
                        {
                            if ( nullptr != auth_code_req_ ) {
                                return *auth_code_req_;
                            }
                            throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                        }
                        
                        /**
                         * @brief Prepare authorization code request, exception for better tracking of variables write acccess.
                         *
                         * @return R/O access to authorization code request.
                         */
                        inline const GrantAuthCodeRequest& auth_code_request (const std::function<void(GrantAuthCodeRequest&)>& a_callback)
                        {
                            CC_DEBUG_ASSERT(nullptr == http_req_);
                            // ... if doesn't exists yet ...
                            if ( nullptr == auth_code_req_ ) {
                                // ... create it now ...
                                auth_code_req_ = new GrantAuthCodeRequest({
                                    /* value_ */ "",
                                    /* scope_ */ "",
                                    /* state_ */ "",
                                    /* timeouts_ */ { -1, -1 },
                                    /* tokens_   */ {
                                        /* type_       */ "",
                                        /* access_     */ "",
                                        /* refresh_    */ "",
                                        /* expires_in_ */  0,
                                        /* scope_      */ "",
                                        /* on_change_  */ nullptr
                                     }
                                });
                            }
                            // ... callback ...
                            a_callback(*auth_code_req_);
                            // ... done ...
                            return *auth_code_req_;
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
                        
                        virtual bool Primitive () const { return parameters_.primitive_; }
                        
                    }; // end of class 'Arguments'

                } // end of namespace 'oauth2'
        
            } // end of namespace 'http'

        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_HTTP_OAUTH2_TYPES_H_
