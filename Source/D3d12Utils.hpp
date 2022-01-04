#pragma once

#include "BaseUtils.hpp"

#include <dxgi1_4.h>
#include <d3d12.h>
#include "..\ThirdParty\d3dx12\d3dx12.h"

#define CHECK_HR(expr)  do { if(FAILED(expr)) { \
        assert(0 && #expr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): FAILED( " #expr " )"); \
    } } while(false)
