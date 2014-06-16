/*
 * Copyright (c) 2011-2013 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_BLOCKCHAIN_FETCH_HISTORY_IPP
#define LIBBITCOIN_BLOCKCHAIN_FETCH_HISTORY_IPP

namespace libbitcoin {
    namespace chain {

const uint8_t* slice_begin(const leveldb::Slice& data_slice)
{
    return reinterpret_cast<const uint8_t*>(data_slice.data());
}

class point_iterator
{
public:
    typedef std::unique_ptr<leveldb::DB> database_ptr;

    point_iterator(database_ptr& db, const payment_address& address)
      : raw_address_(1 + short_hash_size)
    {
        set_raw_address(address);
        set_iterator(db, raw_address_);
    }
    ~point_iterator()
    {
        BITCOIN_ASSERT(it_->status().ok());
    }

    bool valid()
    {
        return it_->Valid() && it_->key().starts_with(slice(raw_address_));
    }

    void load()
    {
        // Deserialize
        load_checksum(it_->key());
        load_data(it_->value());
    }

    void next()
    {
        it_->Next();
    }

    uint64_t checksum() const
    {
        return checksum_;
    }

protected:
    virtual void load_data(leveldb::Slice data) = 0;

private:
    void set_raw_address(const payment_address& address)
    {
        BITCOIN_ASSERT(raw_address_.size() == 1 + short_hash_size);
        auto serial = make_serializer(raw_address_.begin());
        serial.write_byte(address.version());
        serial.write_short_hash(address.hash());
        BITCOIN_ASSERT(
            std::distance(raw_address_.begin(), serial.iterator()) ==
            1 + short_hash_size);
    }

    void set_iterator(database_ptr& db, const data_chunk& raw_address)
    {
        it_.reset(db->NewIterator(leveldb::ReadOptions()));
        it_->Seek(slice(raw_address));
    }

    void load_checksum(leveldb::Slice key)
    {
        const uint8_t* begin = slice_begin(key.data());
        const uint8_t* end = begin + key.size();
        BITCOIN_ASSERT(key.size() == 1 + short_hash_size + 8);
        auto deserial = make_deserializer(begin + 1 + short_hash_size, end);
        checksum_ = deserial.read_8_bytes();
        BITCOIN_ASSERT(deserial.iterator() == end);
    }

    leveldb_iterator it_;
    data_chunk raw_address_;

    uint64_t checksum_;
};

class outpoint_iterator
  : public point_iterator
{
public:
    outpoint_iterator(database_ptr& db, const payment_address& address)
      : point_iterator(db, address) {}

    const output_point& outpoint() const
    {
        return outpoint_;
    }
    uint64_t value() const
    {
        return value_;
    }
    uint32_t height() const
    {
        return height_;
    }

protected:
    void load_data(leveldb::Slice data)
    {
        const uint8_t* begin = slice_begin(data.data());
        const uint8_t* end = begin + data.size();
        BITCOIN_ASSERT(data.size() == 36 + 8 + 4);
        auto deserial = make_deserializer(begin, end);
        outpoint_.hash = deserial.read_hash();
        outpoint_.index = deserial.read_4_bytes();
        value_ = deserial.read_8_bytes();
        height_ = deserial.read_4_bytes();
        BITCOIN_ASSERT(deserial.iterator() == end);
    }

private:
    output_point outpoint_;
    uint64_t value_;
    uint32_t height_;
};

class inpoint_iterator
  : public point_iterator
{
public:
    inpoint_iterator(database_ptr& db, const payment_address& address)
      : point_iterator(db, address) {}

    input_point inpoint()
    {
        return inpoint_;
    }
    uint32_t height() const
    {
        return height_;
    }

protected:
    void load_data(leveldb::Slice data)
    {
        const uint8_t* begin = slice_begin(data.data());
        const uint8_t* end = begin + data.size();
        BITCOIN_ASSERT(data.size() == 36 + 4);
        auto deserial = make_deserializer(begin, end);
        inpoint_.hash = deserial.read_hash();
        inpoint_.index = deserial.read_4_bytes();
        height_ = deserial.read_4_bytes();
        BITCOIN_ASSERT(deserial.iterator() == end);
    }

private:
    input_point inpoint_;
    uint32_t height_ = max_index;
};

template <typename DatabasePtr>
fetch_history_t<DatabasePtr>::fetch_history_t(
    DatabasePtr& db_credit, DatabasePtr& db_debit)
  : db_credit_(db_credit), db_debit_(db_debit)
{
}

template <typename DatabasePtr>
blockchain::history_list fetch_history_t<DatabasePtr>::operator()(
    const payment_address& address, size_t from_height)
{
    struct spend_data
    {
        input_point point;
        uint32_t height;
    };
    typedef std::unordered_map<uint64_t, spend_data> spend_map;

    // First we create map of spends...
    spend_map spends;
    for (inpoint_iterator debit_it(db_debit_, address);
        debit_it.valid(); debit_it.next())
    {
        debit_it.load();
        uint64_t checksum = debit_it.checksum();
        spends.emplace(checksum,
            spend_data{debit_it.inpoint(), debit_it.height()});
    }
    // ... Then we load outputs.
    blockchain::history_list history;
    for (outpoint_iterator credit_it(db_credit_, address);
        credit_it.valid(); credit_it.next())
    {
        credit_it.load();
        // Row with no spend (yet)
        blockchain::history_row row{
            credit_it.outpoint(),
            credit_it.height(),
            credit_it.value(),
            input_point{null_hash, max_index},
            max_height
        };
        // Now search for the spend. If it exists,
        // then load and add it to the row.
        uint64_t checksum = credit_it.checksum();
        auto it = spends.find(checksum);
        const bool spend_exists = it != spends.end();
        if (spend_exists)
        {
            const spend_data& data = it->second;
            row.spend = data.point;
            row.spend_height = data.height;
            BITCOIN_ASSERT(row.spend_height >= row.output_height);
        }
        // Filter entries below the from_height.
        if (row.output_height >= from_height ||
            (spend_exists && row.spend_height >= from_height))
        {
            history.push_back(row);
        }
    }
    return history;
}

template <typename DatabasePtr>
fetch_history_t<DatabasePtr> fetch_history_functor(
    DatabasePtr& db_credit, DatabasePtr& db_debit)
{
    return fetch_history_t<DatabasePtr>(db_credit, db_debit);
}

    } // namespace chain
} // namespace libbitcoin

#endif

