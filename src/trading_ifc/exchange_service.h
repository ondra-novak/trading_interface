#pragma once

#include "exchange_context.h"


#include "config_desc.h"
namespace trading_api {



///Implements exchange features - exchange implementation
class IExchangeService {
public:


    virtual ~IExchangeService() = default;

    ///Retrieve configuration schema for configuration needed to initialize the instance
    /**
     * this function can be called without init();
     * @return configuration schema
     */
    virtual ConfigSchema get_exchange_config_schema() const = 0;

    ///Retrieve configuration schema to configure api key fields
    /**
     * this function can be called without init()
     * @return
     */
    virtual ConfigSchema get_api_key_config_schema() const = 0;

    ///Called to init exchange service
    /**
     * @param context exchange context
     * @param config optional configuration - probably you will need to pass
     * an API key through this
     */
    virtual void init(ExchangeContext context, const Config &exchange_config) = 0;


    ///Add new api key.
    /**
     *
     * @param name an unique name which identifies this api key for other calls
     * @param api key configuration
     * @note multiple calls of this function with same name changes the api key.

     */
    virtual void set_api_key(std::string_view name, const Config &api_key_config) = 0;

    ///Deletes api key
    /**
     * @param name name of api key
     *
     * @note accounts associated with current api key are no longer useable
     */
    virtual void unset_api_key(std::string_view name) = 0;

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


    ///Query for instruments
    /**
     * @param query query string - this is exchange specific
     * @param label defines label for populated instruments. It also works
     *  as negative query, because causes removal of instruments populated under
     *  different label from the result To repopulate instrument under
     *  different label you need to destroy all copies of instrument instance
     * @param cb callback function which receives result for each instrument. There is
     *  'end' callback, however the closure of the function is eventually destroyed at
     *  the end.
     *
     * @note function is asynchronous
     */
    virtual void query_instruments(std::string_view query, std::string_view label, Function<void(Instrument)> cb) = 0;



    ///Query for accounts
    /**
     * @param api_key_name identifier of registered api_key
     * @param query query string - this is exchange specific.
     * @param label defines label for populated instruments. It also works
     *  as negative query, because causes removal of accounts populated under
     *  different label from the result To repopulate account under
     *  different label you need to destroy all copies of instrument instance
     * @param cb callback function which receives result for each instrument. There is
     *  'end' callback, however the closure of the function is eventually destroyed at
     *  the end.
     *
     * @note function is asynchronous
     */
    virtual void query_accounts(std::string_view api_key_name, std::string_view query, std::string_view label, Function<void(Account)> cb) = 0;

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

///Export the strategy, so the strategy can be loaded by the loader
/**
 * @param class_name name of the strategy (class name). The strategy is
 * registered under its name
 */
#define EXPORT_EXCHANGE(class_name) ::trading_api::IModule::Factory<IExchangeService> exchange_reg_##class_name(#class_name, std::in_place_type<class_name>)

///Export the strategy, but specify other name
/**
 * @param class_name class name of strategy
 * @param export_name exported name
 */
#define EXPORT_EXCHANGE_AS(class_name, export_name) ::trading_api::IModule::Factory<IExchangeService> exchange_reg_##class_name(export_name, std::in_place_type<class_name>)

}
