#include <t3d/t3d.h>
#include <libdragon.h>
#include "../../core.h"
#include <t3d/t3dmodel.h>

typedef struct
{
    PlyNum occupyingPlynum;
    T3DMat4FP *blockMatFP;
    rspq_block_t *dplDirtBlock;
    bool isPlayerOccupied;
    bool isContainingChest;
    bool isDestroyed;
    int damage;
    T3DVec3 dirtBlockPos;
} DirtBlock;

void initDirtBlock(DirtBlock *dirtBlock, T3DModel *dirtBlockModel, float placementX, float placementY, float placementZ)
{
    dirtBlock->blockMatFP = malloc_uncached(sizeof(T3DMat4FP));
    float blockScale = 0.6f;
    t3d_mat4fp_from_srt_euler(dirtBlock->blockMatFP, (float[3]){blockScale, blockScale, blockScale}, (float[3]){0, 0, 0}, (float[3]){placementX, placementZ, placementY});

    rspq_block_begin();
      t3d_matrix_push(dirtBlock->blockMatFP);
      rdpq_set_prim_color(RGBA32(0, 0, 0, 200));

      t3d_model_draw(dirtBlockModel);
      t3d_matrix_pop(1);
    dirtBlock->dplDirtBlock = rspq_block_end();
}