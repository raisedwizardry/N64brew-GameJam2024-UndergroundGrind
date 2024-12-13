#include <libdragon.h>
#include "../../core.h"
#include "../../minigame.h"
#include "./snakeplayer.h"
#include <rspq_profile.h>

#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>


const MinigameDef minigame_def = {
    .gamename = "Underground Grind",
    .developername = "raisedwizardry",
    .description = "Be the first one to find the treasure by grinding it out underground in a 10 by 10 by 10 grid",
    .instructions = "Tap A and Z in squence repeatedly to dig. Use the control stick to change digging directions"
};

#define FONT_TEXT           1
#define FONT_BILLBOARD      2
#define TEXT_COLOR          0x6CBB3CFF
#define TEXT_OUTLINE        0x30521AFF

#define ATTACK_TIME_START   0.333f
#define ATTACK_TIME_END     0.4f

#define COUNTDOWN_DELAY     3.0f
#define GO_DELAY            1.0f
#define WIN_DELAY           5.0f
#define WIN_SHOW_DELAY      2.0f

#define BILLBOARD_YOFFSET   15.0f


static float getFloorHeight(const T3DVec3 *pos) {
  // Usually you would have some collision / raycast for this
  // Here we just hardcode the floor height and the one stair
  if(pos->v[2] < -75.0f) {
    float stairScale = 1.0f - (115 + pos->v[2]) / (115.0f-75.0f);
    stairScale = fmaxf(fminf(stairScale, 1.0f), 0.0f);
    return stairScale * 38;
  }
  return 0.0f;
}

rdpq_font_t *font;
rdpq_font_t *fontBillboard;

SnakePlayer players[MAXPLAYERS];

float countDownTimer;
bool isEnding;
float endTimer;
PlyNum winner;

float time = 0.0f;
float rotAngle = 0.0f;

T3DVec3 camTarget = {{0,0,0}};
T3DVec3 camTargetCurr = {{0,0,0}};
T3DVec3 camDir = {{1,1,1}};
float viewZoom = 96.0f;
float textTimer = 7.0f;
rspq_syncpoint_t syncPoint = 0;

xm64player_t music;

rspq_block_t *dplMap;

color_t lightColorOff = {0,0,0, 0xFF};
uint8_t colorAmbient[4] = {20, 20, 20, 0xFF};

T3DMat4FP* modelMatFP;
T3DMat4FP* cursorMatFP;
T3DMat4FP* matLightFP;

T3DModel *model;


T3DViewport viewport;

wav64_t sfx_start;
wav64_t sfx_countdown;
wav64_t sfx_stop;
wav64_t sfx_winner;


void minigame_init()
{

  debug_init_isviewer();
	debug_init_usblog();
	asset_init_compression(2);

  //const initialPlayerDetail 
  const color_t colors[] = {
    PLAYERCOLOR_1,
    PLAYERCOLOR_2,
    PLAYERCOLOR_3,
    PLAYERCOLOR_4,
  };
  dfs_init(DFS_DEFAULT_LOCATION);

  display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
  rdpq_init();
  //rdpq_debug_start();

  joypad_init();
  t3d_init((T3DInitParams){});
  viewport = t3d_viewport_create();

  // Credits (CC0):  
  model = t3d_model_load("rom://undergroundgrind/scene.t3dm");

  font = rdpq_font_load("rom:/undergroundgrind/m6x11plus.font64");
  rdpq_text_register_font(FONT_TEXT, font);
  rdpq_font_style(font, 0, &(rdpq_fontstyle_t){.color = color_from_packed32(TEXT_COLOR) });

  fontBillboard = rdpq_font_load("rom:/squarewave.font64");
  rdpq_text_register_font(FONT_BILLBOARD, fontBillboard);
  for (size_t i = 0; i < MAXPLAYERS; i++)
  {
    rdpq_font_style(fontBillboard, i, &(rdpq_fontstyle_t){ .color = colors[i] });
  }

  modelMatFP = malloc_uncached(sizeof(T3DMat4FP));
  cursorMatFP = malloc_uncached(sizeof(T3DMat4FP));
  //matLightFP = malloc_uncached(sizeof(T3DMat4FP) * 4);


  t3d_mat4fp_from_srt_euler(modelMatFP,
    (float[]){0.15f, 0.15f, 0.15f},
    (float[]){0.0f, 0.0f, 0.0f},
    (float[]){0.0f, 0.0f, 0.0f}
  );

  rspq_block_begin();
    t3d_model_draw(model);
  dplMap = rspq_block_end();

  T3DVec3 startPositions[] = {
    (T3DVec3){{-100,0.15f,0}},
    (T3DVec3){{0,0.15f,-100}},
    (T3DVec3){{100,0.15f,0}},
    (T3DVec3){{0,0.15f,100}},
  };


  float startRotations[] = {
    M_PI/2,
    0,
    3*M_PI/2,
    M_PI
  };

  for (size_t i = 0; i < MAXPLAYERS; i++)
  {
  	player_init(&players[i], colors[i], startPositions[i], startRotations[i]);
    players[i].plynum = i;
  }

  wav64_open(&sfx_start, "rom:/core/Start.wav64");
  wav64_open(&sfx_countdown, "rom:/core/Countdown.wav64");
  wav64_open(&sfx_stop, "rom:/core/Stop.wav64");
  wav64_open(&sfx_winner, "rom:/core/Winner.wav64");
  xm64player_open(&music, "rom:/undergroundgrind/bottled_bubbles.xm64");
  xm64player_play(&music, 0);
}

void player_draw(SnakePlayer *player)
{

    t3d_matrix_set(player->matLightFP, true);
    rspq_block_run(player->dplSnake);
}

void player_draw_billboard(SnakePlayer *player, PlyNum playerNum)
{
  T3DVec3 billboardPos = (T3DVec3){{
    player->playerPos.v[0],
    player->playerPos.v[1] + BILLBOARD_YOFFSET,
    player->playerPos.v[2]
  }};

  T3DVec3 billboardScreenPos;
  t3d_viewport_calc_viewspace_pos(&viewport, &billboardScreenPos, &billboardPos);

  int x = floorf(billboardScreenPos.v[0]);
  int y = floorf(billboardScreenPos.v[1]);

  rdpq_sync_pipe(); // Hardware crashes otherwise
  rdpq_sync_tile(); // Hardware crashes otherwise

  rdpq_text_printf(&(rdpq_textparms_t){ .style_id = playerNum }, FONT_BILLBOARD, x-5, y-16, "P%d", playerNum+1);
}

bool player_has_control(SnakePlayer *player)
{
  return countDownTimer < 0.0f;
}

void player_fixedloop(SnakePlayer *player, float deltaTime, joypad_port_t port, bool is_human)
{
  float speed = 0.0f;
  T3DVec3 newDir = {0};

  if (player_has_control(player)) {
    if (is_human) {
      joypad_inputs_t joypad = joypad_get_inputs(port);

      newDir.v[0] = (float)joypad.stick_x * 0.05f;
      newDir.v[2] = -(float)joypad.stick_y * 0.05f;
      speed = sqrtf(t3d_vec3_len2(&newDir));
    } else {
      SnakePlayer* target = &players[player->ai_target];
      if (player->plynum != target->plynum) { // Check for a valid target
        // Move towards the direction of the target
        float dist, norm;
        newDir.v[0] = (target->playerPos.v[0] - player->playerPos.v[0]);
        newDir.v[2] = (target->playerPos.v[2] - player->playerPos.v[2]);
        dist = sqrtf(newDir.v[0]*newDir.v[0] + newDir.v[2]*newDir.v[2]);
        norm = 1/dist;
        newDir.v[0] *= norm;
        newDir.v[2] *= norm;
        speed = 20;
    
        // Attack if close, and the reaction time has elapsed
        if (dist < 25 && !player->isAttack) {
          if (player->ai_reactionspeed <= 0) {
            t3d_anim_set_playing(&player->animAttack, true);
            t3d_anim_set_time(&player->animAttack, 0.0f);
            player->isAttack = true;
            player->attackTimer = 0;
            player->ai_reactionspeed = (2-core_get_aidifficulty())*5 + rand()%((3-core_get_aidifficulty())*3);
          } else {
            player->ai_reactionspeed--;
          }
        }
      } else {
        player->ai_target = rand()%MAXPLAYERS; // (Attempt) to aquire a new target this frame
      }
    }
  }

  // Player movement
  if(speed > 0.15f && !player->isAttack) {
    newDir.v[0] /= speed;
    newDir.v[2] /= speed;
    player->moveDir = newDir;

    float newAngle = atan2f(player->moveDir.v[0], player->moveDir.v[2]);
    player->rotY = t3d_lerp_angle(player->rotY, newAngle, 0.5f);
    player->currSpeed = t3d_lerp(player->currSpeed, speed * 0.3f, 0.15f);
  } else {
    player->currSpeed *= 0.64f;
  }

  // use blend based on speed for smooth transitions
  player->animBlend = player->currSpeed / 0.51f;
  if(player->animBlend > 1.0f)player->animBlend = 1.0f;

  // move player...
  player->playerPos.v[0] += player->moveDir.v[0] * player->currSpeed;
  player->playerPos.v[2] += player->moveDir.v[2] * player->currSpeed;
  // ...and limit position inside the box
  const float BOX_SIZE = 140.0f;
  if(player->playerPos.v[0] < -BOX_SIZE)player->playerPos.v[0] = -BOX_SIZE;
  if(player->playerPos.v[0] >  BOX_SIZE)player->playerPos.v[0] =  BOX_SIZE;
  if(player->playerPos.v[2] < -BOX_SIZE)player->playerPos.v[2] = -BOX_SIZE;
  if(player->playerPos.v[2] >  BOX_SIZE)player->playerPos.v[2] =  BOX_SIZE;

  if (player->isAttack) {
    player->attackTimer += deltaTime;
    if (player->attackTimer > ATTACK_TIME_START && player->attackTimer < ATTACK_TIME_END) {
      //player_do_damage(player);
    }
  }
}

void minigame_fixedloop(float deltaTime)
{
  bool controlbefore = player_has_control(&players[0]);
  uint32_t playercount = core_get_playercount();
  for (size_t i = 0; i < MAXPLAYERS; i++)
  {
    player_fixedloop(&players[i], deltaTime, core_get_playercontroller(i), i < playercount);
  }

  if (countDownTimer > -GO_DELAY)
  {
    float prevCountDown = countDownTimer;
    countDownTimer -= deltaTime;
    if ((int)prevCountDown != (int)countDownTimer && countDownTimer >= 0)
      wav64_play(&sfx_countdown, 31);
  }
  if (!controlbefore && player_has_control(&players[0]))
    wav64_play(&sfx_start, 31);

  if (!isEnding) {
    // Determine if a player has won
    uint32_t alivePlayers = 0;
    PlyNum lastPlayer = 0;
    for (size_t i = 0; i < MAXPLAYERS; i++)
    {
      if (players[i].isTreasureAquired)
      {
        alivePlayers++;
        lastPlayer = i;
      }
    }
    
    if (alivePlayers == 1) {
      isEnding = true;
      winner = lastPlayer;
      wav64_play(&sfx_stop, 31);
    }
  } else {
    float prevEndTime = endTimer;
    endTimer += deltaTime;
    if ((int)prevEndTime != (int)endTimer && (int)endTimer == WIN_SHOW_DELAY)
      wav64_play(&sfx_winner, 31);
    if (endTimer > WIN_DELAY) {
      core_set_winner(winner);
      minigame_end();
    }
  }
}

void player_loop(SnakePlayer *player, float deltaTime, joypad_port_t port, bool is_human)
{
  if (is_human)
  {
    joypad_buttons_t btn = joypad_get_buttons_pressed(port);

    // Player Attack
    if((btn.a) && !player->animAttack.isPlaying) {
      t3d_anim_set_playing(&player->animAttack, true);
      t3d_anim_set_time(&player->animAttack, 0.0f);
      player->isAttack = true;
      player->attackTimer = 0;
    }
  }

  // Update the animation and modify the skeleton, this will however NOT recalculate the matrices
  t3d_anim_update(&player->animIdle, deltaTime);
  t3d_anim_set_speed(&player->animWalk, player->animBlend + 0.15f);
  t3d_anim_update(&player->animWalk, deltaTime);

  if(player->isAttack) {
    t3d_anim_update(&player->animAttack, deltaTime); // attack animation now overrides the idle one
    if(!player->animAttack.isPlaying)player->isAttack = false;
  }

  // We now blend the walk animation with the idle/attack one
  t3d_skeleton_blend(&player->skel, &player->skel, &player->skelBlend, player->animBlend);

  // Update the animation and modify the skeleton, this will however NOT recalculate the matrices
  t3d_anim_update(&player->animIdle, deltaTime);

  // We now blend the walk animation with the idle/attack one
  if(syncPoint)rspq_syncpoint_wait(syncPoint); // wait for the RSP to process the previous frame

  // Now recalc. the matrices, this will cause any model referencing them to use the new pose
  t3d_skeleton_update(&player->skel);

  // Update player matrix
  t3d_mat4fp_from_srt_euler(player->modelMatFP,
    (float[3]){1.0f, 1.0f, 1.0f},
    (float[3]){0.0f, -player->rotY, 0},
    player->playerPos.v
  );
}

void minigame_loop(float deltatime)
{
  time += deltatime;
  rotAngle += deltatime * 1.25f;

  uint32_t playercount = core_get_playercount();
  for (size_t i = 0; i < MAXPLAYERS; i++)
  {
    player_loop(&players[i], deltatime, core_get_playercontroller(i), i < playercount);
  }
  joypad_poll();
  joypad_inputs_t joypad = joypad_get_inputs(JOYPAD_PORT_1);
  joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
  if(joypad.stick_x < 10 && joypad.stick_x > -10)joypad.stick_x = 0;
  if(joypad.stick_y < 10 && joypad.stick_y > -10)joypad.stick_y = 0;

  if(pressed.start) minigame_end();

  // setup camera, look at an isometric angle onto the floor below the selected light
  // and interpolate the camera position to make it smooth
  camTarget = (T3DVec3){{
    players[0].playerPos.v[0], getFloorHeight(&players[0].playerPos) + 2.0f, players[0].playerPos.v[2]
  }};
  t3d_vec3_lerp(&camTargetCurr, &camTargetCurr, &camTarget, 0.2f);

  T3DVec3 camPos;
  t3d_vec3_scale(&camPos, &camDir, 65.0f);
  t3d_vec3_add(&camPos, &camTargetCurr, &camPos);

  if(syncPoint)rspq_syncpoint_wait(syncPoint);

  // cursor below selected point light
  float cursorScale = 0.075f + fm_sinf(rotAngle*6.0f)*0.005f;
  t3d_mat4fp_from_srt_euler(cursorMatFP,
    (float[3]){cursorScale, cursorScale, cursorScale},
    (float[3]){0.0f, rotAngle*0.5f, 0.0f},
    (float[3]){camTarget.v[0], camTarget.v[1] + 0.5f, camTarget.v[2]}
  );

  float scale = 0.1f;

  // update matrix for lights (crystal balls)
  for(int i=0; i<4; ++i)
  {
    t3d_mat4fp_from_srt_euler(players[i].matLightFP,
      (float[]){scale, scale, scale},
      (float[3]){0.0f, -players[i].rotY, 0},
      (float[]){
        players[i].playerPos.v[0],
        players[i].playerPos.v[1] + getFloorHeight(&players[i].playerPos),
        players[i].playerPos.v[2]
      }
    );
  }

  // projection or ortho/isometric matrix
  float aspect = (float)display_get_height() / (float)display_get_width();
    t3d_viewport_set_ortho(&viewport,
      -viewZoom, viewZoom,
      -viewZoom * aspect, viewZoom * aspect,
      3.0f, 250.0f
    );


  t3d_viewport_look_at(&viewport, &camPos, &camTargetCurr, &(T3DVec3){{0,1,0}});

  // ----------- DRAW (3D) ------------ //
  rdpq_attach(display_get(), display_get_zbuf());

  t3d_frame_start();
  rdpq_mode_antialias(AA_NONE);

  t3d_viewport_attach(&viewport);

  rdpq_set_prim_color((color_t){0xFF, 0xFF, 0xFF, 0xFF});
  t3d_screen_clear_color(RGBA32(40, 40, 60, 0xFF));
  t3d_screen_clear_depth();

  t3d_light_set_ambient(colorAmbient);
  for (size_t i = 0; i < MAXPLAYERS; i++) {
    // Sets the actual point light
    t3d_light_set_point(i, &players[i].color.r, &(T3DVec3){{
      players[i].playerPos.v[0],
      players[i].playerPos.v[1] + getFloorHeight(&players[i].playerPos),
      players[i].playerPos.v[2]
    }}, players[i].strength, false);
  }
  t3d_light_set_count(5);

  // We can still use an ambient light as usual, this doesn't count towards the 7 light limit

  t3d_light_set_count(5);

  // directional lights can still be used together with point lights
  t3d_light_set_directional(5, (uint8_t[]){0xAA, 0xAA, 0xFF, 0xFF}, &(T3DVec3){{1.0f, 0.0f, 0.0f}});
  t3d_light_set_count(6);
  

  t3d_matrix_push_pos(1);
  t3d_matrix_set(modelMatFP, true);
  rspq_block_run(dplMap);



  for (size_t i = 0; i < MAXPLAYERS; i++)
  {
    player_draw(&players[i]);
  }



  t3d_matrix_pop(1);
  syncPoint = rspq_syncpoint_new();

  // ----------- DRAW (2D) ------------ //
  if(textTimer > 0.0f) {
    textTimer -= deltatime;
    float height = (2.0f - textTimer) / 2.0f;
    height = fminf(1.0f, fmaxf(0.0f, height));
    height = (height*height) * -74.0f;

    rdpq_sync_pipe();
    rdpq_sync_tile();

    rdpq_textparms_t txtParam = {.align = ALIGN_CENTER, .width = 320, .wrap = WRAP_WORD};
    rdpq_text_printf(&txtParam, FONT_BILLBOARD, 0, 240 - height - 64, "L/R - Change Light");
    rdpq_text_printf(&txtParam, FONT_BILLBOARD, 0, 240 - height - 48, "C - Height / Strength");
  }

  rdpq_sync_pipe();
  rdpq_sync_tile();
  rdpq_text_printf(NULL, FONT_TEXT, 20, 240-20, "FPS: %.2f", display_get_fps());

  rdpq_detach_show();
}



/*==============================
    minigame_cleanup
    Clean up any memory used by your game just before it ends.
==============================*/
void minigame_cleanup()
{
  for (size_t i = 0; i < MAXPLAYERS; i++)
  {
    player_cleanup(&players[i]);
  }

  wav64_close(&sfx_start);
  wav64_close(&sfx_countdown);
  wav64_close(&sfx_stop);
  wav64_close(&sfx_winner);
  xm64player_stop(&music);
  xm64player_close(&music);
  rspq_block_free(dplMap);

  rdpq_text_unregister_font(FONT_BILLBOARD);
  rdpq_font_free(fontBillboard);
  rdpq_text_unregister_font(FONT_TEXT);
  rdpq_font_free(font);

  t3d_destroy();

  display_close();
}