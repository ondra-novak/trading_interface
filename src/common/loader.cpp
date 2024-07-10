
#include "loader.h"

#include <iostream>
#include <dlfcn.h>

namespace trading_api {


const IStrategyFactory *load_strategy_module(std::string module_pathname) {

    void * handle = dlopen(module_pathname.c_str(), RTLD_NOW);
    if (!handle) {
        throw load_strategy_exception(module_pathname, dlerror());
    }

    auto entryfn = (EntryPointFn)dlsym(handle, "__trading_api_strategy_entry_point");
    if (!entryfn){
        throw load_strategy_exception(module_pathname, "Missing entry point");
    }
    return entryfn();


}

load_strategy_exception::load_strategy_exception(
        std::string module_name, std::string message)
:module_name(module_name), message(message) {}

const char* load_strategy_exception::what() const noexcept {
    if (what_msg.empty()) {
        what_msg = message + " : " + module_name;
    }
    return what_msg.c_str();
}

}

