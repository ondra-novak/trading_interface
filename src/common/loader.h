#pragma once

#include "../trading_ifc/strategy.h"

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

///Load strategy from module
/**
 * @param module_pathname pathname to SO module
 * @return pointer to strategy factory. Can return nullptr when initialization fails
 *
 * @note you can call this function only once per module. Do not call again
 * for the same module. Store the return value for later usage
 *
 * @exception
 */
const IStrategyFactory *load_strategy_module(std::string module_pathname);




}
