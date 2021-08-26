#ifdef _MSC_VER
  #define _CRTDBG_MAP_ALLOC
  #include <stdlib.h>
  #include <crtdbg.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include "parg.h"
#include "common.h"
#include "gifdump.h"

static void show_help(void)
{
    printf("\nUsage:\n");
    printf("  %s (-d | -e | -c | -i) gif_file [-o <output_file>]\n", PROGRAM_NAME);
    printf("  %s [-i] gif_file\n", PROGRAM_NAME);
    printf("  %s [-h]\n", PROGRAM_NAME);
    printf("  %s -v\n", PROGRAM_NAME);
    printf("\nOptions:\n");
    printf("  -h                  Show this screen\n");
    printf("  -v                  Show version\n");
    printf("  -i                  Show info about the gif file (Default)\n");
    printf("  -d[index]           Output the decoded image bytes\n");
    printf("  -e[index]           Output the lzw encoded bytes\n");
    printf("  -c[index]           Output the color table bytes\n");
    printf("  -o <output_file>    The output file to write to\n\n");
    printf("If no index is given for -d and -e, select the first image (index=1)\n");
    printf("  For option -c select the global color table (index=0)\n");
}

static void show_version(void)
{
    printf("%s %s\n", PROGRAM_NAME, GIFDUMP_VERSION);
}

static bool set_option(char option, const char* arg, Config* config)
{
    if (config->option != '\0')
    {
        if (config->option == option)
        {
            fprintf(stderr, "'-%c' option specified multiple times\n", option);
            return false;
        }
        else
        {
            fprintf(stderr, "Options '-i', '-d', '-e', '-c' are mutually exclusive\n");
            return false;
        }
    }

    config->option = option;

    if (option == 'd' || option == 'e')
    {
        config->index = 1;
    }
    if (arg == NULL)
    {
        return true;
    }

    int min = config->index;
    if (sscanf(arg, "%d", &config->index) < 1)
    {
        fprintf(stderr, "'%s' is not a valid index for option '-%c'\n", arg, option);
        return false;
    }

    if (config->index < min)
    {
        fprintf(stderr, "index '%d' too small for option '-%c' (min=%d)\n",
                config->index, option, min);

        return false;
    }

    return true;
}

static bool parse_args(int argc, char* argv[], Config* config)
{
    struct parg_state ps;
    int c;

    parg_init(&ps);

    while ((c = parg_getopt(&ps, argc, argv, "hvid::e::c::o:")) != -1)
    {
        switch (c) {
        case 1:
            if (config->input.name == NULL)
            {
                assert(ps.optarg != NULL);
                config->input.name = ps.optarg;
            }
            else
            {
                fprintf(stderr, "Only one input file please\n");
                return false;
            }
            break;

        case 'h':
            show_help();
            exit(EXIT_SUCCESS);

        case 'v':
            show_version();
            exit(EXIT_SUCCESS);

        case 'i':
            if (!set_option('i', NULL, config)) return false;
            break;

        case 'd':
            if (!set_option('d', ps.optarg, config)) return false;
            break;

        case 'e':
            if (!set_option('e', ps.optarg, config)) return false;
            break;

        case 'c':
            if (!set_option('c', ps.optarg, config)) return false;
            break;

        case 'o':
            if (config->output.name != NULL)
            {
                fprintf(stderr, "'-o' option specified multiple times\n");
                return false;
            }
            config->output.name = ps.optarg;
            break;

        case '?':
            if (ps.optopt == 'o')
            {
                fprintf(stderr, "Option '-%c' missing required argument\n", ps.optopt);
            }
            else if (isprint(ps.optopt))
            {
                fprintf(stderr, "'-%c' is not an option\n", ps.optopt);
            }
            else
            {
                fprintf(stderr, "'0x%X' is not an option\n", ps.optopt);
            }
            return false;

        default:
            abort();
        }
    }

    for (c = ps.optind; c < argc; ++c)
    {
        if (config->input.name == NULL)
        {
            config->input.name = argv[c];
        }
        else
        {
            fprintf(stderr, "Only one input file please\n");
            return false;
        }
    }

    if (config->option == '\0')
    {
        if (config->input.name == NULL && config->output.name == NULL)
        {
            show_help();
            exit(EXIT_SUCCESS);
        }

        config->option = 'i';
    }

    if (config->input.name == NULL)
    {
        fprintf(stderr, "Missing input file\n");
        return false;
    }

    if (config->output.name != NULL)
    {
        if (strcmp(config->input.name, config->output.name) == 0)
        {
            fprintf(stderr, "Input file same as output file (%s)\n", config->input.name);
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    int return_code = EXIT_SUCCESS;

    Config config;
    config_init(&config);

    if (!parse_args(argc, argv, &config))
    {
        fprintf(stderr, "Type '%s -h' for usage information\n", argv[0]);
        return EXIT_FAILURE;
    }

    assert(config.input.name != NULL);

    config.input.file = fopen(config.input.name, "rb");
    if (config.input.file == NULL)
    {
        fprintf(stderr, "Unable to open gif file: %s\n", config.input.name);
        return EXIT_FAILURE;
    }

    if (config.output.name != NULL)
    {
        if (config.option == 'i')
        {
            config.output.file = fopen(config.output.name, "w");
        }
        else
        {
            config.output.file = fopen(config.output.name, "wb");
        }

        if (config.output.file == NULL)
        {
            fprintf(stderr, "Unable to create output file: %s\n", config.output.name);
            return_code = EXIT_FAILURE;
            goto io_cleanup;
        }
    }
    else
    {
        config.output.file = stdout;
    }

#ifndef NDEBUG
    config_dump(&config);
#endif

    process_gifstream(&config);

io_cleanup:
    if (config.input.file != NULL)
    {
        fclose(config.input.file);
        config.input.file = NULL;
    }
    if (config.output.file != NULL)
    {
        fclose(config.output.file);
        config.output.file = NULL;
    }

    return return_code;
}
