IrcChat Plugin
==============

Building
--------

To build:

	cd src
	g++ -c IRC.cc
	g++ -c ircchat.cpp -lpthread
	g++ -shared ircchat.o IRC.o -o ircchat.so
	cp ircchat.so ../../../bin
	rm *.{o,so}

Config
------

Takes following config options in bin/config.cfg:

	ircchat.server = ""
	ircchat.username = ""
	ircchat.password = ""
	ircchat.nickname = ""
	ircchat.channel = ""

Functionality
------------

Currently just repeats everything said in minecraft chat. Will add IRC to minecraft chat functionality sometime in the future. The aim is to allow an admin to respond to players' needs from IRC.
