#define Binary_h // a kind of nasty thing: disable warning about redefinition of BXXXX
#include <Arduino.h>
#include <mz_console.h>
#include <vector>
#include "esp_console.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"
#include "threadsync.h"

static const char * const history_file_name = "/settings/.history";
bool dumb_mode = true;

#define DUMB_PROMPT  "MZ5> "
#define ANSI_PROMPT  "\x1b[32m" "MZ5> " "\x1b[39m"

#define MAX_CMDLINE_ARGS 16
//#define MAX_CMDLINE_LENGTH 256

static std::vector<cmd_base_t *> commands; // a list of commands




void cmd_base_t::usage() const
{
	printf("Usage: %s", name);
	arg_print_syntax(stdout, at, "\n");
	printf("%s.\n\n", hint);
	arg_print_glossary(stdout, at, "  %-25s %s\n");
}

int cmd_base_t::handler(int argc, char **argv)
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

cmd_base_t::cmd_base_t(const char *_name, const char *_hint, void ** const _argtable) :
	name(_name), hint(_hint), at(_argtable)
{
	commands.push_back(this);
}

bool cmd_base_t::are_you(const char* _name) const
{
	return !strcmp(_name, this->name);
}



/**
 * Run a command.
 * */
static int run_command(const char *line)
{
	// esp_console_split_argv accepts only [char *] not [const char *]
	// so we need to dup it.
	char * l = strdup(line);
	char * argv [MAX_CMDLINE_ARGS];
	size_t argc = esp_console_split_argv(l, argv, MAX_CMDLINE_ARGS);
	int retval = -1;

	if(!argc)
	{
		// empty
		retval = 0;
		goto quit;
	}

	// argv[0] must be a command name. search it.
	for(auto && i : commands)
	{
		if(i->are_you(argv[0]))
		{
			// command found. execute it.
			retval = i->handler(argc, argv);
			goto quit;
		}
	}

	// command not found
	printf("Command not found. Type 'help' to show help.\n");

	quit:
	if(l) free(l);
	return retval;
}


/**
 * help handler
 * */
namespace cmd_help
{
    struct arg_lit *help = arg_litn(NULL, "help", 0, 1, "Display help and exit");
    struct arg_end *end = arg_end(5);
	struct arg_str *cmd;
    void * argtable[] = { help,
		cmd = arg_strn(nullptr, nullptr, "<command>", 0, 1, "Command name to show help"),
		end };

    class _cmd : public cmd_base_t
    {

    public:
        _cmd() : cmd_base_t("help", "Show command help", argtable) {}

    private:
        int func(int argc, char **argv)
        {
			if(cmd->count > 0)
			{
				// search command and show help of it
				for(auto && i : commands)
				{
					if(i->are_you(cmd->sval[0]))
					{
						i->usage();
						return 0;
					}
				}
				printf("Command '%s' not found.\n\n", cmd->sval[0]);
			}

			// list each command line help
			printf("Available commands:\n");
			for(auto && i : commands)
			{
				printf("%-10s %s\n", i->get_name(), i->get_hint());
			}
			return 0;
        }
    };
}




void console_probe()
{
	if (linenoiseProbe()) { /* zero indicates success */
          printf("\n"
	    "Your terminal application does not support escape sequences.\n"
	    "Line editing and history features are disabled.\n"
	    "On linux , try screen.\n"
	    "On Windows, try using Putty instead.\n"
		"If you are using PlatformIO monitor, try an option \"--eol LF\" to avoid newline problems.\n");
          linenoiseSetDumbMode(1);
		dumb_mode = true;
  	}
	else
	{
        printf("Line editing and history features enabled.\n");
		linenoiseSetDumbMode(0);
		dumb_mode = false;
	}
}


/**
 * Console task handler
 * */
static void console_task(void *)
{
	console_probe(); // will take a few seconds, so call it from the non-main thread

	// Note, this function is to be run inside separete thread
	char *line;
	for(;;) {
		if(dumb_mode)
			printf("\n"
			"info: Command line editing and history features are currently disabled.\n"
			"info: Try 't<enter>' to enable command line editing and history.\n");

		if((line = linenoise(dumb_mode ? DUMB_PROMPT : ANSI_PROMPT)) != NULL) {
			linenoiseHistoryAdd(line);

			linenoiseHistorySave(history_file_name);

			run_command(line);

			linenoiseFree(line); 
		}
	}
}


void init_console_commands(); // in console_commands.cpp

static void initialize_commands()
{
	esp_console_register_help_command();
	init_console_commands();
}

void init_console()
{
	uart_config_t uart_config;
	uart_config.baud_rate = 115200;
	uart_config.data_bits = UART_DATA_8_BITS;
	uart_config.parity = UART_PARITY_DISABLE;
	uart_config.stop_bits = UART_STOP_BITS_1;
	uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uart_param_config(UART_NUM_1, &uart_config);

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	/* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
	esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
	/* Move the caret to the beginning of the next line on '\n' */
	esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

	/* Install UART driver for interrupt-driven reads and writes */
	ESP_ERROR_CHECK( uart_driver_install((uart_port_t)CONFIG_CONSOLE_UART_NUM,
						256, 0, 0, NULL, 0) );

	/* Tell VFS to use UART driver */
	esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);

	linenoiseHistorySetMaxLen(30);

	static cmd_help::_cmd help_cmd;
	initialize_commands();
}


static void load_console_history()
{
	linenoiseHistoryLoad(history_file_name);
}

void begin_console()
{
	load_console_history();

	xTaskCreate(
	      console_task,           /* Task function. */
	      "console Task",        /* name of task. */
	      8192,                    /* Stack size of task */
	      (void *)0,                     /* parameter of the task */
	      1,                        /* priority of the task */
	      nullptr); /* Task handle to keep track of created task */
}

