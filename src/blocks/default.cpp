/*
   Copyright (c) 2011, The Mineserver Project
   All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the The Mineserver Project nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../mineserver.h"
#include "../map.h"

#include "default.h"

void BlockDefault::onStartedDigging(User* user, int8_t status, int32_t x, int8_t y, int32_t z, int8_t direction)
{
}

void BlockDefault::onDigging(User* user, int8_t status, int32_t x, int8_t y, int32_t z, int8_t direction)
{
}

void BlockDefault::onStoppedDigging(User* user, int8_t status, int32_t x, int8_t y, int32_t z, int8_t direction)
{
}

void BlockDefault::onBroken(User* user, int8_t status, int32_t x, int8_t y, int32_t z, int8_t direction)
{
  uint8_t block;
  uint8_t meta;

  if (!Mineserver::get()->map()->getBlock(x, y, z, &block, &meta))
  {
    return;
  }

  Mineserver::get()->map()->sendBlockChange(x, y, z, BLOCK_AIR, 0);
  Mineserver::get()->map()->setBlock(x, y, z, BLOCK_AIR, 0);
  this->spawnBlockItem(x,y,z,block,meta);
}

void BlockDefault::onNeighbourBroken(User* user, int8_t oldblock, int32_t x, int8_t y, int32_t z, int8_t direction)
{
}

void BlockDefault::onPlace(User* user, int8_t newblock, int32_t x, int8_t y, int32_t z, int8_t direction)
{
  uint8_t oldblock;
  uint8_t oldmeta;

  if (!Mineserver::get()->map()->getBlock(x, y, z, &oldblock, &oldmeta))
  {
    return;
  }

  //Combine two steps
  if(newblock == BLOCK_STEP && oldblock == BLOCK_STEP && direction == BLOCK_TOP)
  {
     Mineserver::get()->map()->setBlock(x, y, z, (char)BLOCK_DOUBLE_STEP, 0);
     Mineserver::get()->map()->sendBlockChange(x, y, z, (char)BLOCK_DOUBLE_STEP, 0);
     return;
  }

  /* Check block below allows blocks placed on top */
  if (!this->isBlockStackable(oldblock))
  {
    return;
  }

  /* move the x,y,z coords dependent upon placement direction */
  if (!this->translateDirection(&x,&y,&z,direction))
  {
    return;
  }

  if (this->isUserOnBlock(x,y,z))
  {
    return;
  }

  if (!this->isBlockEmpty(x,y,z))
  {
    return;
  }

  direction = user->relativeToBlock(x, y, z);

  Mineserver::get()->map()->setBlock(x, y, z, (char)newblock, direction);
  Mineserver::get()->map()->sendBlockChange(x, y, z, (char)newblock, direction);
}

void BlockDefault::onNeighbourPlace(User* user, int8_t newblock, int32_t x, int8_t y, int32_t z, int8_t direction)
{
}

void BlockDefault::onReplace(User* user, int8_t newblock, int32_t x, int8_t y, int32_t z, int8_t direction)
{
  uint8_t oldblock;
  uint8_t oldmeta;

  if (!Mineserver::get()->map()->getBlock(x, y, z, &oldblock, &oldmeta))
  {
    return;
  }

  Mineserver::get()->map()->sendBlockChange(x, y, z, BLOCK_AIR, 0);
  Mineserver::get()->map()->setBlock(x, y, z, BLOCK_AIR, 0);
  Mineserver::get()->map()->createPickupSpawn(x, y, z, oldblock, 1, 0, NULL);
}

void BlockDefault::onNeighbourMove(User* user, int8_t oldblock, int32_t x, int8_t y, int32_t z, int8_t direction)
{
}

