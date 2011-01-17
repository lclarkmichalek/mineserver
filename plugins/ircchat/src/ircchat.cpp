/*
    Copyright (c) 2011 Laurie Clark-Michalek

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use,
    copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
*/
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <deque>
#include <pthread.h>

#define MINESERVER_C_API

#include "../../../src/plugin_api.h"

#include "ircchat.h"
#define LOG_INFO 6

#define PLUGIN_IRCCHAT_VERSION 0.1
const char CHATCMDPREFIX   = '/';

mineserver_pointer_struct* mineserver;
IrcThread* ircthread;

std::string pluginName = "IrcChat";

std::string dtos(double n)
{
  std::ostringstream result;
  result << n;
  return result.str();
}

void *thread_init(void*_)
{
  mineserver->logger.log(LOG_INFO, "plugin.ircchat", "Started IRC Chat thread");
  std::string server = mineserver->config.sData("ircchat.server");
  std::string username = mineserver->config.sData("ircchat.username");
  std::string nickname = mineserver->config.sData("ircchat.nickname");
  std::string password = mineserver->config.sData("ircchat.password");
  std::string channel = mineserver->config.sData("ircchat.channel");
  int out = 0;
  out += ircthread->irc_client.start((char*)server.c_str(), 6667, (char*)nickname.c_str(), (char*)username.c_str(), (char*)nickname.c_str(), (char*)password.c_str());
  out +=ircthread->irc_client.join((char*)channel.c_str());
  if(out != 0)
  {
    mineserver->logger.log(LOG_INFO, "plugging.ircchat", "Could not connect to IRC server");
    return (void*)-1;
  }
  
  ircthread->irc_client.message_loop();
}

void IrcThread::quit()
{
  this->irc_client.quit((char*)"Goodbye");
}

void IrcThread::say(Message mes)
{
  std::string channel = mineserver->config.sData("ircchat.channel");
  std::ostringstream text;
  text << "[" << mes.user << "]: " << mes.message;
  this->irc_client.privmsg((char*)channel.c_str(), (char*)text.str().c_str());
}

bool message_handler(const char* user, size_t timestamp, const char* msg)
{
  Message mes;
  mes.message = msg;
  mes.user = user;
  ircthread->say(mes);
  return true;
}

PLUGIN_API_EXPORT void CALLCONVERSION ircchat_init(mineserver_pointer_struct* mineserver_temp)
{
  mineserver = mineserver_temp;
  ircthread = new IrcThread();
  pthread_create(&(ircthread->irc_thread), NULL, &thread_init, NULL);

  if (mineserver->plugin.getPluginVersion(pluginName.c_str()) > 0)
  {
    std::string msg = "ircchat is already loaded v."+dtos(mineserver->plugin.getPluginVersion(pluginName.c_str()));
    mineserver->logger.log(LOG_INFO, "plugin.ircchat", msg.c_str());
    return;
  }
  std::string msg = "Loaded "+pluginName+"!";
  mineserver->logger.log(LOG_INFO, "plugin.ircchat", msg.c_str());

  mineserver->plugin.setPluginVersion(pluginName.c_str(), PLUGIN_IRCCHAT_VERSION);

  mineserver->plugin.addCallback("PlayerChatPre", (void *)message_handler);
}