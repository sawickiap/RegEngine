#pragma once

#include "../ThirdParty/str_view/str_view.hpp"

#include <memory>
#include <exception>
#include <stdexcept>
#include <array>

#include <cstdio>
#include <cstdint>
#include <cassert>

using std::unique_ptr;
using std::make_unique;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define FAIL(msgStr)  do { \
        assert(0 && msgStr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): " msgStr); \
    } while(false)
#define CHECK_BOOL(expr)  do { if(!(expr)) { \
        assert(0 && #expr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): ( " #expr " ) == false"); \
    } } while(false)

#define CATCH_PRINT_ERROR(extraCatchCode) \
    catch(const std::exception& ex) \
    { \
        fwprintf(stderr, L"ERROR: %hs\n", ex.what()); \
        extraCatchCode \
    } \
    catch(...) \
    { \
        fwprintf(stderr, L"UNKNOWN ERROR.\n"); \
        extraCatchCode \
    }
