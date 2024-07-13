
#include "loader.h"

#include <iostream>
#include <dlfcn.h>

namespace trading_api {


load_strategy_exception::load_strategy_exception(
        std::string module_name, std::string message)
:module_name(module_name), message(message) {}

const char* load_strategy_exception::what() const noexcept {
    if (what_msg.empty()) {
        what_msg = message + " : " + module_name;
    }
    return what_msg.c_str();
}

void ModuleRepository::add_module(const std::string &module_pathname) {
    void * handle = dlopen(module_pathname.c_str(), RTLD_NOW);
    if (!handle) {
        throw load_strategy_exception(module_pathname, dlerror());
    }
    auto entryfn = reinterpret_cast<EntryPointFn>(
            dlsym(handle, "__trading_api_module_entry_point"));

    if (!entryfn){
        throw load_strategy_exception(module_pathname, "Not a module file - missing entry point");
    }
    _modules.push_back(ModuleInfo{
        module_pathname,handle,entryfn()
    });
}

template<typename Fn>
auto ModuleRepository::create_something(std::string_view name, Fn &&fn) {
    using RetType = decltype(fn(_modules.begin()->instance).find(name)->create());
    auto iter = _modules.rbegin();
    auto end = _modules.rend();
    while (iter != end) {
        auto s = fn(iter->instance);
        auto miter = s.find(name);
        if (miter != s.end()) return miter->create();
        ++iter;
    }
    return RetType{};
}


std::unique_ptr<IStrategy> ModuleRepository::create_strategy(std::string_view name) {
    return create_something(name, [&](auto ptr){return ptr->get_strategies();});
}

std::unique_ptr<IExchangeService> ModuleRepository::create_exchange(std::string_view name) {
    return create_something(name, [&](auto ptr){return ptr->get_exchanges();});

}

void ModuleRepository::housekeeping() {
}

}

