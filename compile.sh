#!/bin/bash

gcc -Wall -g -O0 -D_FILE_OFFSET_BITS=64 main.c parse_data.c linked_list.c -lexpat -pthread
#gcc -g -O0 main.c parse_data.c linked_list.c `xml2-config --cflags` `xml2-config --libs`
