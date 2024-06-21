#include "check.h"
#include "../common/loader.h"
#include "../common/config_desc.h"

int main() {

    std::string path(MODULE_PATH);
    std::string libpath = path + "example_strategy.so";


    auto module = trading_api::load_strategy_module(libpath);
    CHECK(module != nullptr);
    auto strategy = module->create_strategy("example");
    CHECK(strategy.get() != nullptr);
    auto config = strategy->get_config_schema();
    auto json = trading_api::config_schema_to_json(config);
    std::cout << json.to_json() << std::endl;



}

