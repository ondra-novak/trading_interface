#pragma once

#include "coro_support.h"
#include "fill.h"
#include "order.h"
#include "tickdata.h"
#include "orderbook.h"
#include "timer.h"
#include "config.h"
#include "log.h"
#include "market_event.h"
#include <span>

namespace trading_api {



template<typename T>
concept BinarySerializable = (
            (std::is_trivially_copy_constructible_v<T> && std::is_standard_layout_v<T>)
            || (std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>));


template<BinarySerializable T>
std::string_view serialize_binary(const T &data) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return std::string_view(data);
    } else {
        return std::string_view(reinterpret_cast<const char *>(&data), sizeof(data));
    }
}

template<BinarySerializable T>
std::optional<T> deserialize_binary(std::string_view binary) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return T(binary);
    } else {
        std::optional<T> ret;
        if (binary.size() == sizeof(T)) {
            ret.emplace(*reinterpret_cast<const T *>(binary.data()));
        }
        return ret;
    }
}

template<BinarySerializable T>
T deserialize_binary(std::string_view binary, const T &defval) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return T(binary);
    } else if (sizeof(T) != binary.size()) {
        return defval;
    } else {
        return T(*reinterpret_cast<const T *>(binary.data()));
    }
}




class IContext {
public:

    virtual ~IContext() = default;

    ///request update account
    virtual void update_account(const Account &a, CompletionCB complete_ptr) = 0;

    ///request update instrument
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) = 0;

    virtual std::span<const Account> get_accounts() const = 0;

    virtual std::span<const Instrument> get_instruments() const = 0;

    virtual const Config &get_config() const = 0;

    ///Retrieves current time
    virtual Timestamp get_event_time() const = 0;

    ///Sets time, which calls function wrapped into runnable object. It is still bound to strategy
    virtual void set_timer(Timestamp at, TimerEventCB fnptr = {}, TimerID id = 0) = 0;

    ///Cancel timer
    virtual bool clear_timer(TimerID id) = 0;

    ///Place an order
    virtual Order place(const Instrument &instrument, const Account &account, const Order::Setup &setup) = 0;

    ///Creates and bind an order to an instrument
    /**
     * You can use replace() function to place the order with new setup. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    virtual Order bind_order(const Instrument &instrument, const Account &account) = 0;

    ///Cancel given order
    virtual void cancel(const Order &order) = 0;

    ///Replace order
    /**
     * @param order order to replace
     * @param setup new setup of the order
     * @return new order
     *
     * @note if replace partially filled order, filled amount is perserved
     * @note replace can fail, if exchange cannot garanteed to replace order
     * without avoiding double execution. In this case, old order is canceled
     * new order is rejected
     */
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) = 0;

    ///Retrieve recent fills
    virtual Fills get_fills(std::size_t limit, std::string_view filter = {}) const = 0;

    ///Retrieve recent fills
    virtual Fills get_fills(Timestamp tp, std::string_view filter = {}) const = 0;

    ///set persistent variable
    /**
     * It is expected, that key-value database is involved.
     * @param var_name variable name - any binary content is allowed
     * @param value value as string - any binary content is allowed
     */
    virtual void set_var(std::string_view var_name, std::string_view value) = 0;

    virtual std::string get_var(std::string_view var_name) const = 0;

    virtual void enum_vars(std::string_view prefix,  Function<void(std::string_view, std::string_view)> &fn) const = 0;

    virtual void enum_vars(std::string_view start, std::string_view end,  Function<void(std::string_view, std::string_view)> &fn) const = 0;


    ///Deletes persistently stored variable
    /**
     * @param var_name name of variable.
     *
     * @note Deleted variable releases some space in database and it
     * also stops appearing in list of variables supplied during init()
     */
    virtual void unset_var(std::string_view var_name) = 0;

    ///allocate equity on given account
    virtual void allocate(const Account &a, double equity) = 0;

    ///subscribe market events
    virtual void subscribe(SubscriptionType type, const Instrument &i) = 0;

    ///unsubscribe instrument
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) = 0;

    virtual void subscribe_channel(Channel channel_id) = 0;

    virtual void unsubscribe_channel(Channel channel_id) = 0;


    virtual Log get_logger() const = 0;

};

class NullContext: public IContext {
public:
    [[noreturn]] void throw_error() const {throw std::runtime_error("Used uninitialized context");}
    virtual std::span<const Account> get_accounts() const  override {throw_error();};
    virtual std::span<const Instrument> get_instruments() const override {throw_error();};
    virtual const Config &get_config() const override {throw_error();};
    virtual void set_timer(Timestamp , TimerEventCB, TimerID ) override {throw_error();}
    virtual void cancel(const Order &) override {throw_error();}
    virtual Order replace(const Order &, const Order::Setup &, bool) override {throw_error();}
    virtual void update_instrument(const Instrument &, CompletionCB) override {throw_error();}
    virtual Fills get_fills(std::size_t , std::string_view) const override {throw_error();}
    virtual Fills get_fills(Timestamp , std::string_view ) const override {throw_error();}
    virtual Timestamp get_event_time() const override{throw_error();}
    virtual bool clear_timer(TimerID ) override{throw_error();}
    virtual void update_account(const Account &, CompletionCB) override{throw_error();}
    virtual Order bind_order(const Instrument &,  const Account &)override{throw_error();}
    virtual Order place(const Instrument &, const Account &, const Order::Setup &) override{throw_error();}
    virtual void allocate(const Account &, double ) override {throw_error();}
    virtual void set_var(std::string_view , std::string_view ) override{throw_error();};
    virtual void unset_var(std::string_view ) override{throw_error();};
    virtual void subscribe(SubscriptionType , const Instrument &) override {throw_error();}
    virtual void unsubscribe(SubscriptionType , const Instrument &) override {throw_error();}
    virtual std::string get_var(std::string_view ) const override  {throw_error();}
    virtual void enum_vars(std::string_view ,  Function<void(std::string_view, std::string_view)> &) const override {throw_error();}
    virtual void enum_vars(std::string_view , std::string_view ,  Function<void(std::string_view, std::string_view)> &) const override  {throw_error();}
    virtual Log get_logger() const override {throw_error();}
    constexpr virtual ~NullContext() {}

};


class Context {
public:


    static constexpr NullContext null_context = {};

    Context():_ptr(const_cast<IContext *>(static_cast<const IContext *>(&null_context))) {}

    Context(IContext *ctx):_ptr(ctx) {}

    std::span<const Account> get_accounts() const {return _ptr->get_accounts();}

    std::span<const Instrument> get_instruments() const {return _ptr->get_instruments();}

    const Config &get_config() const {return _ptr->get_config();}



    ///store value under a variable name in a persistent storage
    /**
     * Key-Value database is expected.
     *
     * @param key a string key under which the value will be stored
     * @param value a string value to be stored
     *
     * @note There is no limitation for key and value. You can use binary
     * content for both key and value. There is also no limit of size. However
     * keep in mind, that to store longer keys and values can have impact on
     * performance.
     *
     * @note store operation is asynchronous. Calling get() immediately
     * after set() can still return previous value
     */
    void set(std::string_view key, std::string_view value) {
        _ptr->set_var(key, value);
    }

    ///store value under a variable name in a persistent storage
    /**
     * Key-Value database is expected.
     *
     * @param key a string key under which the value will be stored
     * @param value any binary serializable value, which includes all basic types, enums, and classes
     * with trivial copy constructors (must not contain pointer)
     *
     * @note store operation is asynchronous.Calling get() immediately
     * after set() can still return previous value
     */
    template<BinarySerializable T>
    void set(std::string_view key, const T &value) {
        _ptr->set_var(key,serialize_binary(value));
    }

    ///retrieve stored value from database
    /**
     * @param varname name of variable
     * @return content. If the variable doesn't exist, returns empty string
     */
    std::string get(std::string_view varname) const{
        return _ptr->get_var(varname);
    }
    ///retrieve stored value
    /**
     * @param key variable name
     * @param default_value default value in case that variable is undefined
     * @return
     */
    template<BinarySerializable T>
    T get(std::string_view key, const T &default_value) const{
        return deserialize_binary(_ptr->get_var(key), default_value);
    }

    ///retrieve multiple variables
    /**
     * @param prefix specifies prefix, function retrieves all variables starting
     * by this prefix
     * @param fn a function, which receives pair of key=value
     */
    template<std::invocable<std::string_view, std::string_view> Fn>
    void mget(std::string_view prefix, Fn &&fn) const{
        Function<void(std::string_view, std::string_view)> ffn(std::forward<Fn>(fn));
        _ptr->enum_vars(prefix, ffn);
    }

    ///retrieve multiple variables
    /**
     *
     * @tparam T type of value
     * @param prefix specifies prefix, function retrieves all variables starting
     * by this prefix
     * @param fn a function, which receives pair of key=value
     */
    template<BinarySerializable T, std::invocable<std::string_view, T> Fn>
    void mget(std::string_view prefix, Fn &&fn) const{

        Function<void(std::string_view, std::string_view)> ffn([&](auto a, auto b){
            auto opt = deserialize_binary<T>(b);
            if (opt.has_value()) fn(a,*opt);
        });
        _ptr->enum_vars(prefix, ffn);
    }
    ///retrieve multiple variables
    /**
     * @param from begining name
     * @param end ending name (inclusive)
     * @param fn a function, which receives pair of key=value
     * @note variables are ordered binary
     */
    template<std::invocable<std::string_view, std::string_view> Fn>
    void mget(std::string_view from, std::string_view to, Fn &&fn) const{
        Function<void(std::string_view, std::string_view)> ffn(std::forward<Fn>(fn));
        return _ptr->enum_vars(from, to, ffn);
    }

    ///retrieve multiple variables
    /**
     *
     * @tparam T type of value
     * @param from begining name
     * @param end ending name (inclusive)
     * @param fn a function, which receives pair of key=value
     */
    template<BinarySerializable T, std::invocable<std::string_view, T> Fn>
    void mget(std::string_view from, std::string_view to, Fn &&fn) const{

        Function<void(std::string_view, std::string_view)> ffn([&](auto a, auto b){
            auto opt = deserialize_binary<T>(b);
            if (opt.has_value()) fn(a,*opt);
        });

        return _ptr->enum_vars(from, to, ffn);
    }



    ///request update account
    /**
     * @param a account to update
     * @param fn function called when update is complete.
     *
     * @note In most cases, you don't need to update account when fill.
     */
    template<std::invocable<> Fn>
    void update_account(const Account &a, Fn &&fn) {
        _ptr->update_account(a, CompletionCB(std::forward<Fn>(fn)));
    }

    ///request update account - use coroutines
    /**
     * @param a account to update
     * @return awaiter object, which must be co_awaited to complete operation.
     *
     * @exception canceled when operation is canceled because the strategy is
     * shut down
     */
    completion_awaiter update_account(const Account &a) {
        return [&](auto fn){
            _ptr->update_account(a, std::move(fn));
        };
    }

    ///request update instrument
    /**
     * @param i instrument to update. The function refresh instrument features
     *  from the exchange
     * @param fn function which is called when update is complete
     *
     * @note you don't need to update instrument often.
     */
    template<std::invocable<> Fn>
    void update_instrument(const Instrument &i, Fn &&fn) {
        _ptr->update_instrument(i, CompletionCB(std::forward<Fn>(fn)));
    }

    ///request update instrument - use coroutine
    /**
     * @param i instrument to update. The function refresh instrument features
     *  from the exchange
     * @return awaiter object, which must be co_awaited to complete operation.
     *
     * @exception canceled when operation is canceled because the strategy is
     * shut down
     *
     * @note you don't need to update instrument often.
     */
    completion_awaiter update_instrument(const Instrument &i) {
        return [&](auto fn){
            _ptr->update_instrument(i, std::move(fn));
        };
    }
    ///Retrieves time of current event
    /**
     * @return time of current event. This contains snapshot of real time clock,
     * when current event was started. It returns same value for current
     * event regardless on how long the event is executed.
     *
     * During backtest, this value doesn't contain real time value, but
     * backtest's time value. When strategy needs source of the time for calculation
     * it always should use event time.
     *
     * @note if coroutine awaits for asynchronous operation, calling
     * of this function after co_await can return different time then
     * value returned before co_await.
     */
    Timestamp get_event_time() const {return _ptr->get_event_time();}
    ///Sets timer, which triggers event, on strategy interface
    /**
     * @param at time point when timer is triggered
     * @param id optional identifier. It can be 0, but you will unable to detect, which timer triggered.
     * The ID also allows you to cancel timer. ID don't need to be unique
     *
     */
    void set_timer(Timestamp at, TimerID id = 0) {return _ptr->set_timer(at, {}, id);}

    ///Sets timer, which triggers event, on strategy interface
    /**
     * @param dur duration
     * @param id optional identifier. It can be 0, but you will unable to detect, which timer triggered.
     * The ID also allows you to cancel timer. ID don't need to be unique
     */
    template<typename A, typename B>
    void set_timer(std::chrono::duration<A, B> dur, TimerID id = 0) {
        return _ptr->set_timer(get_event_time() + dur, {}, id);
    }

    ///Schedule a function call to given time
    /**
     * @param at time point
     * @param fn function
     * @param id identifier
     *
     * @note this timer doesn't trigger on_timer
     */
    template<std::invocable<> Fn>
    void set_timer(Timestamp at, Fn &&fn, TimerID id = 0) {
        return _ptr->set_timer(at, make_runnable(std::forward<Fn>(fn)), id);
    }

    ///Schedule a function call to given time
    /**
     * @param dur duration
     * @param fn function
     * @param id identifier
     *
     * @note this timer doesn't trigger on_timer
     */
    template<typename A, typename B, std::invocable<> Fn>
    void set_timer(std::chrono::duration<A, B> dur, Fn &&fn, TimerID id = 0) {
        return _ptr->set_timer(get_event_time() + dur, make_runnable(std::forward<Fn>(fn)), id);
    }

    ///sleep until timestamp reached - coroutines
    /**
     * You need to co_await result to sleep
     * @param at time point
     * @param id optional id
     */
    completion_awaiter sleep_until(Timestamp &at, TimerID id = 0) {
        return [&](auto fn) {
            _ptr->set_timer(at, [fn = std::move(fn)]{
                AsyncStatus st;
                  fn(st);
            },id);
        };
    }
    ///sleep until timestamp reached - coroutines
    /**
     * You need to co_await result to sleep
     * @param dur duration
     * @param id optional id
     */
    template<typename A, typename B>
    completion_awaiter sleep_for(std::chrono::duration<A, B> dur, TimerID id = 0) {
        return sleep_until(get_event_time()+dur);
    }

    ///Cancel timer
    bool clear_timer(TimerID id) {return _ptr->clear_timer(id);}
    ///Place an order
    Order place(const Instrument &instrument, const Account &account, const Order::Setup &setup) {
        return _ptr->place(instrument, account, setup);
    }
    ///Creates an order, which is asociated with an instrument, but it is not placed
    /**
     * You can use replace() function to place the order with new setup. This
     * allows to track single order without need to know, whether order is actually
     * placed or not
     *
     * @param instrument associated instrument
     * @return dummy order (can be replaced)
     */
    Order bind_order(const Instrument &instrument, const Account &account) {return _ptr->bind_order(instrument, account);}
    ///Cancel given order
    void cancel(const Order &order) {_ptr->cancel(order);}

    ///Replace order
    /**
     * @param order order to replace.
     * @param setup new setup of the order
     * @param amend try to amend current order - the exchange just modifies amount,
     * limit or stop price if order is pending (waiting for trigger). So you can
     * amend only order with same side, instrument, etc. If amend is not possible,
     * because above rules, new order is discarded and original order is untouched.
     * If exchange doesn't support amend, the service provider can simulate this
     * feature. In all cases, filled amount is transfered to the new order.
     *
     * @return new order
     *
     * @note if replace partially filled order, filled amount is perserved
     * @note replace can fail, if exchange cannot garanteed to replace order
     * without avoiding double execution. In this case, old order is canceled
     * new order is rejected
     *
     * @note if order is pending, you probably can replace with order of same
     * side and behavior. If order is done, it can be replaced with any
     * order (it only associates with order's original instrument)
     */
    Order replace(const Order &order, const Order::Setup &setup, bool amend) {
        return _ptr->replace(order, setup, amend);
    }

    ///Retrieve last fills
    Fills get_fills(std::size_t limit, std::string_view filter = {}) {
        return _ptr->get_fills(limit, filter);
    }

    Fills get_fills(Timestamp tp, std::string_view filter = {}) {
        return _ptr->get_fills(tp, filter);
    }

    ///allocate equity for current strategy
    /**
     * If equity is shared between multiple strategies,
     * it helps to visualise, how much equity is unallocated or overallocated
     * @param a account
     * @param equity amount equity;
     *
     * @note not persistent.
     *
     */
    void allocate(const Account &a, double equity) {
         _ptr->allocate(a, equity);
    }

    ///Subscribe market data
    /**
     * @param type type of subscription (ticker, orderbook)
     * @param i instrument instance
     *
     * Events are generated as they are received from market stream (as fastest as possible)
     *
     */
    void subscribe(SubscriptionType type, const Instrument &i) {
        _ptr->subscribe(type, i);
    }



    ///Unsubscribe market data
    /**
     * @param type type of subscription
     * @param i instrument
     *
     * @note if there is no subscription, it does nothing
     *
     * @note your strategy doesn't need to call unsubscribe in the destructor.
     *
     */
    void unsubscribe(SubscriptionType type, const Instrument &i) {
        _ptr->unsubscribe(type, i);
    }

    ///Retrieve logger object (for logging and output)
    Log get_logger() {return _ptr->get_logger();}


protected:
    IContext *_ptr;
};


}

