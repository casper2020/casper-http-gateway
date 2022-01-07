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

/**
 * @brief Default constructor.
 *
 * @param a_owner                   Script owner.
 * @param a_name                    Script name
 * @param a_uri                     Unused.
 * @param a_out_path                Writable directory.
 * @param a_signature_output_format One of \link ::cc::crypto::RSA::SignOutputFormat \link.
 */
casper::proxy::worker::v8::Script::Script (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                                           const std::string& a_out_path,
                                           const ::cc::crypto::RSA::SignOutputFormat a_signature_output_format)
: ::cc::v8::basic::Evaluator(a_owner, a_name, a_uri, a_out_path,
                             /* a_functions */
                             {
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
: ::cc::v8::basic::Evaluator(a_script.owner_, a_script.name_, a_script.uri_, a_script.out_path_,
                             /* a_functions */
                             {
                                 { "NowUTCISO8601", casper::proxy::worker::v8::Script::NowUTCISO8601 },
                                 { "RSASignSHA256", casper::proxy::worker::v8::Script::RSASignSHA256 }
                             }
                             ),
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
 * @brief ISO8061 UTC date and time combined.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::NowUTCISO8601 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    TryCall([] (const ::v8::HandleScope&, const ::v8::FunctionCallbackInfo<::v8::Value>& a_args, const casper::proxy::worker::v8::Script* /* a_script */) {
        a_args.GetReturnValue().Set(::v8::String::NewFromUtf8(a_args.GetIsolate(), ::cc::UTCTime::NowISO8601DateTime().c_str(), ::v8::NewStringType::kNormal).ToLocalChecked());
    }, 0, a_args);
}

/**
 * @brief Sign using a RSA key and SHA256 algorithm.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::RSASignSHA256 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    TryCall([] (const ::v8::HandleScope&, const ::v8::FunctionCallbackInfo<::v8::Value>& a_args, const casper::proxy::worker::v8::Script* a_script) {
        
        const ::v8::String::Utf8Value& value = ::v8::String::Utf8Value(a_args.GetIsolate(), a_args[1]);
        const ::v8::String::Utf8Value& pem   = ::v8::String::Utf8Value(a_args.GetIsolate(), a_args[2]);
        
        std::string signature;
        
        if ( a_args.Length() >= 3 && false == a_args[3].IsEmpty() ) {
            const ::v8::String::Utf8Value& pwd = ::v8::String::Utf8Value(a_args.GetIsolate(), a_args[3]);
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), (*pwd), a_script->signature_output_format_);
        } else {
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), a_script->signature_output_format_);
        }
        
        a_args.GetReturnValue().Set(::v8::String::NewFromUtf8(a_args.GetIsolate(), signature.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked());
        
    }, 2, a_args);
}

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
        
        a_args.GetReturnValue().SetUndefined();
        // ... invalid number of arguments?
        if ( a_args.Length() < 1 || true == a_args[0].IsEmpty() ) {
            // ... done, can't throw exceptions here ...
            return;
        }
        // ... from now on we can throw exceptions ...
        instance = ::cc::ObjectFromHexAddr<casper::proxy::worker::v8::Script>((*::v8::String::Utf8Value(a_args.GetIsolate(), a_args[0])));
        if ( nullptr == instance ) {
            throw ::cc::v8::Exception("Invalid expression evaluation: instance arg is %s", "nullptr");
        }
        if ( nullptr != instance->last_exception_ ){
            delete instance->last_exception_;
            instance->last_exception_ = nullptr;
        }
        // ... validate number of arguments for this call ..
        if ( a_args.Length() < ( a_argc + 1 ) || true == a_args[0].IsEmpty() || true == a_args[1].IsEmpty() || true == a_args[2].IsEmpty() ) {
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
