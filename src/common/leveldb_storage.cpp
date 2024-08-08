#include "leveldb_storage.h"
#include "../trading_ifc/mq.h"

#include <unordered_map>

namespace trading_api {


void LvlDBStorage::begin_transaction()
{
    _txlevel++;
}

void LvlDBStorage::put_var(std::string_view name, std::string_view value)
{
    _batch.Put(build_key(RecordType::variable, name), {value.data(), value.size()});
    auto_commit();
}

const std::string &LvlDBStorage::build_key(std::string &buffer, RecordType type, const std::string_view &rest) const
{
    buffer.clear();
    buffer.append(_key_pfx);
    buffer.push_back(type);
    buffer.append(rest);
    return buffer;
}



const std::string &LvlDBStorage::build_key(RecordType type, const std::string_view &rest) 
{
    return build_key(_buffer, type, rest);
}

const std::string &LvlDBStorage::build_fill_key(Timestamp tm, std::string_view id)
{
    return build_fill_key(_buffer, tm, id);
}

const std::string &LvlDBStorage::build_fill_key(std::string &buffer, Timestamp tm, std::string_view id) const
{
    build_key(buffer, RecordType::fill, "");
    auto tmn = tm.time_since_epoch().count();
    for (int i = 0; i < 8; ++i) {
        char c = static_cast<char>(tmn >> ((7-i)*8));
        buffer.push_back(c);
    }
    buffer.append(id);
    return buffer;
}

void LvlDBStorage::erase_var(std::string_view name)
{
    _batch.Delete(build_key(RecordType::variable, name));
}

void LvlDBStorage::put_order(const Order &ord)
{
    auto b = ord.to_binary();
    if (ord.done()) {
        _batch.Delete(build_key(RecordType::order,b.order_id));
    } else {
        _batch.Put(build_key(RecordType::order, b.order_id), b.order_content);
    }
    auto_commit();
}


using FillRecord = MessageFormat<
    Timestamp, //time
    std::string, //id
    std::string,// label;
    std::string,// pos_id;
    IInstrument::Type, //instrument.type;
    double,// multiplier;
    std::string, // instrument_id;
    std::string, // price_unit;
    Side,// side;
    double,// price;
    double,// amount;
    double// fees
>;

void LvlDBStorage::put_fill(const Fill &fill)
{
    std::string v = FillRecord::compose(fill.time, fill.id, fill.label, fill.pos_id,
                    fill.instrument.type, fill.instrument.multiplier,
                    fill.instrument.instrument_id, fill.instrument.price_unit,
                    fill.side, fill.price, fill.amount, fill.fees);    
    _batch.Put(build_fill_key(fill.time, fill.id), v);
    auto_commit();
}

void LvlDBStorage::commit()
{
    if (--_txlevel <= 0) {
        if (!_batch_rollback) {
            leveldb::Status s = _db->Write(_write_opts, &_batch);
            if (!s.ok()) throw s;
        }
        _txlevel = 0;
        _batch.Clear();
        _batch_rollback = false;
    }
}

void LvlDBStorage::rollback() {
    _batch_rollback = true;
    commit();
}

bool LvlDBStorage::is_duplicate_fill(const Fill &fill) const
{
    std::string k;
    build_fill_key(k, fill.time, fill.id);
    auto s = _db->Get({},k,&k);
    return s.ok();
}

static Fill restore_fill(decltype(FillRecord::parse("")) &&parsed) {  
        return {
                std::move(std::get<0>(parsed)),
                std::move(std::get<1>(parsed)), 
                std::move(std::get<2>(parsed)),
                std::move(std::get<3>(parsed)),
                {
                    std::move(std::get<4>(parsed)),
                    std::move(std::get<5>(parsed)),
                    std::move(std::get<6>(parsed)),
                    std::move(std::get<7>(parsed)),
                },
                std::move(std::get<8>(parsed)),
                std::move(std::get<9>(parsed)), 
                std::move(std::get<10>(parsed)),
                std::move(std::get<11>(parsed))
            };
}

Fills LvlDBStorage::load_fills(std::size_t limit, std::string_view filter) const
{
    Fills fills;
    std::string k;
    build_fill_key(k, std::chrono::system_clock::time_point::max(), {});
    std::unique_ptr<leveldb::Iterator> iter(_db->NewIterator({}));
    iter->Seek(k);
    if (!iter->Valid()) iter->SeekToLast();
    build_key(k, RecordType::fill,{});
    while (limit> 0 && iter->Valid() && key_match_prefix(k, iter->key())) {
        auto parsed = FillRecord::parse(extract_slice(iter->value()));
        const std::string &label = std::get<2>(parsed);
        if (filter.empty() || std::string_view(label).substr(0, filter.size()) == filter) {
            --limit;
            fills.push_back(restore_fill(std::move(parsed)));
        }
        iter->Prev();
    }
    return fills;
}

Fills LvlDBStorage::load_fills(Timestamp limit, std::string_view filter) const
{
    Fills fills;
    std::string k;
    std::unique_ptr<leveldb::Iterator> iter(_db->NewIterator({}));
    iter->Seek(build_fill_key(k, limit, {}));
    build_key(k, RecordType::fill,{});
    while (iter->Valid() && key_match_prefix(k, iter->key())) {
        auto parsed = FillRecord::parse(extract_slice(iter->value()));
        const std::string &label = std::get<2>(parsed);
        if (filter.empty() || std::string_view(label).substr(0, filter.size()) == filter) {
            fills.push_back(restore_fill(std::move(parsed)));
        }
        iter->Next();
    }
    std::reverse(fills.begin(), fills.end());
    return fills;

}

std::vector<SerializedOrder> LvlDBStorage::load_open_orders() const
{
    std::vector<SerializedOrder> ret;
    std::unique_ptr<leveldb::Iterator> iter (_db->NewIterator({}));
    std::string s;
    iter->Seek(build_key(s,RecordType::order,""));
    while (iter->Valid() && key_match_prefix(s,iter->key()))  {
        ret.push_back({std::string(remove_key_prefix(iter->key())), std::string(extract_slice(iter->value()))});
        iter->Next();
    }
    return ret;
}

std::string LvlDBStorage::get_var(std::string_view var_name) const
{
    std::string s;
    build_key(s, RecordType::variable, var_name);
    if (_db->Get({},s,&s).ok()) return s;
    return {};
}

void LvlDBStorage::enum_vars(std::string_view start, std::string_view end, Function<void(std::string_view, std::string_view)> &fn) const
{
    std::string b;
    std::string e;
    build_key(b, RecordType::variable, start);
    build_key(e, RecordType::variable, end);
    leveldb::Slice bs(b);
    leveldb::Slice es(e);
    std::unique_ptr<leveldb::Iterator> iter(_db->NewIterator({}));
    iter->Seek(bs);
    while (iter->Valid()) {
        auto ks = iter->key();
        if (ks.compare(es) > 0) break;
        auto vs = iter->value();
        fn(remove_key_prefix(ks), extract_slice(vs));
        iter->Next();
    }
}

void LvlDBStorage::enum_vars(std::string_view prefix, Function<void(std::string_view, std::string_view)> &fn) const
{
    std::string pfx;
    std::unique_ptr<leveldb::Iterator> iter(_db->NewIterator({}));
    iter->Seek(build_key(pfx, RecordType::variable, prefix));
    while (iter->Valid() && key_match_prefix(pfx, iter->key())) {
        auto ks = iter->key();
        auto vs = iter->value();
        fn(remove_key_prefix(ks), extract_slice(vs));
        iter->Next();
    }
}

struct PositionInfo {
    Side side;
    double pos;
    double sum;
    Fill last_fill = {};
    double fees = 0;
};

Positions LvlDBStorage::load_positions(std::string_view filter) const
{
    std::unordered_map<std::string, PositionInfo> positions;

    std::string k;
    std::unique_ptr<leveldb::Iterator> iter(_db->NewIterator({}));
    iter->Seek(build_key(k, RecordType::fill, {}));
    while (iter->Valid() && key_match_prefix(k, iter->key())) {
        Fill f = restore_fill(FillRecord::parse(extract_slice(iter->value())));
        auto &nfo = positions[f.pos_id];
        nfo.last_fill = f;
        double fp;
        double fa;
        if (f.instrument.type == Instrument::Type::inverted_contract) {
            fp = 1.0/f.price;
            fa = -static_cast<int>(f.side)*f.amount*f.instrument.multiplier;
        } else {
            fp = f.price;
            fa = static_cast<int>(f.side)*f.amount*f.instrument.multiplier;
        }
        if (f.side == nfo.side) {
            nfo.pos += f.amount;
            nfo.sum += f.amount*fp;
        } else {
            nfo.pos -= f.amount;
            if (nfo.pos <= 0) {
                nfo.sum = 0;
                nfo.pos = -nfo.pos;
                nfo.side = nfo.side;
            }
        }
        nfo.fees += f.fees;
    }

    Positions res;
    for (const auto &[id, info]: positions) {
        if (info.pos == 0) {
            continue;
        }
        std::string_view f= info.last_fill.label;
        if (filter.empty() || f.substr(0,filter.size()) == filter) {
            double open = info.sum/info.pos;
            double price = 
                info.last_fill.instrument.type == Instrument::Type::inverted_contract?1.0/open: open;
            res.push_back({
                info.last_fill.time,
                info.last_fill.id,
                info.last_fill.label,
                id,
                info.last_fill.instrument,
                info.side,
                price,
                info.pos,
                info.fees,     
            });
        }
    }

    return res; 
}

Trades LvlDBStorage::load_closed(Timestamp limit, std::string_view filter) const
{
    Trades res;
    std::unordered_map<std::string, PositionInfo> positions;

    std::string k;
    std::unique_ptr<leveldb::Iterator> iter(_db->NewIterator({}));
    iter->Seek(build_key(k, RecordType::fill, {}));
    while (iter->Valid() && key_match_prefix(k, iter->key())) {
        Fill f = restore_fill(FillRecord::parse(extract_slice(iter->value())));
        auto &nfo = positions[f.pos_id];
        nfo.last_fill = f;
    }
}

bool LvlDBStorage::key_match_prefix(const std::string_view & pfx, const leveldb::Slice & slice)
{
    std::string_view key (slice.data(), slice.size());
    return key.substr(0,pfx.size()) == pfx;
}


std::string_view LvlDBStorage::remove_key_prefix(const leveldb::Slice &slice) const
{
    return std::string_view(slice.data(), slice.size()).substr(_key_pfx.size()+1);
}
std::string_view LvlDBStorage::extract_slice(const leveldb::Slice &slice)
{
    return std::string_view(slice.data(), slice.size());
}
}