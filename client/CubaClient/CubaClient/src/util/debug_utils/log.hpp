#pragma once
#ifdef _DEBUG
#include <iostream>
#define DBG_LOG(x)  (std::cout  << (x) << '\n')
#define DBG_LOGW(x) (std::wcout << (x) << '\n')
#else
#define DBG_LOG(x)  ((void)0)
#define DBG_LOGW(x) ((void)0)
#endif
