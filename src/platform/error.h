#pragma once

namespace Platform {

enum class ErrorCode : u8 {
    SUCCESS = 0,
    UNKNOWN = 1,
    FILE_NOT_FOUND = 2,
    FILE_LOCKED = 3,
    FILE_EXISTS = 4
};

void Fail(const char* message);

}
