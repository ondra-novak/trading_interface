#include "check.h"
#include "../common/loader.h"

int main() {

    std::string path(MODULE_PATH);
    std::string libpath = path + "example_strategy.so";


    auto module = trading_api::load_strategy_module(libpath);
    CHECK(module != nullptr);
    auto strategy = module->create_strategy("example");
    CHECK(strategy.get() != nullptr);


}
