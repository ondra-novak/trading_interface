#pragma once
#include <string>
#include <memory>

struct Identity {
    std::string name;
    std::string secret;

    static std::shared_ptr<Identity> create(Identity id) {
        return std::make_shared<Identity>(std::move(id));
    }
};



using PIdentity = std::shared_ptr<Identity>;
