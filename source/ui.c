#include "ui.h"

#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static C3D_RenderTarget *s_top;
static C3D_RenderTarget *s_bottom;
static C2D_TextBuf s_textbuf;

static C2D_SpriteSheet s_pokemon;
static C2D_SpriteSheet s_shiny;
static C2D_SpriteSheet s_male;
static C2D_SpriteSheet s_female;
static C2D_SpriteSheet s_genderless;

static char *s_species_buf;
static char *s_nature_buf;
static char *s_ability_buf;
static char *s_species[722];
static char *s_natures[25];
static char *s_abilities[256];

static const u32 COL_BG_TOP      = C2D_Color32(17, 26, 43, 255);
static const u32 COL_BG_BOTTOM   = C2D_Color32(23, 34, 52, 255);
static const u32 COL_PANEL       = C2D_Color32(35, 49, 70, 255);
static const u32 COL_PANEL_ALT   = C2D_Color32(43, 59, 82, 255);
static const u32 COL_TEXT        = C2D_Color32(244, 247, 250, 255);
static const u32 COL_MUTED       = C2D_Color32(169, 183, 199, 255);
static const u32 COL_ACCENT      = C2D_Color32(63, 194, 218, 255);
static const u32 COL_GAME        = C2D_Color32(246, 191, 66, 255);
static const u32 COL_GREEN       = C2D_Color32(64, 186, 121, 255);
static const u32 COL_ORANGE      = C2D_Color32(224, 142, 62, 255);
static const u32 COL_RED         = C2D_Color32(208, 77, 82, 255);
static const u32 COL_BLACK       = C2D_Color32(8, 12, 18, 255);
static const u32 COL_SHINY       = C2D_Color32(251, 214, 72, 255);

static unsigned load_lines(const char *path, char **buffer,
                           char **lines, unsigned max_lines)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    if (len <= 0) { fclose(f); return 0; }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return 0; }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = '\0';

    unsigned count = 0;
    char *p = buf;
    while (*p && count < max_lines) {
        lines[count++] = p;
        char *e = strpbrk(p, "\r\n");
        if (!e) break;
        *e++ = '\0';
        while (*e == '\r' || *e == '\n') ++e;
        p = e;
    }
    *buffer = buf;
    return count;
}

static void text_at(const char *str, float x, float y, float scale,
                    u32 color)
{
    if (!str || !*str) return;
    C2D_Text t;
    C2D_TextParse(&t, s_textbuf, str);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.7f, scale, scale, color);
}

static void text_center(const char *str, float x, float y, float scale,
                        u32 color)
{
    if (!str || !*str) return;
    C2D_Text t;
    C2D_TextParse(&t, s_textbuf, str);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor | C2D_AlignCenter,
                 x, y, 0.7f, scale, scale, color);
}

static void panel(float x, float y, float w, float h, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.1f, w, h, color);
}

static void border(float x, float y, float w, float h, float thickness, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.2f, w, thickness, color);
    C2D_DrawRectSolid(x, y + h - thickness, 0.2f, w, thickness, color);
    C2D_DrawRectSolid(x, y, 0.2f, thickness, h, color);
    C2D_DrawRectSolid(x + w - thickness, y, 0.2f, thickness, h, color);
}

static void draw_pokemon_sprite(u16 species, float x, float y, float scale)
{
    if (!s_pokemon || species == 0 || species > 721) return;
    C2D_Image img = C2D_SpriteSheetGetImage(s_pokemon, species);
    C2D_DrawImageAt(img, x, y, 0.5f, NULL, scale, scale);
}

static void draw_shiny(float x, float y, float scale)
{
    if (!s_shiny) return;
    C2D_Image img = C2D_SpriteSheetGetImage(s_shiny, 0);
    C2D_DrawImageAt(img, x, y, 0.6f, NULL, scale, scale);
}

static void draw_gender(u8 gender, float x, float y)
{
    C2D_SpriteSheet sheet = NULL;
    if (gender == 0) sheet = s_male;
    else if (gender == 1) sheet = s_female;
    else if (gender == 2) sheet = s_genderless;
    if (!sheet) return;
    C2D_Image img = C2D_SpriteSheetGetImage(sheet, 0);
    C2D_DrawImageAt(img, x, y, 0.6f, NULL, 1.0f, 1.0f);
}

bool ui_init(void)
{
    gfxInitDefault();
    if (R_FAILED(romfsInit())) return false;
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
    C2D_Prepare();

    s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    s_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    s_textbuf = C2D_TextBufNew(8192);

    s_pokemon = C2D_SpriteSheetLoad("romfs:/assets/pkm_spritesheet.t3x");
    s_shiny = C2D_SpriteSheetLoad("romfs:/assets/icon_shiny.t3x");
    s_male = C2D_SpriteSheetLoad("romfs:/assets/icon_male.t3x");
    s_female = C2D_SpriteSheetLoad("romfs:/assets/icon_female.t3x");
    s_genderless = C2D_SpriteSheetLoad("romfs:/assets/icon_genderless.t3x");

    load_lines("romfs:/strings/species.txt", &s_species_buf, s_species, 722);
    load_lines("romfs:/strings/natures.txt", &s_nature_buf, s_natures, 25);
    load_lines("romfs:/strings/abilities.txt", &s_ability_buf, s_abilities, 256);

    return s_top && s_bottom && s_textbuf && s_pokemon;
}

void ui_exit(void)
{
    free(s_species_buf); s_species_buf = NULL;
    free(s_nature_buf); s_nature_buf = NULL;
    free(s_ability_buf); s_ability_buf = NULL;

    if (s_pokemon) C2D_SpriteSheetFree(s_pokemon);
    if (s_shiny) C2D_SpriteSheetFree(s_shiny);
    if (s_male) C2D_SpriteSheetFree(s_male);
    if (s_female) C2D_SpriteSheetFree(s_female);
    if (s_genderless) C2D_SpriteSheetFree(s_genderless);

    if (s_textbuf) C2D_TextBufDelete(s_textbuf);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
}

const char *ui_species_name(u16 species)
{
    return (species < 722 && s_species[species]) ? s_species[species] : "Unknown";
}

const char *ui_nature_name(u8 nature)
{
    return (nature < 25 && s_natures[nature]) ? s_natures[nature] : "Unknown";
}

const char *ui_ability_name(u8 ability)
{
    return (s_abilities[ability]) ? s_abilities[ability] : "Unknown";
}

static void selector_card(const GameEntry *g, int index, bool selected)
{
    const int col = index % 2;
    const int row = index / 2;
    const float x = 10.0f + col * 155.0f;
    const float y = 22.0f + row * 50.0f;
    const float w = 145.0f, h = 42.0f;

    u32 fill = COL_PANEL;
    if (g->present && g->adapter_ready) fill = C2D_Color32(31, 87, 79, 255);
    else if (g->present) fill = C2D_Color32(91, 67, 40, 255);

    panel(x, y, w, h, fill);
    border(x, y, w, h, selected ? 3.0f : 1.0f,
           selected ? COL_ACCENT : C2D_Color32(76, 94, 116, 255));

    text_at(g->name, x + 8, y + 6, 0.48f, COL_TEXT);

    char line[48];
    if (!g->present) snprintf(line, sizeof(line), "Not detected");
    else if (g->adapter_ready) snprintf(line, sizeof(line), "Ready - %s", game_media_name(g->media));
    else snprintf(line, sizeof(line), "Detected - adapter next");
    text_at(line, x + 8, y + 23, 0.38f,
            g->present ? (g->adapter_ready ? COL_GREEN : COL_ORANGE) : COL_MUTED);
}

void ui_render_game_selector(const GameEntry games[GAME_SELECTOR_COUNT],
                             int selected, const char *status)
{
    C2D_TextBufClear(s_textbuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(s_top, COL_BG_TOP);
    C2D_SceneBegin(s_top);

    text_at("PokeBank-CFW", 18, 16, 0.82f, COL_TEXT);
    text_at("Choose a Pokemon game", 18, 46, 0.52f, COL_MUTED);

    panel(18, 78, 364, 116, COL_PANEL);
    const GameEntry *g = &games[selected];
    text_at(g->name, 34, 94, 0.72f, COL_TEXT);
    char desc[128];
    if (!g->present) {
        snprintf(desc, sizeof(desc), "This title was not found on the SD card or game card.");
    } else if (!g->adapter_ready) {
        snprintf(desc, sizeof(desc), "Detected on %s. Gen %u adapter is not enabled yet.",
                 game_media_name(g->media), g->generation);
    } else {
        snprintf(desc, sizeof(desc), "Detected on %s. Save adapter is ready.",
                 game_media_name(g->media));
    }
    text_at(desc, 34, 128, 0.43f, g->present ? COL_TEXT : COL_MUTED);
    text_at(status ? status : "", 34, 165, 0.40f,
            (g->present && g->adapter_ready) ? COL_GREEN : COL_ORANGE);

    text_at("A Open   X Rescan   START Exit", 34, 207, 0.42f, COL_MUTED);

    C2D_TargetClear(s_bottom, COL_BG_BOTTOM);
    C2D_SceneBegin(s_bottom);
    text_center("Installed / inserted games", 160, 5, 0.45f, COL_TEXT);
    for (unsigned i = 0; i < GAME_SELECTOR_COUNT; ++i)
        selector_card(&games[i], (int)i, (int)i == selected);

    C3D_FrameEnd(0);
}

static void draw_box_slots(const UiPokemon slots[UI_BOX_SLOTS],
                           unsigned selected, bool focus,
                           float start_x, float start_y,
                           float cell_w, float cell_h,
                           u32 focus_color)
{
    for (unsigned i = 0; i < UI_BOX_SLOTS; ++i) {
        unsigned col = i % 6;
        unsigned row = i / 6;
        float x = start_x + col * cell_w;
        float y = start_y + row * cell_h;

        panel(x + 1, y + 1, cell_w - 3, cell_h - 3,
              slots[i].occupied ? COL_PANEL_ALT : C2D_Color32(27, 39, 57, 255));
        if (i == selected)
            border(x, y, cell_w - 1, cell_h - 1, focus ? 3.0f : 1.5f,
                   focus ? focus_color : COL_MUTED);

        if (slots[i].occupied) {
            draw_pokemon_sprite(slots[i].species, x + 4, y + 1, 0.72f);
            if (slots[i].shiny) draw_shiny(x + cell_w - 13, y + 2, 0.7f);
        }
    }
}

static void draw_detail(const UiPokemon *p)
{
    panel(249, 44, 139, 181, COL_PANEL);
    text_at("SELECTED", 262, 52, 0.37f, COL_MUTED);

    if (!p || !p->occupied) {
        text_center("Empty slot", 318, 126, 0.55f, COL_MUTED);
        return;
    }

    draw_pokemon_sprite(p->species, 286, 68, 1.85f);
    if (p->shiny) {
        draw_shiny(356, 67, 1.15f);
        text_at("SHINY", 340, 101, 0.36f, COL_SHINY);
    }
    draw_gender(p->gender, 263, 101);

    const char *species = ui_species_name(p->species);
    const char *shown = p->nickname[0] ? p->nickname : species;
    text_center(shown, 318, 117, 0.52f, COL_TEXT);

    char line[96];
    snprintf(line, sizeof(line), "#%03u  %s", p->species, species);
    text_at(line, 259, 140, 0.37f, COL_MUTED);

    snprintf(line, sizeof(line), "Nature  %s", ui_nature_name(p->nature));
    text_at(line, 259, 156, 0.37f, COL_TEXT);

    snprintf(line, sizeof(line), "Ability %s", ui_ability_name(p->ability));
    text_at(line, 259, 172, 0.37f, COL_TEXT);

    snprintf(line, sizeof(line), "IV %u/%u/%u/%u/%u/%u",
             p->ivs[0], p->ivs[1], p->ivs[2],
             p->ivs[3], p->ivs[4], p->ivs[5]);
    text_at(line, 259, 188, 0.33f, COL_TEXT);

    snprintf(line, sizeof(line), "OT %s  %u/%u",
             p->ot_name[0] ? p->ot_name : "-", p->tid, p->sid);
    text_at(line, 259, 204, 0.33f, COL_MUTED);
}

void ui_render_bank(const char *game_name, const char *media_name,
                    unsigned bank_box, unsigned game_box,
                    unsigned bank_selected, unsigned game_selected,
                    bool game_focus,
                    const UiPokemon bank_slots[UI_BOX_SLOTS],
                    const UiPokemon game_slots[UI_BOX_SLOTS],
                    const UiPokemon *detail,
                    const char *status,
                    bool overwrite_armed)
{
    C2D_TextBufClear(s_textbuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(s_top, COL_BG_TOP);
    C2D_SceneBegin(s_top);

    text_at("PokeBank-CFW", 14, 8, 0.62f, COL_TEXT);
    char title[64];
    snprintf(title, sizeof(title), "BANK BOX %03u / 100", bank_box + 1);
    text_at(title, 14, 30, 0.43f, game_focus ? COL_MUTED : COL_ACCENT);

    draw_box_slots(bank_slots, bank_selected, !game_focus,
                   12, 52, 38.0f, 34.0f, COL_ACCENT);
    draw_detail(detail);

    C2D_TargetClear(s_bottom, COL_BG_BOTTOM);
    C2D_SceneBegin(s_bottom);

    char game_title[96];
    snprintf(game_title, sizeof(game_title), "%s  |  %s  |  BOX %02u / 31",
             game_name ? game_name : "Game",
             media_name ? media_name : "",
             game_box + 1);
    text_at(game_title, 10, 7, 0.42f, game_focus ? COL_GAME : COL_MUTED);

    draw_box_slots(game_slots, game_selected, game_focus,
                   10, 30, 41.5f, 30.0f, COL_GAME);

    panel(8, 183, 304, 48, COL_PANEL);
    text_at(status ? status : "", 15, 188, 0.36f,
            overwrite_armed ? COL_RED : COL_TEXT);
    if (overwrite_armed)
        text_at("Press X again to confirm overwrite", 15, 204, 0.36f, COL_RED);
    else
        text_at("Y Focus  X Copy  L/R Box  SELECT Games", 15, 208, 0.32f, COL_MUTED);

    C3D_FrameEnd(0);
}

int ui_game_index_at_touch(touchPosition pos)
{
    for (int i = 0; i < (int)GAME_SELECTOR_COUNT; ++i) {
        int col = i % 2, row = i / 2;
        int x = 10 + col * 155;
        int y = 22 + row * 50;
        if (pos.px >= x && pos.px < x + 145 &&
            pos.py >= y && pos.py < y + 42) return i;
    }
    return -1;
}

int ui_game_slot_at_touch(touchPosition pos)
{
    for (int i = 0; i < 30; ++i) {
        int col = i % 6, row = i / 6;
        float x = 10.0f + col * 41.5f;
        float y = 30.0f + row * 30.0f;
        if (pos.px >= x && pos.px < x + 40.5f &&
            pos.py >= y && pos.py < y + 29.0f) return i;
    }
    return -1;
}
