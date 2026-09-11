#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#define GAME_SELECTOR_COUNT 8u

typedef enum {
    GAME_X = 0,
    GAME_Y,
    GAME_OMEGA_RUBY,
    GAME_ALPHA_SAPPHIRE,
    GAME_SUN,
    GAME_MOON,
    GAME_ULTRA_SUN,
    GAME_ULTRA_MOON
} GameId;

typedef struct {
    GameId id;
    const char *name;
    const char *short_name;
    u64 title_id;
    unsigned generation;
    bool present;
    bool adapter_ready;
    FS_MediaType media;
} GameEntry;

void games_scan(GameEntry out[GAME_SELECTOR_COUNT]);
int games_first_ready(const GameEntry games[GAME_SELECTOR_COUNT]);
const char *game_media_name(FS_MediaType media);
