#include "game/har.h"
#include "utils/log.h"
#include "video/texture.h"
#include "video/video.h"
#include "audio/sound.h"
#include <stdlib.h>
#include <string.h>

int har_load(har *h, sd_palette *pal, char *soundtable, const char *file, int x, int y, int direction) {
    h->x = x;
    h->y = y;
    h->state = STATE_STANDING;
    h->direction = direction;
    h->x_per_tick = 0;
    h->y_per_tick = 0;
    h->tick = 0; // TEMPORARY
    h->af = sd_af_create();
    // fill the input buffer with 'pauses'
    memset(h->inputs, '5', 10);
    h->inputs[10] = '\0';
    
    // Load AF
    if(sd_af_load(h->af, file)) {
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
            animation_create(ani, move->animation, pal, -1, soundtable);
            array_set(&h->animations, i, ani);
            DEBUG("Loading animation %d", i);
        }
    }
    
    // Start player with animation 11
    h->player.x = h->x;
    h->player.y = h->y;
    animationplayer_create(&h->player, 11, array_get(&h->animations, 11));
    animationplayer_set_direction(&h->player, h->direction);
    animationplayer_set_repeat(&h->player, 1);
    animationplayer_run(&h->player);
    DEBUG("Har %s loaded!", file);
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

void har_tick(har *har) {
    har->x += har->x_per_tick;
    har->y += har->y_per_tick;
    
    //bounds
    if (har->x < 24) {
        har->x = 24;
    }
    if (har->x > 295) {
        har->x = 295;
    }
    if (har->y < 0) {
        har->y = 0;
        // start falling
        har->y_per_tick = 2;
        // jump to next frame in animation
        DEBUG("switching to falling");
        animationplayer_next_frame(&har->player);
    }

    // jumping magic
    if (har->y > 190 - 40 && har->state == STATE_JUMPING) {
        har->y = 190;
        har->y_per_tick = 0;
        har->player.finished = 1;
        DEBUG("stopped jumping");
        har->state = STATE_STANDING;
    }

    har->player.x = har->x;
    har->player.y = har->y;
    har->tick++;

    if(har->tick > 3) {
        animationplayer_run(&har->player);
        har->tick = 0;
    }
    if(har->player.finished) {
        // 11 will never be finished, if it is set to repeat
        har->tick = 0;
        int animation = 11;
        switch(har->state) {
            case STATE_STANDING:
                animation = 11;
                break;
            case STATE_CROUCHING:
                animation = 4;
                break;
        }
        DEBUG("next animation is %d", animation);
        animationplayer_free(&har->player);
        animationplayer_create(&har->player, animation, array_get(&har->animations, animation));
        animationplayer_set_repeat(&har->player, 1);
        animationplayer_set_direction(&har->player, har->direction);
        animationplayer_run(&har->player);
    }
}

void har_render(har *har) {
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

void har_act(har *har, int act_type) {
    if (har->player.id != 11 && har->player.id != 4 && har->player.id != 10 && har->player.id != 1) {
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
            if (har->state != STATE_STANDING && har->state != STATE_JUMPING) {
                DEBUG("standing");
                har->state = STATE_STANDING;
                animationplayer_free(&har->player);
                animationplayer_create(&har->player, 11, array_get(&har->animations, 11));
                animationplayer_set_direction(&har->player, har->direction);
                animationplayer_set_repeat(&har->player, 1);
                animationplayer_run(&har->player);
            } else if (har->state == STATE_JUMPING && har->y_per_tick < 0) {
                // start falling
                har->y_per_tick = 2;
                // jump to next frame in animation
                DEBUG("switching to falling");
                animationplayer_next_frame(&har->player);
            }
            har->x_per_tick = 0;
            add_input(har, '5');
            break;
        case ACT_WALKLEFT:
            if (har->state != STATE_WALKING && har->state != STATE_JUMPING) {
                DEBUG("walk left");
                har->state = STATE_WALKING;
                // TODO technically we should play this animation in reverse
                animationplayer_free(&har->player);
                animationplayer_create(&har->player, 10, array_get(&har->animations, 10));
                animationplayer_set_direction(&har->player, har->direction);
                animationplayer_set_repeat(&har->player, 1);
                animationplayer_run(&har->player);
            }
            har->x_per_tick = -1;
            break;
        case ACT_WALKRIGHT:
            if (har->state != STATE_WALKING && har->state != STATE_JUMPING) {
                DEBUG("walk right");
                har->state = STATE_WALKING;
                animationplayer_free(&har->player);
                animationplayer_create(&har->player, 10, array_get(&har->animations, 10));
                animationplayer_set_direction(&har->player, har->direction);
                animationplayer_set_repeat(&har->player, 1);
                animationplayer_run(&har->player);
            }
            har->x_per_tick = 1;
            break;
        case ACT_CROUCH:
            if (har->state != STATE_CROUCHING && har->state != STATE_JUMPING) {
                DEBUG("crouching");
                har->state = STATE_CROUCHING;
                har->x_per_tick = 0;
                animationplayer_free(&har->player);
                animationplayer_create(&har->player, 4, array_get(&har->animations, 4));
                animationplayer_set_direction(&har->player, har->direction);
                animationplayer_set_repeat(&har->player, 1);
                animationplayer_run(&har->player);
            }
            break;
        case ACT_JUMP:
            if (har->state != STATE_JUMPING) {
                DEBUG("jump");
                har->state = STATE_JUMPING;
                har->y_per_tick = -2;
                har->y -= 40;
                animationplayer_free(&har->player);
                animationplayer_create(&har->player, 1, array_get(&har->animations, 1));
                animationplayer_set_direction(&har->player, har->direction);
                animationplayer_run(&har->player);
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
                if ((har->state == STATE_JUMPING && move->unknown[13] == CAT_JUMPING) || har->state != STATE_JUMPING) {
                    DEBUG("matched move %d with string %s", i, move->move_string);
                    DEBUG("input was %s", har->inputs);
                    animationplayer_free(&har->player);
                    animationplayer_create(&har->player, i, array_get(&har->animations, i));
                    animationplayer_set_direction(&har->player, har->direction);
                    animationplayer_run(&har->player);
                    har->inputs[0]='\0';
                    har->x_per_tick = 0;
                    if (har->state == STATE_JUMPING) {
                        har->y_per_tick = 2;
                    } else {
                        har->y_per_tick = 0;
                    }
                    break;
                }
            }
        }
    }
}
