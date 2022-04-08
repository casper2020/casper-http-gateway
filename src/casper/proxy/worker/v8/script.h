/**
 * @file script.h
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
 * along with casper-proxy-worker. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CASPER_PROXY_WORKER_V8_SCRIPT_H_
#define CASPER_PROXY_WORKER_V8_SCRIPT_H_

#include "cc/v8/basic/evaluator.h"
#include "cc/v8/value.h"
#include "cc/v8/exception.h"

#include "cc/macros.h"

#include "cc/non-movable.h"
#include "cc/non-copyable.h"

#include "cc/crypto/rsa.h"

#include "ev/logger_v2.h"

namespace casper
{
    
    namespace proxy
    {
        
        namespace worker
        {
            
            namespace v8
            {

                class Script final : public ::cc::v8::basic::Evaluator
                {
                    
                public: // Data Type(s)
                    
                    typedef std::function<void(const std::string&, const bool)> LogCallback;
                    
                private: // Static Const Data

                    static const char* const k_evaluate_basic_expression_func_name_;
                    static const char* const k_evaluate_basic_expression_func_;
                    static const char* const k_variable_log_func_name_;
                    static const char* const k_variable_log_func_;
                    
                private: // Data
                    
                    ::ev::Loggable::Data                loggable_data_;
                    ::cc::crypto::RSA::SignOutputFormat signature_output_format_;

                private: // Data

                    ::cc::v8::Exception* last_exception_;
                    
                private: // Helper(s)
                    
                    ::ev::LoggerV2::Client* logger_client_;
                    std::string             logger_token_;
                    
                private: // Callback(s)
                    
                    LogCallback             log_callback_;
                    
                public: // Constructor(s) / Destructor
                    
                    Script () = delete;
                    Script (const ::ev::Loggable::Data& a_loggable_data,
                            const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                            const std::string& a_out_path, const ::cc::crypto::RSA::SignOutputFormat a_signature_output_format);
                    Script (const Script& a_script);
                    virtual ~Script ();
                    
                public: // Inherited Method(s) / Function(s) - from ::cc::v8::Script
                    
                    virtual void Load (const Json::Value& a_external_scripts, const Expressions& a_expressions);
                
                public: // Inherited Method(s) / Function(s) - from ::cc::v8::Script
                    
                    virtual void Evaluate (const ::v8::Persistent<::v8::Value>& a_object, const std::string& a_expr_string,
                                           ::cc::v8::Value& o_value);
                    
                private: // Static Method(s) / Function(s)
                    
                    static void NativeLog     (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args);
                    static void NowUTCISO8601 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args);
                    static void RSASignSHA256 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args);
                    static void TryCall       (const std::function<void(const ::v8::HandleScope&, const ::v8::FunctionCallbackInfo<::v8::Value>&, const casper::proxy::worker::v8::Script*)> a_function,
                                               const size_t, const ::v8::FunctionCallbackInfo<::v8::Value>&);
                    
                public: // Inline Method(s) / Function(s)
                    
                    inline void                        Register       (LogCallback a_callback) { log_callback_ = a_callback; }
                    inline const bool                  IsExceptionSet () const {                                        return nullptr != last_exception_;  }
                    inline const ::cc::v8::Exception&  exception      () const { CC_ASSERT(nullptr != last_exception_); return *last_exception_;            }
                    inline void                        Reset          ()       { if ( nullptr != last_exception_ ) { delete last_exception_; last_exception_ = nullptr; }}
          
                }; // end of class 'Script'
                
            } // end of namespace 'v8'
            
        } // end of namespace 'worker'
        
    } // end of namespace 'proxy'
    
} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_V8_SCRIPT_H_
