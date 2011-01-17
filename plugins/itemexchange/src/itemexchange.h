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
#include <deque>

#ifndef ITEMEXCHANGE_H
#define ITEMEXCHANGE_H

struct Transaction
{
  int tid;
  int out_n;
  std::string out_i;
  std::string out_p;
  int in_n;
  std::string in_i;
  std::string in_p;
  bool completed;
  bool aborted;
};

struct ReturnState
{
  int return_code;
  std::string return_msg;
};

class ItemExchange
{

public:
  ItemExchange();
  
  ReturnState handle_trade_response(const char* userIn, const char* cmdIn, int argc, char** argv);
  ReturnState handle_trade_request(const char* userIn, const char* cmdIn, int argc, char** argv);
  
private:
  
  ReturnState handle_init_transaction(Transaction trans);
  ReturnState send_user_message(Transaction trans);
  ReturnState execute_transaction(Transaction trans);
  
  std::map<std::string, std::vector<Transaction>* > user_trans_map;
  std::deque<Transaction> trans_queue;
};

#endif // ITEMEXCHANGE_H
