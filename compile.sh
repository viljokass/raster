#!/usr/bin/bash

g++ `pkg-config --cflags gtk+-3.0` -o manninen main.cpp `pkg-config --libs gtk+-3.0` -O3
