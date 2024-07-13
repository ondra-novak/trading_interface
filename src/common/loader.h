#pragma once

#include "../trading_ifc/strategy.h"
#include "../trading_ifc/exchange_service.h"

#include <string>
#include <memory>

#include <list>
namespace trading_api {


class load_strategy_exception: public std::exception {
public:

    load_strategy_exception(std::string module_name, std::string message);

    const std::string& getMessage() const {
        return message;
    }

    const std::string& getModuleName() const {
        return module_name;
    }

    virtual const char* what() const noexcept override;


protected:
    std::string module_name;
    std::string message;
    mutable std::string what_msg;
};



class ModuleRepository {
public:

    ///add and load module into repository
    /**
     * @param module_pathname name of module
     */
    void add_module(const std::string &module_pathname);
    ///create strategy object identified by name
    std::unique_ptr<IStrategy> create_strategy(std::string_view name);
    ///create exchange object identified by name
    std::unique_ptr<IExchangeService> create_exchange(std::string_view name);
    ///unload unused modules
    /**
     * Unused module is module which doesn't have any strategy or exchange
     * active. Such modules are unloaded
     */
    void housekeeping();

protected:
    struct ModuleInfo {
        std::string pathname;
        void *handle;
        const IModule *instance;
    };
    std::list<ModuleInfo> _modules;

    template<typename Fn>
    auto create_something(std::string_view name, Fn &&fn);


};


}
