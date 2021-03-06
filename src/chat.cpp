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

#include <ctime>
#include <iostream>
#include <fstream>

#include "constants.h"
#include "user.h"
#include "logger.h"
#include "mineserver.h"
#include "permissions.h"
#include "tools.h"
#include "plugin.h"

#include "chat.h"

Chat::Chat()
{
}

Chat::~Chat()
{
}

bool Chat::sendUserlist(User* user)
{
  this->sendMsg(user, MC_COLOR_BLUE + "[ " + dtos(User::all().size()) + " players online ]", USER);

  for(unsigned int i = 0; i < User::all().size(); i++)
  {
    std::string playerDesc = "> " + User::all()[i]->nick;
    if(User::all()[i]->muted)
    {
        playerDesc += MC_COLOR_YELLOW + " (muted)";
    }
    if(User::all()[i]->dnd)
    {
      playerDesc += MC_COLOR_YELLOW + " (dnd)";
    }

    this->sendMsg(user, playerDesc, USER);
  }

  return true;
}

std::deque<std::string> Chat::parseCmd(std::string cmd)
{
  int del;
  std::deque<std::string> temp;

  while(cmd.length() > 0)
  {
    while(cmd[0] == ' ')
    {
      cmd = cmd.substr(1);
    }

    del = cmd.find(' ');

    if(del > -1)
    {
      temp.push_back(cmd.substr(0, del));
      cmd = cmd.substr(del+1);
    }
    else
    {
      temp.push_back(cmd);
      break;
    }
  }

  if(temp.empty())
  {
    temp.push_back("empty");
  }

  return temp;
}

bool Chat::handleMsg(User* user, std::string msg)
{
  if (msg.empty()) // If the message is empty handle it as if there is no message.
      return true;

  // Timestamp
  time_t rawTime = time(NULL);
  struct tm* Tm  = localtime(&rawTime);
  std::string timeStamp (asctime(Tm));
  timeStamp = timeStamp.substr(11, 5);

  if ((static_cast<Hook3<bool,const char*,time_t,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerChatPre")))->doUntilFalse(user->nick.c_str(), rawTime, msg.c_str()))
  {
    return false;
  }
  (static_cast<Hook3<bool,const char*,time_t,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerChatPost")))->doAll(user->nick.c_str(), rawTime, msg.c_str());
  char prefix = msg[0];

  switch(prefix)
  {
    // Servermsg (Admin-only)
    case SERVERMSGPREFIX:
      if(IS_ADMIN(user->permissions))
      {
        handleServerMsg(user, msg, timeStamp);
      }
      break;

    // Admin message
    case ADMINCHATPREFIX:
      if(IS_ADMIN(user->permissions))
      {
        handleAdminChatMsg(user, msg, timeStamp);
      }
      break;

    case CHATCMDPREFIX:
      handleCommand(user, msg, timeStamp);
      break;
    // Normal chat message
    default:
      handleChatMsg(user, msg, timeStamp);
      break;
  }

  return true;
}

void Chat::handleCommand(User* user, std::string msg, const std::string& timeStamp)
{
  std::deque<std::string> cmd = parseCmd(msg.substr(1));

  if(!cmd.size() || !cmd[0].size())
  {
    return;
  }

  std::string command = cmd[0];
  cmd.pop_front();

  char **param = new char *[cmd.size()];

  for(uint32_t i = 0; i < cmd.size(); i++)
  {
    param[i] = (char *)cmd[i].c_str();
  }

  (static_cast<Hook4<bool,const char*,const char*,int,const char**>*>(Mineserver::get()->plugin()->getHook("PlayerChatCommand")))->doAll(user->nick.c_str(), command.c_str(), cmd.size(), (const char **)param);

  delete [] param;

}


void Chat::handleServerMsg(User* user, std::string msg, const std::string& timeStamp)
{
  // Decorate server message
  Mineserver::get()->logger()->log(LogType::LOG_INFO, "Chat", "[!] " + msg.substr(1));
  msg = MC_COLOR_RED + "[!] " + MC_COLOR_GREEN + msg.substr(1);
  this->sendMsg(user, msg, ALL);
}

void Chat::handleAdminChatMsg(User* user, std::string msg, const std::string& timeStamp)
{
  Mineserver::get()->logger()->log(LogType::LOG_INFO, "Chat", "[@] <"+ user->nick + "> " + msg.substr(1));
  msg = timeStamp +  MC_COLOR_RED + " [@]" + MC_COLOR_WHITE + " <"+ MC_COLOR_DARK_MAGENTA + user->nick + MC_COLOR_WHITE + "> " + msg.substr(1);
  this->sendMsg(user, msg, ADMINS);
}

void Chat::handleChatMsg(User* user, std::string msg, const std::string& timeStamp)
{
  if(user->isAbleToCommunicate("chat") == false)
  {
    return;
  }

  // Check for Admins or Server Console
  if (user->UID == SERVER_CONSOLE_UID)
  {
    Mineserver::get()->logger()->log(LogType::LOG_INFO, "Chat",  user->nick + " " + msg);
    msg = timeStamp + " " + MC_COLOR_RED + user->nick + MC_COLOR_WHITE + " " + msg;
  }
  else if(IS_ADMIN(user->permissions))
  {
    Mineserver::get()->logger()->log(LogType::LOG_INFO, "Chat", "<"+ user->nick + "> " + msg);
    msg = timeStamp + " <"+ MC_COLOR_DARK_MAGENTA + user->nick + MC_COLOR_WHITE + "> " + msg;
  }
  else
  {
    Mineserver::get()->logger()->log(LogType::LOG_INFO, "Chat", "<"+ user->nick + "> " + msg);
    msg = timeStamp + " <"+ user->nick + "> " + msg;
  }

  this->sendMsg(user, msg, ALL);
}

bool Chat::sendMsg(User* user, std::string msg, MessageTarget action)
{
  size_t tmpArrayLen = msg.size()+3;
  uint8_t* tmpArray    = new uint8_t[tmpArrayLen];

  tmpArray[0] = 0x03;
  tmpArray[1] = 0;
  tmpArray[2] = msg.size()&0xff;

  for(unsigned int i = 0; i < msg.size(); i++)
  {
    tmpArray[i+3] = msg[i];
  }

  switch(action)
  {
  case ALL:
    user->sendAll(tmpArray, tmpArrayLen);
    break;

  case USER:
     user->buffer.addToWrite(tmpArray, tmpArrayLen);
    break;

  case ADMINS:
    user->sendAdmins(tmpArray, tmpArrayLen);
    break;

  case OPS:
    user->sendOps(tmpArray, tmpArrayLen);
    break;

  case GUESTS:
    user->sendGuests(tmpArray, tmpArrayLen);
    break;

  case OTHERS:
    user->sendOthers(tmpArray, tmpArrayLen);
    break;
  }

  delete[] tmpArray;

  return true;
}
