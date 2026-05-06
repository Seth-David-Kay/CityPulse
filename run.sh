#!/bin/bash
#Compile and run for osX machines

#Usage: run ./run.sh with the first cli as the c file you want to run.
#	If no file is given, main.c will run. Then input cli's as normal.
# if serve as arg then also run python serve.py and open localhost:8000/capstone.html
rm ./build.bin

./build.sh web "$@"
if [[ "$*" == *"serve"* ]]; then
    python serve.py &
    open http://localhost:8001/capstone.html
fi
