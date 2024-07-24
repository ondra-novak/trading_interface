#pragma once

#include <string>
#include <vector>
#include <variant>

namespace trading_api {

namespace params {

struct VisibilityRule {
    std::string variable;
    std::vector<std::string> values;
    VisibilityRule(std::string variable)
        :variable(variable) {}
    VisibilityRule(std::string variable, std::initializer_list<std::string> values)
        :variable(variable)
        ,values(values.begin(), values.end()) {}
    bool operator==(const VisibilityRule &) const = default;
};

struct VisibilityControl {
    std::vector<VisibilityRule> rules;
    VisibilityControl() = default;
    VisibilityControl(std::initializer_list<VisibilityRule> rules)
        :rules(rules.begin(), rules.end()) {}
    bool operator==(const VisibilityControl &) const = default;
};

struct Options {
    bool read_only = false;
    VisibilityControl show_if = {};
    VisibilityControl hide_if = {};
    bool operator==(const Options &) const = default;
};

struct Common {
    std::string name = {};
    Options opts = {};
};

///Text element (mark)
struct Text: Common {
    Text(std::string name, Options opts = {})
        :Common{name, opts} {}
};

///Arbitrary text input
struct TextInput: Common {
    std::string def_val;
    std::size_t limit;
    TextInput(std::string name, std::string def_val, std::size_t limit = 256, Options opts = {})
        :Common{name, opts}, def_val(def_val), limit(limit){}
};

struct TextArea: TextInput {
    unsigned int rows;
    TextArea(std::string name, unsigned int rows, std::string def_val, std::size_t limit = 65536, Options opts = {})
        :TextInput(name, def_val, limit, opts), rows(rows) {}
};


struct Range {
    double min = -std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::max();
    double step = 0.0;
    bool expand_min = false;
    bool expand_max = false;
    bool log_scale = false;
    bool operator==(const Range &) const = default;
};


struct Number : Common {
    Range r = {};
    double def_val;
    Number(std::string name,double def_val, Range range = {}, Options opts = {})
        :Common{name, opts},r(range),def_val(def_val) {}
};




struct DateRange {
    DateValue min = {0,1,1};
    DateValue max = {9999,12,31};
    bool operator==(const DateRange &) const = default;
};


struct Date: Common {
    DateRange r = {};
    DateValue def_val;
    Date(std::string name, DateValue def_val, DateRange range = {}, Options opts = {})
        :Common{name, opts},r(range),def_val(def_val) {}
};


struct TimeRange {
    TimeValue min = {0,0,0};
    TimeValue max = {23,59,59};
    bool hide_seconds = false;
    bool operator==(const TimeRange &) const = default;
};


struct Time: Common {
    TimeRange r = {};
    TimeValue def_val;
    Time(std::string name, TimeValue def_val, TimeRange range = {}, Options opts = {})
        :Common{name, opts},r(range),def_val(def_val) {}
};

struct TimeZoneSelect: Common {
    TimeZoneSelect(std::string name, Options opt = {})
        :Common{name, opt} {}
};

struct Slider: Number {
    using Number::Number;
};

struct CheckBox: Common {
    bool def_val;
    CheckBox(std::string name,bool def_val, Options opts = {})
        :Common{name, opts}, def_val(def_val) {}
};

struct Select: Common {
    std::string def_val;
    std::vector<std::pair<std::string, std::string> > choices;
    Select(std::string name,
            std::initializer_list<std::pair<std::string_view, std::string_view> > choices,
            std::string def_val = {},
            Options opts = {})
        :Common{name, opts}
        ,def_val(def_val)
        ,choices(choices.begin(), choices.end()) {}

};

struct Section;
struct Group;
struct Compound;

using Control = std::variant<Text,
        TextInput,
        TextArea,
        Number,
        Slider,
        CheckBox,
        Select,
        Date,
        Time,
        TimeZoneSelect,
        Group,
        Section,
        Compound>;



struct Group: Common {
    std::vector<Control> controls;

    Group(std::initializer_list<Control> controls, Options opts = {});
    Group(std::string name, std::initializer_list<Control> controls, Options opts = {});
    ~Group();
};

struct Compound: Group{
    Compound(std::initializer_list<Control> controls, Options opts = {})
        :Group(controls, opts) {}
};

inline constexpr bool shown = true;

struct Section: Group{
    bool shown = false;
    Section(std::string name,std::initializer_list<Control> controls, Options opts = {})
        :Group(name, controls, opts) {}
    Section(std::string name,std::initializer_list<Control> controls, bool shown, Options opts = {})
        :Group(name, controls, opts),shown(shown) {}
};


inline Group::Group(std::initializer_list<Control> controls, Options opts)
    :Common{{}, opts}, controls(controls.begin(), controls.end()) {}
inline Group::Group(std::string name, std::initializer_list<Control> controls, Options opts)
    :Common{name, opts}, controls(controls.begin(), controls.end()) {}
inline Group::~Group() {}


}

class ConfigSchema {
public:
    std::vector<params::Control> controls;

    ConfigSchema(std::initializer_list<params::Control> controls)
        :controls(controls.begin(), controls.end()) {}

};

}
