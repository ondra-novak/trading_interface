#include "storage.h"
#include <leveldb/db.h>
#include <leveldb/write_batch.h>


namespace trading_api {
class LvlDBStorage: public IStorage {
public:

    class RecordType {
    public:
        enum Type: char {
            variable='V',
            order='O',
            fill='F'
        };
        constexpr RecordType(Type x):_val(x) {}
        constexpr RecordType(char c):_val(static_cast<Type>(c)) {}
        constexpr bool operator==(const RecordType &) const = default;
        std::string_view to_string() const;
        constexpr operator char() const {return static_cast<char>(_val);}
    protected:
        Type _val;
    };


    virtual void begin_transaction() override;
    virtual void put_var(std::string_view name, std::string_view value) override;
    virtual void erase_var(std::string_view name) override;
    virtual void put_order(const Order &ord) override;
    virtual void put_fill(const Fill &fill) override;
    virtual void commit() override;
    virtual void rollback() override;
    virtual bool is_duplicate_fill(const Fill &fill) const override;
    virtual Fills load_fills(std::size_t limit, std::string_view filter = {}) const override;
    virtual Fills load_fills(Timestamp limit, std::string_view filter = {}) const override;
    virtual std::vector<SerializedOrder> load_open_orders() const override;
    virtual std::string get_var(std::string_view var_name) const override;
    virtual void enum_vars(std::string_view start, std::string_view end,
            Function<void(std::string_view,std::string_view)> &fn) const override;
    virtual void enum_vars(std::string_view prefix,
            Function<void(std::string_view,std::string_view)> &fn) const override;
    virtual Positions load_positions(std::string_view filter = {}) const override;
    virtual Trades load_closed(Timestamp limit, std::string_view filter = {}) const override;

protected:
    std::shared_ptr<leveldb::DB> _db;
    std::string _key_pfx;
    leveldb::WriteBatch _batch;
    leveldb::WriteOptions _write_opts;
    unsigned int _txlevel;
    bool _batch_rollback = false;
    std::string _buffer;

    void auto_commit() {
        if (!_txlevel) commit();
    }
    const std::string &build_key(RecordType type, const std::string_view &rest);
    const std::string &build_fill_key(Timestamp tm, std::string_view id);
    const std::string &build_key(std::string &buff, RecordType type, const std::string_view &rest) const;
    const std::string &build_fill_key(std::string &buff, Timestamp tm, std::string_view id) const;
    static bool key_match_prefix(const std::string_view &pfx, const leveldb::Slice &slice);
    std::string_view remove_key_prefix(const leveldb::Slice &slice) const;
    static std::string_view extract_slice(const leveldb::Slice &slice);
};
}