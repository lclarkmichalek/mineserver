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
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <string.h>
#include <iostream>


#define MINESERVER_C_API

#include "../../../src/plugin_api.h"

#include "ircbridge.h"

using namespace std;

#define LOG_INFO 6
#define LOG_WARN 4

#define PLUGIN_IRCBRIDGE_VERSION 0.1
const char CHATCMDPREFIX   = '/';

mineserver_pointer_struct* mineserver;
IrcThread* ircthread;

string pluginName = "IrcBridge";

string dtos(double n)
{
  ostringstream result;
  result << n;
  return result.str();
}

void IrcThread::quit()
{
  this->irc_client.quit((char*)"Goodbye");
}

bool IrcThread::join_channels()
{
  int out;
  ostringstream mes1, mes2;
  
  mes1 << "Joining channel " << this->config.admin_channel.c_str();
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", mes1.str().c_str());
  out += this->irc_client.join((char*)this->config.admin_channel.c_str());
  
  if(this->config.spew && this->config.spew_channel != this->config.admin_channel && out == 0)
  {
    mes2 << "Joining channel " << this->config.spew_channel.c_str();
    mineserver->logger.log(LOG_INFO, "plugins.ircbridge", mes1.str().c_str());
    out += this->irc_client.join((char*)this->config.spew_channel.c_str());
  }
  
  if (out != 0)
    return false;
  else
    return true;
}

bool minecraft_message_handler(const char* user, size_t timestamp, const char* msg)
{
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", "Message");
  Message mes;
  mes.message = msg;
  mes.user = user;
  mes.destination = DEST_IRC;
  
  ostringstream logmsg;
  logmsg << mes.user << " said: " << mes.message;
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", logmsg.str().c_str());
  
  Action act = ircthread->message_to_action(mes);
  
  if (!ircthread->action_allowed(act))
  {
    ircthread->irc_client.privmsg((char*)act.user.c_str(), "Insufficient permissions");
    return 0;
  }
  
  ircthread->handle_action(act);
  
  return true;
}

bool minecraft_command_handler(const char* userIn, const char* cmdIn, int argc, char** argv)
{
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", "Command");
  Message mes;
  mes.user = userIn;
  ostringstream in;
  in << "/" << cmdIn;
  for(int i = 0; i != argc; i++)
  {
    in << " " << argv[i];
  }
  
  ostringstream logmsg;
  logmsg << mes.user << " said: " << in.str();
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", logmsg.str().c_str());
  
  mes.message = in.str();
  mes.destination = DEST_IRC;
  
  Action act = ircthread->message_to_action(mes);
  
  if (!ircthread->action_allowed(act))
  {
    ircthread->irc_client.privmsg((char*)act.user.c_str(), "Insufficient permissions");
    return 0;
  }
  
  ircthread->handle_action(act);
  
  return true;
}

int irc_message_handler(char* params, irc_reply_data* hostd, void* conn)
{
  string text(params);
  //remove first char wich will be a ':' char
  text.erase(text.begin());
  string user(hostd->nick);
  
  ostringstream logmsg;
  logmsg << user << " said: " << text;
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", logmsg.str().c_str());
  
  Message mes;
  mes.user = user;
  mes.message = text;
  mes.destination = DEST_MINECRAFT;
  Action act = ircthread->message_to_action(mes);
  
  if (!ircthread->action_allowed(act))
  {
    ircthread->irc_client.privmsg((char*)act.user.c_str(), "Insufficient permissions");
    return 0;
  }
  
  ircthread->handle_action(act);
  return 0;
}

Action IrcThread::message_to_action(Message mes)
{
  Action act;
  
  act.destination = mes.destination;
  act.user = mes.user;
  char* ptr = strtok((char*)mes.message.c_str(), " ");
  string first;
  while(ptr != NULL)
  {
    ptr = strtok(NULL, " ");
    if (ptr != NULL)
      act.params.push_back(ptr);
    else
      break;
  }
  
  act.permission = PERM_USER;
  if (act.destination == DEST_IRC)
    for (vector<string>::iterator it = this->config.admins_minecraft.begin(); it != this->config.admins_minecraft.end(); it++)
      if (*it == act.user)
      {
        act.permission = PERM_ADMIN;
        break;
      }
  else
    for (vector<string>::iterator it = this->config.admins_irc.begin(); it != this->config.admins_irc.end(); it++)
      if (*it == act.user)
      {
        act.permission = PERM_ADMIN;
        break;
      }
    
  
  if (act.destination == DEST_IRC)
  {
    if (mes.message[0] != this->config.m_command_prefix)
      act.type = TYPE_CHAT;
    else
    {
      string key = "";
      key += strtok((char*)mes.message.c_str(), " ");
      key.erase(key.begin());
      
      if (key == this->config.m_talk_command || (this->config.m_command_aliases.count(key) > 0 && this->config.m_command_aliases[key] == this->config.m_talk_command))
        act.type = TYPE_MSG;
      else if (key == this->config.m_notice_command || (this->config.m_command_aliases.count(key) > 0 && this->config.m_command_aliases[key] == this->config.m_notice_command))
        act.type = TYPE_NOTICE;
      else
        act.type = TYPE_CMD;
    }
  }
  else
  {
    if (mes.message.c_str()[0] != this->config.i_command_prefix)
      act.type = TYPE_CHAT;
    else
    {
      string key = "";
      key += strtok((char*)mes.message.c_str(), " ");
      key.erase(key.begin());
      
      if (key == this->config.i_talk_command || (this->config.i_command_aliases.count(key) > 0 && this->config.i_command_aliases[key] == this->config.i_talk_command))
        act.type = TYPE_MSG;
      else if (key == this->config.i_notice_command || (this->config.i_command_aliases.count(key) > 0 && this->config.i_command_aliases[key] == this->config.i_notice_command))
        act.type = TYPE_NOTICE;
      else if (key == this->config.i_execute_command || (this->config.i_command_aliases.count(key) > 0 && this->config.i_command_aliases[key] == this->config.i_execute_command))
        act.type = TYPE_CMD;
      else
        act.type = TYPE_CHAT;
    }
  }
  
  if (act.type == TYPE_CHAT)
    act.params.insert(act.params.begin(), strtok((char*)mes.message.c_str(), " "));
  
  return act;
}

void IrcThread::handle_action(Action act)
{
  switch (act.type)
  {
    case TYPE_CHAT:
      this->resolve_chat_action(act);
      break;
    case TYPE_MSG:
      this->resolve_msg_action(act);
      break;
    case TYPE_NOTICE:
      this->resolve_notice_action(act);
      break;
    default:
      this->resolve_cmd_action(act);
      break;
  }
}

void IrcThread::resolve_chat_action(Action act)
{
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", (string("Got chat action from ") + act.user).c_str());
  if (!this->config.spew)
    return;
  
  string output = this->format_action(act);
  if(act.destination == DEST_IRC)
    this->irc_client.say(output, config.spew_channel);
  else
    mineserver->chat.sendmsg(output.c_str());
}

void IrcThread::resolve_cmd_action(Action act)
{
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", (string("Got cmd action from ") + act.user).c_str());
  if (act.destination == DEST_IRC && this->config.spew_commands)
  {
    string output = this->format_action(act);
    this->irc_client.say(output, config.spew_channel);
  }
  else
    this->execute_minecraft_cmd(act);
}

void IrcThread::resolve_msg_action(Action act)
{
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", (string("Got msg action from ") + act.user).c_str());
  string output = this->format_action(act);
  if(act.destination == DEST_IRC)
    this->irc_client.say(output, config.admin_channel);
  else
    mineserver->chat.sendmsg(output.c_str());
}

void IrcThread::resolve_notice_action(Action act)
{
  mineserver->logger.log(LOG_INFO, "plugins.ircbridge", (string("Got notice action from ") + act.user).c_str());
  string output = this->format_action(act);
  if(act.destination == DEST_IRC)
    this->irc_client.say(output, config.admin_channel);
  else
    mineserver->chat.sendmsg(output.c_str());
}

bool IrcThread::action_allowed(Action act)
{
  switch (act.type)
  {
    case TYPE_CHAT:
      if (config.chat_auth_level >= act.permission)
        return true;
      else
        return false;
    case TYPE_CMD:
      if (config.cmd_auth_level >= act.permission)
        return true;
      else
        return false;
    case TYPE_MSG:
      if (config.msg_auth_level >= act.permission)
        return true;
      else
        return false;
    case TYPE_NOTICE:
      if (config.notice_auth_level >= act.permission)
        return true;
      else
        return false;
  }
}

string IrcThread::format_action(Action act)
{
  stringstream out;
  if (act.destination == DEST_IRC)
  {
    if (act.type == TYPE_CHAT || act.type == TYPE_MSG || act.type == TYPE_CMD)
      out << "[" << act.user << "]:";
    else
      out << "*" << act.user << "*:";
      for (vector<string>::iterator it = act.params.begin(); it != act.params.end(); it++)
        out << " " << *it;
  }
  else
  {
    if (act.type == TYPE_CHAT || act.type == TYPE_MSG)
      out << "[" << act.user << "]:";
    else if(act.type == TYPE_CMD)
      // Won't be used, pass
      0;
    else if(act.type == TYPE_NOTICE)
      out << "NOTICE:";
    
    for (vector<string>::iterator it = act.params.begin(); it != act.params.end(); it++)
      out << " " << *it;
  }
  return out.str();
}

void IrcThread::execute_minecraft_cmd(Action act)
{
  //not implemented
  return;
}

void WrappedIRC::say(string words, string channel)
{
  this->privmsg((char*)channel.c_str(), (char*)words.c_str());
}

int connected_lock;
int end_of_motd(char* params, irc_reply_data* hostd, void* conn)
{
  connected_lock = 0;
  return 0;
}

void *thread_init(void* _)
{
  string server = ircthread->config.irc_server;
  int port = ircthread->config.irc_port;
  string username = ircthread->config.irc_username;
  string nickname = ircthread->config.irc_nickname;
  string password = ircthread->config.irc_password;
  
  ostringstream message;
  message << "Connecting to " << server.c_str() << " as " << username << " - " << password;
  mineserver->logger.log(LOG_INFO, "plugin.ircbridge", (char*)message.str().c_str());
  int out = 0;
  
  ircthread->irc_client.hook_irc_command("PRIVMSG", &irc_message_handler);
  ircthread->irc_client.hook_irc_command("376", &end_of_motd);
  out += ircthread->irc_client.start((char*)server.c_str(), port, (char*)nickname.c_str(), (char*)username.c_str(), (char*)nickname.c_str(), (char*)password.c_str());
  
  if(out != 0)
  {
    mineserver->logger.log(LOG_INFO, "plugging.ircbridge", "Could not connect to IRC server");
    connected_lock = 1;
    return (void*)-1;
  }
  
  connected_lock = 0;
  ircthread->irc_client.message_loop();
  return (void*)0;
}

PLUGIN_API_EXPORT void CALLCONVERSION ircbridge_init(mineserver_pointer_struct* mineserver_temp)
{
  mineserver = mineserver_temp;
  ircthread = new IrcThread();

  if (mineserver->plugin.getPluginVersion(pluginName.c_str()) > 0)
  {
    string msg = "ircbridge is already loaded v."+dtos(mineserver->plugin.getPluginVersion(pluginName.c_str()));
    mineserver->logger.log(LOG_INFO, "plugin.ircbridge", msg.c_str());
    return;
  }
  
  if(!ircthread->config.load_config(mineserver))
  {
    mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "IrcChat has found a bad config, Exiting");
    return;
  }
  
  string msg = "Loaded "+pluginName+"!";
  mineserver->logger.log(LOG_INFO, "plugin.ircbridge", msg.c_str());

  
  connected_lock = -1;
  
  pthread_create(&(ircthread->irc_thread), NULL, &thread_init, NULL);
  
  while(connected_lock == -1)
    sleep(0.1);
  
  bool joined_channels = ircthread->join_channels();
  
  if (connected_lock != 0 && !joined_channels)
  {
    mineserver->logger.log(LOG_WARN, "plugin.ircbridge", "IrcBridge Exiting");
    return;
  }
  else
  {
    mineserver->logger.log(LOG_WARN, "plugin.ircbridge", "IrcBridge connected to network.");
  }
  
  mineserver->plugin.setPluginVersion(pluginName.c_str(), PLUGIN_IRCBRIDGE_VERSION);
  
  mineserver->plugin.addCallback("PlayerChatPre", (void *)minecraft_message_handler);
  mineserver->plugin.addCallback("PlayerChatCommand", (void *)minecraft_command_handler);
  
}

PLUGIN_API_EXPORT void CALLCONVERSION ircbridge_shutdown(void)
{
  if (mineserver->plugin.getPluginVersion(pluginName.c_str()) <= 0)
  {
    mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "ircbridge is not loaded!");
    return;
  }
  else
  {
    ircthread->quit();
  }
}