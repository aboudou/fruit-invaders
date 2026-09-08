/* Entry point: loads graphics resources then alternates between the title
 * screen and the (placeholder) game screen -- see CLAUDE.md, "Title
 * screen" and "Current project state". title_screen_run() returns on
 * Space, game_screen_run() returns on H, so this just bounces between the
 * two forever. */

#include "gameplay/game.h"
#include "gameplay/title.h"
#include "graphics/bigfont.h"
#include "graphics/decor.h"
#include "graphics/font.h"
#include "graphics/screen.h"
#include "graphics/sprites.h"
#include "sound/sound.h"

int main(void) {
    sprites_load();
    font_load();
    bigfont_load();
    decor_load();
    sound_init();

    for (;;) {
        screen_clear();
        title_screen_run();
        game_screen_run();
    }

    return 0;
}
