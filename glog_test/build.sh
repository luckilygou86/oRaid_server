#!/bin/bash

g++ -g -o test test.cpp -I./ -L./ -lglog -lgflags -lpthread
