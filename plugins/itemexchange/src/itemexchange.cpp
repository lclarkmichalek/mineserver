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

#define MINESERVER_C_API

#include "../../../src/plugin_api.h"

#include "itemexchange.h"
#define LOG_INFO 6

#define PLUGIN_ITEMEXCHANGE_VERSION 0.1
const char CHATCMDPREFIX   = '/';
mineserver_pointer_struct* mineserver;
ItemExchange itemexchange;

std::string pluginName = "ItemExchange";

std::string dtos(double n)
{
  std::ostringstream result;
  result << n;
  return result.str();
}

std::string itoa(int n)
{
  std::ostringstream result;
  result << n;
  return result.str();
}

ItemExchange::ItemExchange()
{
}

ReturnState ItemExchange::handle_trade_request(const char* userIn, const char* cmdIn, int argc, char** argv)
{
  ReturnState ret;
  Transaction tran;

  tran.in_n = atoi(argv[0]);
  tran.in_i = std::string(argv[1]);
  tran.in_p = std::string(userIn);

  tran.out_n = atoi(argv[4]);
  tran.out_i = std::string(argv[4]);
  tran.out_p = std::string(argv[6]);
  
  if (tran.out_n + tran.in_n < 1)
  {
    ret.return_code = 1;
    ret.return_msg = std::string("Amount must be at least one");
    return ret;
  } else {
    this->handle_init_transaction(tran);
    
    ret.return_code = 0;
    return ret;
  }
}

ReturnState ItemExchange::handle_trade_response(const char* userIn, const char* cmdIn, int argc, char** argv)
{
  //format "accept_trade player"
  std::string user = std::string(argv[0]);
  std::vector<Transaction>* trans = this->user_trans_map[userIn];
  Transaction tran;
  
  for(std::vector<Transaction>::iterator it = trans->begin(); it != trans->end(); it++)
  {
    if(it->in_p == user && !it->completed && !it->aborted)
    {
      tran = *it;
      break;
    }
  }
  if (tran.tid == -1)
  {
    ReturnState ret;
    ret.return_code = 1;
    ret.return_msg = std::string("No transactions found for that user");
    return ret;
  }
  
  ReturnState out = this->execute_transaction(tran);
  if (out.return_code > 0)
    return out;
  else
  {
    ReturnState ret;
    ret.return_code = 0;
    ret.return_msg = std::string("Transaction carried out successfully");
    return ret;
  }
}

void command_handler(const char* userIn, const char* cmdIn, int argc, char** argv)
{
  if (argc < 1)
    return;
  //format is "Trade n items for n items to player
  if (cmdIn == "trade" && argc == 7)
  {
    if (argv[2] != "for" && argv[5] != "to")
      //yup we're going to be pedantic
      return;
    itemexchange.handle_trade_request(userIn, cmdIn, argc, argv);
  }
}

PLUGIN_API_EXPORT void CALLCONVERSION command_init(mineserver_pointer_struct* mineserver_temp)
{
  mineserver = mineserver_temp;
  itemexchange = ItemExchange();

  if (mineserver->plugin.getPluginVersion(pluginName.c_str()) > 0)
  {
    std::string msg = "itemexchange is already loaded v."+dtos(mineserver->plugin.getPluginVersion(pluginName.c_str()));
    mineserver->logger.log(LOG_INFO, "plugin.itemexchange", msg.c_str());
    return;
  }
  std::string msg = "Loaded "+pluginName+"!";
  mineserver->logger.log(LOG_INFO, "plugin.itemexchange", msg.c_str());

  mineserver->plugin.setPluginVersion(pluginName.c_str(), PLUGIN_ITEMEXCHANGE_VERSION);

  mineserver->plugin.addCallback("PlayerChatCommand", (void *)command_handler);
}

ReturnState ItemExchange::handle_init_transaction(Transaction trans)
{
  if(this->user_trans_map.count(trans.in_p) == 0)
    this->user_trans_map[trans.in_p] = new std::vector<Transaction>();
  this->user_trans_map[trans.in_p]->push_back(trans);
  
  if(this->user_trans_map.count(trans.out_p) == 0)
    this->user_trans_map[trans.out_p] = new std::vector<Transaction>();
  this->user_trans_map[trans.out_p]->push_back(trans);
  
  this->trans_queue.push_back(trans);
  
  ReturnState out = this->send_user_message(trans);
  
  if (out.return_code > 0)
    return out;
  else
  {
    ReturnState ret;
    ret.return_code = 0;
    return ret;
  }
}

ReturnState ItemExchange::send_user_message(Transaction trans)
{
  std::ostringstream out;
  out << trans.in_p << " requests " << itoa(trans.out_n) << " " << trans.out_i << " for " <<
    itoa(trans.in_n) << " " << trans.in_i << ". Type /accept_trade " << itoa(trans.tid) << " to accept the trade";
  
  ReturnState ret;
  if (mineserver->chat.sendmsgTo(trans.out_p.c_str(), out.str().c_str()))
  {
    ret.return_code = 1;
    ret.return_msg = std::string("Failed to send message");
  }
  else
    ret.return_code = 0;
  return ret;
}

ReturnState ItemExchange::execute_transaction(Transaction trans)
{
  // On hold untill addItem/takeItem are implemented
}