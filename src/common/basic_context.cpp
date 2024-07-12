#include "basic_context.h"

#include "basic_order.h"
namespace trading_api {


template class BasicContext<MemoryStorage, std::function<void(Timestamp, Function<void(Timestamp)>, const void *)> >;

}
