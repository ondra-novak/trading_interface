#include "check.h"
#include "../common/loader.h"
#include "../common/config_desc.h"

int main() {

    std::string path(MODULE_PATH);
    std::string libpath = path + "example_strategy.so";

    trading_api::ModuleRepository repo;
    repo.add_module(libpath);


    auto strategy = repo.create_strategy("Example");
    CHECK(strategy.get() != nullptr);
    auto config = strategy->get_config_schema();
    auto json = trading_api::config_schema_to_json(config);
    std::cout << json.to_json() << std::endl;



}

