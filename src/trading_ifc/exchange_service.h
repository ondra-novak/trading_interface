#pragma once

#include "exchange_context.h"

#include "config_desc.h"
namespace trading_api {



///Implements exchange features - exchange implementation
class IExchangeService {
public:

    virtual ~IExchangeService() = default;

    ///Called to init exchange service
    /**
     * @param context exchange context
     * @param config optional configuration - probably you will need to pass
     * an API key through this
     */
    virtual void init(ExchangeContext context, const StrategyConfig &config) = 0;

    ///Retrieve configuration schema for configuration needed to initialize the instance
    /**
     * this function can be called without init();
     * @return configuration schema
     */
    virtual StrategyConfigSchema get_config_schema() const = 0;

    ///Subscribe instrument
    /** Subscribe this object to market data for given instrument
     *
     * @param type type of subscription
     * @param i instrument
     *
     * Market data are passed through income_data()
     *
     * @note multiple subscriptions should be ignored
     * @note called under lock
     */
    virtual void subscribe(SubscriptionType type, const Instrument &i) = 0;
    ///Unsubscribe instrument
    /** Unsubscribe this object from market data for given instrument
     *
     * @param type type of subscription
     * @param i instrument
     *
     * @note multiple unsubscptions should be ignored
     * @note called under lock
     */
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) = 0;
    ///Request update account object
    /**
     * @param a account to update
     * @note when update is complete, call object_updated
     * @note called under lock
     */
    virtual void update_account(const Account &a) = 0;
    ///Request update account object
    /**
     * @param i instrument to update
     * @note when update is complete, call object_updated
     * @note called under lock
     */
    virtual void update_instrument(const Instrument &i) = 0;
    ///Place orders on exchange
    /**
     * @param orders list of orders
     * @note order report is passed through order_fill or order_restore
     * @note called under lock
     */
    virtual void batch_place(std::span<Order> orders) = 0;

    ///Cancel orders
    /**
     * @param orders list of orders to cancel
     * @note canceled orders should receive status canceled after successfuly canceled
     *
     */
    virtual void batch_cancel(std::span<Order> orders) = 0;

    ///Create accounts
    /**
     * @param account_idents list of account identifiers (exchange specifics)
     * @param cb function which receives accounts per identifier. If
     * account doesn't exists, it returns null account object
     *
     * @note each account should be created only once. Repeatedly called this function
     * with same identifier should return the same account.
     */
    virtual void create_accounts(std::vector<std::string> account_idents, Function<void(std::vector<Account>)> cb) = 0;

    ///Creates instruments available for current account
    /**
     * @param instruments idenfiers of instruments
     * @param accunt account
     * @param cb function which receives instruments per identifier. If
     * instrument doesn't exists, it returns null account object
     *
     * @note each instrument should be created only once. Repeatedly called this function
     * with same identifier should return the same instrument.
     */
    virtual void create_instruments(std::vector<std::string> instruments, Account accunt, Function<void(std::vector<Account>)> cb) = 0;


    ///Get exchange human readable name
    virtual std::string get_name() const = 0;
    ///Get exchange unique identifier
    virtual std::string get_id() const  = 0;

    ///Get exchange's icon (optional)
    virtual std::optional<IExchange::Icon> get_icon() const = 0;

    ///Create order base on request
    /**
     * Implementation of the order is internal issue of the exchange service
     * @param instrument associated instrument
     * @param account associated account
     * @param setup order configuration
     * @return created order.
     * @note if order cannot be created, the function should create error order,
     * which is returned in discarded state.
     */
    virtual Order create_order(const Instrument &instrument, const Account &account, const Order::Setup &setup) = 0;


    ///Create order which replaces other order
    /**
     * @param replace order to replace
     * @param setup setup of new order
     * @param amend set true if amend is required. In this case,
     * order is not canceled, but setup is used to change its properties. If
     * this is false, it creates atomic cancel+place operation. Both types of
     * operation can fail and it is also possible, that failure causes that
     * original order will be canceled but new order is not placed
     * @return created order.
     * @note if order cannot be created, the function should create error order,
     * which is returned in discarded state.
     */
    virtual Order create_order_replace(const Order &replace, const Order::Setup &setup, bool amend) = 0;

    ///Restore order which has been stored in permanent storage
    /**
     * @param context an arbitrary pointer passed to the function. This pointer is passed
     * to function order_restored.
     * @param orders list of orders
     *
     * The binary representation of the order can be obtained by function to_binary(). The
     * format of binary representation is not specified. It can be just order identifier
     * from the exchange. The function must use connection to the exchange to
     * retrieve informations about the order, its status and fills and
     * call following functions
     *
     *   - order_restore() - when order is created
     *   - order_fill() - for all fills
     *   - order_state_change - for final order status
     *
     *   This order must be kept for single order. However it is possible to
     *   mix events from different orders. For example you can call order_restore
     *   for each order, then order_fill for each order and fill, and then order_state_change
     *   for each order. Or you can call this sequence per signle order.
     *
     */
    virtual void restore_orders(void *context, std::span<SerializedOrder> orders) = 0;

    ///Applies report to order object
    /**
     * Order object is owned by strategy and can be accessed anytime during processing,
     * because there is no locking scheme. This means, that new order state must
     * be applied synchronously with the strategy. This function is called in
     * strategy thread before the order is passed to the strategy and allows to
     * apply report to the order's internal state.
     *
     * @param order subject
     * @param report report to apply
     *
     * @note the order must be created by this exchange, otherwise function can throw exception
     */
    virtual void order_apply_report(const Order &order, const Order::Report &report)  =0;


    ///Applies fill to order object
    /**
     * Because order object can be modified only synchronously with the strategy,
     * this function must be called in strategy thread.
     *
     * @param order order
     * @param fill fill
     *
     * @note the order must be created by this exchange, otherwise function can throw exception
     *
     */
    virtual void order_apply_fill(const Order &order, const Fill &fill) = 0;



};

}
