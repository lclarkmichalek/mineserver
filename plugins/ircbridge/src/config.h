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
#include <map>
#include <vector>
#include "../../../src/plugin_api.h"

class Config
{
public:
  bool good_config;
  
  bool spew;
  bool spew_commands;
  std::string spew_channel;
  std::string admin_channel;
  std::string admin_channel_passwd;
  
  std::string irc_server;
  int irc_port;
  std::string irc_username;
  std::string irc_nickname;
  std::string irc_realname;
  std::string irc_password;
  
  //names of admins on irc
  std::vector<std::string> admins_irc;
  //names of admins on minecraft
  std::vector<std::string> admins_minecraft;
  
  char m_command_prefix;
  char i_command_prefix;
  
  //commands from minecraft -> irc
  std::string m_notice_command;
  std::string m_talk_command;
  std::map<std::string, std::string> m_command_aliases;
  
  //commands from irc -> minecraft
  std::string i_notice_command;
  std::string i_talk_command;
  //not supported currently
  std::string i_execute_command;
  std::map<std::string, std::string> i_command_aliases;
  
  int chat_auth_level;
  int cmd_auth_level;
  int notice_auth_level;
  int msg_auth_level;
  
  bool load_config(mineserver_pointer_struct* mineserver);
  Config();

private:
  std::vector<std::string> load_vector_config(std::string var_name);
  std::map<std::string, std::string> load_map_config(std::string var_name);
  
  mineserver_pointer_struct* mineserver;
};