
#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include "check.h"

int main() {
    try {
        YAML::Node config = YAML::Load("name: Example\n"
                                       "version: 1.0\n"
                                       "libraries:\n"
                                       "  - yaml-cpp\n"
                                       "  - boost\n");

        std::string name = config["name"].as<std::string>();
        double version = config["version"].as<double>();
        std::vector<std::string> libraries = config["libraries"].as<std::vector<std::string>>();

        CHECK_EQUAL(name,"Example");
        CHECK_EQUAL(version,1.0);
        CHECK_EQUAL(libraries.size(), 2);
        CHECK_EQUAL(libraries[0], "yaml-cpp");
        CHECK_EQUAL(libraries[1], "boost");
    } catch (const std::exception& e) {
        std::cerr << "Error parsing YAML file: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

