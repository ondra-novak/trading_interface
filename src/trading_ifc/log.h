#pragma once

#include "common.h"
#include "wrapper.h"
#include <iterator>
#include <bitset>

#include <vector>
namespace trading_api {

class ILog {
public:

    enum class Serverity {
        trace = 0,
        debug = 1,
        info = 2,
        warning = 3,
        error = 4,
        fatal = 5,
        disabled = 100
    };

    virtual void output(Serverity level, std::string_view msg) const = 0;
    virtual Serverity get_min_level() const = 0;
    virtual ~ILog() = default;

    class Null;
};

inline std::string_view to_string(ILog::Serverity srvt) {
    switch (srvt) {
        case ILog::Serverity::debug: return "debug";
        case ILog::Serverity::trace: return "trace";
        case ILog::Serverity::info: return "info";
        case ILog::Serverity::warning: return "warning";
        case ILog::Serverity::error: return "ERROR";
        case ILog::Serverity::fatal: return "FATAL";
        case ILog::Serverity::disabled: return "disabled";
        default: return "unknown";
    }
}

class ILog::Null: public ILog {
public:
    virtual void output(Serverity , std::string_view ) const override {}
    virtual Serverity get_min_level() const override {return Serverity::disabled;}
};


template<typename T> constexpr bool const_false = false;

class Log  : public Wrapper<ILog>{
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

    Log() = default;
    Log(std::shared_ptr<const ILog> lgptr):Wrapper<ILog>(lgptr),_min_level(this->_ptr->get_min_level()) {}

    ///Create new log object with context
    /**
     * @param other source log object
     * @param pattern pattern to format context
     * @param args arguments to format context
     *
     * @note context is copied to every log line in square brackets
     *      [context1][context2][context3] log line
     */
    template<typename ... Args>
    Log(const Log &other, std::string_view pattern, Args && ... args)
        :Wrapper<ILog>(other)
        ,_buffer(other._buffer)
        ,_min_level(other._min_level)
    {
        if (_min_level != Serverity::disabled) {
            append_context(pattern, std::forward<Args>(args)...);
        }
    }

    template<typename ... Args>
    Log(Log &&other, std::string_view pattern, Args && ... args)
        :Wrapper<ILog>(std::move(other))
        ,_buffer(std::move(other._buffer))
        ,_min_level(other._min_level)
    {
        if (_min_level != Serverity::disabled) {
            append_context(pattern, std::forward<Args>(args)...);
        }
    }

    template<typename ... Args>
    void output(Serverity level, const std::string_view &pattern, Args && ... args) {
        if (level >= _min_level) {
            {
                vector_streambuf buffer(_buffer);
                std::ostream os(&buffer);
                format(os, pattern, std::forward<Args>(args)...);
            }
            _ptr->output(level, {_buffer.data(), _buffer.size()});
            _buffer.resize(_context_size);
        }
    }

    template<typename ... Args>
    void trace(const std::string_view &pattern, Args && ... args) {
        output(Serverity::trace, pattern, std::forward<Args>(args)...);
    }
    template<typename ... Args>
    void debug(const std::string_view &pattern, Args && ... args) {
        output(Serverity::debug, pattern, std::forward<Args>(args)...);
    }
    template<typename ... Args>
    void info(const std::string_view &pattern, Args && ... args) {
        output(Serverity::info, pattern, std::forward<Args>(args)...);
    }
    template<typename ... Args>
    void warning(const std::string_view &pattern, Args && ... args) {
        output(Serverity::warning, pattern, std::forward<Args>(args)...);
    }
    template<typename ... Args>
    void error(const std::string_view &pattern, Args && ... args) {
        output(Serverity::error, pattern, std::forward<Args>(args)...);
    }
    template<typename ... Args>
    void fatal(const std::string_view &pattern, Args && ... args) {
        output(Serverity::fatal, pattern, std::forward<Args>(args)...);
    }

protected:
    std::vector<char> _buffer;
    Serverity _min_level;
    std::size_t _context_size = 0;

    template<typename ... Args>
    void append_context(const std::string_view &pattern, Args && ... args) {
        _buffer.push_back('[');
        {
            vector_streambuf buffer(_buffer);
            std::ostream os(&buffer);
            format(os, pattern, std::forward<Args>(args)...);
        }
        _buffer.push_back(']');
        _context_size = _buffer.size();
    }

    template<typename T>
    void format_item(std::ostream &out, const T &val) {
        if constexpr(can_output_to_ostream<T>) {
            out << val;
        } else if constexpr(std::is_invocable_v<T>) {
            format_item(out,val());
        } else if constexpr(has_to_string_global<T>) {
            format_item(out,to_string(val));
        } else if constexpr(std::is_same_v<T, std::exception_ptr>) {
            try {
                std::rethrow_exception(val);
            } catch (std::exception &e) {
                format_item(out, e.what());
            } catch (...) {
                format_item(out, "<unknown exception>");
            }
        } else if constexpr(is_container<T>) {
            out << '(';
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
            out << ')';
        } else if constexpr(is_pair_type<T>) {
            _buffer.push_back('<');
            format_item(val.first);
            _buffer.push_back(':');
            format_item(val.second);
            _buffer.push_back('>');
        } else {
            static_assert(const_false<T>, "Cannot print type, define operator << ");
        }
    }

    template<typename ... Args>
    void format(std::ostream &out, const std::string_view &pattern, Args && ... args) {
        std::bitset<64> mask={};
        auto iter = pattern.begin();
        auto end = pattern.end();
        unsigned int cur_index = 0;
        while (iter != end) {
            if (*iter == '{') {
                unsigned int index = 0;
                ++iter;
                if (iter != end && *iter == '{') {
                    out << '{';
                    ++iter;
                    continue;
                }
                while (iter != end && *iter >='0' && *iter <='9') {
                    index = index * 10 +  (*iter-'0');
                }
                if (iter != end && *iter == '}') ++iter;
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
    void format_nth_item(unsigned int index, std::ostream &out) {
        out << "{" << index << "}";
    }

};





}
