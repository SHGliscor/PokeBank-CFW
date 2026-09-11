#pragma once

#include <3ds.h>
#include <stdbool.h>
#include "game_select.h"

#define UI_BOX_SLOTS 30u

typedef struct {
    bool occupied;
    bool shiny;
    u16 species;
    u16 held_item;
    u16 tid;
    u16 sid;
    u32 pid;
    u8 nature;
    u8 ability;
    u8 gender;
    u8 form;
    u8 ivs[6];
    char nickname[14];
    char ot_name[14];
} UiPokemon;

bool ui_init(void);
void ui_exit(void);

void ui_render_game_selector(const GameEntry games[GAME_SELECTOR_COUNT],
                             int selected, const char *status);

void ui_render_bank(const char *game_name, const char *media_name,
                    unsigned bank_box, unsigned game_box,
                    unsigned bank_selected, unsigned game_selected,
                    bool game_focus,
                    const UiPokemon bank_slots[UI_BOX_SLOTS],
                    const UiPokemon game_slots[UI_BOX_SLOTS],
                    const UiPokemon *detail,
                    const char *status,
                    bool overwrite_armed);

int ui_game_index_at_touch(touchPosition pos);
int ui_game_slot_at_touch(touchPosition pos);

const char *ui_species_name(u16 species);
const char *ui_nature_name(u8 nature);
const char *ui_ability_name(u8 ability);
