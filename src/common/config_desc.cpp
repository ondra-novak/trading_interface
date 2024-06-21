#include "config_desc.h"

namespace trading_api {

template<typename ... SkipTypes>
json::value config_schema_to_json_t(const std::vector<params::Control> &controls, unsigned int level);

inline constexpr params::Options default_opts = {};
inline constexpr params::VisibilityControl default_vc = {};


template<typename T>
static json::value skip_if_default(const T &val, const T &def) {
    if (val == def) return json::undefined;
    else return json::value(val);
}

template<typename T, typename Fn>
static json::value skip_if_default(const T &val, const T &def, Fn &&fn) {
    if (val == def) return json::undefined;
    else return fn(val);
}

static json::value visibility_to_json(const params::VisibilityControl &vc) {
    return skip_if_default(vc, default_vc, [](const params::VisibilityControl &vc) -> json::value {
        std::vector<json::key_value_t> rules;
        std::transform(vc.rules.begin(), vc.rules.end(), std::back_inserter(rules), [](const params::VisibilityRule &vr) -> json::key_value_t{
            if (vr.values.empty()) return {vr.variable, true};
            return {vr.variable, json::value(vr.values.begin(), vr.values.end())};
        });
        return json::value(rules.begin(), rules.end());
    });
}

static json::value options_to_json(const params::Options &opt) {
    return skip_if_default(opt, default_opts, [](const params::Options &opt) -> json::value {
       return {
           {"read_only", skip_if_default(opt.read_only, default_opts.read_only)},
           {"show_if", skip_if_default(opt.show_if, default_opts.show_if, visibility_to_json)},
           {"hide_if", skip_if_default(opt.hide_if, default_opts.hide_if, visibility_to_json)}
       } ;
    });
}

inline constexpr params::Range default_range = {};
static json::value range_to_json(const params::Range &r) {

    return skip_if_default(r, default_range, [](const auto &r) -> json::value {
        return {
            {"expand_max",skip_if_default(r.expand_max, default_range.expand_max)},
            {"expand_min",skip_if_default(r.expand_min, default_range.expand_min)},
            {"log_scale",skip_if_default(r.log_scale, default_range.log_scale)},
            {"max",skip_if_default(r.max, default_range.max)},
            {"min",skip_if_default(r.min, default_range.min)},
            {"step",skip_if_default(r.step, default_range.step)},
        };
    });
}

static json::value value_to_json(const DateValue &v) {
    char buffer[16];
    snprintf(buffer,sizeof(buffer), "%04d-%02d-%02d", v.year, v.month, v.day);
    return std::string_view(buffer);
}

static json::value value_to_json(const TimeValue &v) {
    char buffer[16];
    snprintf(buffer,sizeof(buffer), "%02d:%02d:%02d", v.hour, v.minute, v.second);
    return std::string_view(buffer);
}

inline constexpr params::DateRange default_date_range = {};
static json::value range_to_json(const params::DateRange &r) {

    return skip_if_default(r, default_date_range, [](const auto &r) -> json::value {
        return {
            {"max",skip_if_default(r.max, default_date_range.max, [](const auto &x) {return value_to_json(x);})},
            {"min",skip_if_default(r.min, default_date_range.min, [](const auto &x) {return value_to_json(x);})},
        };
    });
}

inline constexpr params::TimeRange default_time_range = {};
static json::value range_to_json(const params::TimeRange &r) {

    return skip_if_default(r, default_time_range, [](const auto &r) -> json::value {
        return {
            {"max",skip_if_default(r.max, default_time_range.max, [](const auto &x) {return value_to_json(x);})},
            {"min",skip_if_default(r.min, default_time_range.min, [](const auto &x) {return value_to_json(x);})},
            {"hide_seconds",r.hide_seconds},
        };
    });
}


static json::value control_to_json(const params::Group &grp, unsigned int level) {
    auto controls = config_schema_to_json_t<params::Section>(grp.controls, level+1);
    return {
        {"class","group"},
        {"name",grp.name},
        {"controls",json::value(controls.begin(), controls.end())},
        {"options",options_to_json(grp.opts)}
    };
}

static json::value control_to_json(const params::Compound &grp, unsigned int level) {
    auto controls = config_schema_to_json_t<params::Section, params::Group>(grp.controls, level+1);
    return {
        {"class","compound"},
        {"controls",json::value(controls.begin(), controls.end())},
        {"options",options_to_json(grp.opts)}
    };
}

static json::value control_to_json(const params::Section &grp, unsigned int level) {
    auto controls = config_schema_to_json_t<params::Section>(grp.controls, level+1);
    return {
        {"class","section"},
        {"name",grp.name},
        {"controls",json::value(controls.begin(), controls.end())},
        {"options",options_to_json(grp.opts)}
    };
}

static json::value control_to_json(const params::Text &txt, unsigned int) {
    return {
        {"class","text"},
        {"name",txt.name},
        {"options",options_to_json(txt.opts)}
    };
}

static json::value control_to_json(const params::TextArea &txt, unsigned int) {
    return {
        {"class","text_area"},
        {"name",txt.name},
        {"default",txt.def_val},
        {"size",txt.limit},
        {"rows",txt.rows},
        {"options",options_to_json(txt.opts)}
    };
}

static json::value control_to_json(const params::TextInput &txt, unsigned int) {
    return {
        {"class","text_input"},
        {"name",txt.name},
        {"default",txt.def_val},
        {"size",txt.limit},
        {"options",options_to_json(txt.opts)}
    };
}

static json::value control_to_json(const params::Number &n, unsigned int) {
    return {
        {"class","number"},
        {"name",n.name},
        {"options",options_to_json(n.opts)},
        {"default",n.def_val},
        {"range", range_to_json(n.r)},
    };
}

static json::value control_to_json(const params::Select &s, unsigned int) {
    return {
        {"class","select"},
        {"name",s.name},
        {"options",options_to_json(s.opts)},
        {"default",s.def_val},
        {"choices",  json::value(s.choices.begin(), s.choices.end(),[](const auto &opts) -> json::key_value_t{
                return {opts.first, opts.second};
            })}
    };
}
static json::value control_to_json(const params::Slider &n, unsigned int) {
    return {
        {"class","slider"},
        {"name",n.name},
        {"options",options_to_json(n.opts)},
        {"default",n.def_val},
        {"range", range_to_json(n.r)},
    };
}
static json::value control_to_json(const params::CheckBox &n, unsigned int) {
    return {
            {"class","checkbox"},
            {"name",n.name},
            {"options",options_to_json(n.opts)},
            {"default",n.def_val},
        };
}
static json::value control_to_json(const params::Date &n, unsigned int) {
    return {
        {"class","date"},
        {"name",n.name},
        {"options",options_to_json(n.opts)},
        {"default",value_to_json(n.def_val)},
        {"range", range_to_json(n.r)},
    };
}

static json::value control_to_json(const params::Time &n, unsigned int) {
    return {
        {"class","date"},
        {"name",n.name},
        {"options",options_to_json(n.opts)},
        {"default",value_to_json(n.def_val)},
        {"range", range_to_json(n.r)},
    };
}

static json::value control_to_json(const params::TimeZoneSelect &n, unsigned int) {
    return {
        {"class","select_tz"},
        {"name",n.name},
        {"options",options_to_json(n.opts)},
    };
}


template<typename ... SkipTypes>
json::value config_schema_to_json_t(const std::vector<params::Control> &controls, unsigned int level) {

    std::vector<json::value> values;
    values.reserve(controls.size());
    for (const auto &c: controls) {
        std::visit([&](const auto &x){
            if constexpr((!std::is_same_v<std::decay_t<decltype(x)>, SkipTypes> && ...)) {
                values.push_back(control_to_json(x,level));
            }
        },c);
    }
    return json::value_t(values.begin(), values.end());

}

json::value config_schema_to_json(const StrategyConfigSchema &desc) {
    return config_schema_to_json_t<void>(desc.controls,0 );
}


json::value config_desc_to_json(const IStrategy *strategy) {
    return config_schema_to_json(strategy->get_config_schema());
}

}
