#include "exchange_main.h"
#include <iostream>

#include "../../common/basic_context.h"
#include "../../common/basic_log.h"
#include "../../trading_ifc/shared_state.h"

class EventTarget: public trading_api::IEventTarget {
public:
    virtual void on_event(const trading_api::Instrument &i,trading_api::AsyncStatus) {}
    virtual void on_event(const trading_api::Account &a, trading_api::AsyncStatus) {}
    virtual void on_event(const trading_api::Instrument &i, trading_api::SubscriptionType subscription_type) {
        std::cout << i.get_id() << " ";
        trading_api::TickData tk;
        i.get_exchange().get_last_ticker(i, tk);
        std::cout << tk << std::endl;
    }
    virtual void on_event(const trading_api::Order &order,const trading_api::Order::Report &report) {}
    virtual void on_event(const trading_api::Order &order, const trading_api::Fill &fill) {}

};

int main() {

    trading_api::TickData dt;
    trading_api::MarketEvent ev(dt);
    std::cout << ev << std::endl;

    trading_api::Log log(std::make_shared<trading_api::BasicLog>(std::cout, trading_api::Log::Serverity::trace));
    auto context = std::make_shared<trading_api::BasicExchangeContext>("Binance",log);

    trading_api::Config exchange_config ( {
            {"server", std::string("testnet")}
    });

    trading_api::Config api_key ( {
            {"api_name",std::string("3cfa2991082a67c0d3e20318b172d6badc07c1169ace1d83dae410b43b34f8d5")},
            {"secret",std::string("13795a26cf2e407347d9a3a3c3283122f63cfe8dd3f214708e80c4cf002845bc")},
    });

    context->init(std::make_unique<BinanceExchange>(), exchange_config);
    context->set_api_key("master", api_key);


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

    trading_api::SharedState<std::vector<trading_api::Account> > astate({},[](auto &res){
        for (const trading_api::Account &acc: res) {
            std::cout<<"Account:" << acc.get_id() << std::endl;
            std::cout<<"Label:" << acc.get_label() << std::endl;
            std::cout<<"Exchange:" << acc.get_exchange().get_label() << std::endl;
            auto info = acc.get_info();
            std::cout<<"Balance:" << info.balance << std::endl;
            std::cout<<"Blocked:" << info.blocked<< std::endl;
            std::cout<<"Currency:" << info.currency<< std::endl;
            std::cout << "-----" << std::endl;
        }
    });

    context->query_accounts("master", "*", "main", [&, astate](trading_api::Account acc){
        std::lock_guard _(astate);
        astate->push_back(acc);
    });

    astate.wait();


    trading_api::Instrument bitcoin;

    {
        trading_api::SharedState<trading_api::Instrument *> state(&bitcoin);
        context->query_instruments("BTCUSDT", "btc", [state](const auto &i){
            **state = i;
        });
        state.wait();
    }



    EventTarget evt;

//    context->subscribe(&evt, trading_api::SubscriptionType::ticker, bitcoin);

    std::cout << std::cin.get();

 //   context->disconnect(&evt);

    std::this_thread::sleep_for(std::chrono::seconds(1));



}
