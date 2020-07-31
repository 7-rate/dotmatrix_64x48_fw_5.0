#include <functional>
#include <vector>
#include "mz_console.h"
#include "wifi.h"
#include "esp_console.h"
#include "esp_system.h"
#include "argtable3/argtable3.h"
#include "threadsync.h"


class cmd_base_t;
static std::vector<cmd_base_t*> commands;

static int generic_handler(int argc, char **argv);

class cmd_base_t
{
    const char * name;
    const char * hint;
    void ** at;

    virtual int func(int argc, char **argv) = 0;

protected:
    void usage()
    {
        printf("Usage: %s", name);
        arg_print_syntax(stdout, at, "\n");
        printf("%s.\n\n", hint);
        arg_print_glossary(stdout, at, "  %-25s %s\n");
    }

private:
    int handler(int argc, char **argv)
    {
        int nerrors;
        nerrors = arg_parse(argc,argv,at);
        if(((struct arg_lit*)at[0])->count > 0) // at[0] must be help
        {
            usage();
            return 0;
        }

        if (nerrors > 0)
        {
            // search for arg_end
            int tabindex;
            struct arg_hdr ** table = (struct arg_hdr**)at;
            for (tabindex = 0; !(table[tabindex]->flag & ARG_TERMINATOR); tabindex++) /**/;

            /* Display the error details contained in the arg_end struct.*/
            arg_print_errors(stdout, (struct arg_end*)(table[tabindex]), name);
            printf("Try '%s --help' for more information.\n", name);
            return 0;
        }

        return func(argc, argv);
    }

    bool are_you(const char *p) const
    {
        return !strcmp(name, p);
    }

public:
    cmd_base_t(const char *_name, const char *_hint, void ** const _argtable) :
        name(_name), hint(_hint), at(_argtable)
    {
        esp_console_cmd_t cmd;

        cmd.command = name;
        cmd.help = hint;
        cmd.hint = nullptr;
        cmd.func = generic_handler;
        cmd.argtable = at;

        esp_console_cmd_register(&cmd);

        commands.push_back(this);
    }


    friend int generic_handler(int argc, char **argv);
};

// because ESP-IDF's esp_console_cmd_func_t does not include
// any user pointer, there is no method to know from the
// handler that which command is invoked. Fortunately
// argv[0] contains command name itself
// so check the list. bullshit.
static int generic_handler(int argc, char **argv)
{
    for(auto && cmd : commands)
    {
        if(cmd->are_you(argv[0])) { return cmd->handler(argc, argv); }
    }
    return -1; // command not found
}

// command handlers --------------------------------------------------
namespace cmd_wifi_show
{
    struct arg_lit *help = arg_litn(NULL, "help", 0, 1, "Display help and exit");
    struct arg_lit *i_am_safe = arg_litn(nullptr, "i-am-safe", 0, 1, "Show non-masked PSK (password)");
    struct arg_end *end = arg_end(5);
    void * argtable[] = { help, i_am_safe, end };

    class _cmd : public cmd_base_t
    {

    public:
        _cmd() : cmd_base_t("wifi-show", "Display WiFi status", argtable) {}

    private:
        int func(int argc, char **argv)
        {
            return run_in_main_thread([] () -> int {
                printf("%s\n", wifi_get_connection_info_string().c_str());
                ip_addr_settings_t settings = wifi_get_ip_addr_settings(true);
                printf("--- current IP status ---\n");
                settings.dump("(not configured)");
                printf("--- configured IP settings ---\n");
                settings = wifi_get_ip_addr_settings(false);
                settings.dump("(use DHCP)");
                printf("--- AP settings ---\n");
                printf("SSID             : %s\n", wifi_get_ap_name().c_str());
                printf("PSK              : %s\n", 
                    (i_am_safe->count > 0) ? wifi_get_ap_pass().c_str() : "******** (try --i-am-safe to show)");
                return 0;
            }) ;       
        }
    };
}

namespace cmd_wifi_ip
{
    struct arg_lit *help, *dhcp;
    struct arg_str *address, *gw, *mask, *dns;
    struct arg_end *end;
    void * argtable[] = {
            help =    arg_litn(NULL, "help", 0, 1, "Display help and exit"),
            dhcp =    arg_litn("d",  "dhcp", 0, 1, "Use DHCP"),
            address = arg_strn("a",  "addr", "<v4addr>", 0, 1, "IPv4 address"),
            gw =      arg_strn("g",  "gw",   "<v4addr>", 0, 1, "IPv4 gateway"),
            mask =    arg_strn("m",  "mask", "<v4mask>", 0, 1, "IPv4 mask"),
            dns =     arg_strn("n",  "dns",  "<v4addr>", 0, 2, "IPv4 DNS server"),
            end =     arg_end(5)
            };

    class _cmd : public cmd_base_t
    {

    public:
        _cmd() : cmd_base_t("wifi-ip", "set DCHP mode or IP addresses manually", argtable) {}

    private:
        static void set_dns(ip_addr_settings_t &settings)
        {
            if(dns->count > 0)
            {
                settings.dns1 = null_ip_addr;
                settings.dns2 = null_ip_addr;
                settings.dns1 = dns->sval[0];
                if(dns->count > 1)
                    settings.dns2 = dns->sval[1];
            }
        }

        static bool validate_ipv4(arg_str * arg, bool (*validator)(const String &), const char * label)
        {
            auto count = arg->count;
            for(typeof(count) i = 0; i < count; ++i)
            {
                if(!validator(arg->sval[i]))
                {
                    printf("Invalid IPv4 %s : '%s'\n", label, arg->sval[i]);
                    return false;
                }
            }
            return true;
        }

        int func(int argc, char **argv)
        {
            return run_in_main_thread([this, argc] () -> int {
                if(argc == 1)
                {
                    // nothing specified
                    usage();
                    return 0;
                }
                const char *label_address = "address";
                const char *label_mask = "net mask";
                if(!validate_ipv4(address, validate_ipv4_address, label_address)) return 1;
                if(!validate_ipv4(gw, validate_ipv4_address, label_address)) return 1;
                if(!validate_ipv4(mask, validate_ipv4_netmask, label_mask)) return 1;
                if(!validate_ipv4(dns, validate_ipv4_address, label_address)) return 1;
                if(dhcp->count > 0)
                {
                    // use DHCP
                    if(address->count || gw->count || mask->count)
                    {
                        printf("DHCP mode can not specify address/gateway/mask.\n");
                        return 1;
                    }

                    ip_addr_settings_t settings = wifi_get_ip_addr_settings();
                    settings.ip_addr = null_ip_addr;
                    settings.ip_gateway = null_ip_addr;
                    settings.ip_mask = null_ip_addr;
                    set_dns(settings);
                    wifi_manual_ip_info(settings);
                }
                else
                {
                    // manual ip settings
                    ip_addr_settings_t settings = wifi_get_ip_addr_settings();
                    if(address->count) settings.ip_addr = address->sval[0];
                    if(gw->count) settings.ip_gateway = gw->sval[0];
                    if(mask->count) settings.ip_mask = mask->sval[0];
                    set_dns(settings);
                    wifi_manual_ip_info(settings);
                }
                 return 0;
             }) ;
        }
    };
}

namespace cmd_wifi_ap
{
    struct arg_lit *help;
    struct arg_str *ssid, *psk;
    struct arg_end *end;
    void * argtable[] = {
            help =    arg_litn(NULL, "help", 0, 1, "Display help and exit"),
            ssid =    arg_strn("s",  "ssid",   "<SSID>", 1, 1, "SSID name"),
            psk =     arg_strn("p",  "psk",    "<password>", 1, 1, "PSK (password)"),
            end =     arg_end(5)
            };

    class _cmd : public cmd_base_t
    {

    public:
        _cmd() : cmd_base_t("wifi-ap", "set AP's SSID and psk(password)", argtable) {}

    private:

        int func(int argc, char **argv)
        {
            return run_in_main_thread([] () -> int {
                wifi_set_ap_info(ssid->sval[0], psk->sval[0]);
                return 0;
            }) ;       
         }
    };
}


namespace cmd_t
{
    struct arg_lit *help = arg_litn(NULL, "help", 0, 1, "Display help and exit");
    struct arg_end *end = arg_end(5);
    void * argtable[] = { help, end };

    class _cmd : public cmd_base_t
    {

    public:
        _cmd() : cmd_base_t("t", "Try to enable line edit / history", argtable) {}

    private:
        int func(int argc, char **argv)
        {
            return run_in_main_thread([] () -> int {
                console_probe();
                return 0;
            }) ;       
        }
    };
}


void init_console_commands()
{
    static cmd_wifi_show::_cmd wifi_show_cmd;
    static cmd_wifi_ip::_cmd wifi_ip_cmd;
    static cmd_wifi_ap::_cmd wifi_ap_cmd;
    static cmd_t::_cmd t_cmd;
}