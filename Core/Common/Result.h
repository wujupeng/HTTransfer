#pragma once

#include <string>
#include <variant>
#include <stdexcept>
#include "ErrorCodes.h"

namespace ht {

template<typename T>
class Result {
public:
    Result() = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    static Result success(T value) {
        Result r;
        r.state_ = State::Ok;
        r.value_ = std::move(value);
        return r;
    }

    static Result failure(ErrorCode code, const std::string& message = "") {
        Result r;
        r.state_ = State::Err;
        r.error_code_ = code;
        r.error_message_ = message;
        return r;
    }

    static Result failure(const std::string& code_str, const std::string& message = "") {
        Result r;
        r.state_ = State::Err;
        r.error_code_str_ = code_str;
        r.error_message_ = message;
        r.error_code_ = ErrorCode::IOError;
        return r;
    }

    bool isOk() const { return state_ == State::Ok; }
    bool isErr() const { return state_ == State::Err; }

    const T& value() const {
        if (state_ != State::Ok) throw std::runtime_error("Result is not Ok");
        return value_;
    }

    T& value() {
        if (state_ != State::Ok) throw std::runtime_error("Result is not Ok");
        return value_;
    }

    ErrorCode errorCode() const { return error_code_; }

    std::string errorCodeString() const {
        if (!error_code_str_.empty()) return error_code_str_;
        return errorCodeToString(error_code_);
    }

    const std::string& errorMessage() const { return error_message_; }

private:
    enum class State { Ok, Err } state_ = State::Err;
    T value_{};
    ErrorCode error_code_ = ErrorCode::IOError;
    std::string error_code_str_;
    std::string error_message_;
};

template<>
class Result<void> {
public:
    Result() = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    static Result success() {
        Result r;
        r.state_ = State::Ok;
        return r;
    }

    static Result failure(ErrorCode code, const std::string& message = "") {
        Result r;
        r.state_ = State::Err;
        r.error_code_ = code;
        r.error_message_ = message;
        return r;
    }

    static Result failure(const std::string& code_str, const std::string& message = "") {
        Result r;
        r.state_ = State::Err;
        r.error_code_str_ = code_str;
        r.error_message_ = message;
        r.error_code_ = ErrorCode::IOError;
        return r;
    }

    bool isOk() const { return state_ == State::Ok; }
    bool isErr() const { return state_ == State::Err; }

    ErrorCode errorCode() const { return error_code_; }

    std::string errorCodeString() const {
        if (!error_code_str_.empty()) return error_code_str_;
        return errorCodeToString(error_code_);
    }

    const std::string& errorMessage() const { return error_message_; }

private:
    enum class State { Ok, Err } state_ = State::Err;
    ErrorCode error_code_ = ErrorCode::IOError;
    std::string error_code_str_;
    std::string error_message_;
};

}