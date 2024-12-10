#include <t3d/t3d.h>
#include <libdragon.h>
#include "../../core.h"
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>

typedef struct
{
    PlyNum plynum;
    T3DMat4FP *modelMatFP;
    rspq_block_t *dplSnake;
    T3DAnim animAttack;
    T3DAnim animIdle;
    T3DAnim animWalk;
    T3DSkeleton skelBlend;
    T3DSkeleton skel;
    T3DVec3 moveDir;
    T3DVec3 playerPos;
    float rotY;
    bool isAttack;
    float currSpeed;
    float animBlend;
    float attackTimer;
    PlyNum ai_target;
    int ai_reactionspeed;
    float strength;
} SnakePlayer;

void player_init(SnakePlayer *player, color_t color, T3DVec3 position, float rotation)
{
    T3DModel *snakeModel;
    T3DModel *modelShadow;
  	snakeModel = t3d_model_load("rom://undergroundgrind/snake.t3dm");
  	modelShadow = t3d_model_load("rom://undergroundgrind/shadow.t3dm");
    player->modelMatFP = malloc_uncached(sizeof(T3DMat4FP));

    player->moveDir = (T3DVec3){{0,0,0}};
    player->playerPos = position;

    // First instantiate skeletons, they will be used to draw models in a specific pose
    // And serve as the target for animations to modify
    player->skel = t3d_skeleton_create(snakeModel);
    player->skelBlend = t3d_skeleton_clone(&player->skel, false); // optimized for blending, has no matrices

    // Now create animation instances (by name), the data in 'model' is fixed,
    // whereas 'anim' contains all the runtime data.
    // Note that tiny3d internally keeps no track of animations, it's up to the user to manage and play them.
    player->animIdle = t3d_anim_create(snakeModel, "Snake_Idle");
    t3d_anim_attach(&player->animIdle, &player->skel); // tells the animation which skeleton to modify

    player->animWalk = t3d_anim_create(snakeModel, "Snake_Walk");
    t3d_anim_attach(&player->animWalk, &player->skelBlend);

    player->animAttack = t3d_anim_create(snakeModel, "Snake_Attack");
    t3d_anim_set_looping(&player->animAttack, false); // don't loop this animation
    t3d_anim_set_playing(&player->animAttack, false); // start in a paused state
    t3d_anim_attach(&player->animAttack, &player->skel);

    rspq_block_begin();
    t3d_matrix_push(player->modelMatFP);
    rdpq_set_prim_color(color);
    t3d_model_draw_skinned(snakeModel, &player->skel); // as in the last example, draw skinned with the main skeleton

    rdpq_set_prim_color(RGBA32(0, 0, 0, 120));
    t3d_model_draw(modelShadow);
    t3d_matrix_pop(1);
    player->dplSnake = rspq_block_end();

    player->rotY = rotation;
    player->currSpeed = 0.0f;
    player->animBlend = 0.0f;
    player->isAttack = false;
    player->ai_target = rand()%MAXPLAYERS;
    player->ai_reactionspeed = (2-core_get_aidifficulty())*5 + rand()%((3-core_get_aidifficulty())*3);
}

void player_loop(SnakePlayer *player, float deltaTime, joypad_port_t port, bool is_human, rspq_syncpoint_t syncPoint)
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
    (float[3]){0.125f, 0.125f, 0.125f},
    (float[3]){0.0f, -player->rotY, 0},
    player->playerPos.v
  );
}


void player_cleanup(SnakePlayer *player)
{
  rspq_block_free(player->dplSnake);

  t3d_skeleton_destroy(&player->skel);
  t3d_skeleton_destroy(&player->skelBlend);

  t3d_anim_destroy(&player->animIdle);
  t3d_anim_destroy(&player->animWalk);

  free_uncached(player->modelMatFP);
}