#include "gifdump.h"
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include "common.h"

#define GIF_SIGNATURE "GIF"
#define GIF_VERSION_87A "87a"
#define GIF_VERSION_89A "89a"

#define IMAGE_DESCR 0x2C
#define EXT_INTRO 0x21
#define TRAILER 0x3B

#define EXT_GRAPHIC_CONTROL 0xF9
#define EXT_COMMENT 0xFE
#define EXT_PLAIN_TEXT 0x01
#define EXT_APPLICATION 0xFF

#define MAX_CODE_SIZE 12

typedef enum
{
    ST_EXIT_FAILURE = 0,
    ST_EXIT_SUCCESS = 1,
    ST_CONTINUE = 2
} Status;

typedef struct
{
    char signature[4];
    char version[4];
} Header;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint8_t fields;
    uint8_t bg_color_idx;
    uint8_t pixel_aspect;
} Screen_descr;

typedef struct
{
    uint8_t has_color_table;
    uint8_t color_res;
    uint8_t sort;
    uint8_t color_table_size;
} Screen_fields;

typedef struct
{
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint8_t fields;
} Image_descr;

typedef struct
{
    uint8_t has_color_table;
    uint8_t interlace;
    uint8_t sort;
    uint8_t reserved;
    uint8_t color_table_size;
} Image_fields;

typedef struct
{
    uint8_t byte;
    int prev;
    int len;
} Dict_entry;

typedef struct
{
    uint8_t fields;
    uint16_t delay;
    uint8_t transp_color_idx;
} Graphic_control;

typedef struct
{
    uint8_t reserved;
    uint8_t disposal;
    uint8_t user_input;
    uint8_t has_transp;
} Graphic_fields;

typedef struct
{
    uint16_t grid_left;
    uint16_t grid_top;
    uint16_t grid_width;
    uint16_t grid_height;
    uint8_t cell_width;
    uint8_t cell_height;
    uint8_t fg_color_idx;
    uint8_t bg_color_idx;
} Plain_text;

typedef struct
{
    char identifier[9];
    char auth_code[4];
} Application;

void config_init(Config* config)
{
    config->input.name = NULL;
    config->input.file = NULL;

    config->output.name = NULL;
    config->output.file = NULL;

    config->index = 0;
    config->option = '\0';
}

void config_dump(Config* config)
{
    printf("-------Config-dump--------\n");
    printf("Input\n");
    printf("    name: %s\n", config->input.name);
    printf("    ptr : %p\n", config->input.file);

    printf("Output\n");
    printf("    name: %s\n", config->output.name);
    if (config->output.file == stdout)
    {
        printf("    ptr : stdout\n");
    }
    else
    {
        printf("    ptr : %p\n", config->output.file);
    }

    printf("\nOption: %c\n", config->option);
    printf("Index : %d\n", config->index);
    printf("--------------------------\n");
}

static Status process_header(Config* config)
{
    char buffer[6];

    if (fread(buffer, 1, 6, config->input.file) < 6)
    {
        fprintf(stderr, "Error reading gif header\n");
        return ST_EXIT_FAILURE;
    }

    Header header;

    memcpy(header.signature, buffer, 3);
    memcpy(header.version, buffer + 3, 3);
    header.signature[3] = '\0';
    header.version[3] = '\0';

    if (strcmp(header.signature, GIF_SIGNATURE) != 0)
    {
        fprintf(stderr, "'%s' is not a gif file, got signature '%s'\n",
            config->input.name, header.signature);

        return ST_EXIT_FAILURE;
    }

    if (strcmp(header.version, GIF_VERSION_87A) != 0 &&
        strcmp(header.version, GIF_VERSION_89A) != 0)
    {
        fprintf(stderr, "Warning: unknown gif version '%s'\n", header.version);
    }

    if (config->option == 'i')
    {
        fprintf(config->output.file, "File: %s\n", config->input.name);
        fprintf(config->output.file, "Signature: %s\n", header.signature);
        fprintf(config->output.file, "Version: %s\n", header.version);
    }

    return ST_CONTINUE;
}

static Status process_screen_descr(Config* config, int* color_table_size)
{
    uint8_t buffer[7];

    if (fread(buffer, 1, 7, config->input.file) < 7)
    {
        fprintf(stderr, "Error reading screen descriptor\n");
        return ST_EXIT_FAILURE;
    }

    Screen_descr screen_descr;
    Screen_fields fields;

    memcpy(&screen_descr.width, buffer, 2);
    memcpy(&screen_descr.height, buffer + 2, 2);
    memcpy(&screen_descr.fields, buffer + 4, 1);
    memcpy(&screen_descr.bg_color_idx, buffer + 5, 1);
    memcpy(&screen_descr.pixel_aspect, buffer + 6, 1);

    if (BYTEORDER == BIG_ENDIAN)
    {
        screen_descr.width = swap_bytes16(screen_descr.width);
        screen_descr.height = swap_bytes16(screen_descr.height);
    }

    fields.has_color_table = screen_descr.fields >> 7;
    fields.color_res = ((screen_descr.fields >> 4) & 0x7) + 1;
    fields.sort = (screen_descr.fields >> 3) & 0x1;
    fields.color_table_size = (screen_descr.fields & 0x7) + 1;

    if (fields.has_color_table)
    {
        *color_table_size = 1 << fields.color_table_size;
    }
    else
    {
        *color_table_size = 0;
    }

    if (config->option == 'i')
    {
        fprintf(config->output.file, "Screen width: %u\n", screen_descr.width);
        fprintf(config->output.file, "Screen height: %u\n", screen_descr.height);
        fprintf(config->output.file, "Fields: 0x%X\n", screen_descr.fields);
        fprintf(config->output.file, "\tHas global color table: %u\n", fields.has_color_table);
        fprintf(config->output.file, "\tColor resolution: %d\n", fields.color_res);
        fprintf(config->output.file, "\tSort: %u\n", fields.sort);
        fprintf(config->output.file, "\tGlobal color table size: %d\n", 1 << fields.color_table_size);
        fprintf(config->output.file, "Background color index: %u\n", screen_descr.bg_color_idx);
        fprintf(config->output.file, "Pixel aspect ratio: %u\n", screen_descr.pixel_aspect);
    }

    return ST_CONTINUE;
}

static Status process_color_table(Config* config, int color_table_size, int index)
{
    if (config->option == 'c' && index == config->index)
    {
        if (color_table_size == 0)
        {
            fprintf(stderr, "No color table to read at index '%d'\n", index);
            return ST_EXIT_FAILURE;
        }

        uint8_t color[3];
        for (int i = 0; i < color_table_size; ++i)
        {
            if (fread(color, 1, 3, config->input.file) < 3)
            {
                fprintf(stderr, "Failed to read color table\n");
                return ST_EXIT_FAILURE;
            }
            if (fwrite(color, 1, 3, config->output.file) < 3)
            {
                fprintf(stderr, "Failed to write color table\n");
                return ST_EXIT_FAILURE;
            }
        }

        return ST_EXIT_SUCCESS;
    }

    if (color_table_size != 0)
    {
        if (fseek(config->input.file, color_table_size * 3, SEEK_CUR) != 0)
        {
            fprintf(stderr, "Failed to skip color table '%d'\n", config->index);
            return ST_EXIT_FAILURE;
        }
    }

    return ST_CONTINUE;
}

static Status process_image_descr(Config* config, int* color_table_size, int index)
{
    uint8_t buffer[9];

    if (fread(buffer, 1, 9, config->input.file) < 9)
    {
        fprintf(stderr, "Error reading image descriptor\n");
        return ST_EXIT_FAILURE;
    }

    Image_descr image_descr;
    Image_fields fields;

    memcpy(&image_descr.left, buffer, 2);
    memcpy(&image_descr.top, buffer + 2, 2);
    memcpy(&image_descr.width, buffer + 4, 2);
    memcpy(&image_descr.height, buffer + 6, 2);
    memcpy(&image_descr.fields, buffer + 8, 1);

    if (BYTEORDER == BIG_ENDIAN)
    {
        image_descr.left = swap_bytes16(image_descr.left);
        image_descr.top = swap_bytes16(image_descr.top);
        image_descr.width = swap_bytes16(image_descr.width);
        image_descr.height = swap_bytes16(image_descr.height);
    }

    fields.has_color_table = image_descr.fields >> 7;
    fields.interlace = (image_descr.fields >> 6) & 0x1;
    fields.sort = (image_descr.fields >> 5) & 0x1;
    fields.reserved = (image_descr.fields >> 3) & 0x3;
    fields.color_table_size = (image_descr.fields & 0x7) + 1;

    if (fields.has_color_table)
    {
        *color_table_size = 1 << fields.color_table_size;
    }
    else
    {
        *color_table_size = 0;
    }

    if (config->option == 'i')
    {
        fprintf(config->output.file, "\nImage [Index=%d]\n", index);
        fprintf(config->output.file, "\tLeft: %u\n", image_descr.left);
        fprintf(config->output.file, "\tTop: %u\n", image_descr.top);
        fprintf(config->output.file, "\tWidth: %u\n", image_descr.width);
        fprintf(config->output.file, "\tHeight: %u\n", image_descr.height);
        fprintf(config->output.file, "\tFields: 0x%X\n", image_descr.fields);
        fprintf(config->output.file, "\t\tHas local color table: %u\n", fields.has_color_table);
        fprintf(config->output.file, "\t\tInterlace: %u\n", fields.interlace);
        fprintf(config->output.file, "\t\tSort: %u\n", fields.sort);
        fprintf(config->output.file, "\t\tReserved: 0x%X\n", fields.reserved);
        fprintf(config->output.file, "\t\tLocal color table size: %d\n", 1 << fields.color_table_size);
    }

    return ST_CONTINUE;
}

static void init_dictionary(Dict_entry* dict, uint8_t lzw_code_size)
{
    for (int i = 0; i < (1 << lzw_code_size); ++i)
    {
        dict[i].byte = i;
        dict[i].prev = -1;
        dict[i].len = 1;
    }
}

static bool decode_image_blocks(Config* config, uint8_t lzw_code_size)
{
    bool status = false;

    uint8_t block[255];
    uint8_t block_size;

    Dict_entry* dict = NULL;
    int dict_idx;

    uint8_t* decoded = NULL; // resizable buffer
    int dec_size = 16;

    int code_size = lzw_code_size;
    int clear_code = 1 << lzw_code_size;
    int stop_code = clear_code + 1;

    dict = malloc(sizeof(Dict_entry) * (1 << MAX_CODE_SIZE));
    init_dictionary(dict, lzw_code_size);
    dict_idx = stop_code + 1;

    decoded = malloc(dec_size);
    int prev = -1;
    int left = 8; // bits left in byte after read

    while (true)
    {
        if (fread(&block_size, 1, 1, config->input.file) < 1)
        {
            fprintf(stderr, "Failed to read block size\n");
            goto clean_up;
        }

        if (block_size == 0)
        {
            fprintf(stderr, "Encountered 0 sized block, before reaching stop code\n");
            goto clean_up;
        }

        if (fread(block, 1, block_size, config->input.file) < block_size)
        {
            fprintf(stderr, "Failed to read sub block\n");
            goto clean_up;
        }

        uint8_t* block_ptr = block;
        uint8_t* block_end = block_ptr + block_size;

        while (block_ptr != block_end)
        {
            int code = 0x0;
            int c = code_size + 1; // bits left to read for code

            while (c)
            {
                int read = (c < left) ? (c) : (left); // next bits to read in byte
                int mask = (1 << read) - 1;
                code |= (*block_ptr & mask) << (code_size + 1 - c);

                c -= read;
                left -= read;
                if (left == 0)
                {
                    ++block_ptr;
                    left = 8;

                    if (c != 0 && block_ptr == block_end)
                    {
                        if (fread(&block_size, 1, 1, config->input.file) < 1)
                        {
                            fprintf(stderr, "Failed to read block size\n");
                            goto clean_up;
                        }

                        // code spans 2 blocks. block size can't be zero
                        assert(block_size != 0);

                        if (fread(block, 1, block_size, config->input.file) < block_size)
                        {
                            fprintf(stderr, "Failed to read sub block\n");
                            goto clean_up;
                        }

                        block_ptr = block;
                        block_end = block_ptr + block_size;
                    }
                    continue;
                }

                *block_ptr >>= read;
            }


            if (code == clear_code)
            {
                code_size = lzw_code_size;
                dict_idx = stop_code + 1;
                prev = -1;
                continue;
            }
            else if (code == stop_code)
            {
                if (block_ptr + 1 != block_end)
                {
                    fprintf(stderr, "Early stop code in image data\n");
                    goto clean_up;
                }

                uint8_t terminator;
                if (fread(&terminator, 1, 1, config->input.file) < 1)
                {
                    fprintf(stderr, "Failed to read block terminator\n");
                    goto clean_up;
                }

                if (terminator != 0x0)
                {
                    fprintf(stderr, "Got block terminator '0x%X'"
                        " while processing image data\n", terminator);
                    goto clean_up;
                }

                status = true;
                goto clean_up;
            }

            if (prev > -1 && code_size < MAX_CODE_SIZE)
            {
                if (code > dict_idx)
                {
                    fprintf(stderr, "Got code '0x%X' larger than"
                        " dictionary index '0x%X'\n", code, dict_idx);
                    goto clean_up;
                }

                if (code == dict_idx)
                {
                    int ptr = prev;
                    while (dict[ptr].prev != -1)
                    {
                        ptr = dict[ptr].prev;
                    }
                    dict[dict_idx].byte = dict[ptr].byte;
                }
                else
                {
                    int ptr = code;
                    while (dict[ptr].prev != -1)
                    {
                        ptr = dict[ptr].prev;
                    }
                    dict[dict_idx].byte = dict[ptr].byte;
                }

                dict[dict_idx].prev = prev;
                dict[dict_idx].len = dict[prev].len + 1;
                ++dict_idx;

                if (dict_idx == (1 << (code_size + 1)) && code_size + 1 < MAX_CODE_SIZE)
                {
                    ++code_size;
                }
            }

            int len = dict[code].len;
            if (len > dec_size)
            {
                int new_size = dec_size + (dec_size >> 1);
                if (len > new_size)
                {
                    new_size = len;
                }

                decoded = realloc(decoded, new_size);
                dec_size = new_size;
            }

            uint8_t* dec_ptr = decoded + len;
            prev = code;
            while (code != -1)
            {
                assert(code != dict[code].prev);
                *(--dec_ptr) = dict[code].byte;
                code = dict[code].prev;
            }

            if (fwrite(decoded, 1, len, config->output.file) < len)
            {
                fprintf(stderr, "Failed to write decoded bytes\n");
                goto clean_up;
            }

        } // while (block_ptr != block_end)
    } // while (true)

clean_up:
    if (dict != NULL)
    {
        free(dict);
        dict = NULL;
    }
    if (decoded != NULL)
    {
        free(decoded);
        decoded = NULL;
    }

    return status;
}

static bool encoded_image_blocks(Config* config, uint8_t lzw_code_size)
{
    uint8_t block_size;
    uint8_t block[255];

    if (fwrite(&lzw_code_size, 1, 1, config->output.file) < 1)
    {
        fprintf(stderr, "Failed to write lzw code size\n");
        return false;
    }

    while (true)
    {
        if (fread(&block_size, 1, 1, config->input.file) < 1)
        {
            fprintf(stderr, "Failed to read block size\n");
            return false;
        }

        if (fwrite(&block_size, 1, 1, config->output.file) < 1)
        {
            fprintf(stderr, "Failed to write block size\n");
            return false;
        }

        if (block_size == 0) break;

        if (fread(block, 1, block_size, config->input.file) < block_size)
        {
            fprintf(stderr, "Failed to read sub block\n");
            return false;
        }

        if (fwrite(block, 1, block_size, config->output.file) < block_size)
        {
            fprintf(stderr, "Failed to write sub block\n");
            return false;
        }
    }

    return true;
}

static bool skip_sub_blocks(Config* config)
{
    uint8_t block_size;

    while (true)
    {
        if (fread(&block_size, 1, 1, config->input.file) < 1)
        {
            fprintf(stderr, "Failed to read block size\n");
            return false;
        }

        if (block_size == 0) break;

        if (fseek(config->input.file, block_size, SEEK_CUR) != 0)
        {
            fprintf(stderr, "Failed to skip sub block\n");
            return false;
        }
    }

    return true;
}

static Status process_image_data(Config* config, int index)
{
    uint8_t lzw_code_size;

    if (fread(&lzw_code_size, 1, 1, config->input.file) < 1)
    {
        fprintf(stderr, "Failed to read lzw code size\n");
        return ST_EXIT_FAILURE;
    }

    switch (config->option) {
    case 'e':
        if (config->index == index)
        {
            return encoded_image_blocks(config, lzw_code_size);
        }
        break;

    case 'd':
        if (config->index == index)
        {
            return decode_image_blocks(config, lzw_code_size);
        }
        break;

    case 'i':
        fprintf(config->output.file, "\tLZW code size: %u\n", lzw_code_size);
        break;
    }

    if (!skip_sub_blocks(config))
    {
        return ST_EXIT_FAILURE;
    }

    return ST_CONTINUE;
}

static bool process_extension(Config* config)
{
    uint8_t label;
    char buffer[13];

    if (fread(&label, 1, 1, config->input.file) < 1)
    {
        fprintf(stderr, "Error reading extension label\n");
        return false;
    }

    if (config->option != 'i')
    {
        return skip_sub_blocks(config);
    }

    switch (label) {

    case EXT_GRAPHIC_CONTROL:
        if (fread(buffer, 1, 6, config->input.file) < 6)
        {
            fprintf(stderr, "Failed to read graphic control extension\n");
            return false;
        }

        Graphic_control gce;
        Graphic_fields fields;

        memcpy(&gce.fields, buffer + 1, 1);
        memcpy(&gce.delay, buffer + 2, 2);
        memcpy(&gce.transp_color_idx, buffer + 4, 1);

        if (BYTEORDER == BIG_ENDIAN)
        {
            gce.delay = swap_bytes16(gce.delay);
        }

        fields.reserved = gce.fields >> 5;
        fields.disposal = (gce.fields >> 2) & 0x7;
        fields.user_input = (gce.fields >> 1) & 0x1;
        fields.has_transp = gce.fields & 0x1;

        fprintf(config->output.file, "\nGraphic control extension\n");
        fprintf(config->output.file, "\tFields: 0x%X\n", gce.fields);

        fprintf(config->output.file, "\t\tReserved: 0x%X\n", fields.reserved);
        fprintf(config->output.file, "\t\tDisposal method: %u\n", fields.disposal);
        fprintf(config->output.file, "\t\tUser input: %u\n", fields.user_input);
        fprintf(config->output.file, "\t\tHas transparent color: %u\n", fields.has_transp);

        fprintf(config->output.file, "\tDelay: %u\n", gce.delay);
        fprintf(config->output.file, "\tTransparent color index: %u\n", gce.transp_color_idx);

        return true;


    case EXT_COMMENT:
        fprintf(config->output.file, "\nComment extension\n");
        return skip_sub_blocks(config);

    case EXT_PLAIN_TEXT:
        if (fread(buffer, 1, 13, config->input.file) < 13)
        {
            fprintf(stderr, "Failed to read plain text extension\n");
            return false;
        }

        Plain_text pt;

        memcpy(&pt.grid_left, buffer + 1, 2);
        memcpy(&pt.grid_top, buffer + 3, 2);
        memcpy(&pt.grid_width, buffer + 5, 2);
        memcpy(&pt.grid_height, buffer + 7, 2);
        memcpy(&pt.cell_width, buffer + 9, 1);
        memcpy(&pt.cell_height, buffer + 10, 1);
        memcpy(&pt.fg_color_idx, buffer + 11, 1);
        memcpy(&pt.bg_color_idx, buffer + 12, 1);

        fprintf(config->output.file, "\nPlain text extension\n");
        fprintf(config->output.file, "\tText grid left position: %u\n", pt.grid_left);
        fprintf(config->output.file, "\tText grid top position: %u\n", pt.grid_top);
        fprintf(config->output.file, "\tText grid width: %u\n", pt.grid_width);
        fprintf(config->output.file, "\tText grid height: %u\n", pt.grid_height);
        fprintf(config->output.file, "\tCharacter cell width: %u\n", pt.cell_width);
        fprintf(config->output.file, "\tCharacter cell height: %u\n", pt.cell_height);
        fprintf(config->output.file, "\tText foreground color index: %u\n", pt.fg_color_idx);
        fprintf(config->output.file, "\tText background color index: %u\n", pt.bg_color_idx);

        return skip_sub_blocks(config);


    case EXT_APPLICATION:
        if (fread(buffer, 1, 12, config->input.file) < 12)
        {
            fprintf(stderr, "Failed to read application extension\n");
            return false;
        }

        Application app;

        memcpy(app.identifier, buffer + 1, 8);
        memcpy(app.auth_code, buffer + 9, 3);
        app.identifier[8] = '\0';
        app.auth_code[3] = '\0';

        fprintf(config->output.file, "\nApplication extension\n");
        fprintf(config->output.file, "\tIdentifier: %s\n", app.identifier);
        fprintf(config->output.file, "\tAuthentication code: %s\n", app.auth_code);

        return skip_sub_blocks(config);

    default:
        fprintf(config->output.file, "\nUnknown extension '0x%X'\n", label);
        return skip_sub_blocks(config);
    }
}

bool process_gifstream(Config* config)
{
    Status status;
    int color_table_size;
    int index = 0;

    status = process_header(config);
    if (status != ST_CONTINUE) return status;

    status = process_screen_descr(config, &color_table_size);
    if (status != ST_CONTINUE) return status;

    status = process_color_table(config, color_table_size, index);
    if (status != ST_CONTINUE) return status;

    uint8_t block_type = 0;
    while (true)
    {
        if (fread(&block_type, 1, 1, config->input.file) < 1)
        {
            if (feof(config->input.file)) break;

            fprintf(stderr, "Error reading block type\n");
            return false;
        }

        switch (block_type)
        {
            case IMAGE_DESCR:
                ++index;
                status = process_image_descr(config, &color_table_size, index);
                if (status != ST_CONTINUE) return status;

                status = process_color_table(config, color_table_size, index);
                if (status != ST_CONTINUE) return status;

                status = process_image_data(config, index);
                if (status != ST_CONTINUE) return status;

                break;

            case EXT_INTRO:
                if (!process_extension(config)) return false;
                break;

            case TRAILER:
                break;

            default:
                fprintf(stderr, "Unknown block type 0x%X\n", block_type);
                return false;
        }
    }

    if (config->option != 'i')
    {
        fprintf(stderr, "Index too large for option '-%c' (max=%d)\n",
            config->option, index);

        return false;
    }

    return true;
}
