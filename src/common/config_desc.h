#pragma once

#include <json20.h>

#include "../trading_ifc/strategy.h"

namespace trading_api {

    json::value config_schema_to_json(const StrategyConfigSchema &desc);
    json::value config_desc_to_json(const IStrategy *strategy);

}

