#include "game_select.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    GameId id;
    const char *name;
    const char *short_name;
    u64 title_id;
    unsigned generation;
    bool adapter_ready;
} GameDef;

static const GameDef s_defs[GAME_SELECTOR_COUNT] = {
    { GAME_X,             "Pokemon X",           "X",  0x0004000000055D00ULL, 6, false },
    { GAME_Y,             "Pokemon Y",           "Y",  0x0004000000055E00ULL, 6, false },
    { GAME_OMEGA_RUBY,    "Pokemon Omega Ruby",  "OR", 0x000400000011C400ULL, 6, true  },
    { GAME_ALPHA_SAPPHIRE,"Pokemon Alpha Sapphire","AS",0x000400000011C500ULL, 6, true  },
    { GAME_SUN,           "Pokemon Sun",         "S",  0x0004000000164800ULL, 7, false },
    { GAME_MOON,          "Pokemon Moon",        "M",  0x0004000000175E00ULL, 7, false },
    { GAME_ULTRA_SUN,     "Pokemon Ultra Sun",   "US", 0x00040000001B5000ULL, 7, false },
    { GAME_ULTRA_MOON,    "Pokemon Ultra Moon",  "UM", 0x00040000001B5100ULL, 7, false }
};

static int index_for_title(u64 title_id)
{
    for (unsigned i = 0; i < GAME_SELECTOR_COUNT; ++i) {
        if (s_defs[i].title_id == title_id) return (int)i;
    }
    return -1;
}

static void scan_media(FS_MediaType media, GameEntry out[GAME_SELECTOR_COUNT],
                       bool overwrite_media)
{
    u32 count = 0;
    if (R_FAILED(AM_GetTitleCount(media, &count)) || count == 0) return;

    u64 *ids = (u64 *)malloc((size_t)count * sizeof(u64));
    if (!ids) return;

    u32 read = 0;
    if (R_SUCCEEDED(AM_GetTitleList(&read, media, count, ids))) {
        for (u32 i = 0; i < read; ++i) {
            int idx = index_for_title(ids[i]);
            if (idx >= 0) {
                if (!out[idx].present || overwrite_media) {
                    out[idx].media = media;
                }
                out[idx].present = true;
            }
        }
    }

    free(ids);
}

void games_scan(GameEntry out[GAME_SELECTOR_COUNT])
{
    if (!out) return;

    memset(out, 0, sizeof(GameEntry) * GAME_SELECTOR_COUNT);
    for (unsigned i = 0; i < GAME_SELECTOR_COUNT; ++i) {
        out[i].id = s_defs[i].id;
        out[i].name = s_defs[i].name;
        out[i].short_name = s_defs[i].short_name;
        out[i].title_id = s_defs[i].title_id;
        out[i].generation = s_defs[i].generation;
        out[i].adapter_ready = s_defs[i].adapter_ready;
        out[i].media = MEDIATYPE_SD;
    }

    /* Prefer an inserted cartridge over an SD-installed copy of the same title. */
    scan_media(MEDIATYPE_SD, out, false);

    FS_CardType card_type;
    if (R_SUCCEEDED(FSUSER_GetCardType(&card_type)) && card_type == CARD_CTR) {
        scan_media(MEDIATYPE_GAME_CARD, out, true);
    }
}

int games_first_ready(const GameEntry games[GAME_SELECTOR_COUNT])
{
    if (!games) return 0;
    for (unsigned i = 0; i < GAME_SELECTOR_COUNT; ++i) {
        if (games[i].present && games[i].adapter_ready) return (int)i;
    }
    for (unsigned i = 0; i < GAME_SELECTOR_COUNT; ++i) {
        if (games[i].present) return (int)i;
    }
    return 0;
}

const char *game_media_name(FS_MediaType media)
{
    switch (media) {
        case MEDIATYPE_GAME_CARD: return "Cartridge";
        case MEDIATYPE_SD: return "SD";
        default: return "Unknown";
    }
}
