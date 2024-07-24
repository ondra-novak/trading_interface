#pragma once
#include <string>
#include <memory>

struct Identity {
    std::string name;
    std::string secret;
};



using PIdentity = std::shared_ptr<Identity>;
