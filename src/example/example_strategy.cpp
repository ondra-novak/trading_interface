#include <trading_api/strategy.h>
#include <trading_api/module.h>


using namespace trading_api;

class Example: public AbstractStrategy {
public:
    virtual void on_init(const Context &ctx) override;
    virtual ConfigSchema get_config_schema() const override;
protected:
    Context _context;
};


EXPORT_STRATEGY(Example);



void Example::on_init(const Context &ctx) {
    _context = ctx;
    _context.mget<int>("aaa", [](auto a, int b){});
    _context.mget<int>("aaa","bbb", [](auto a,int b){});

}


ConfigSchema Example::get_config_schema() const {
    using namespace trading_api::params;
    return {
        Group("gr1",{
            Text("text_example"),
            TextInput("text_area_example", "defval"),
            Select("s2",{
                {"opt1","label1"},
                {"opt2","label2"},
            }),
            Number("any",100),
        }),
        Group({
            Number("n1", 0.0, {.min=0.0,.max=100.0, .step=1.0}),
            Slider("n2", 0.0, {.min=0.0,.max=100.0, .step=1.0, .log_scale = true}),
            CheckBox("chk1", false),
            Select("s1",{
                    {"opt1","label1"},
                    {"opt2","label2"},
                    {"opt3","label3"}
            }, ""),
            TextArea("txt1",10,"hello world!",65536,{
                    .show_if = {{"chk1"}}
            }),
            Section("not_seen",{})
        },{
          .show_if = {
                  {"s2",{"opt1"}}
          }
        }),
        Section("ext1", {
                Compound({
                    Date("date1", {2020,10,12}, {.min={2000,1,1}}),
                    Time("time1", {12,5,30}, {}),
                    TimeZoneSelect("tz1"),
                    Section("not seen",{}),
                    Group("not seen",{})
                })
        }),
        Section("ext2", {}),
        Section("ext3", {}, shown),
    };
}

