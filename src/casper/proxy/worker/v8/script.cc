/**
 * @file script.cc
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

#include "casper/proxy/worker/v8/script.h"

#include "cc/v8/exception.h"

#include "cc/types.h"
#include "cc/utc_time.h"
#include "cc/fs/dir.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data           TO BE COPIED
 * @param a_owner                   Script owner.
 * @param a_name                    Script name
 * @param a_uri                     Unused.
 * @param a_out_path                Writable directory.
 * @param a_signature_output_format One of \link ::cc::crypto::RSA::SignOutputFormat \link.
 */
casper::proxy::worker::v8::Script::Script (const ::ev::Loggable::Data& a_loggable_data,
                                           const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                                           const std::string& a_out_path,
                                           const ::cc::crypto::RSA::SignOutputFormat a_signature_output_format)
: ::cc::v8::basic::Evaluator(a_loggable_data,
                             a_owner, a_name, a_uri, a_out_path,
                             /* a_functions */
                             {
                                 { "NativeLog"    , casper::proxy::worker::v8::Script::NativeLog     },
                                 { "NowUTCISO8601", casper::proxy::worker::v8::Script::NowUTCISO8601 },
                                 { "RSASignSHA256", casper::proxy::worker::v8::Script::RSASignSHA256 }
                             }),
    signature_output_format_(a_signature_output_format)
{
    last_exception_ = nullptr;
}

/**
 * @brief Copy constructor.
 *
 * @param a_script Object to copy.
 */
casper::proxy::worker::v8::Script::Script (const casper::proxy::worker::v8::Script& a_script)
: ::cc::v8::basic::Evaluator(a_script),
    signature_output_format_(a_script.signature_output_format_)
{
    last_exception_ = ( nullptr != a_script.last_exception_ ? new ::cc::v8::Exception(*last_exception_) : nullptr );
}

/**
 * @brief Destructor.
 */
casper::proxy::worker::v8::Script::~Script ()
{
    if ( nullptr != last_exception_ ){
        delete last_exception_;
    }
}

// MARK: -

/**
 * @brief Load this script to a specific context.
 *
 * @param a_external_scripts External scripts to load, as JSON string.
 * @param a_expressions      Expressions to load.
 * @param a_ss               Stream to use when loading additional functions.
 */
void casper::proxy::worker::v8::Script::InnerLoad (const Json::Value& a_external_scripts, const casper::proxy::worker::v8::Script::Expressions& a_expressions, std::stringstream& a_ss)
{
    // ... load external scripts ( 😨 ) ...
    if ( false == a_external_scripts.isNull() ) {
        cc::fs::Dir::ListFiles(cc::fs::Dir::Normalize(a_external_scripts.asString()), /* a_pattern */ "*.js", [this, &a_ss] (const std::string& a_uri) -> bool {
            // ... log ...
            ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), "Loading '%s'...", a_uri.c_str());
            // ... load ...
            a_ss << "\n\n//\n// " << a_uri << "\n//\n";
            std::ifstream file(a_uri);
            if ( file ) {
                a_ss << file.rdbuf();
                file.close();
            } else {
                throw ::cc::v8::Exception("Unable to load file %s: check permissions!", a_uri.c_str());
            }
            // ... next ...
            return true;
        });
    }
}

// MARK: -

/**
 * @brief ISO8061 UTC date and time combined.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::NowUTCISO8601 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    TryCall([] (const ::v8::HandleScope&, const ::v8::FunctionCallbackInfo<::v8::Value>& a_args_t, const casper::proxy::worker::v8::Script* /* a_script */) {
        a_args_t.GetReturnValue().Set(::v8::String::NewFromUtf8(a_args_t.GetIsolate(), ::cc::UTCTime::NowISO8601DateTime().c_str(), ::v8::NewStringType::kNormal).ToLocalChecked());
    }, 0, a_args);
}

/**
 * @brief Sign using a RSA key and SHA256 algorithm.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::RSASignSHA256 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    TryCall([] (const ::v8::HandleScope&, const ::v8::FunctionCallbackInfo<::v8::Value>& a_args_t, const casper::proxy::worker::v8::Script* a_script) {
        
        const ::v8::String::Utf8Value& value = ::v8::String::Utf8Value(a_args_t.GetIsolate(), a_args_t[0]);
        const ::v8::String::Utf8Value& pem   = ::v8::String::Utf8Value(a_args_t.GetIsolate(), a_args_t[1]);
        
        std::string signature;
        
        if ( a_args_t.Length() >= 3 && false == a_args_t[2].IsEmpty() ) {
            const ::v8::String::Utf8Value& pwd = ::v8::String::Utf8Value(a_args_t.GetIsolate(), a_args_t[2]);
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), (*pwd), a_script->signature_output_format_);
        } else {
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), a_script->signature_output_format_);
        }
        
        a_args_t.GetReturnValue().Set(::v8::String::NewFromUtf8(a_args_t.GetIsolate(), signature.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked());
        
    }, 2, a_args);
}

// MARK: -

/**
 * @brief Try-catch function call.
 *
 * @param a_function Function to call.
 * @param a_args     V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::TryCall (const std::function<void(const ::v8::HandleScope&, const ::v8::FunctionCallbackInfo<::v8::Value>&, const casper::proxy::worker::v8::Script*)> a_function,
                                                 const size_t a_argc, const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    casper::proxy::worker::v8::Script* instance = nullptr;
    try {
        // ... handle scope needs to be created here ( ignore it if unused, not a bug ) ...
        const ::v8::HandleScope scope(a_args.GetIsolate());
        // ... reset ...
        a_args.GetReturnValue().SetUndefined();
        // ... grab handler ...
        instance = dynamic_cast<casper::proxy::worker::v8::Script*>((cc::v8::basic::Evaluator*)scope.GetIsolate()->GetData(0));
        if ( nullptr == instance ) {
            // ... done, can't throw exceptions here ...
            return;
        }
        // ... reset ...
        if ( nullptr != instance->last_exception_ ){
            delete instance->last_exception_;
            instance->last_exception_ = nullptr;
        }
        // ... validate number of arguments for this call ..
        if ( a_args.Length() < a_argc ) {
            throw ::cc::v8::Exception("Invalid expression evaluation: wrong number of arguments got " INT64_FMT ", expected " SIZET_FMT "!", static_cast<int64_t>(a_args.Length()), a_argc);
        }
        // ... perform call ...
        a_function(scope, a_args, instance);
    } catch (const ::cc::v8::Exception& a_exception) {
        // ... track it ...
        if ( nullptr != instance ) {
            instance->last_exception_ = new ::cc::v8::Exception(a_exception);
        }
        // ... can't throw exceptions here ...
        a_args.GetReturnValue().SetUndefined();
    } catch (...) {
        // ... track it ...
        if ( nullptr != instance ) {
            try {
                ::cc::v8::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
            } catch (const ::cc::v8::Exception& a_exception) {
                instance->last_exception_ = new ::cc::v8::Exception(a_exception);
            }
        }
        // ... can't throw exceptions here ...
        a_args.GetReturnValue().SetUndefined();
    }
}
