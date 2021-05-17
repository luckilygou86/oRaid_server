#!/bin/sh

 g++ -g -o oraid_server src/*.cpp -I./include -L./lib -L ./lib/cjson/ -L ./lib/restclient/ -lrestclient-cpp -lglog -lgflags -levent -lcjson -lcurl -lpthread -std=c++11
