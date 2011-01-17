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

#include <stdlib.h>
#ifdef WIN32
  #include <conio.h>
  #include <winsock2.h>
  #include <process.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <string.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <sys/times.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <cassert>
#include <deque>
#include <map>
#include <iostream>
#include <fstream>
#include <event.h>
#include <ctime>
#include <vector>
#include <zlib.h>
#include <signal.h>

#include "constants.h"
#include "mineserver.h"
#include "logger.h"
#include "sockets.h"
#include "tools.h"
#include "map.h"
#include "user.h"
#include "chat.h"
#include "worldgen/mapgen.h"
#include "config.h"
#include "config/node.h"
#include "nbt.h"
#include "packets.h"
#include "physics.h"
#include "plugin.h"
#include "furnaceManager.h"
#include "cursesScreen.h"
#include "cliScreen.h"
#include "hook.h"
#ifdef WIN32
static bool quit = false;
#endif

int setnonblock(int fd)
{
#ifdef WIN32
  u_long iMode = 1;
  ioctlsocket(fd, FIONBIO, &iMode);
#else
  int flags;

  flags  = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
#endif

  return 1;
}

// Handle signals
void sighandler(int sig_num)
{
  Mineserver::get()->stop();
}

int main(int argc, char* argv[])
{
  signal(SIGTERM, sighandler);
  signal(SIGINT, sighandler);

  srand((uint32_t)time(NULL));

  return Mineserver::get()->run(argc, argv);
}

bool log_to_screen(int type, const char* source, const char* message)
{
  Mineserver::get()->screen()->log((LogType::LogType)type, std::string(source), std::string(message));
  return true;
}

Mineserver::Mineserver()
{
  m_map            = new Map;
  m_chat           = new Chat;
  m_plugin         = new Plugin;
  m_screen         = new CliScreen;
  m_physics        = new Physics;
  m_config         = new Config;
  m_furnaceManager = new FurnaceManager;
  m_packetHandler  = new PacketHandler;
  m_mapGen         = new MapGen;
  m_logger         = new Logger;
  m_inventory      = new Inventory;
}

event_base* Mineserver::getEventBase()
{
  return m_eventBase;
}

void Mineserver::updatePlayerList()
{
  // Update the player window
  Mineserver::get()->screen()->updatePlayerList(users());
}

void Mineserver::saveAllPlayers()
{
  for (int i = users().size()-1; i >= 0; i--)
  {
    if (users()[i]->logged)
    {
      users()[i]->saveData();
    }
  }
}


int Mineserver::run(int argc, char *argv[])
{
  uint32_t starttime = (uint32_t)time(0);
  uint32_t tick      = (uint32_t)time(0);

#ifdef FADOR_PLUGIN
  init_plugin_api();
#endif

  static_cast<Hook3<bool,int,const char*,const char*>*>(plugin()->getHook("LogPost"))->addCallback(&log_to_screen);

  // Init our Screen
  screen()->init(VERSION);
  logger()->log(LogType::LOG_INFO, "Mineserver", "Welcome to Mineserver v" + VERSION);
  updatePlayerList();

  initConstants();

  std::string file_config;
  file_config.assign(CONFIG_FILE);

  if (argc > 1)
  {
    file_config.assign(argv[1]);
  }

  // Initialize conf
  Mineserver::get()->config()->load(file_config);

  // If needed change interface and reinitialize the new Screen
  std::string iface = Mineserver::get()->config()->sData("system.interface");
  if (iface == "curses")
  {
    screen()->end();
    // TODO: we lose everything written to the screen
    //      up to this point when using curses
    m_screen = new CursesScreen;
    screen()->init(VERSION);
    logger()->log(LogType::LOG_INFO, "Mineserver", "Interface changed to curses");
    updatePlayerList();
  }

  if (Mineserver::get()->config()->has("system.plugins") && (Mineserver::get()->config()->type("system.plugins") == CONFIG_NODE_LIST))
  {
    std::list<std::string>* tmp = Mineserver::get()->config()->mData("system.plugins")->keys();
    std::list<std::string>::iterator it = tmp->begin();
    for (;it!=tmp->end();++it)
    {
      Mineserver::get()->plugin()->loadPlugin(*it, Mineserver::get()->config()->sData("system.plugins."+(*it)));
    }
    delete tmp;
  }

  // Write PID to file
  std::ofstream pid_out((Mineserver::get()->config()->sData("system.pid_file")).c_str());
  if (!pid_out.fail())
  {
#ifdef WIN32
    pid_out << _getpid();
#else
    pid_out << getpid();
#endif
  }
  pid_out.close();

  // Set physics enable state according to config
  Mineserver::get()->physics()->enabled = (Mineserver::get()->config()->bData("system.physics.enabled"));

  // Initialize map
  Mineserver::get()->map()->init();

  if (Mineserver::get()->config()->bData("map.generate_spawn.enabled"))
  {
    logger()->log(LogType::LOG_INFO, "Mapgen", "Generating spawn area...");
    int size = Mineserver::get()->config()->iData("map.generate_spawn.size");
    bool show_progress = Mineserver::get()->config()->bData("map.generate_spawn.show_progress");
#ifdef WIN32
    DWORD t_begin = 0, t_end = 0;
#else
    clock_t t_begin = 0, t_end = 0;
#endif

    for (int x=-size;x<=size;x++)
    {
#ifdef WIN32
      if (show_progress)
      {
        t_begin = timeGetTime();
      }
#else
      if (show_progress)
      {
        t_begin = clock();
      }
#endif
      for (int z = -size; z <= size; z++)
      {
        Mineserver::get()->map()->loadMap(x, z);
      }

      if (show_progress)
      {
#ifdef WIN32
        t_end = timeGetTime ();
        logger()->log(LogType::LOG_INFO, "Map", dtos((x+size+1)*(size*2+1)) + "/" + dtos((size*2+1)*(size*2+1)) + " done. " + dtos((t_end-t_begin)/(size*2+1)) + "ms per chunk");
#else
        t_end = clock();
        logger()->log(LogType::LOG_INFO, "Map", dtos((x+size+1)*(size*2+1)) + "/" + dtos((size*2+1)*(size*2+1)) + " done. " + dtos(((t_end-t_begin)/(CLOCKS_PER_SEC/1000))/(size*2+1)) + "ms per chunk");
#endif
      }
    }
#ifdef _DEBUG
    LOG(DEBUG, "Map", "Spawn area ready!");
#endif
  }

  // Initialize packethandler
  Mineserver::get()->packetHandler()->init();

  // Load ip from config
  std::string ip = Mineserver::get()->config()->sData("net.ip");

  // Load port from config
  int port = Mineserver::get()->config()->iData("net.port");

#ifdef WIN32
  WSADATA wsaData;
  int iResult;
  // Initialize Winsock
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0)
  {
    printf("WSAStartup failed with error: %d\n", iResult);
    Mineserver::get()->screen()->end();
    return EXIT_FAILURE;
  }
#endif

  struct sockaddr_in addresslisten;
  int reuse = 1;

  m_eventBase = (event_base*)event_init();
#ifdef WIN32
  m_socketlisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
  m_socketlisten = socket(AF_INET, SOCK_STREAM, 0);
#endif

  if (m_socketlisten < 0)
  {
    Mineserver::get()->logger()->log(LogType::LOG_ERROR, "Socket", "Failed to create listen socket");
    Mineserver::get()->screen()->end();
    return 1;
  }

  memset(&addresslisten, 0, sizeof(addresslisten));

  addresslisten.sin_family      = AF_INET;
  addresslisten.sin_addr.s_addr = inet_addr(ip.c_str());
  addresslisten.sin_port        = htons(port);

  setsockopt(m_socketlisten, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

  // Bind to port
  if (bind(m_socketlisten, (struct sockaddr*)&addresslisten, sizeof(addresslisten)) < 0)
  {
    Mineserver::get()->logger()->log(LogType::LOG_ERROR, "Socket", "Failed to bind to " + ip + ":" + dtos(port));
    Mineserver::get()->screen()->end();
    return 1;
  }
  
  if (listen(m_socketlisten, 5) < 0)
  {
    Mineserver::get()->logger()->log(LogType::LOG_ERROR, "Socket", "Failed to listen to socket" );
    Mineserver::get()->screen()->end();
    return 1;
  }

  setnonblock(m_socketlisten);
  event_set(&m_listenEvent, m_socketlisten, EV_WRITE|EV_READ|EV_PERSIST, accept_callback, NULL);
  event_add(&m_listenEvent, NULL);

  if (ip == "0.0.0.0")
  {
    // Print all local IPs
    char name[255];
    gethostname (name, sizeof(name));
    struct hostent* hostinfo = gethostbyname(name);
    Mineserver::get()->logger()->log(LogType::LOG_INFO, "Socket", "Listening on: ");
    int ipIndex = 0;
    while (hostinfo && hostinfo->h_addr_list[ipIndex])
    {
      std::string ip(inet_ntoa(*(struct in_addr*)hostinfo->h_addr_list[ipIndex++]));
      Mineserver::get()->logger()->log(LogType::LOG_INFO, "Socket", ip + ":" + dtos(port));
    }
  }
  else
  {
    std::string myip(ip);
    Mineserver::get()->logger()->log(LogType::LOG_INFO, "Socket", myip + ":" + dtos(port));
  }
  // std::cout << std::endl;

  timeval loopTime;
  loopTime.tv_sec  = 0;
  loopTime.tv_usec = 200000; // 200ms

  m_running = true;
  event_base_loopexit(m_eventBase, &loopTime);

  // Create our Server Console user so we can issue commands
  User* serverUser = new User(-1, SERVER_CONSOLE_UID);
  serverUser->changeNick("[Server]");

  time_t timeNow = time(NULL);
  while (m_running && event_base_loop(m_eventBase, 0) == 0)
  {
    // Run 200ms timer hook
    static_cast<Hook0<bool>*>(plugin()->getHook("Timer200"))->doAll();

    // Append current command and check if user entered return
    if (Mineserver::get()->screen()->hasCommand())
    {
      // Now handle this command as normal
      Mineserver::get()->chat()->handleMsg(serverUser, Mineserver::get()->screen()->getCommand().c_str());
    }

    timeNow = time(0);
    if (timeNow-starttime > 10)
    {
      starttime = (uint32_t)timeNow;

      // If users, ping them
      if (User::all().size() > 0)
      {
        // 0x00 package
        uint8_t data = 0;
        User::all()[0]->sendAll(&data, 1);

        // Send server time
        Packet pkt;
        pkt << (int8_t)PACKET_TIME_UPDATE << (int64_t)Mineserver::get()->map()->mapTime;
        User::all()[0]->sendAll((uint8_t*)pkt.getWrite(), pkt.getWriteLen());
      }

      // Check for tree generation from saplings
      map()->checkGenTrees();

      // TODO: Run garbage collection for chunk storage dealie?

      // Run 10s timer hook
      static_cast<Hook0<bool>*>(plugin()->getHook("Timer10000"))->doAll();
    }

    // Every second
    if (timeNow-tick > 0)
    {
      tick = (uint32_t)timeNow;
      // Loop users
      for (int i = users().size()-1; i >= 0; i--)
      {
        // No data received in 3s, timeout
        if (users()[i]->logged && (timeNow-users()[i]->lastData) > 3)
        {
          Mineserver::get()->logger()->log(LogType::LOG_INFO, "Sockets", "Player "+users()[i]->nick+" timed out");

          delete users()[i];
        }
        else
        {
          users()[i]->pushMap();
          users()[i]->popMap();
        }

        // Minecart hacks!!
        /*
        if (User::all()[i]->attachedTo)
        {
          Packet pkt;
          pkt << PACKET_ENTITY_VELOCITY << (int32_t)User::all()[i]->attachedTo <<  (int16_t)10000       << (int16_t)0 << (int16_t)0;
          // pkt << PACKET_ENTITY_RELATIVE_MOVE << (int32_t)User::all()[i]->attachedTo <<  (int8_t)100       << (int8_t)0 << (int8_t)0;
          User::all()[i]->sendAll((int8_t*)pkt.getWrite(), pkt.getWriteLen());
        }
        */

      }

      map()->mapTime+=20;
      if (map()->mapTime >= 24000)
      {
        map()->mapTime = 0;
      }


      // Check for Furnace activity
      Mineserver::get()->furnaceManager()->update();

      // Run 1s timer hook
      static_cast<Hook0<bool>*>(plugin()->getHook("Timer1000"))->doAll();
    }

    // Physics simulation every 200ms
    Mineserver::get()->physics()->update();

    // Underwater check / drowning
    // ToDo: this could be done a bit differently? - Fador
    int i = 0;
    int s = User::all().size();
    for (i=0;i<s;i++)
    {
      User::all()[i]->isUnderwater();
    }

    event_base_loopexit(m_eventBase, &loopTime);
  }

#ifdef WIN32
  closesocket(m_socketlisten);
#else
  close(m_socketlisten);
#endif

  // Remove the PID file
#ifdef WIN32
  _unlink((Mineserver::get()->config()->sData("system.pid_file")).c_str());
#else
  unlink((Mineserver::get()->config()->sData("system.pid_file")).c_str());
#endif

  // Let the user know we're shutting the server down cleanly
  logger()->log(LogType::LOG_INFO, "Mineserver", "Shutting down...");

  // End our NCurses session
  screen()->end();

  /* Free memory */
  delete m_map;
  delete m_chat;
  delete m_plugin;
  delete m_screen;
  delete m_physics;
  delete m_config;
  delete m_furnaceManager;
  delete m_packetHandler;
  delete m_mapGen;
  delete m_logger;
  delete m_inventory;

  delete serverUser;

  freeConstants();

  event_base_free(m_eventBase);

  return EXIT_SUCCESS;
}

bool Mineserver::stop()
{
  m_running = false;
  return true;
}
