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

#include <map>
#include <vector>
#include <string>
#include <stdarg.h>

#include "sys/stat.h"

#include "mineserver.h"
#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "logger.h"
#include "chat.h"

#include "hook.h"
#include "plugin.h"
#include "config.h"
#include "map.h"
#include "blocks/default.h"
#include "blocks/falling.h"
#include "blocks/torch.h"
#include "blocks/plant.h"
#include "blocks/snow.h"
#include "blocks/liquid.h"
#include "blocks/fire.h"
#include "blocks/stair.h"
#include "blocks/door.h"
#include "blocks/sign.h"
#include "blocks/tracks.h"
#include "blocks/chest.h"

#define MINESERVER
#include "plugin_api.h"

mineserver_pointer_struct plugin_api_pointers;

// HELPER FUNCTIONS
User* userFromName(std::string user)
{
  for(unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if(Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged)
    {
      // Don't send to his user if he is DND and the message is a chat message
      if(user == Mineserver::get()->users()[i]->nick)
      {
        return Mineserver::get()->users()[i];
      }
    }
  }
  return NULL;
}

// PLUGIN_API FUNCTIONS

bool plugin_hasPluginVersion(const char* name)
{
 return Mineserver::get()->plugin()->hasPluginVersion(std::string(name));
}

float plugin_getPluginVersion(const char* name)
{
 return Mineserver::get()->plugin()->getPluginVersion(std::string(name));
}

void plugin_setPluginVersion(const char* name, float version)
{
  Mineserver::get()->plugin()->setPluginVersion(std::string(name),version);
}

void plugin_remPluginVersion(const char* name)
{
  Mineserver::get()->plugin()->remPluginVersion(std::string(name));
}

bool plugin_hasPointer(const char* name)
{
  return Mineserver::get()->plugin()->hasPointer(std::string(name));
}

void* plugin_getPointer(const char* name)
{
  return Mineserver::get()->plugin()->getPointer(std::string(name));
}

void plugin_setPointer(const char* name, void* pointer)
{
  Mineserver::get()->plugin()->setPointer(std::string(name), pointer);
}

void plugin_remPointer(const char* name)
{
  Mineserver::get()->plugin()->remPointer(std::string(name));
}

bool plugin_hasHook(const char* hookID)
{
  return Mineserver::get()->plugin()->hasHook(hookID);
}

Hook* plugin_getHook(const char* hookID)
{
  return Mineserver::get()->plugin()->getHook(hookID);
}

void plugin_setHook(const char* hookID, Hook* hook)
{
  Mineserver::get()->plugin()->setHook(hookID, hook);
}

void plugin_remHook(const char* hookID)
{
  Mineserver::get()->plugin()->remHook(hookID);
}

bool hook_hasCallback(const char* hookID, void* function)
{
  return Mineserver::get()->plugin()->getHook(hookID)->hasCallback(function);
}

void hook_addCallback(const char* hookID, void* function)
{
  Mineserver::get()->plugin()->getHook(hookID)->addCallback(function);
}

void hook_addIdentifiedCallback(const char* hookID, void* identifier, void* function)
{
  Mineserver::get()->plugin()->getHook(hookID)->addIdentifiedCallback(identifier, function);
}

void hook_remCallback(const char* hookID, void* function)
{
  Mineserver::get()->plugin()->getHook(hookID)->remCallback(function);
}

bool hook_doUntilTrue(const char* hookID, ...)
{
  bool result = false;
  va_list argList;
  va_start(argList, hookID);
  result = Mineserver::get()->plugin()->getHook(hookID)->doUntilTrueVA(argList);
  va_end(argList);
  return result;
}

bool hook_doUntilFalse(const char* hookID, ...)
{
  bool result = false;
  va_list argList;
  va_start(argList, hookID);
  result = Mineserver::get()->plugin()->getHook(hookID)->doUntilFalseVA(argList);
  va_end(argList);
  return result;
}

void hook_doAll(const char* hookID, ...)
{
  va_list argList;
  va_start(argList, hookID);
  Mineserver::get()->plugin()->getHook(hookID)->doAllVA(argList);
  va_end(argList);
}

// LOGGER WRAPPER FUNCTIONS
void logger_log(int type, const char* source, const char* message)
{
  Mineserver::get()->logger()->log((LogType::LogType)type, std::string(source), std::string(message));
}

// CHAT WRAPPER FUNCTIONS
bool chat_sendmsgTo(const char* user,const char* msg)
{
  std::string userStr(user);
  if (userStr == "[Server]")
  {
	  LOG(INFO, "Chat", msg);
	  return true;
  }
  for(unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if(Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged)
    {
      // Don't send to his user if he is DND and the message is a chat message
      if(userStr == Mineserver::get()->users()[i]->nick)
      {
        Mineserver::get()->users()[i]->buffer << (int8_t)PACKET_CHAT_MESSAGE << std::string(msg);
        return true;
      }
    }
  }
  return false;
}

bool chat_sendmsg(const char* msg)
{
  std::string msgStr(msg);
  for(unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if(Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged)
    {
      // Don't send to his user if he is DND and the message is a chat message
      if(!(Mineserver::get()->users()[i]->dnd))
      {
        Mineserver::get()->users()[i]->buffer << (int8_t)PACKET_CHAT_MESSAGE << msgStr;
      }
    }
  }
  return true;
}

bool chat_sendUserlist(const char* user)
{
  std::string userStr(user);

  User *userPtr = userFromName(userStr);
  if(userPtr != NULL)
  {
    Mineserver::get()->chat()->sendUserlist(userPtr);
    return true;
  }
  return false;
}

// MAP WRAPPER FUNCTIONS
bool map_setTime(int timeValue)
{
  Mineserver::get()->map()->mapTime = timeValue;
  Packet pkt;
  pkt << (int8_t)PACKET_TIME_UPDATE << (int64_t)Mineserver::get()->map()->mapTime;

  if(User::all().size())
  {
    User::all()[0]->sendAll((uint8_t*)pkt.getWrite(), pkt.getWriteLen());
  }
  return true;
}

void map_createPickupSpawn(int x, int y, int z, int type, int count, int health, const char* user)
{
  User* tempUser = NULL;
  if(user != NULL)
  {
    tempUser = userFromName(std::string(user));
  }
  Mineserver::get()->map()->createPickupSpawn(x,y,z,type,count,health,tempUser);
}

void map_getSpawn(int* x, int* y, int* z)
{  
  *x = Mineserver::get()->map()->spawnPos.x();
  *y = Mineserver::get()->map()->spawnPos.y();
  *z = Mineserver::get()->map()->spawnPos.z();
}

bool map_getBlock(int x, int y, int z, unsigned char* type,unsigned char* meta)
{
  return Mineserver::get()->map()->getBlock(x,y,z, type, meta);
}

bool map_setBlock(int x, int y, int z, unsigned char type,unsigned char meta)
{
  Mineserver::get()->map()->sendBlockChange(x, y, z, type, meta);
  return Mineserver::get()->map()->setBlock(x,y,z, type, meta);
}

void map_saveWholeMap(void)
{
  Mineserver::get()->map()->saveWholeMap();
}

unsigned char* map_getMapData_block(int x, int z)
{
  sChunk* chunk=Mineserver::get()->map()->getMapData(x,z);
  if(chunk != NULL)
  {
    return chunk->blocks;
  }
  return NULL;
}
unsigned char* map_getMapData_meta(int x, int z)
{
  sChunk* chunk=Mineserver::get()->map()->getMapData(x,z);
  if(chunk != NULL)
  {
    return chunk->data;
  }
  return NULL;
}
unsigned char* map_getMapData_skylight(int x, int z)
{
  sChunk* chunk=Mineserver::get()->map()->getMapData(x,z);
  if(chunk != NULL)
  {
    return chunk->skylight;
  }
  return NULL;
}
unsigned char* map_getMapData_blocklight(int x, int z)
{
  sChunk* chunk=Mineserver::get()->map()->getMapData(x,z);
  if(chunk != NULL)
  {
    return chunk->blocklight;
  }
  return NULL;
}

// USER WRAPPER FUNCTIONS
bool user_getPosition(const char* user, double* x, double* y, double* z, float* yaw, float* pitch, double *stance)
{
  std::string userStr(user);
  for(unsigned int i = 0; i < Mineserver::get()->users().size(); i++)
  {
    if(Mineserver::get()->users()[i]->fd && Mineserver::get()->users()[i]->logged)
    {
      //Is this the user?
      if(userStr == Mineserver::get()->users()[i]->nick)
      {
        //For safety, check for NULL pointers!
        if(x != NULL)
          *x=Mineserver::get()->users()[i]->pos.x;
        if(y != NULL)
          *y=Mineserver::get()->users()[i]->pos.y;
        if(z != NULL)
          *z=Mineserver::get()->users()[i]->pos.z;
        if(yaw != NULL)
          *yaw=Mineserver::get()->users()[i]->pos.yaw;
        if(pitch != NULL)
          *pitch=Mineserver::get()->users()[i]->pos.pitch;
        if(stance != NULL)
          *stance=Mineserver::get()->users()[i]->pos.stance;
        //We found the user
        return true;
      }
    }
  }
  return false;
}

bool user_teleport(const char* user,double x, double y, double z)
{
  User* tempUser = userFromName(std::string(user));
  if(tempUser != NULL)
  {
    tempUser->teleport(x, y, z);
    return true;
  }
  return false;
}

bool user_sethealth(const char* user,int userHealth)
{
  User* tempUser = userFromName(std::string(user));
  if(tempUser != NULL)
  {
    tempUser->sethealth(userHealth);
    return true;
  }
  return false;
}

// CONFIG WRAPPER FUNCTIONS
bool config_has(const char* name)
{
  return Mineserver::get()->config()->has(std::string(name));
}

int config_iData(const char* name)
{
  return Mineserver::get()->config()->iData(std::string(name));
}

int64_t config_lData(const char* name)
{
  return Mineserver::get()->config()->lData(std::string(name));
}

float config_fData(const char* name)
{
  return Mineserver::get()->config()->fData(std::string(name));
}

double config_dData(const char* name)
{
  return Mineserver::get()->config()->dData(std::string(name));
}

const char* config_sData(const char* name)
{
  return Mineserver::get()->config()->sData(std::string(name)).c_str();
}

bool config_bData(const char* name)
{
  return Mineserver::get()->config()->bData(std::string(name));
}

// Initialization of the plugin_api function pointer array
void init_plugin_api(void)
{
  plugin_api_pointers.logger.log                   = &logger_log;

  plugin_api_pointers.chat.sendmsg                 = &chat_sendmsg;
  plugin_api_pointers.chat.sendmsgTo               = &chat_sendmsgTo;
  plugin_api_pointers.chat.sendUserlist            = &chat_sendUserlist;

  plugin_api_pointers.plugin.hasPluginVersion      = &plugin_hasPluginVersion;
  plugin_api_pointers.plugin.getPluginVersion      = &plugin_getPluginVersion;
  plugin_api_pointers.plugin.setPluginVersion      = &plugin_setPluginVersion;
  plugin_api_pointers.plugin.remPluginVersion      = &plugin_remPluginVersion;
  plugin_api_pointers.plugin.hasPointer            = &plugin_hasPointer;
  plugin_api_pointers.plugin.getPointer            = &plugin_getPointer;
  plugin_api_pointers.plugin.setPointer            = &plugin_setPointer;
  plugin_api_pointers.plugin.remPointer            = &plugin_remPointer;
  plugin_api_pointers.plugin.hasHook               = &plugin_hasHook;
  plugin_api_pointers.plugin.getHook               = &plugin_getHook;
  plugin_api_pointers.plugin.setHook               = &plugin_setHook;
  plugin_api_pointers.plugin.remHook               = &plugin_remHook;
  plugin_api_pointers.plugin.hasCallback           = &hook_hasCallback;
  plugin_api_pointers.plugin.addCallback           = &hook_addCallback;
  plugin_api_pointers.plugin.addIdentifiedCallback = &hook_addIdentifiedCallback;
  plugin_api_pointers.plugin.remCallback           = &hook_remCallback;
  plugin_api_pointers.plugin.doUntilTrue           = &hook_doUntilTrue;
  plugin_api_pointers.plugin.doUntilFalse          = &hook_doUntilFalse;
  plugin_api_pointers.plugin.doAll                 = &hook_doAll;

  plugin_api_pointers.map.setTime                  = &map_setTime;
  plugin_api_pointers.map.createPickupSpawn        = &map_createPickupSpawn;
  plugin_api_pointers.map.getSpawn                 = &map_getSpawn;
  plugin_api_pointers.map.setBlock                 = &map_setBlock;
  plugin_api_pointers.map.getBlock                 = &map_getBlock;
  plugin_api_pointers.map.saveWholeMap             = &map_saveWholeMap;
  plugin_api_pointers.map.getMapData_block         = &map_getMapData_block;
  plugin_api_pointers.map.getMapData_meta          = &map_getMapData_meta;
  plugin_api_pointers.map.getMapData_skylight      = &map_getMapData_skylight;
  plugin_api_pointers.map.getMapData_blocklight    = &map_getMapData_blocklight;

  plugin_api_pointers.user.getPosition             = &user_getPosition;
  plugin_api_pointers.user.teleport                = &user_teleport;
  plugin_api_pointers.user.sethealth               = &user_sethealth;

  plugin_api_pointers.config.has                   = &config_has;
  plugin_api_pointers.config.iData                 = &config_iData;
  plugin_api_pointers.config.lData                 = &config_lData;
  plugin_api_pointers.config.fData                 = &config_fData;
  plugin_api_pointers.config.dData                 = &config_dData;
  plugin_api_pointers.config.sData                 = &config_sData;
  plugin_api_pointers.config.bData                 = &config_bData;



}
