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
#include "cc/crypto/rsa.h"

/**
 * @brief Default constructor.
 *
 * @param a_owner    Script owner.
 * @param a_name     Script name
 * @param a_uri
 * @param a_out_path Writable directory.
 */
casper::proxy::worker::v8::Script::Script (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                                           const std::string& a_out_path)
: ::cc::v8::basic::Evaluator(a_owner, a_name, a_uri, a_out_path,
                             /* a_functions */
                             {
                                 { "NowUTCISO8601", casper::proxy::worker::v8::Script::NowUTCISO8601 },
                                 { "RSASignSHA256", casper::proxy::worker::v8::Script::RSASignSHA256 }
                             }
)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
casper::proxy::worker::v8::Script::~Script ()
{
    /* empty */
}

// MARK: -

/**
 * @brief ISO8061 UTC date and time combined.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::NowUTCISO8601 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    const ::v8::HandleScope handle_scope(a_args.GetIsolate());
    
    a_args.GetReturnValue().SetUndefined();
    
    if ( 0 != a_args.Length() ) {
        // ... can't throw exceptions here ...
        return;
    }
    
    try {
        const ::v8::Local<::v8::String> now = ::v8::String::NewFromUtf8(a_args.GetIsolate(), ::cc::UTCTime::NowISO8601DateTime().c_str(), ::v8::NewStringType::kNormal).ToLocalChecked();
        a_args.GetReturnValue().Set(now);
    } catch (...) {
        // ... can't throw exceptions here ...
        a_args.GetReturnValue().SetUndefined();
    }
}

/**
 * @brief Sign using a RSA key and SHA256 algorithm.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void casper::proxy::worker::v8::Script::RSASignSHA256 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    const ::v8::HandleScope handle_scope(a_args.GetIsolate());
    
    a_args.GetReturnValue().SetUndefined();
    
    if ( a_args.Length() < 2 || true == a_args[0].IsEmpty() || true == a_args[1].IsEmpty() ) {
        // ... can't throw exceptions here ...
        return;
    }
        
    try {
        const ::v8::String::Utf8Value& value = ::v8::String::Utf8Value(a_args.GetIsolate(), a_args[0]);
        const ::v8::String::Utf8Value& pem   = ::v8::String::Utf8Value(a_args.GetIsolate(), a_args[1]);

        std::string signature;
        if ( a_args.Length() >= 3 && false == a_args[3].IsEmpty() ) {
            const ::v8::String::Utf8Value& pwd = ::v8::String::Utf8Value(a_args.GetIsolate(), a_args[2]);
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), (*pwd), ::cc::crypto::RSA::SignOutputFormat::BASE64_RFC4648);
        } else {
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), ::cc::crypto::RSA::SignOutputFormat::BASE64_RFC4648);
        }
        a_args.GetReturnValue().Set(::v8::String::NewFromUtf8(a_args.GetIsolate(), signature.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked());
    } catch (...) {
        // ... can't throw exceptions here ...
        a_args.GetReturnValue().SetUndefined();
    }
}
