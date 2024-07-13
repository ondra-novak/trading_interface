#include "check.h"
#include "../common/loader.h"

int main() {

    std::string path(MODULE_PATH);
    std::string strategy_path = path + "example_strategy.so";
    std::string exchange_path = path + "example_exchange.so";

    trading_api::ModuleRepository repo;
    repo.add_module(strategy_path);
    repo.add_module(exchange_path);
    auto strategy = repo.create_strategy("Example");
    CHECK(strategy.get() != nullptr);
    auto exchange = repo.create_exchange("ExampleExchange");
    CHECK(exchange.get() != nullptr);


}
