include "constants.tl"
include "player.tl"
include "monster.tl"
include "item.tl"
include "room.tl"
include "level.tl"
include "utils.tl"
include "bestiary.tl"
include "combat.tl"
include "factory.tl"
include "mapgen.tl"
include "fov.tl"
include "ai.tl"
include "render.tl"

// ── Helpers ──

fun _game_attack(level=[], player=[], messages=[], mi=0) {
    monsters = level_monsters(level)
    dmg = combat_damage(player_atk(player), monster_def(monsters[mi]))
    monsters[mi] = monster_damage(monsters[mi], dmg)
    messages = messages + ["You hit " + bestiary_name(monster_type(monsters[mi])) + " for " + dmg + " damage!"]
    if monster_hp(monsters[mi]) <= 0 {
        monsters[mi] = monster_set_alive(monsters[mi], 0)
        player = player_inc_kills(player)
        gv = bestiary_gold(monster_type(monsters[mi]))
        player = player_add_gold(player, gv)
        messages = messages + [bestiary_name(monster_type(monsters[mi])) + " defeated! (+" + gv + " gold)"]
    }
    level = level_set_monsters(level, monsters)
    ret [level, player, messages]
}

fun _game_move_to(level=[], player=[], messages=[], nx=0, ny=0) {
    _S = 2
    level = level_set_pos(level, nx, ny)
    level = level_set_vis(level, fov_reveal(level_vis(level), level_px(level), level_py(level)))

    its = level_items(level)
    ii = item_find_at(level_px(level), level_py(level), its)
    if ii >= 0 {
        if item_is_potion(its[ii]) {
            player = player_heal(player, item_val(its[ii]))
            messages = messages + ["Health potion!"]
        } else {
            player = player_add_gold(player, item_val(its[ii]))
            messages = messages + ["Gold!"]
        }
        its[ii] = its[#its - 1]
        its = its[: #its - 1]
        level = level_set_items(level, its)
    }

    if level_map(level)[level_py(level)][level_px(level)] == _S {
        player = player_descend(player)
        messages = messages + ["Level " + player_level(player) + "!"]
        level = mapgen_level(player_level(player))
        level = level_set_vis(level, fov_reveal(level_vis(level), level_px(level), level_py(level)))
    }

    ret [level, player, messages]
}

// ── State ──

level = mapgen_level(1)
player = player_new(30, 30, 5, 2, 0, 1, 0)
messages = ["You enter the dungeon...", "Find > stairs to go deeper."]
quit = 0
level = level_set_vis(level, fov_reveal(level_vis(level), level_px(level), level_py(level)))
print("\x1b[?25l")

// ── Loop ──

for !quit {
    render_frame(level, player, messages)

    k = key()
    k0 = 0
    if #k > 0 { k0 = k[0] }
    if k0 == key_QUIT_Q || k0 == key_QUIT_CAPQ || k0 == key_CTRL_C { quit = 1 }

    dx = 0; dy = 0; took_action = 0
    if k0 == key_UP_W || k0 == key_UP_K   { dy = -1 }
    if k0 == key_DOWN_S || k0 == key_DOWN_J { dy = 1 }
    if k0 == key_LEFT_A || k0 == key_LEFT_H { dx = -1 }
    if k0 == key_RIGHT_D || k0 == key_RIGHT_L { dx = 1 }

    if dx != 0 || dy != 0 {
        nx = level_px(level) + dx
        ny = level_py(level) + dy

        if nx >= 0 && nx < map_W && ny >= 0 && ny < map_H &&
           level_map(level)[ny][nx] != 0 {

            mi = monster_find_at(nx, ny, level_monsters(level))

            if mi >= 0 {
                r = _game_attack(level, player, messages, mi)
            } else {
                r = _game_move_to(level, player, messages, nx, ny)
            }
            level = r[0]
            player = r[1]
            messages = r[2]
            took_action = 1
        }
    }

    if k0 == key_WAIT { took_action = 1 }

    if took_action {
        r = ai_process_monsters(level, player, messages)
        level = r[0]
        player = r[1]
        messages = r[2]
        if player_hp(player) <= 0 { quit = 1 }
    }
}

// ── Game over ──

print("\x1b[?25h")
utils_println("================================")
utils_println("           GAME OVER")
utils_println("================================")
utils_println("Level: " + player_level(player))
utils_println("Gold:  " + player_gold(player))
utils_println("Kills: " + player_kills(player))
utils_println("Thanks for playing!")
