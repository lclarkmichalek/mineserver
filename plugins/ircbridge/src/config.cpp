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
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <string.h>

#include "config.h"
#include "structs.h"

#include "../../../src/plugin_api.h"

#define LOG_WARN 4
#define LOG_INFO 6
using namespace std;

bool Config::load_config(mineserver_pointer_struct* mineserver)
{
    mineserver = mineserver;

    if (mineserver->config.has("ircbridge.spew"))
        spew = mineserver->config.bData("ircbridge.spew");

    if (mineserver->config.has("ircbridge.spew_commands"))
        spew_commands = mineserver->config.bData("ircbridge.spew_commands");

    if (mineserver->config.has("ircbridge.admin_channel"))
        admin_channel = mineserver->config.sData("ircbridge.admin_channel");
    else
    {
        mineserver->logger.log(LOG_WARN, "plugin.ircbridge", "No value for admin channel, quitting");
        return false;
    }
    if (mineserver->config.has("ircbridge.admin_channel_password"))
        admin_channel_passwd = mineserver->config.sData("admin_channel_password.admin_channel");

    if (mineserver->config.has("ircbridge.spew_channel"))
        spew_channel = mineserver->config.sData("ircbridge.spew");
    else
    {
        mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "No value for spew channel, using admin channel instead");
        spew_channel = admin_channel;
    }

    if (mineserver->config.has("ircbridge.irc_server"))
        irc_server = mineserver->config.sData("ircbridge.irc_server");
    else
    {
        mineserver->logger.log(LOG_WARN, "plugin.ircbridge", "No value for irc channel, quitting");
        return false;
    }

    if (mineserver->config.has("ircbridge.irc_port"))
        irc_port = mineserver->config.iData("ircbridge.irc_port");

    if (mineserver->config.has("ircbridge.irc_username"))
        irc_username = mineserver->config.sData("ircbridge.irc_username");
    else
    {
        mineserver->logger.log(LOG_WARN, "plugin.ircbridge", "No value for irc username, quitting");
        return false;
    }

    if (mineserver->config.has("ircbridge.irc_nickname"))
        irc_nickname = mineserver->config.sData("ircbridge.irc_nickname");
    else
    {
        mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "No value for irc nickname, using irc username instead");
        irc_nickname = irc_username;
    }

    if (mineserver->config.has("ircbridge.irc_password"))
        irc_password = mineserver->config.sData("ircbridge.irc_password");
    else
        mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "No value for password, assuming no password");

    if (mineserver->config.has("ircbridge.irc_admins"))
        admins_irc = load_vector_config("ircbridge.irc_admins");
    else
        mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "No irc admins defined, assuming all are admins");

    if (mineserver->config.has("ircbridge.minecraft_admins"))
        admins_minecraft = load_vector_config("ircbridge.minecraft_admins");
    else
        mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "No minecraft admins defined, assuming all are admins");

    if (mineserver->config.has("ircbridge.irc_notice_command"))
        i_notice_command = mineserver->config.sData("ircbridge.irc_notice_command");

    if (mineserver->config.has("ircbridge.irc_talk_command"))
        i_talk_command = mineserver->config.sData("ircbridge.irc_talk_command");

    if (mineserver->config.has("ircbridge.irc_execute_command"))
        i_talk_command = mineserver->config.sData("ircbridge.irc_execute_command");

    if (mineserver->config.has("ircbridge.irc_command_aliases"))
        i_command_aliases = load_map_config("ircbridge.irc_command_aliases");

    if (mineserver->config.has("ircbridge.minecraft_notice_command"))
        m_notice_command = mineserver->config.sData("ircbridge.minecraft_notice_command");

    if (mineserver->config.has("ircbridge.minecraft_talk_command"))
        m_talk_command = mineserver->config.sData("ircbridge.minecraft_talk_command");

    if (mineserver->config.has("ircbridge.minecraft_command_aliases"))
        m_command_aliases = load_map_config("ircbridge.minecraft_command_aliases");

    if (mineserver->config.has("ircbridge.irc_command_prefix"))
    {
        string pref = mineserver->config.sData("ircbridge.irc_command_prefix");
        if (pref.length() != 1)
          mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "Irc command prefix too long, using default");
        else
          i_command_prefix = pref[0];
    }
    
    if (mineserver->config.has("ircbridge.minecraft_command_prefix"))
    {
        string pref = mineserver->config.sData("ircbridge.minecraft_command_prefix");
        if (pref.length() != 1)
          mineserver->logger.log(LOG_INFO, "plugin.ircbridge", "Minecraft command prefix too long, using default");
        else
          m_command_prefix = pref[0];
    }
    
    if (mineserver->config.has("ircbridge.chat_auth_level"))
        chat_auth_level = mineserver->config.iData("ircbridge.chat_auth_level");
    if (mineserver->config.has("ircbridge.msg_auth_level"))
        msg_auth_level = mineserver->config.iData("ircbridge.msg_auth_level");
    if (mineserver->config.has("ircbridge.notice_auth_level"))
        notice_auth_level = mineserver->config.iData("ircbridge.notice_auth_level");
    if (mineserver->config.has("ircbridge.cmd_auth_level"))
        cmd_auth_level = mineserver->config.iData("ircbridge.cmd_auth_level");
    
    return true;
}

Config::Config()
{
    good_config = false;

    spew = false;
    spew_commands = false;

    admin_channel_passwd = "";

    irc_port = 6667;
    irc_password = "";

    m_notice_command = "admin";
    m_talk_command = "irc";

    i_command_prefix = '.';
    m_command_prefix = '/';

    i_notice_command = "minecraft_notice";
    i_talk_command = "minecraft_talk";
    i_execute_command = "minecraft_execute";
    i_command_aliases["mn"] = "minecraft_notice";
    i_command_aliases["mt"] = "minecraft_talk";
    i_command_aliases["me"] = "minecraft_execute";
    
    chat_auth_level = PERM_USER;
    msg_auth_level = PERM_USER;
    cmd_auth_level = PERM_ADMIN;
    notice_auth_level = PERM_ADMIN;
}

std::vector<std::string> Config::load_vector_config(std::string var_name)
{
    std::string var = mineserver->config.sData(var_name.c_str());
    std::vector<std::string> tokens;
    std::istringstream iss(var);
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              std::back_inserter<std::vector<std::string> >(tokens));
    return tokens;
}

std::map<std::string, std::string> Config::load_map_config(std::string var_name)
{
    std::map<std::string, std::string> out;
    std::vector<std::string> tokens = load_vector_config(var_name);

    for (std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end(); it++)
    {
        char* pch;
        char* pch2;
        pch = strtok((char*)it->c_str(), ":");
        pch2 = strtok((char*)NULL, ":");

        out[pch] = pch2;
    }
    return out;
}
