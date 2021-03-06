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

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <deque>
#include <algorithm>
#include <fstream>
#include <map>

#include <sys/stat.h>
#ifdef WIN32
  #include <winsock2.h>
  #include <direct.h>
#else
  #include <netinet/in.h>
  #include <string.h>
#endif
#include <zlib.h>
#include <ctime>

#include "constants.h"

#include "logger.h"
#include "map.h"
#include "user.h"
#include "chat.h"
#include "plugin.h"
#include "packets.h"
#include "mineserver.h"
#include "config.h"
#include "permissions.h"

// Generate "unique" entity ID
uint32_t generateEID()
{
  static uint32_t EID = 0;
  return ++EID;
}


User::User(int sock, uint32_t EID)
{
  this->action          = 0;
  this->muted           = false;
  this->dnd             = false;
  this->waitForData     = false;
  this->fd              = sock;
  this->UID             = EID;
  this->logged          = false;
  // ENABLED FOR DEBUG

  this->pos.x           = Mineserver::get()->map()->spawnPos.x();
  this->pos.y           = Mineserver::get()->map()->spawnPos.y();
  this->pos.z           = Mineserver::get()->map()->spawnPos.z();
  this->write_err_count = 0;
  this->health          = 20;
  this->attachedTo      = 0;
  this->timeUnderwater  = 0;
  this->isOpenInv       = false;
  this->lastData        = time(NULL);

  this->m_currentItemSlot = 0;
  this->inventoryHolding  = Item();

  // Ignore this user if it's the server console
  if (this->UID != SERVER_CONSOLE_UID)
  {
    Mineserver::get()->users().push_back(this);
  }
}

bool User::changeNick(std::string _nick)
{
  (static_cast<Hook2<bool,const char*,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerNickPost")))->doAll(nick.c_str(), _nick.c_str());

  nick = _nick;
  SET_ADMIN(permissions);

  return true;
}

User::~User()
{
  event_del(GetEvent());

  if (fd != -1)
  {
#ifdef WIN32
    closesocket(fd);
#else
    close(fd);
#endif
  }

  this->buffer.reset();

  // Remove all known chunks
  for (int i=0;i<mapKnown.size();i++)
  {
    delKnown(mapKnown[i].x(), mapKnown[i].z());
  }

  std::vector<User*>::iterator it_a = Mineserver::get()->users().begin();
  std::vector<User*>::iterator it_b = Mineserver::get()->users().end();
  for (;it_a!=it_b;++it_a)
  {
    if ((*it_a) == this)
    {
      Mineserver::get()->users().erase(it_a);
      break;
    }
  }

  if (logged)
  {
    for (int mapx = -viewDistance+curChunk.x(); mapx <= viewDistance+curChunk.x(); mapx++)
    {
      for (int mapz = -viewDistance+curChunk.z(); mapz <= viewDistance+curChunk.z(); mapz++)
      {
        sChunk* chunk = Mineserver::get()->map()->chunks.getChunk(mapx, mapz);
        if (chunk != NULL)
        {
          chunk->users.erase(this);
          if (chunk->users.size() == 0)
          {
            Mineserver::get()->map()->releaseMap(mapx, mapz);
          }
        }
      }
    }

    Mineserver::get()->chat()->sendMsg(this, this->nick + " disconnected!", Chat::OTHERS);
    this->saveData();

    // Send signal to everyone that the entity is destroyed
    uint8_t entityData[5];
    entityData[0] = 0x1d; // Destroy entity;
    putSint32(&entityData[1], this->UID);
    this->sendOthers(&entityData[0], 5);
  }

  // Update player list
  if (fd != -1)
  {
    (static_cast<Hook1<bool,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerQuitPost")))->doAll(nick.c_str());
  }
}

bool User::sendLoginInfo()
{
  // Load user data
  loadData();

  // Login OK package
  buffer << (int8_t)PACKET_LOGIN_RESPONSE << (int32_t)UID << std::string("") << std::string("") << (int64_t)0 << (int8_t)0;

  // Send spawn position
  buffer << (int8_t)PACKET_SPAWN_POSITION << (int32_t)pos.x << ((int32_t)pos.y+2) << (int32_t)pos.z;

  // Put nearby chunks to queue
  for (int x = -viewDistance; x <= viewDistance; x++)
  {
    for (int z = -viewDistance; z <= viewDistance; z++)
    {
      addQueue((int32_t)pos.x/16+x, (int32_t)pos.z/16+z);
    }
  }
  // Push chunks to user
  pushMap(); pushMap();



  // Teleport player
  teleport(pos.x, pos.y+2, pos.z);

  // Send server time (after dawn)
  buffer << (int8_t)PACKET_TIME_UPDATE << (int64_t)Mineserver::get()->map()->mapTime;


  // Inventory
  for (int i=1; i<45; i++)
  {   
    if (inv[i].type != -1 && inv[i].count)
    {
      buffer << (int8_t)PACKET_SET_SLOT << (int8_t)0 << (int16_t)(i) << (int16_t)inv[i].type << (int8_t)(inv[i].count) << (int16_t)inv[i].health;
    }
  }

  // Spawn this user to others
  spawnUser((int32_t)pos.x*32, ((int32_t)pos.y+2)*32, (int32_t)pos.z*32);
  // Spawn other users for connected user
  spawnOthers();

  sethealth(health);
  logged = true;

  Mineserver::get()->chat()->sendMsg(this, nick+" connected!", Chat::ALL);

  pushMap(); pushMap(); pushMap();
  // Teleport player (again)
  teleport(pos.x, pos.y+2, pos.z);

  return true;
}

// Kick player
bool User::kick(std::string kickMsg)
{
  buffer << (int8_t)PACKET_KICK << kickMsg;

  (static_cast<Hook2<bool,const char*,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerKickPost")))->doAll(nick.c_str(), kickMsg.c_str());

  Mineserver::get()->logger()->log(LogType::LOG_WARNING, "User", nick + " kicked. Reason: " + kickMsg);

  return true;
}

bool User::mute(std::string muteMsg)
{
  if (!muteMsg.empty())
  {
    muteMsg = MC_COLOR_YELLOW + "You have been muted.  Reason: " + muteMsg;
  }
  else
  {
    muteMsg = MC_COLOR_YELLOW + "You have been muted. ";
  }

  Mineserver::get()->chat()->sendMsg(this, muteMsg, Chat::USER);
  this->muted = true;
  Mineserver::get()->logger()->log(LogType::LOG_WARNING, "User", nick + " muted. Reason: " + muteMsg);
  return true;
}

bool User::unmute()
{
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You have been unmuted.", Chat::USER);
    this->muted = false;
    Mineserver::get()->logger()->log(LogType::LOG_WARNING, "User", nick + " unmuted.");
    return true;
}

bool User::toggleDND()
{
  if (!this->dnd)
  {
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You have enabled 'Do Not Disturb' mode.", Chat::USER);
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You will no longer see chat or private messages.", Chat::USER);
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "Type /dnd again to disable 'Do Not Disturb' mode.", Chat::USER);
    this->dnd = true;
  }
  else
  {
    this->dnd = false;
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You have disabled 'Do Not Disturb' mode.", Chat::USER);
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You can now see chat and private messages.", Chat::USER);
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "Type /dnd again to enable 'Do Not Disturb' mode.", Chat::USER);
  }
  return this->dnd;
}

bool User::isAbleToCommunicate(std::string communicateCommand)
{
  // Check if this is chat or a regular command and prefix with a slash accordingly
  if (communicateCommand != "chat")
    communicateCommand = "/" + communicateCommand;

  if (this->muted) {
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You cannot " + communicateCommand + " while muted.", Chat::USER);
    return false;
  }
  if (this->dnd) {
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "You cannot " + communicateCommand + " while in 'Do Not Disturb' mode.", Chat::USER);
    Mineserver::get()->chat()->sendMsg(this, MC_COLOR_YELLOW + "Type /dnd to disable.", Chat::USER);
    return false;
  }
  return true;
}

bool User::loadData()
{
  std::string infile = Mineserver::get()->map()->mapDirectory+"/players/"+this->nick+".dat";

  struct stat stFileInfo;
  if (stat(infile.c_str(), &stFileInfo) != 0)
    return false;

  NBT_Value*  playerRoot = NBT_Value::LoadFromFile(infile.c_str());
  NBT_Value& nbtPlayer = *playerRoot;
  if (playerRoot == NULL)
  {
    LOGLF("Failed to open player file");
    return false;
  }

  std::vector<NBT_Value*>* _pos = nbtPlayer["Pos"]->GetList();
  pos.x = (double)(*(*_pos)[0]);
  pos.y = (double)(*(*_pos)[1]);
  pos.z = (double)(*(*_pos)[2]);

  health = *nbtPlayer["Health"];

  std::vector<NBT_Value*>* rot = nbtPlayer["Rotation"]->GetList();
  pos.yaw = (float)(*(*rot)[0]);
  pos.yaw = (float)(*(*rot)[1]);

  std::vector<NBT_Value*>* _inv = nbtPlayer["Inventory"]->GetList();
  std::vector<NBT_Value*>::iterator iter = _inv->begin(), end = _inv->end();

  for ( ; iter != end ; iter++ )
  {
    int8_t slot, count;
    int16_t damage, item_id;

    slot    = *(**iter)["Slot"];
    count   = *(**iter)["Count"];
    damage  = *(**iter)["Damage"];
    item_id = *(**iter)["id"];
    if (item_id == 0 || count < 1)
    {
      item_id = -1;
      count   =  0;
    }

    // Main inventory slot, converting 0-35 slots to 9-44
    if (slot >= 0 && slot <= 35)
    {
      inv[(uint8_t)slot+9].count  = count;
      inv[(uint8_t)slot+9].health = damage;
      inv[(uint8_t)slot+9].type   = item_id;
    }
    // Crafting, converting 80-83 slots to 1-4
    else if (slot >= 80 && slot <= 83)
    {
      inv[(uint8_t)slot-79].count  = count;
      inv[(uint8_t)slot-79].health = damage;
      inv[(uint8_t)slot-79].type   = item_id;
    }
    // Equipped, converting 100-103 slots to 8-5 (reverse order!)
    else if (slot >= 100 && slot <= 103)
    {
      inv[(uint8_t)8+(100-slot)].count  = count;
      inv[(uint8_t)8+(100-slot)].health = damage;
      inv[(uint8_t)8+(100-slot)].type   = item_id;
    }
  }

  delete playerRoot;

  return true;
}

bool User::saveData()
{
  std::string outfile = Mineserver::get()->map()->mapDirectory+"/players/"+this->nick+".dat";
  // Try to create parent directories if necessary
  struct stat stFileInfo;
  if (stat(outfile.c_str(), &stFileInfo) != 0)
  {
    std::string outdir = Mineserver::get()->map()->mapDirectory+"/players";

    if (stat(outdir.c_str(), &stFileInfo) != 0)
    {
#ifdef WIN32
      if (_mkdir(outdir.c_str()) == -1)
#else
      if (mkdir(outdir.c_str(), 0755) == -1)
#endif

        return false;
    }
  }

  NBT_Value val(NBT_Value::TAG_COMPOUND);
  val.Insert("OnGround", new NBT_Value((int8_t)1));
  val.Insert("Air", new NBT_Value((int16_t)300));
  val.Insert("AttackTime", new NBT_Value((int16_t)0));
  val.Insert("DeathTime", new NBT_Value((int16_t)0));
  val.Insert("Fire", new NBT_Value((int16_t)-20));
  val.Insert("Health", new NBT_Value((int16_t)health));
  val.Insert("HurtTime", new NBT_Value((int16_t)0));
  val.Insert("FallDistance", new NBT_Value(54.f));

  NBT_Value* nbtInv = new NBT_Value(NBT_Value::TAG_LIST, NBT_Value::TAG_COMPOUND);

  char itemslot = 0;
  // Start with main items
  for (int slotid = 9; slotid < 45; slotid++,itemslot++)
  {
    if (inv[(uint8_t)slotid].count && inv[(uint8_t)slotid].type != 0 && inv[(uint8_t)slotid].type != -1)
    {
      NBT_Value* val = new NBT_Value(NBT_Value::TAG_COMPOUND);
      val->Insert("Count", new NBT_Value((int8_t)inv[(uint8_t)slotid].count));
      val->Insert("Slot", new NBT_Value((int8_t)itemslot));
      val->Insert("Damage", new NBT_Value((int16_t)inv[(uint8_t)slotid].health));
      val->Insert("id", new NBT_Value((int16_t)inv[(uint8_t)slotid].type));
      nbtInv->GetList()->push_back(val);
    }    
  }
  // Crafting slots
  itemslot = 80;
  for (int slotid = 1; slotid < 6; slotid++,itemslot++)
  {
    if (inv[(uint8_t)slotid].count && inv[(uint8_t)slotid].type != 0 && inv[(uint8_t)slotid].type != -1)
    {
      NBT_Value* val = new NBT_Value(NBT_Value::TAG_COMPOUND);
      val->Insert("Count", new NBT_Value((int8_t)inv[(uint8_t)slotid].count));
      val->Insert("Slot", new NBT_Value((int8_t)itemslot));
      val->Insert("Damage", new NBT_Value((int16_t)inv[(uint8_t)slotid].health));
      val->Insert("id", new NBT_Value((int16_t)inv[(uint8_t)slotid].type));
      nbtInv->GetList()->push_back(val);
    }    
  }

  // Equipped items last
  itemslot = 103;
  for (int slotid = 5; slotid < 9; slotid++,itemslot--)
  {
    if (inv[(uint8_t)slotid].count && inv[(uint8_t)slotid].type != 0 && inv[(uint8_t)slotid].type != -1)
    {
      NBT_Value* val = new NBT_Value(NBT_Value::TAG_COMPOUND);
      val->Insert("Count", new NBT_Value((int8_t)inv[(uint8_t)slotid].count));
      val->Insert("Slot", new NBT_Value((int8_t)itemslot));
      val->Insert("Damage", new NBT_Value((int16_t)inv[(uint8_t)slotid].health));
      val->Insert("id", new NBT_Value((int16_t)inv[(uint8_t)slotid].type));
      nbtInv->GetList()->push_back(val);
    }    
  }


  val.Insert("Inventory", nbtInv);

  NBT_Value* nbtPos = new NBT_Value(NBT_Value::TAG_LIST, NBT_Value::TAG_DOUBLE);
  nbtPos->GetList()->push_back(new NBT_Value((double)pos.x));
  nbtPos->GetList()->push_back(new NBT_Value((double)pos.y));
  nbtPos->GetList()->push_back(new NBT_Value((double)pos.z));
  val.Insert("Pos", nbtPos);


  NBT_Value* nbtRot = new NBT_Value(NBT_Value::TAG_LIST, NBT_Value::TAG_FLOAT);
  nbtRot->GetList()->push_back(new NBT_Value((float)pos.yaw));
  nbtRot->GetList()->push_back(new NBT_Value((float)pos.pitch));
  val.Insert("Rotation", nbtRot);


  NBT_Value* nbtMotion = new NBT_Value(NBT_Value::TAG_LIST, NBT_Value::TAG_DOUBLE);
  nbtMotion->GetList()->push_back(new NBT_Value((double)0.0));
  nbtMotion->GetList()->push_back(new NBT_Value((double)0.0));
  nbtMotion->GetList()->push_back(new NBT_Value((double)0.0));
  val.Insert("Motion", nbtMotion);

  val.SaveToFile(outfile);

  return true;

}



bool User::updatePos(double x, double y, double z, double stance)
{

  // Riding on a minecart?
  if (y==-999)
  {
    // attachedTo
    // ToDo: Get pos from minecart/player/boat
    return false;
  }

  if (logged)
  {
    sChunk* newChunk = Mineserver::get()->map()->loadMap(blockToChunk((int32_t)x), blockToChunk((int32_t)z));
    sChunk* oldChunk = Mineserver::get()->map()->loadMap(blockToChunk((int32_t)pos.x), blockToChunk((int32_t)pos.z));

    if (newChunk == oldChunk)
    {
      Packet telePacket;
      telePacket << (int8_t)PACKET_ENTITY_TELEPORT
                 << (int32_t)UID << (int32_t)(x * 32) << (int32_t)(y * 32) 
                 << (int32_t)(z * 32) << angleToByte(pos.yaw) << angleToByte(pos.pitch);
      newChunk->sendPacket(telePacket, this);
    }
    else if (abs(newChunk->x - oldChunk->x) <= 1  && abs(newChunk->z - oldChunk->z) <= 1)
    {
      std::list<User*> toremove;
      std::list<User*> toadd;

      sChunk::userBoundary(oldChunk, toremove, newChunk, toadd);

      if (toremove.size())
      {
        Packet pkt;
        pkt << (int8_t)PACKET_DESTROY_ENTITY << (int32_t)UID;
        std::list<User*>::iterator iter = toremove.begin(), end = toremove.end();
        for ( ; iter != end ; iter++)
        {
          (*iter)->buffer.addToWrite(pkt.getWrite(), pkt.getWriteLen());
        }
      }

      if (toadd.size())
      {
        Packet pkt;
        pkt << (int8_t)PACKET_NAMED_ENTITY_SPAWN << (int32_t)UID << nick
            << (int32_t)(x * 32) << (int32_t)(y * 32) << (int32_t)(z * 32)
            << angleToByte(pos.yaw) << angleToByte(pos.pitch) << (int16_t)curItem;

        std::list<User*>::iterator iter = toadd.begin(), end = toadd.end();
        for ( ; iter != end ; iter++)
        {
          if ((*iter) != this)
          {
            (*iter)->buffer.addToWrite(pkt.getWrite(), pkt.getWriteLen());
          }
        }
      }

      // TODO: Determine those who where present for both.
      Packet telePacket;
      telePacket << (int8_t)PACKET_ENTITY_TELEPORT
                 << (int32_t)UID << (int32_t)(x * 32) << (int32_t)(y * 32) << (int32_t)(z * 32) 
                 << angleToByte(pos.yaw) << angleToByte(pos.pitch);
      newChunk->sendPacket(telePacket, this);

      int chunkDiffX = newChunk->x - oldChunk->x;
      int chunkDiffZ = newChunk->z - oldChunk->z;

      // Send new chunk and clear old chunks
      for (int mapx = newChunk->x-viewDistance; mapx <= newChunk->x+viewDistance; mapx++)
      {
        for (int mapz = newChunk->z-viewDistance; mapz <= newChunk->z+viewDistance; mapz++)
        {


          
          if (!withinViewDistance((mapx - chunkDiffX), newChunk->x) || !withinViewDistance((mapz - chunkDiffZ), newChunk->z))
          {
            addRemoveQueue(mapx-chunkDiffX, mapz-chunkDiffZ);
          }

          // If this chunk wasn't in the view distance before
          // if (!withinViewDistance(chunkDiffX, oldChunk->x) || !withinViewDistance(chunkDiffZ, oldChunk->z))
          //{
          
          // This will remove the chunks from being removed if they were put to the remove queue.
          addQueue(mapx, mapz);
          
          
          //}
        }
      }
    }
    else
    {
      std::set<User*> toRemove;
      std::set<User*> toAdd;

      int chunkDiffX = newChunk->x - oldChunk->x;
      int chunkDiffZ = newChunk->z - oldChunk->z;
      for (int mapx = newChunk->x-viewDistance; mapx <= newChunk->x+viewDistance; mapx++)
      {
        for (int mapz = newChunk->z-viewDistance; mapz <= newChunk->z+viewDistance; mapz++)
        {
          if (!withinViewDistance(chunkDiffX, oldChunk->x) || !withinViewDistance(chunkDiffZ, oldChunk->z))
          {
            addQueue(mapx, mapz);
            sChunk* chunk = Mineserver::get()->map()->chunks.getChunk(mapx, mapz);

            if (chunk != NULL)
            {
              toAdd.insert(chunk->users.begin(), chunk->users.end());
            }
          }

          if (!withinViewDistance((mapx - chunkDiffX), newChunk->x) || !withinViewDistance((mapz - chunkDiffZ), newChunk->z))
          {
            addRemoveQueue(mapx-chunkDiffX, mapz-chunkDiffZ);

            sChunk* chunk = Mineserver::get()->map()->chunks.getChunk((mapx - chunkDiffX), (mapz - chunkDiffZ));

            if (chunk != NULL)
            {
              toRemove.insert(chunk->users.begin(), chunk->users.end());
            }
          }
        }
      }

      std::set<User*> toTeleport;
      std::set<User*>::iterator iter = toRemove.begin(), end = toRemove.end();
      for ( ; iter != end ; iter++ )
      {
        std::set<User*>::iterator result = toAdd.find(*iter);
        if (result != toAdd.end())
        {
          toTeleport.insert(*iter);
          toAdd.erase(result);

  #ifdef _MSC_VER
          iter = toRemove.erase(iter);
  #else
          // TODO: Optimise
          toRemove.erase(iter);
          iter = toRemove.begin();
  #endif
          end = toRemove.end();
          if (iter == end)
            break;
        }
      }

      Packet destroyPkt;
      destroyPkt << (int8_t)PACKET_DESTROY_ENTITY << (int32_t)UID;

      Packet spawnPkt;
      spawnPkt << (int8_t)PACKET_NAMED_ENTITY_SPAWN << (int32_t)UID << nick
               << (int32_t)(x * 32) << (int32_t)(y * 32) << (int32_t)(z * 32) << angleToByte(pos.yaw) << angleToByte(pos.pitch) << (int16_t)curItem;

      Packet telePacket;
      telePacket << (int8_t)PACKET_ENTITY_TELEPORT
                 << (int32_t)UID << (int32_t)(x * 32) << (int32_t)(y * 32) << (int32_t)(z * 32) << angleToByte(pos.yaw) << angleToByte(pos.pitch);

      toTeleport.erase(this);
      toAdd.erase(this);
      toRemove.erase(this);

      iter = toRemove.begin(); end = toRemove.end();
      for ( ; iter != end ; iter++ )
      {
        (*iter)->buffer.addToWrite(destroyPkt.getWrite(), destroyPkt.getWriteLen());
      }

      iter = toAdd.begin(); end = toAdd.end();
      for ( ; iter != end ; iter++ )
      {
        (*iter)->buffer.addToWrite(spawnPkt.getWrite(), spawnPkt.getWriteLen());
      }

      iter = toTeleport.begin(); end = toTeleport.end();
      for ( ; iter != end ; iter++ )
      {
        (*iter)->buffer.addToWrite(telePacket.getWrite(), telePacket.getWriteLen());
      }
    }


    if (newChunk->items.size())
    {
      // Loop through items and check if they are close enought to be picked up
      std::vector<spawnedItem*>::iterator iter = newChunk->items.begin(), end = newChunk->items.end();
      for (;iter!=end;++iter)
      {
        // No more than 2 blocks away
        if ( abs((int32_t)x-((*iter)->pos.x()/32)) < 2 &&
            abs((int32_t)y-((*iter)->pos.y()/32)) < 2 &&
            abs((int32_t)z-((*iter)->pos.z()/32)) < 2)
        {
          // Dont pickup own spawns right away
          if ((*iter)->spawnedBy != this->UID || (*iter)->spawnedAt+2 < time(NULL))
          {
            // Check player inventory for space!
            if (Mineserver::get()->inventory()->isSpace(this,(*iter)->item,(*iter)->count))
            {
              // Send player collect item packet
              buffer << (int8_t)PACKET_COLLECT_ITEM << (int32_t)(*iter)->EID << (int32_t)UID;

              // Send everyone destroy_entity-packet
              Packet pkt;
              pkt << (int8_t)PACKET_DESTROY_ENTITY << (int32_t)(*iter)->EID;
              newChunk->sendPacket(pkt);

              // Add items to inventory
              Mineserver::get()->inventory()->addItems(this,(*iter)->item,(*iter)->count, (*iter)->health);

              Mineserver::get()->map()->items.erase((*iter)->EID);
              delete *iter;
              iter = newChunk->items.erase(iter);
              end = newChunk->items.end();

              if (iter == end)
              {
                break;
              }
            }
          }
        }
      }
    }
  }

  this->pos.x      = x;
  this->pos.y      = y;
  this->pos.z      = z;
  this->pos.stance = stance;
  curChunk.x() = (int)(x/16);
  curChunk.z() = (int)(z/16);

  return true;
}

bool User::checkOnBlock(int32_t x, int8_t y, int32_t z)
{
   double diffX = x - this->pos.x;
   double diffZ = z - this->pos.z;

   if ((y == (int)this->pos.y)
          && (diffZ > -1.3 && diffZ < 0.3)
          && (diffX > -1.3 && diffX < 0.3))
      return true;
   return false;
}

bool User::updateLook(float yaw, float pitch)
{
  Packet pkt;
  pkt << (int8_t)PACKET_ENTITY_LOOK << (int32_t)UID << angleToByte(yaw) << angleToByte(pitch);

  sChunk* chunk = Mineserver::get()->map()->chunks.getChunk(blockToChunk((int32_t)pos.x),blockToChunk((int32_t)pos.z));
  if (chunk != NULL)
  {
    chunk->sendPacket(pkt, this);
  }

  this->pos.yaw   = yaw;
  this->pos.pitch = pitch;
  return true;
}

bool User::sendOthers(uint8_t* data, uint32_t len)
{
  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->fd != this->fd && Mineserver::get()->users()[i]->logged)
    {
      // Don't send to his user if he is DND and the message is a chat message
      if (!(Mineserver::get()->users()[i]->dnd && data[0] == PACKET_CHAT_MESSAGE))
      {
        Mineserver::get()->users()[i]->buffer.addToWrite(data, len);
      }
    }
  }
  return true;
}

int8_t User::relativeToBlock(const int32_t x, const int8_t y, const int32_t z)
{
   int8_t direction;
   double diffX, diffZ;
   diffX = x - this->pos.x;
   diffZ = z - this->pos.z;

   if (diffX > diffZ)
   {
     // We compare on the x axis
     if (diffX > 0)
     {
       direction = BLOCK_BOTTOM;
     }
     else
     {
       direction = BLOCK_EAST;
     }
   }
   else
   {
     // We compare on the z axis
     if (diffZ > 0)
     {
       direction = BLOCK_SOUTH;
     }
     else
     {
       direction = BLOCK_NORTH;
     }
   }
   return direction;
}

bool User::sendAll(uint8_t* data, uint32_t len)
{
  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged)
    {
      // Don't send to his user if he is DND and the message is a chat message
      if (!(Mineserver::get()->users()[i]->dnd && data[0] == PACKET_CHAT_MESSAGE))
      {
        Mineserver::get()->users()[i]->buffer.addToWrite(data, len);
      }
    }
  }
  return true;
}

bool User::sendAdmins(uint8_t* data, uint32_t len)
{
  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged && IS_ADMIN(Mineserver::get()->users()[i]->permissions))
    {
      Mineserver::get()->users()[i]->buffer.addToWrite(data, len);
    }
  }
  return true;
}

bool User::sendOps(uint8_t* data, uint32_t len)
{
  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged && IS_ADMIN(Mineserver::get()->users()[i]->permissions))
    {
      Mineserver::get()->users()[i]->buffer.addToWrite(data, len);
    }
  }
  return true;
}

bool User::sendGuests(uint8_t* data, uint32_t len)
{
  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged && IS_ADMIN(Mineserver::get()->users()[i]->permissions))
    {
      Mineserver::get()->users()[i]->buffer.addToWrite(data, len);
    }
  }
  return true;
}

bool User::addQueue(int x, int z)
{
  vec newMap(x, 0, z);

  // Make sure this chunk is not being removed, if it is, delete it from remove queue
  for (unsigned int i = 0; i < mapRemoveQueue.size(); i++)
  {
    if (mapRemoveQueue[i].x() == newMap.x() && mapRemoveQueue[i].z() == newMap.z())
    {
      mapRemoveQueue.erase(mapRemoveQueue.begin()+i);
      break;
    }
  }

  for (unsigned int i = 0; i < mapQueue.size(); i++)
  {
    // Check for duplicates
    if (mapQueue[i].x() == newMap.x() && mapQueue[i].z() == newMap.z())
    {
      return false;
    }
  }

  for (unsigned int i = 0; i < mapKnown.size(); i++)
  {
    // Check for duplicates
    if (mapKnown[i].x() == newMap.x() && mapKnown[i].z() == newMap.z())
    {
      return false;
    }
  }

  this->mapQueue.push_back(newMap);

  return true;
}

bool User::addRemoveQueue(int x, int z)
{
  vec newMap(x, 0, z);

  this->mapRemoveQueue.push_back(newMap);

  return true;
}

bool User::addKnown(int x, int z)
{
  vec newMap(x, 0, z);
  sChunk* chunk = Mineserver::get()->map()->chunks.getChunk(x,z);
  if (chunk == NULL)
  {
    return false;
  }

  chunk->users.insert(this);
  this->mapKnown.push_back(newMap);

  return true;
}

bool User::delKnown(int x, int z)
{
  sChunk* chunk = Mineserver::get()->map()->chunks.getChunk(x,z);
  if (chunk != NULL)
  {
    chunk->users.erase(this);
    // If no user needs this chunk
    if (chunk->users.size() == 0)
    {
      Mineserver::get()->map()->releaseMap(x,z);
    }
  }

  for (unsigned int i = 0; i < mapKnown.size(); i++)
  {
    if (mapKnown[i].x() == x && mapKnown[i].z() == z)
    {
      mapKnown.erase(mapKnown.begin()+i);
      return true;
    }
  }

  return false;
}

bool User::popMap()
{
  // If map in queue, push it to client
  while (this->mapRemoveQueue.size())
  {
    // Pre chunk
    buffer << (int8_t)PACKET_PRE_CHUNK << (int32_t)mapRemoveQueue[0].x() << (int32_t)mapRemoveQueue[0].z() << (int8_t)0;

    // Delete from known list
    delKnown(mapRemoveQueue[0].x(), mapRemoveQueue[0].z());

    // Remove from queue
    mapRemoveQueue.erase(mapRemoveQueue.begin());

    // return true;
  }

  return false;
}

namespace
{

  class DistanceComparator
  {
  private:
    vec target;
  public:
    DistanceComparator(vec tgt) : target(tgt)
    {
      target.y() = 0;
    }
    bool operator()(vec a, vec b) const
    {
      a.y() = 0;
      b.y() = 0;
      return vec::squareDistance(a, target) <
             vec::squareDistance(b, target);
    }
  };

}

bool User::pushMap()
{
  // Dont send all at once
  int maxcount = 10;
  // If map in queue, push it to client
  while (this->mapQueue.size() > 0 && maxcount > 0)
  {
    maxcount--;
    // Sort by distance from center
    vec target(static_cast<int>(pos.x / 16),
               static_cast<int>(pos.y / 16),
               static_cast<int>(pos.z / 16));
    sort(mapQueue.begin(), mapQueue.end(), DistanceComparator(target));

    Mineserver::get()->map()->sendToUser(this, mapQueue[0].x(), mapQueue[0].z());

    // Add this to known list
    addKnown(mapQueue[0].x(), mapQueue[0].z());

    // Remove from queue
    mapQueue.erase(mapQueue.begin());
  }

  return true;
}

bool User::teleport(double x, double y, double z)
{
  if (y > 127.0)
  {
    y = 127.0;
    LOGLF("Player Attempted to teleport with y > 127.0");
  }
  buffer << (int8_t)PACKET_PLAYER_POSITION_AND_LOOK << x << y << (double)0.0 << z
         << (float)0.f << (float)0.f << (int8_t)1;

  // Also update pos for other players
  updatePos(x, y, z, 0);

  pushMap();
  return true;
}

bool User::spawnUser(int x, int y, int z)
{
  Packet pkt;
  pkt << (int8_t)PACKET_NAMED_ENTITY_SPAWN << (int32_t)UID << nick
      << (int32_t)x << (int32_t)y << (int32_t)z << (int8_t)0 << (int8_t)0
      << (int16_t)0;
  sChunk*chunk = Mineserver::get()->map()->chunks.getChunk(blockToChunk(x >> 5), blockToChunk(z >> 5));
  if (chunk != NULL)
    chunk->sendPacket(pkt, this);
  return true;
}

bool User::spawnOthers()
{

  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->UID != this->UID && Mineserver::get()->users()[i]->nick != this->nick)
    {
      buffer << (int8_t)PACKET_NAMED_ENTITY_SPAWN << (int32_t)Mineserver::get()->users()[i]->UID << Mineserver::get()->users()[i]->nick
             << (int32_t)(Mineserver::get()->users()[i]->pos.x * 32) << (int32_t)(Mineserver::get()->users()[i]->pos.y * 32) << (int32_t)(Mineserver::get()->users()[i]->pos.z * 32)
             << (int8_t)0 << (int8_t)0 << (int16_t)0;
    }
  }
  return true;
}

bool User::sethealth(int userHealth)
{
  health = userHealth;
  buffer << (int8_t)PACKET_UPDATE_HEALTH << (int16_t)userHealth;
  // ToDo: Send destroy entity and spawn entity again
  return true;
}

bool User::respawn()
{
  this->health = 20;
  this->timeUnderwater = 0;
  buffer << (int8_t)PACKET_RESPAWN;
  return true;
}

bool User::dropInventory()
{
  for ( int i = 1; i < 45; i++ )
  {
    if ( inv[i].type != -1 )
    {
      Mineserver::get()->map()->createPickupSpawn((int)pos.x, (int)pos.y, (int)pos.z, inv[i].type, inv[i].count,inv[i].health,this);
      inv[i] = Item();
    }
  }
  return true;
}

bool User::isUnderwater()
{
   uint8_t topblock, topmeta;
   int y = ( pos.y - int(pos.y) <= 0.25 ) ? (int)pos.y + 1: (int)pos.y + 2;

   Mineserver::get()->map()->getBlock((int)pos.x, y, (int)pos.z, &topblock, &topmeta);

   if ( topblock == BLOCK_WATER || topblock == BLOCK_STATIONARY_WATER )
   {
      if ( (timeUnderwater / 5) > 15 && timeUnderwater % 5 == 0 )// 13 is Trial and Erorr
      {
        sethealth( health - 2 );
      }
      timeUnderwater += 1;
      return true;
   }

   timeUnderwater = 0;
   return false;
}

struct event* User::GetEvent()
{
  return &m_event;
}

std::vector<User*>& User::all()
{
  return Mineserver::get()->users();
}

bool User::isUser(int sock)
{
  uint8_t i;
  for (i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (Mineserver::get()->users()[i]->fd == sock)
    {
      return true;
    }
  }
  return false;
}

// Not case-sensitive search
User* User::byNick(std::string nick)
{
  // Get coordinates
  for (unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if (strToLower(Mineserver::get()->users()[i]->nick) == strToLower(nick))
    {
      return Mineserver::get()->users()[i];
    }
  }
  return NULL;
}

// Getter/Setter for item currently in hold
int16_t User::currentItemSlot()
{
  return m_currentItemSlot;
}

void User::setCurrentItemSlot(int16_t item_slot)
{
  m_currentItemSlot = item_slot;
}
