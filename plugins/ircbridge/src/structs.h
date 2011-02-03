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

enum Destination
{
  DEST_MINECRAFT,
  DEST_IRC
};

enum MessageType
{
  TYPE_NOTICE,
  TYPE_MSG,
  //chat is straight chat, ignored unless spew = true;
  TYPE_CHAT,
  TYPE_CMD,
};

enum Permission
{
  PERM_USER,
  PERM_ADMIN,
};

struct Message
{
  std::string user;
  std::string message;
  Destination destination;
  bool isadmin;
  MessageType type;
};

struct Action
{
  MessageType type;
  std::vector<std::string> params;
  Destination destination;
  std::string user;
  Permission permission;
};

struct ReturnState
{
  int return_code;
  std::string return_msg;
};