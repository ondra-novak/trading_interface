#pragma once

#include "wrapper.h"
#include <iterator>
#include <bitset>

#include <string_view>
namespace trading_api {

class ILog {
public:

    enum class Serverity {
        trace,
        debug,
        info,
        warning,
        error,
        fatal
    };

    virtual void output(Serverity level, std::string_view msg) = 0;
    virtual Serverity get_min_level() const = 0;
    virtual ~ILog() = default;

    class Null;
};

template<typename T>
concept is_container = requires(T x) {
    {x.begin()}->std::input_iterator;
    {x.end()}->std::input_iterator;
    {x.begin() == x.end()};
};

template<typename T>
concept is_pair = requires(T x) {
    {x.first};
    {x.second};
};

class Log  {
public:

    // Vlastní buffer pro zápis do std::vector<char>
    class vector_streambuf : public std::streambuf {
    public:
        vector_streambuf(std::vector<char>& vec) : vec_(vec) {}

    protected:
        // Přetížená metoda pro zápis jednoho znaku
        virtual int_type overflow(int_type ch) override {
            if (ch != traits_type::eof()) {
                vec_.push_back(static_cast<char>(ch));
                return ch;
            } else {
                return traits_type::eof();
            }
        }

        // Přetížená metoda pro zápis více znaků
        virtual std::streamsize xsputn(const char* s, std::streamsize count) override {
            vec_.insert(vec_.end(), s, s + count);
            return count;
        }

    private:
        std::vector<char>& vec_;
    };

    using Serverity = ILog::Serverity;

    explicit Log(std::shared_ptr<ILog> ptr):_ptr(std::move(ptr)),_min_level(_ptr->get_min_level()) {}

    template<typename ... Args>
    void output(Serverity level, std::string_view pattern, Args && ... args) {
        if (level >= _min_level) {
            {
                vector_streambuf buffer(_buffer);
                std::ostream os(&buffer);
                format(os, pattern, std::forward<Args>(args)...);
            }
            _ptr->output(level, {_buffer.data(), _buffer.size()});
            _buffer.clear();
        }
    }

protected:
    std::shared_ptr<ILog> _ptr;
    std::vector<char> _buffer;
    Serverity _min_level;

    template<typename T>
    void format_item(std::ostream &out, const T &val) {
        if constexpr(std::is_invocable_v<T>) {
            format_item(out,val());
        } else if constexpr(is_container<T>) {
            out << '[';
            auto beg = val.begin();
            auto end = val.end();
            if (beg != end) {
                format_item(out,*beg);
                ++beg;
                while (beg != end) {
                    out << ',';
                    format_item(out,*beg);
                    ++beg;
                }
            }
            out << ']';
        } else if constexpr(is_pair<T>) {
            _buffer.push_back('<');
            format_item(val.first);
            _buffer.push_back(':');
            format_item(val.second);
            _buffer.push_back('>');
        } else {
            out << val;
        }
    }

    template<typename ... Args>
    void format(std::ostream &out, std::string pattern, Args && ... args) {
        std::bitset<64> mask={};
        auto iter = pattern.begin();
        auto end = pattern.end();
        unsigned int cur_index = 0;
        while (iter != end) {
            if (*iter == '{') {
                unsigned int index = 0;
                ++iter;
                while (iter != end && *iter >='0' && *iter <='9') {
                    index = index * 10 +  (*iter-'0');
                }
                if (index == 0) {
                    ++cur_index;
                    while (cur_index < mask.size() && mask[cur_index-1]) ++cur_index;
                    index = cur_index;
                } else {
                    mask.set(index-1);
                }
                format_nth_item(index-1, out, std::forward<Args>(args)...);
            } else {
                out.put(*iter);
                ++iter;
            }
        }
    }

    template<typename T, typename ... Args>
    void format_nth_item(unsigned int index, std::ostream &out, T && val, Args && ... args) {
        if (index) {
            format_nth_item(index-1, out, std::forward<Args>(args)...);
        } else {
            format_item(out, val);
        }
    }
    template<typename T>
    void format_nth_item(unsigned int index, std::ostream &out) {
        out << "{" << index << "}";
    }

};






}
