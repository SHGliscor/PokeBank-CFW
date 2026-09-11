#include "oras.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OR_TITLE_ID 0x000400000011C400ULL
#define AS_TITLE_ID 0x000400000011C500ULL
#define ORAS_SAVE_SIZE 0x76000ULL
#define ORAS_BOX_OFFSET 0x33000ULL
#define ORAS_LAST_VIEWED_BOX_OFFSET 0x483FULL
#define PK6_BLOCK_LENGTH 56u
#define PK6_ENCRYPTION_START 8u

static bool s_am_ready = false;
static const u16 s_main_path[] = {'/', 'm', 'a', 'i', 'n', 0};

static const u8 s_block_positions[24][4] = {
    {0,1,2,3}, {0,1,3,2}, {0,2,1,3}, {0,3,1,2},
    {0,2,3,1}, {0,3,2,1}, {1,0,2,3}, {1,0,3,2},
    {2,0,1,3}, {3,0,1,2}, {2,0,3,1}, {3,0,2,1},
    {1,2,0,3}, {1,3,0,2}, {2,1,0,3}, {3,1,0,2},
    {2,3,0,1}, {3,2,0,1}, {1,2,3,0}, {1,3,2,0},
    {2,1,3,0}, {3,1,2,0}, {2,3,1,0}, {3,2,1,0}
};

static u16 read_le16(const u8 *p)
{
    return (u16)(p[0] | ((u16)p[1] << 8));
}

static u32 read_le32(const u8 *p)
{
    return (u32)p[0] |
           ((u32)p[1] << 8) |
           ((u32)p[2] << 16) |
           ((u32)p[3] << 24);
}

static bool is_oras_title(u64 id)
{
    return id == OR_TITLE_ID || id == AS_TITLE_ID;
}

const char *oras_game_name(OrasGame game)
{
    switch (game) {
        case ORAS_GAME_OMEGA_RUBY: return "Omega Ruby";
        case ORAS_GAME_ALPHA_SAPPHIRE: return "Alpha Sapphire";
        default: return "None";
    }
}

const char *oras_media_name(FS_MediaType media)
{
    switch (media) {
        case MEDIATYPE_GAME_CARD: return "Cartridge";
        case MEDIATYPE_SD: return "SD";
        default: return "Unknown";
    }
}

Result oras_services_init(void)
{
    Result res = amInit();
    s_am_ready = R_SUCCEEDED(res);
    return res;
}

void oras_services_exit(void)
{
    if (s_am_ready) {
        amExit();
        s_am_ready = false;
    }
}

static bool find_title_on_media(FS_MediaType media, u64 *out_id)
{
    u32 count = 0;
    Result res = AM_GetTitleCount(media, &count);
    if (R_FAILED(res) || count == 0) return false;

    u64 *ids = (u64 *)malloc((size_t)count * sizeof(u64));
    if (!ids) return false;

    u32 read = 0;
    res = AM_GetTitleList(&read, media, count, ids);
    if (R_FAILED(res)) {
        free(ids);
        return false;
    }

    bool found = false;
    for (u32 i = 0; i < read; ++i) {
        if (is_oras_title(ids[i])) {
            *out_id = ids[i];
            found = true;
            break;
        }
    }

    free(ids);
    return found;
}

static Result open_main_save(const OrasSource *source,
                             FS_Archive *archive,
                             Handle *file,
                             u64 *size)
{
    if (!source || !source->found) return (Result)-1;

    const u32 path_data[3] = {
        (u32)source->media,
        (u32)source->title_id,
        (u32)(source->title_id >> 32)
    };
    FS_Path archive_path = { PATH_BINARY, sizeof(path_data), path_data };

    Result res = FSUSER_OpenArchive(archive, ARCHIVE_USER_SAVEDATA, archive_path);
    if (R_FAILED(res)) return res;

    res = FSUSER_OpenFile(file, *archive,
                          fsMakePath(PATH_UTF16, s_main_path),
                          FS_OPEN_READ, 0);
    if (R_FAILED(res)) {
        FSUSER_CloseArchive(*archive);
        *archive = 0;
        return res;
    }

    res = FSFILE_GetSize(*file, size);
    if (R_FAILED(res)) {
        FSFILE_Close(*file);
        FSUSER_CloseArchive(*archive);
        *file = 0;
        *archive = 0;
    }

    return res;
}

static void close_main_save(FS_Archive archive, Handle file)
{
    if (file) FSFILE_Close(file);
    if (archive) FSUSER_CloseArchive(archive);
}

bool oras_detect(OrasSource *out, char *detail, size_t detail_size)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    if (!s_am_ready) {
        snprintf(detail, detail_size, "AM service unavailable");
        return false;
    }

    u64 id = 0;
    FS_MediaType media = MEDIATYPE_SD;

    FS_CardType card_type;
    if (R_SUCCEEDED(FSUSER_GetCardType(&card_type)) && card_type == CARD_CTR &&
        find_title_on_media(MEDIATYPE_GAME_CARD, &id)) {
        media = MEDIATYPE_GAME_CARD;
    } else if (find_title_on_media(MEDIATYPE_SD, &id)) {
        media = MEDIATYPE_SD;
    } else {
        snprintf(detail, detail_size, "No ORAS title detected");
        return false;
    }

    out->found = true;
    out->title_id = id;
    out->media = media;
    out->game = (id == OR_TITLE_ID) ? ORAS_GAME_OMEGA_RUBY : ORAS_GAME_ALPHA_SAPPHIRE;

    FS_Archive archive = 0;
    Handle file = 0;
    u64 size = 0;
    Result res = open_main_save(out, &archive, &file, &size);
    if (R_FAILED(res)) {
        snprintf(detail, detail_size, "Save open failed: %08lX",
                 (unsigned long)(u32)res);
        memset(out, 0, sizeof(*out));
        return false;
    }

    out->save_size = size;
    if (size < ORAS_SAVE_SIZE) {
        snprintf(detail, detail_size, "Save too small: %lu bytes",
                 (unsigned long)size);
        close_main_save(archive, file);
        memset(out, 0, sizeof(*out));
        return false;
    }

    u8 current = 0;
    u32 bytes_read = 0;
    res = FSFILE_Read(file, &bytes_read, ORAS_LAST_VIEWED_BOX_OFFSET,
                      &current, sizeof(current));
    if (R_SUCCEEDED(res) && bytes_read == 1 && current < ORAS_BOX_COUNT) {
        out->current_box = current;
    } else {
        out->current_box = 0;
    }

    close_main_save(archive, file);

    snprintf(detail, detail_size, "%s %s save OK",
             oras_game_name(out->game), oras_media_name(out->media));
    return true;
}

static void pk6_decrypt(u8 data[PK6_BOX_LENGTH])
{
    const bool encrypted =
        read_le16(data + 0xC8) != 0 ||
        read_le16(data + 0x58) != 0;

    if (!encrypted) return;

    const u32 ec = read_le32(data);
    u32 seed = ec;

    for (unsigned i = PK6_ENCRYPTION_START; i < PK6_BOX_LENGTH; i += 2) {
        seed = seed * 0x41C64E6Du + 0x6073u;
        data[i] ^= (u8)(seed >> 16);
        data[i + 1] ^= (u8)(seed >> 24);
    }

    u8 temp[PK6_BLOCK_LENGTH * 4];
    memcpy(temp, data + PK6_ENCRYPTION_START, sizeof(temp));

    const unsigned sv = ((ec >> 13) & 31u) % 24u;
    for (unsigned block = 0; block < 4; ++block) {
        const unsigned src = s_block_positions[sv][block];
        memcpy(data + PK6_ENCRYPTION_START + block * PK6_BLOCK_LENGTH,
               temp + src * PK6_BLOCK_LENGTH,
               PK6_BLOCK_LENGTH);
    }
}

static u16 pk6_checksum(const u8 data[PK6_BOX_LENGTH])
{
    u32 sum = 0;
    for (unsigned i = 8; i < PK6_BOX_LENGTH; i += 2) {
        sum += read_le16(data + i);
    }
    return (u16)sum;
}

static void decode_slot(const u8 raw[PK6_BOX_LENGTH], OrasSlotInfo *out)
{
    memset(out, 0, sizeof(*out));
    memcpy(out->raw, raw, PK6_BOX_LENGTH);

    bool all_zero = true;
    for (unsigned i = 0; i < PK6_BOX_LENGTH; ++i) {
        if (raw[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) return;

    u8 data[PK6_BOX_LENGTH];
    memcpy(data, raw, sizeof(data));
    pk6_decrypt(data);

    out->species = read_le16(data + 0x08);
    if (out->species == 0 || out->species > 721) return;

    out->occupied = true;
    out->tid = read_le16(data + 0x0C);
    out->sid = read_le16(data + 0x0E);
    out->ability = data[0x14];
    out->pid = read_le32(data + 0x18);
    out->nature = data[0x1C];

    const u16 stored_checksum = read_le16(data + 0x06);
    out->checksum_valid = stored_checksum == pk6_checksum(data);

    const u16 psv = (u16)((out->pid & 0xFFFFu) ^ (out->pid >> 16));
    const u16 tsv = (u16)(out->tid ^ out->sid);
    out->shiny = (u16)(psv ^ tsv) < 16u;
}

bool oras_read_box(const OrasSource *source, unsigned box,
                   OrasSlotInfo out[ORAS_SLOTS_PER_BOX],
                   char *detail, size_t detail_size)
{
    if (!source || !source->found || box >= ORAS_BOX_COUNT || !out) {
        snprintf(detail, detail_size, "Invalid ORAS box request");
        return false;
    }

    FS_Archive archive = 0;
    Handle file = 0;
    u64 size = 0;
    Result res = open_main_save(source, &archive, &file, &size);
    if (R_FAILED(res)) {
        snprintf(detail, detail_size, "Save open failed: %08lX",
                 (unsigned long)(u32)res);
        return false;
    }

    const u64 offset = ORAS_BOX_OFFSET +
        (u64)box * ORAS_SLOTS_PER_BOX * PK6_BOX_LENGTH;
    const u32 block_size = ORAS_SLOTS_PER_BOX * PK6_BOX_LENGTH;
    u8 raw[ORAS_SLOTS_PER_BOX * PK6_BOX_LENGTH];
    u32 bytes_read = 0;

    res = FSFILE_Read(file, &bytes_read, offset, raw, block_size);
    close_main_save(archive, file);

    if (R_FAILED(res) || bytes_read != block_size) {
        snprintf(detail, detail_size, "Box read failed: %08lX (%lu/%lu)",
                 (unsigned long)(u32)res,
                 (unsigned long)bytes_read,
                 (unsigned long)block_size);
        return false;
    }

    unsigned occupied = 0;
    for (unsigned slot = 0; slot < ORAS_SLOTS_PER_BOX; ++slot) {
        decode_slot(raw + slot * PK6_BOX_LENGTH, &out[slot]);
        if (out[slot].occupied) occupied++;
    }

    snprintf(detail, detail_size, "%s box %u: %u Pokemon",
             oras_game_name(source->game), box + 1, occupied);
    return true;
}
