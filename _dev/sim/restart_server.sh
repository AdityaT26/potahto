#!/bin/bash
# (Re)starts the simulator's HTTP front on http://127.0.0.1:8080
cd "$(dirname "$0")"
[ -f server.pid ] && kill "$(cat server.pid)" 2>/dev/null
pkill -x potahto_sim 2>/dev/null
sleep 0.5
nohup python3 sim_server.py > server.log 2>&1 &
echo $! > server.pid
sleep 1
