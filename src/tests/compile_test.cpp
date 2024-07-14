#include <iostream>
#include <cstdlib>

#include "../common/scheduler.h"

template class trading_api::Scheduler<void (*)() >;
template class trading_api::SchedulerRealTime<void (*)() >;

int main(int argc, char **argv) {
    return 0;
}



