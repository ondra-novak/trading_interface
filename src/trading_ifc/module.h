#pragma once

#include "strategy.h"

#include <map>

namespace trading_api {


class Module: public IStrategyFactory {
public:

    using Factory = PStrategy (*)();

    template<std::derived_from<IStrategy> Strategy>
    void export_strategy(std::string_view name) noexcept {
        strategies.emplace(std::string(name), []()->PStrategy{
                return std::make_unique<Strategy>();
        });
    }

    virtual std::unique_ptr<IStrategy> create_strategy(std::string_view strategy_name) const noexcept {
        auto iter = strategies.find(strategy_name);
        if (iter == strategies.end()) return {};
        return iter->second();
    }

protected:
    std::map<std::string, Factory, std::less<> > strategies;
};


}

void strategy_main(trading_api::Module &m) ;


extern "C" {
const trading_api::IStrategyFactory * __trading_api_strategy_entry_point() {
    static trading_api::Module module;
    strategy_main(module);
    return &module;
}
}


