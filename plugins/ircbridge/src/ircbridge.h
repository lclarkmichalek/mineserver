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
#include <deque>
#include <map>
#include <vector>
#include <pthread.h>

#include "irc.h"
#include "config.h"
#include "structs.h"

#ifndef IRCBRIDGE_H
#define IRCBRIDGE_H

class WrappedIRC : public IRC
{
public:
  void say(std::string words, std::string channel);
};

class IrcThread
{
public:
  pthread_t irc_thread;
  WrappedIRC irc_client;
  Config config;
  
  void say_i(Message mes);
  void say_m(Message mes);
  void spew_i(Message mes);
  void spew_m(Message mes);
  void quit();
  bool join_channels();
  void handle_action(Action act);
  Action message_to_action(Message mes);
  bool action_allowed(Action act);
  
private:
  std::string format_action(Action act);
  
  void resolve_chat_action(Action act);
  void resolve_cmd_action(Action act);
  void resolve_msg_action(Action act);
  void resolve_notice_action(Action act);
  
  void execute_minecraft_cmd(Action act);
};

#endif