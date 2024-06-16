#include <trading_api.h>

using namespace trading_api;

class Example: public IStrategy {
public:
    virtual void on_account(Account a) override;
    virtual void on_ticker(Ticker tk) override;
    virtual PStrategy clone() override {return do_clone(this);}
    virtual void on_timer(TimerID id) override;
    virtual void on_init(Context ctx) override;
    virtual void on_instrument(Instrument i) override;
    virtual void on_fill(Order ord, Fill fill)
            override;
    virtual void on_order(Order ord) override;
};



void strategy_main(Module &m) {
    m.export_strategy<Example>("example");
}

inline void Example::on_account(Account a) {
}

inline void Example::on_ticker(Ticker tk) {
}

inline void Example::on_timer(TimerID id) {
}

inline void Example::on_init(Context ctx) {
}

inline void Example::on_instrument(Instrument i) {
}

inline void Example::on_fill(Order ord, Fill fill) {
}

inline void Example::on_order(Order ord) {
}
