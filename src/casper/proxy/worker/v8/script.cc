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

const char* const casper::proxy::worker::v8::Script::k_evaluate_basic_expression_func_name_ = "_basic_expr_eval";
const char* const casper::proxy::worker::v8::Script::k_evaluate_basic_expression_func_ =
"function _basic_expr_eval(expr, $) {\n"
"    return eval(expr);\n"
"}"
;
const char* const casper::proxy::worker::v8::Script::k_variable_log_func_name_ = "_log";
const char* const casper::proxy::worker::v8::Script::k_variable_log_func_ =
    "function _log($) {\n"
     "    NativeLog(JSON.stringify($));\n"
    "}"
;

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
: ::cc::v8::basic::Evaluator(a_owner, a_name, a_uri, a_out_path,
                             /* a_functions */
                             {
                                 { "NativeLog"    , casper::proxy::worker::v8::Script::NativeLog     },
                                 { "NowUTCISO8601", casper::proxy::worker::v8::Script::NowUTCISO8601 },
                                 { "RSASignSHA256", casper::proxy::worker::v8::Script::RSASignSHA256 }
                             }),
    loggable_data_(a_loggable_data), signature_output_format_(a_signature_output_format)
{
    last_exception_ = nullptr;
    logger_client_  = nullptr;
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
                                 { "NativeLog"    , casper::proxy::worker::v8::Script::NativeLog     },
                                 { "NowUTCISO8601", casper::proxy::worker::v8::Script::NowUTCISO8601 },
                                 { "RSASignSHA256", casper::proxy::worker::v8::Script::RSASignSHA256 }
                             }),
    loggable_data_(a_script.loggable_data_), signature_output_format_(a_script.signature_output_format_)
{
    last_exception_ = ( nullptr != a_script.last_exception_ ? new ::cc::v8::Exception(*last_exception_) : nullptr );
    logger_client_  = nullptr;
}

/**
 * @brief Destructor.
 */
casper::proxy::worker::v8::Script::~Script ()
{
    if ( nullptr != last_exception_ ){
        delete last_exception_;
    }
    // ... unregister from logger ...
    if ( nullptr != logger_client_ ) {
        ::ev::LoggerV2::GetInstance().Unregister(logger_client_);
        delete logger_client_;
    }
}

// MARK: -

/**
 * @brief Load this script to a specific context.
 *
 * @param a_external_scripts
 * @param a_expressions
 */
void casper::proxy::worker::v8::Script::Load (const Json::Value& a_external_scripts, const casper::proxy::worker::v8::Script::Expressions& a_expressions)
{
    // ... register at logger ...
    if ( nullptr == logger_client_ ) {
        logger_client_ = new ::ev::LoggerV2::Client(loggable_data_);
        logger_client_->Unset(ev::LoggerV2::Client::LoggableFlags::IPAddress | ev::LoggerV2::Client::LoggableFlags::OwnerPTR);
        logger_token_ = name_;
        ::ev::LoggerV2::GetInstance().cc::logs::Logger::Register(logger_token_, ::cc::fs::Dir::Normalize(out_path_) + logger_token_ + ".log");
        ::ev::LoggerV2::GetInstance().Register(logger_client_, { logger_token_.c_str() });
    }
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "V8 Script");
    // ... prepare script ...
    std::stringstream ss;
    ss.str("");
    ss << "\"use strict\";\n";
    // ... basic expression function ...
    ss << "\n//\n// " << k_evaluate_basic_expression_func_name_ << "\n//\n";
    ss << k_evaluate_basic_expression_func_;
    // ... log function ...
    ss << "\n\n//\n// " << k_variable_log_func_name_ << "\n//\n";
    ss << k_variable_log_func_;
    // ... load external scripts ( 😨 ) ...
    if ( false == a_external_scripts.isNull() ) {
        cc::fs::Dir::ListFiles(cc::fs::Dir::Normalize(a_external_scripts.asString()), /* a_pattern */ "*.js", [this, &ss] (const std::string& a_uri) -> bool {
            // ... log ...
            ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), "Loading '%s'...", a_uri.c_str());
            // ... load ...
            ss << "\n\n//\n// " << a_uri << "\n//\n";
            std::ifstream file(a_uri);
            if ( file ) {
                ss << file.rdbuf();
                file.close();
            } else {
                throw ::cc::v8::Exception("Unable to load file %s: check permissions!", a_uri.c_str());
            }
            // ... next ...
            return true;
        });
    }
    // ... keep track of this function ...
    std::string loaded_script = ss.str();
    if ( '\n' == loaded_script[loaded_script.length() - 1] ) {
        loaded_script.pop_back();
    }
    // ... keep track of this instance ...
    SetIsolateData(/* slot */ 0,  this);
    // ... compile script ...
    IsolatedCall([this, &loaded_script]
                 (::v8::Local<::v8::Context>& /* a_context */, ::v8::TryCatch& /* a_try_catch */, ::v8::Isolate* a_isolate) {
                     const ::v8::Local<::v8::String>                script    = ::v8::String::NewFromUtf8(a_isolate, loaded_script.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked();
                     const std::vector<::cc::v8::Context::Function> functions = {
                         { /* name  */ k_evaluate_basic_expression_func_name_   },
                         { /* name_ */ k_variable_log_func_name_                }
                    };
                    try {
                        // ... log ...
                        ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(CYAN) "%s" CC_LOGS_LOGGER_RESET_ATTRS, "Compiling...");
                        // ... compile ...
                        std::string compiled_script;
                        Compile(script, &functions, &compiled_script);
                        // ... log ...
                        ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(DARK_GRAY) "\n%s" CC_LOGS_LOGGER_RESET_ATTRS, compiled_script.c_str());
                        ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), "%s", CC_LOGS_LOGGER_COLOR(GREEN) "Compiled." CC_LOGS_LOGGER_RESET_ATTRS);
                    } catch (const cc::v8::Exception& a_v8_exception) {
                        // ... log ...
                        ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(DARK_GRAY) "\n%s" CC_LOGS_LOGGER_RESET_ATTRS, loaded_script.c_str());
                        ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(RED) "\n%s" CC_LOGS_LOGGER_RESET_ATTRS, a_v8_exception.what());
                        ::ev::LoggerV2::GetInstance().Log(logger_client_, logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(RED) "%s" CC_LOGS_LOGGER_RESET_ATTRS, "Failed.");
                        // ... re-throw ...
                        throw a_v8_exception;
                    }
                }
    );
}

/**
 * @brief Call this function to evaluate an expression
 *
 * @param a_log_callback Function to call to log data ( when NativeLog is called ).
 * @param a_object       V8 Object, as \link ::v8::Value \link.
 * @param a_expression   Expression to evaluate.
 *
 * @param o_value Evalutation result as \link ::cc::v8::Value \link.
 */
void casper::proxy::worker::v8::Script::Evaluate (const ::v8::Persistent<::v8::Value>& a_object, const std::string& a_expr_string,
                                                  ::cc::v8::Value& o_value)
{
    try {
        ::cc::v8::basic::Evaluator::Evaluate(a_object, a_expr_string, o_value);
    } catch (const ::cc::v8::Exception& a_v8_exception) {
        if ( nullptr != log_callback_ ) {
            log_callback_(a_v8_exception.what(), /* a_success */ false);
        }
        throw a_v8_exception;
    } catch (...) {
        try {
            ::cc::v8::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::v8::Exception& a_v8_exception) {
            if ( nullptr != log_callback_ ) {
                log_callback_(a_v8_exception.what(), /* a_success */ false);
            }
            throw a_v8_exception;
        }
    }
}

// MARK: -

/**
 * @brief The callback that is invoked by v8 whenever the JavaScript 'NativeLog' function is called.
 *
 * @param a_args
 */
void casper::proxy::worker::v8::Script::NativeLog (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    std::stringstream ss;
    if ( 0 == a_args.Length() ) {
        return;
    }        
    const ::v8::HandleScope handle_scope(a_args.GetIsolate());
    const casper::proxy::worker::v8::Script* self = (casper::proxy::worker::v8::Script*)handle_scope.GetIsolate()->GetData(0);
    if ( nullptr == self->logger_client_ ) {
        return;
    }
    if ( a_args.Length() > 0 ) {
        ::v8::String::Utf8Value first(a_args.GetIsolate(), a_args[0]);
        ss << *first;
        for ( int i = 1; i < a_args.Length(); i++ ) {
            ::v8::String::Utf8Value str(a_args.GetIsolate(), a_args[i]);
            ss << ", " << *str;
        }
    }
    if ( nullptr != self->log_callback_ ) {
        self->log_callback_(ss.str(), /* a_success */ true);
    } else {
        ::ev::LoggerV2::GetInstance().Log(self->logger_client_, self->logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(DARK_GRAY) "%s\n" CC_LOGS_LOGGER_RESET_ATTRS, ss.str().c_str());
    }
}

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
        
        const ::v8::String::Utf8Value& value = ::v8::String::Utf8Value(a_args_t.GetIsolate(), a_args_t[1]);
        const ::v8::String::Utf8Value& pem   = ::v8::String::Utf8Value(a_args_t.GetIsolate(), a_args_t[2]);
        
        std::string signature;
        
        if ( a_args_t.Length() >= 3 && false == a_args_t[3].IsEmpty() ) {
            const ::v8::String::Utf8Value& pwd = ::v8::String::Utf8Value(a_args_t.GetIsolate(), a_args_t[3]);
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), (*pwd), a_script->signature_output_format_);
        } else {
            signature = ::cc::crypto::RSA::SignSHA256((*value), (*pem), a_script->signature_output_format_);
        }
        
        a_args_t.GetReturnValue().Set(::v8::String::NewFromUtf8(a_args_t.GetIsolate(), signature.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked());
        
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
