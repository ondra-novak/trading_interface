#pragma once


#include "../trading_ifc/log.h"

#include <chrono>
#include <iostream>
#include <functional>

namespace trading_api {

class BasicLog: public ILog {
public:

    using TimeSource = std::function<std::chrono::system_clock::time_point()>;

    BasicLog(std::ostream &f, Serverity min_serverity):_f(f),_min_serverity(min_serverity) {}

    virtual void output(Serverity level, std::string_view msg) const override;
    virtual Serverity get_min_level() const override {return _min_serverity;}

    void set_time_source(TimeSource tmsrc);
protected:
    std::ostream &_f;
    mutable std::mutex _mx;
    Serverity _min_serverity;
    TimeSource _time_source;

};


}
