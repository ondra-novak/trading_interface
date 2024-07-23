#include "exchange_main.h"
#include <iostream>

#include "../../common/basic_context.h"
#include "../../common/basic_log.h"
#include "../../trading_ifc/shared_state.h"

class EventTarget: public trading_api::IEventTarget {
public:
    virtual void on_event(const trading_api::Instrument &i) {}
    virtual void on_event(const trading_api::Account &a) {}
    virtual void on_event(const trading_api::Instrument &i, trading_api::SubscriptionType subscription_type) {
        std::cout << i.get_id() << " ";
        trading_api::Ticker tk;
        i.get_exchange().get_last_ticker(i, tk);
        std::cout << tk << std::endl;
    }
    virtual void on_event(const trading_api::Order &order,const trading_api::Order::Report &report) {}
    virtual void on_event(const trading_api::Order &order, const trading_api::Fill &fill) {}

};

int main() {

    trading_api::MarketEvent mev;
    mev.set<trading_api::OrderBook>();
    mev >> [](trading_api::Ticker &tk) {
        std::cout << "Is Ticker" << std::endl;
    };
    mev >> [](trading_api::OrderBook &odb) noexcept {
        std::cout << "Is Orderbook" << std::endl;
    };

    trading_api::Log log(std::make_shared<trading_api::BasicLog>(std::cout, trading_api::Log::Serverity::trace));
    auto context = std::make_shared<trading_api::BasicExchangeContext>("Binance",log);

    context->init(std::make_unique<BinanceExchange>(), trading_api::StrategyConfig({
            {"api_name",std::string("3cfa2991082a67c0d3e20318b172d6badc07c1169ace1d83dae410b43b34f8d5")},
            {"private_key",std::string("13795a26cf2e407347d9a3a3c3283122f63cfe8dd3f214708e80c4cf002845bc")},
            {"server",std::string("testnet")}
    }));


    trading_api::SharedState<std::vector<trading_api::Instrument> > state({},[](auto &res){
        for (const trading_api::Instrument &instr: res) {
            std::cout<<"Instrument:" << instr.get_id() << std::endl;
            std::cout<<"Label:" << instr.get_label() << std::endl;
            std::cout<<"Exchange:" << instr.get_exchange().get_label() << std::endl;
            auto cfg = instr.get_config();
            std::cout<<"LotSize:" << cfg.lot_size << std::endl;
            std::cout<<"TickSize:" << cfg.tick_size << std::endl;
            std::cout << "-----" << std::endl;
        }
    });

    context->query_instruments("BTCUSDT", "bitcoin", [&,state](trading_api::Instrument instr){
        std::lock_guard _(state);
        state->push_back(instr);
    });
    context->query_instruments("ETHUSDT", "ethereum", [&,state](trading_api::Instrument instr){
        std::lock_guard _(state);
        state->push_back(instr);
    });

    state.wait();

    trading_api::Instrument bitcoin;

    {
        trading_api::SharedState<trading_api::Instrument *> state(&bitcoin);
        context->query_instruments("BTCUSDT", "btc", [state](const auto &i){
            **state = i;
        });
        state.wait();
    }


    EventTarget evt;

    context->subscribe(&evt, trading_api::SubscriptionType::ticker, bitcoin);

    std::cout << std::cin.get();

    context->disconnect(&evt);

    std::this_thread::sleep_for(std::chrono::seconds(1));



}
