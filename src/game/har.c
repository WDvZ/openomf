#include "game/har.h"
#include "utils/log.h"
#include "video/texture.h"
#include "video/video.h"
#include "audio/sound.h"
#include "utils/array.h"
#include "utils/list.h"
#include <stdlib.h>
#include <string.h>

// Internal functions
void har_add_ani_player(void *userdata, int id, int mx, int my);
void har_set_ani_finished(void *userdata, int id);

void har_phys(void *userdata, int x, int y);

void har_add_ani_player(void *userdata, int id, int mx, int my) {
    har *har = userdata;
    animation *ani = array_get(&har->animations, id);
    if(ani != NULL) {
        animationplayer np;
        DEBUG("spawning %id at %d + %d +%d", id, ani->sdani->start_x, mx, har->phy.pos.x);
        np.x = ani->sdani->start_x + mx + har->phy.pos.x;
        np.y = ani->sdani->start_y + my + har->phy.pos.y;
        animationplayer_create(&np, id, ani);
        animationplayer_set_direction(&np, har->direction);
        np.userdata = userdata;
        np.add_player = har_add_ani_player;
        np.del_player = har_set_ani_finished;
        list_append(&har->child_players, &np, sizeof(animationplayer));
        animationplayer_run(&np);
        DEBUG("Create animation %d @ x,y = %d,%d", id, np.x, np.y);
        return;
    } 
}

void har_set_ani_finished(void *userdata, int id) {
    har *har = userdata;
    iterator it;
    animationplayer *tmp = 0;
    
    list_iter_begin(&har->child_players, &it);
    while((tmp = iter_next(&it)) != NULL) {
        if(tmp->id == id) {
            tmp->finished = 1;
            return;
        }
    }
}

void har_phys(void *userdata, int x, int y) {
    DEBUG("setting recoil velocity to %f , %f", (float)x, (float)y);
    har *har = userdata;
    physics_recoil(&har->phy, (float)x, (float)y);
    physics_tick(&har->phy);
}

void har_switch_animation(har *har, int id) {
    animationplayer_free(&har->player);
    animationplayer_create(&har->player, id, array_get(&har->animations, id));
    animationplayer_set_direction(&har->player, har->direction);
    har->player.userdata = har;
    har->player.add_player = har_add_ani_player;
    har->player.del_player = har_set_ani_finished;
    har->player.phys = har_phys;
    animationplayer_run(&har->player);
}

void phycb_fall(physics_state *state, void *userdata) {
    har *h = userdata;
    if (h->state == STATE_JUMPING) {
        animationplayer_next_frame(&h->player);
        DEBUG("switching to falling");
    }
}

void phycb_floor_hit(physics_state *state, void *userdata, int flight_mode) {
    har *h = userdata;
    h->player.finished = 1;
    if (h->state == STATE_JUMPING) {
        h->state = STATE_STANDING;
        DEBUG("stopped jumping");
    } else if (h->state == STATE_RECOIL) {
        DEBUG("crashed into ground");
        har_switch_animation(h, ANIM_STANDUP);
        physics_move(&h->phy, 0.0f);
        //h->state = STATE_STANDING;
    }
}

void phycb_stop(physics_state *state, void *userdata) {
    har *h = userdata;
    if(h->state != STATE_STANDING && h->state != STATE_SCRAP && h->state != STATE_DESTRUCTION && h->state != STATE_RECOIL) {
        h->state = STATE_STANDING;
        har_switch_animation(h, ANIM_IDLE);
        animationplayer_set_repeat(&h->player, 1);
        DEBUG("switching to idle");
    }
}

void phycb_jump(physics_state *state, void *userdata) {
    har *h = userdata;
    h->state = STATE_JUMPING;
    har_switch_animation(h, ANIM_JUMPING);
    DEBUG("switching to jumping");
}

void phycb_move(physics_state *state, void *userdata) {
    har *h = userdata;
    if (h->state != STATE_RECOIL) {
        h->state = STATE_WALKING;
        har_switch_animation(h, ANIM_WALKING);
        animationplayer_set_repeat(&h->player, 1);
        DEBUG("switching to walking");
        // TODO: Handle reverse animation if necessary
    }
}

void phycb_crouch(physics_state *state, void *userdata) {
    har *h = userdata;
    if(h->state != STATE_CROUCHING) {
        h->state = STATE_CROUCHING;
        har_switch_animation(h, ANIM_CROUCHING);
        //animationplayer_set_repeat(&h->player, 1);
        DEBUG("crouching");
    }
}

int har_load(har *h, sd_palette *pal, int id, int x, int y, int direction) {
    // Physics & callbacks
    physics_init(&h->phy, x, y, 0.0f, 0.0f, 190, 10, 24, 295, 1.0f, h);
    h->phy.fall = phycb_fall;
    h->phy.floor_hit = phycb_floor_hit;
    h->phy.stop = phycb_stop;
    h->phy.jump = phycb_jump;
    h->phy.move = phycb_move;
    h->phy.crouch = phycb_crouch;

    h->cd_debug_enabled = 0;
    image_create(&h->cd_debug, 420, 350);
    h->cd_debug_tex.data = NULL;
    
    // Initial bot stuff
    h->state = STATE_STANDING;
    h->direction = direction;
    h->tick = 0; // TEMPORARY
    h->af = sd_af_create();
    
    // fill the input buffer with 'pauses'
    memset(h->inputs, '5', 10);
    h->inputs[10] = '\0';
    
    // Load AF
    int ret;
    switch (id) {
        case HAR_JAGUAR:
            ret = sd_af_load(h->af, "resources/FIGHTR0.AF");
            break;
        case HAR_SHADOW:
            ret = sd_af_load(h->af, "resources/FIGHTR1.AF");
            break;
        case HAR_THORN:
            ret = sd_af_load(h->af, "resources/FIGHTR2.AF");
            break;
        case HAR_PYROS:
            ret = sd_af_load(h->af, "resources/FIGHTR3.AF");
            break;
        case HAR_ELECTRA:
            ret = sd_af_load(h->af, "resources/FIGHTR4.AF");
            break;
        case HAR_KATANA:
            ret = sd_af_load(h->af, "resources/FIGHTR5.AF");
            break;
        case HAR_SHREDDER:
            ret = sd_af_load(h->af, "resources/FIGHTR6.AF");
            break;
        case HAR_FLAIL:
            ret = sd_af_load(h->af, "resources/FIGHTR7.AF");
            break;
        case HAR_GARGOYLE:
            ret = sd_af_load(h->af, "resources/FIGHTR8.AF");
            break;
        case HAR_CHRONOS:
            ret = sd_af_load(h->af, "resources/FIGHTR9.af");
            break;
        case HAR_NOVA:
            ret = sd_af_load(h->af, "resources/FIGHTR10.AF");
            break;
        default:
            return 1;
            break;
    }

    if (ret) {
        PERROR("Failed to load HAR %d", id);
        return 1;
    }
    
    // Handle animations
    animation *ani;
    sd_move *move;
    array_create(&h->animations);
    for(unsigned int i = 0; i < 70; i++) {
        move = h->af->moves[i];
        if(move != NULL) {
            // Create animation + textures, etc.
            ani = malloc(sizeof(animation));
            animation_create(ani, move->animation, pal, -1, h->af->soundtable);
            array_set(&h->animations, i, ani);
            DEBUG("Loading animation %d", i);
        }
    }
    
    list_create(&h->child_players);

    // Har properties
    h->health = h->health_max = 500;
    /*h->endurance = h->endurance_max = h->af->endurance;*/
    h->endurance = h->endurance_max = 500;
    
    // Start player with animation 11
    h->player.x = h->phy.pos.x;
    h->player.y = h->phy.pos.y;
    animationplayer_create(&h->player, ANIM_IDLE, array_get(&h->animations, ANIM_IDLE));
    animationplayer_set_direction(&h->player, h->direction);
    animationplayer_set_repeat(&h->player, 1);
    h->player.userdata = h;
    h->player.add_player = har_add_ani_player;
    h->player.del_player = har_set_ani_finished;
    h->player.phys = har_phys;
    animationplayer_run(&h->player);
    DEBUG("Har %d loaded!", id);
    return 0;
}

void har_free(har *h) {
    // Free AF
    sd_af_delete(h->af);
    
    // Free animations
    animation *ani = 0;
    iterator it;
    array_iter_begin(&h->animations, &it);
    while((ani = iter_next(&it)) != 0) {
        animation_free(ani);
        free(ani);
    }
    array_free(&h->animations);
    
    // Unload player
    animationplayer_free(&h->player);
}

void har_take_damage(har *har, int amount, const char *string) {
    har->health -= amount / 2.0f;
    har->endurance -= amount / 2.0f;
    if(har->health <= 0) {
        har->health = 0;
    }
    if(har->endurance <= 0) {
        har->endurance = 0;
    }
    DEBUG("HAR took %f damage, and its health is now %d -- %d", amount / 2.0f, har->health, har->endurance);
    if (har->health == 0 && har->endurance == 0) {
        har_switch_animation(har, ANIM_DEFEAT);
    } else if (har->endurance == 0) {
        har_switch_animation(har, ANIM_STUNNED);
    } else {
        animationplayer_free(&har->player);
        animationplayer_create(&har->player, ANIM_DAMAGE, array_get(&har->animations, ANIM_DAMAGE));
        animationplayer_set_direction(&har->player, har->direction);
        if (string) {
            animationplayer_set_string(&har->player, string);
        }
        har->player.userdata = har;
        har->player.add_player = har_add_ani_player;
        har->player.del_player = har_set_ani_finished;
        har->player.phys = har_phys;
        har->state = STATE_RECOIL;
        animationplayer_run(&har->player);
    }
}

void har_collision_har(har *har_a, har *har_b) {
    // Make stuff easier to get to :)
    int ani_id = har_a->player.id;

    sd_animation *ani = har_a->af->moves[ani_id]->animation;

    int other_ani_id = har_b->player.id;
    if (other_ani_id == ANIM_DAMAGE) {
        // can't kick them while they're down
        return;
    }

    // har_b frame must be ready here or else it will crash
    if(har_b->player.parser->is_frame_ready) {
        int frame_id = animationplayer_get_frame(&har_a->player);
        char other_frame_letter = animationplayer_get_frame_letter(&har_b->player);
        sd_sprite *sprite = har_b->af->moves[other_ani_id]->animation->sprites[(int)other_frame_letter - 65];
        int x = har_b->phy.pos.x + sprite->pos_x;
        int y = har_b->phy.pos.y + sprite->pos_y;
        int w = sprite->img->w;
        int h = sprite->img->h;
        int hit = 0;
        int boxhit = 0;

       if (har_b->direction == -1) {
            x = har_b->phy.pos.x + ((sprite->pos_x * har_b->direction) - sprite->img->w);
        }


       sd_vga_image *vga = sd_sprite_vga_decode(sprite->img);

       // XXX the graphical collision detection debug stuff below is commented out because it is a little buggy

       if(har_a->cd_debug_enabled) {
           image_clear(&har_a->cd_debug, color_create(0, 0, 0, 0));
           // draw the bounding box
           image_rect(&har_a->cd_debug, x+50, y+50, w, h, color_create(0, 0, 0, 255));

           // draw the 'ghost'
           for (int i = 0; i < vga->w*vga->h; i++) {
               if (vga->data[i] > 0 && vga->data[i] < 48) {
                   if (har_b->direction == -1) {
                       image_set_pixel(&har_a->cd_debug, x + 50 + (vga->w - (i % vga->w)), 50 + y + (i / vga->w), color_create(255, 255, 255, 100));
                   } else {
                       image_set_pixel(&har_a->cd_debug, x + 50 + (i % vga->w), 50 + y + (i / vga->w), color_create(255, 255, 255, 100));
                   }
               }
           }
       }

        image_set_pixel(&har_a->cd_debug, har_a->phy.pos.x + 50, har_a->phy.pos.y + 50, color_create(255, 255, 0, 255));
        image_set_pixel(&har_a->cd_debug, har_b->phy.pos.x + 50 , har_b->phy.pos.y + 50, color_create(255, 255, 0, 255));

        // Find collision points, if any
        for(int i = 0; i < ani->col_coord_count; i++) {
            if(ani->col_coord_table[i].y_ext == frame_id) {
                if(har_a->cd_debug_enabled) {
                    image_set_pixel(&har_a->cd_debug, 50 + (ani->col_coord_table[i].x * har_a->direction) + har_a->phy.pos.x, 50 + ani->col_coord_table[i].y + har_a->phy.pos.y, color_create(0, 0, 255, 255));
                }
                // coarse check vs sprite dimensions
                if ((ani->col_coord_table[i].x * har_a->direction) + har_a->phy.pos.x > x && (ani->col_coord_table[i].x * har_a->direction) +har_a->phy.pos.x < x + w) {
                    if (ani->col_coord_table[i].y + har_a->phy.pos.y > y && ani->col_coord_table[i].y + har_a->phy.pos.y < y + h) {
                        boxhit = 1;
                        if(har_a->cd_debug_enabled) {
                            image_set_pixel(&har_a->cd_debug, 50 + (ani->col_coord_table[i].x * har_a->direction) + har_a->phy.pos.x, 50 + ani->col_coord_table[i].y + har_a->phy.pos.y, color_create(0, 255, 0, 255));
                        }
                        // Do a fine grained per-pixel check for a hit

                        int xoff = x - har_a->phy.pos.x;
                        int yoff = har_b->phy.pos.y - har_a->phy.pos.y;
                        int xcoord = (ani->col_coord_table[i].x * har_a->direction) - xoff;
                        int ycoord = (h + (ani->col_coord_table[i].y - yoff)) - (h+sprite->pos_y);

                        if (har_b->direction == -1) {
                            xcoord = vga->w - xcoord;
                        }

                        unsigned char hitpixel = vga->data[ycoord*vga->w+xcoord];
                        if (hitpixel > 0 && hitpixel < 48) {
                            // this is a HAR pixel

                            DEBUG("hit point was %d, %d -- %d", xcoord, ycoord, h+sprite->pos_y);
                            hit = 1;
                            if (!har_a->cd_debug_enabled) {
                                // not debugging, we can break out of the loop and not draw any more pixels
                                break;
                            } else {
                                image_set_pixel(&har_a->cd_debug, 50 + (ani->col_coord_table[i].x * har_a->direction) + har_a->phy.pos.x,  50 + ani->col_coord_table[i].y + har_a->phy.pos.y, color_create(255, 0, 0, 255));
                            }
                        }
                    }
                }
            }
        }
        if (hit || har_a->af->moves[ani_id]->unknown[13] == CAT_CLOSE) {
            // close moves alwaya hit, for now
            har_take_damage(har_b, har_a->af->moves[ani_id]->unknown[17], har_a->af->moves[ani_id]->footer_string);
            if (har_b->health == 0 && har_b->endurance == 0) {
                har_a->state = STATE_VICTORY;
                har_switch_animation(har_a, ANIM_VICTORY);
            }
        }

        if ((hit || boxhit) && har_a->cd_debug_enabled) {
            if (har_a->cd_debug_tex.data) {
                texture_free(&har_a->cd_debug_tex);
            }

            texture_create_from_img(&har_a->cd_debug_tex, &har_a->cd_debug);
        }
    }

    har_a->close = 0;

    if (har_a->state == STATE_WALKING && har_a->direction == -1) {
        // walking towards the enemy
        // 35 is a made up number that 'looks right'
        if (har_a->phy.pos.x < har_b->phy.pos.x+35 && har_a->phy.pos.x > har_b->phy.pos.x) {
            har_a->phy.pos.x = har_b->phy.pos.x+35;
            har_a->close = 1;
        }
    }
    if (har_a->state == STATE_WALKING && har_a->direction == 1) {
        // walking towards the enemy
        if (har_a->phy.pos.x+35 > har_b->phy.pos.x && har_a->phy.pos.x < har_b->phy.pos.x) {
            har_a->phy.pos.x = har_b->phy.pos.x - 35;
            har_a->close = 1;
        }
    }
}

void har_tick(har *har) {

    har->player.x = har->phy.pos.x;
    if(physics_is_in_air(&har->phy)) {
        har->player.y = har->phy.pos.y - 20;
    } else {
        har->player.y = har->phy.pos.y;
    }
    har->tick++;

    iterator it;
    animationplayer *tmp = 0;
    list_iter_begin(&har->child_players, &it);
    while((tmp = iter_next(&it)) != NULL) {
        animationplayer_run(tmp);
    }

    if(har->tick > 3) {
        physics_tick(&har->phy);
        animationplayer_run(&har->player);
        har->tick = 0;
        //regenerate endurance if not attacking, and not dead
        if ((har->health > 0 || har->endurance > 0) && har->endurance < har->endurance_max &&
                (har->player.id == ANIM_IDLE || har->player.id == ANIM_CROUCHING ||
                 har->player.id == ANIM_WALKING || har->player.id == ANIM_JUMPING)) {
            har->endurance++;
        }
    }

    /*if (har->player.id == ANIM_DAMAGE && animationplayer_get_frame(&har->player) > 4) {*/
        // bail out of the 'hit' animation early, because the hit animation
        // contains ALL the hit aniamtions one after the other
        /*har->player.finished = 1;*/
    /*}*/

    if(har->player.finished) {
        if (har->state == STATE_RECOIL) {
            har->state = STATE_STANDING;
        }
        // 11 will never be finished, if it is set to repeat
        har->tick = 0;
        int animation = ANIM_IDLE;
        switch(har->state) {
            case STATE_STANDING:
                animation = ANIM_IDLE;
                break;
            case STATE_CROUCHING:
                animation = ANIM_CROUCHING;
                break;
            case STATE_SCRAP:
                animation = ANIM_VICTORY;
                break;
            case STATE_DESTRUCTION:
                animation = ANIM_VICTORY;
                break;
        }
        DEBUG("next animation is %d", animation);
        har_switch_animation(har, animation);
        animationplayer_set_repeat(&har->player, 1);
    }
}

void har_render(har *har) {
    iterator it;
    animationplayer *tmp = 0;
    list_iter_begin(&har->child_players, &it);
    while((tmp = iter_next(&it)) != NULL) {
        animationplayer_render(tmp);
    }

    animationplayer_render(&har->player);

}

void har_set_direction(har *har, int direction) {
    har->direction = direction;
    if (har->player.id != 11 && har->player.id != 4 && har->player.id != 10 && har->player.id != 1) {
        // don't change the direction yet, non-idle move in progress
        return;
    }
    animationplayer_set_direction(&har->player, har->direction);
}

void add_input(har *har, char c) {
    // only add it if it is not the current head of the array
    if (har->inputs[0] == c) {
        return;
    }

    // use memmove to move everything over one spot in the array, leaving the first slot free
    memmove((har->inputs)+1, har->inputs, 9);
    // write the new first element
    har->inputs[0] = c;
}

int move_allowed(har *har, sd_move *move) {
    int cat = move->unknown[13];
    switch(har->state) {
        case STATE_JUMPING:
            if(cat == CAT_JUMPING) {
                return 1;
            }
            break;
        /*case STATE_CROUCHING:*/
            /*if (cat == CAT_CROUCH) {*/
                /*return 1;*/
            /*}*/
            /*break;*/
        case  STATE_VICTORY:
            if (cat == CAT_SCRAP) {
                har->state = STATE_SCRAP;
                return 1;
            }
            break;
        case STATE_SCRAP:
            if (cat == CAT_DESTRUCTION) {
                har->state = STATE_DESTRUCTION;
                return 1;
            }
            break;
        default:
            if (cat == CAT_CLOSE) {
                DEBUG("trying a CLOSE move %d", har->close);
                return har->close;
            } else if (cat == CAT_JUMPING) {
                return 0;
            }
            // allow anything else, for now
            return 1;
            break;
    }
    return 0;
}

void har_act(har *har, int act_type) {
    if (!(
            har->player.id == ANIM_IDLE ||
            har->player.id == ANIM_CROUCHING ||
            har->player.id == ANIM_WALKING ||
            har->player.id == ANIM_JUMPING ||
            har->state == STATE_VICTORY ||
            har->state == STATE_SCRAP)) {
        // if we're not in the idle loop, bail for now
        return;
    }

    // for the reason behind the numbers, look at a numpad sometime
    switch(act_type) {
        case ACT_UP:
            add_input(har, '8');
            break;
        case ACT_DOWN:
            add_input(har, '2');
            break;
        case ACT_LEFT:
            if (har->direction == -1) {
                add_input(har, '6');
            } else {
                add_input(har, '4');
            }
            break;
        case ACT_RIGHT:
            if (har->direction == -1) {
                add_input(har, '4');
            } else {
                add_input(har, '6');
            }
            break;
        case ACT_UPRIGHT:
            if (har->direction == -1) {
                add_input(har, '7');
            } else {
                add_input(har, '9');
            }
            break;
        case ACT_UPLEFT:
            if (har->direction == -1) {
                add_input(har, '9');
            } else {
                add_input(har, '7');
            }
            break;
        case ACT_DOWNRIGHT:
            if (har->direction == -1) {
                add_input(har, '1');
            } else {
                add_input(har, '3');
            }
            break;
        case ACT_DOWNLEFT:
            if (har->direction == -1) {
                add_input(har, '3');
            } else {
                add_input(har, '1');
            }
            break;
        case ACT_KICK:
            add_input(har, 'K');
            break;
        case ACT_PUNCH:
            add_input(har, 'P');
            break;
        case ACT_STOP:
            if (har->state != STATE_VICTORY && har->state != STATE_SCRAP && har->state != STATE_DESTRUCTION) {
                physics_move(&har->phy, 0);
            }
            add_input(har, '5');
            break;
        case ACT_WALKLEFT:
            if (har->state != STATE_VICTORY && har->state != STATE_SCRAP && har->state != STATE_DESTRUCTION) {
                physics_move(&har->phy, -3.0f);
            }
            break;
        case ACT_WALKRIGHT:
            if (har->state != STATE_VICTORY && har->state != STATE_SCRAP && har->state != STATE_DESTRUCTION) {
                physics_move(&har->phy, 3.0f);
            }
            break;
        case ACT_CROUCH:
            if (har->state != STATE_VICTORY && har->state != STATE_SCRAP && har->state != STATE_DESTRUCTION) {
                physics_crouch(&har->phy);
            }
            break;
        case ACT_JUMP:
            if (har->state != STATE_VICTORY && har->state != STATE_SCRAP && har->state != STATE_DESTRUCTION) {
                physics_jump(&har->phy, -15.0f);
            }
            break;
    }
    /*DEBUG("input buffer is now %s", har->inputs);*/

    // try to find a matching move
    sd_move *move;
    size_t len;
    for(unsigned int i = 0; i < 70; i++) {
        move = har->af->moves[i];
        if(move != NULL) {
            len = strlen(move->move_string);
            if (!strncmp(move->move_string, har->inputs, len)) {
                if (move_allowed(har, move)) {
                    DEBUG("matched move %d with string %s", i, move->move_string);
                    DEBUG("input was %s", har->inputs);

                    physics_move(&har->phy, 0);
                    har_switch_animation(har, i);
                    har->inputs[0]='\0';
                    break;
                }
            }
        }
    }
}
