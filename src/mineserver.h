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

#ifndef _MINESERVER_H
#define _MINESERVER_H

#include <iostream>
#include <vector>


#ifdef WIN32
  // This is needed for event to work on Windows.
  #include <Winsock2.h>
#endif
#include <event.h>

class User;
class Map;
class Chat;
class Plugin;
class Screen;
class Physics;
class Config;
class FurnaceManager;
class PacketHandler;
class MapGen;
class Logger;
class Inventory;

#ifdef FADOR_PLUGIN
#define MINESERVER
#include "plugin_api.h"
#undef MINESERVER
#endif

struct event_base;

class Mineserver
{
public:
  static Mineserver* get()
  {
    static Mineserver* m_instance;

    if (!m_instance)
    {
      m_instance = new Mineserver;
    }

    return m_instance;
  }

  int run(int argc, char* argv[]);
  bool stop();
  event_base* getEventBase();

  std::vector<User*>& users() { return m_users; }

  struct event m_listenEvent;
  int m_socketlisten;
  void updatePlayerList();

  Map* map() const { return m_map; }
  void setMap(Map* map) { m_map = map; }
  Chat* chat() const { return m_chat; }
  void setChat(Chat* chat) { m_chat = chat; }
  Plugin* plugin() const { return m_plugin; }
  void setPlugin(Plugin* plugin) { m_plugin = plugin; }
  Screen* screen() const { return m_screen; }
  void setScreen(Screen* screen) { m_screen = screen; }
  Physics* physics() const { return m_physics; }
  void setPhysics(Physics* physics) { m_physics = physics; }
  Config* config() const { return m_config; }
  void setConfig(Config* config) { m_config = config; }
  FurnaceManager* furnaceManager() const { return m_furnaceManager; }
  void setFurnaceManager(FurnaceManager* furnaceManager) { m_furnaceManager = furnaceManager; }
  PacketHandler* packetHandler() const { return m_packetHandler; }
  void setPacketHandler(PacketHandler* packetHandler) { m_packetHandler = packetHandler; }
  MapGen* mapGen() const { return m_mapGen; }
  void setMapGen(MapGen* mapGen) { m_mapGen = mapGen; }
  Logger* logger() const { return m_logger; }
  void setLogger(Logger* logger) { m_logger = logger; }
  Inventory* inventory() const { return m_inventory; }
  void setInventory(Inventory* inventory) { m_inventory = m_inventory; }

  void saveAllPlayers();

private:
  Mineserver();
  event_base* m_eventBase;
  bool m_running;
  // holds all connected users
  std::vector<User*> m_users;

  Map* m_map;
  Chat* m_chat;
  Plugin* m_plugin;
  Screen* m_screen;
  Physics* m_physics;
  Config* m_config;
  FurnaceManager* m_furnaceManager;
  PacketHandler* m_packetHandler;
  MapGen* m_mapGen;
  Logger* m_logger;
  Inventory* m_inventory;
};

#endif
