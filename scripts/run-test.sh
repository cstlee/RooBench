#! /bin/bash

if [[ $# -ne 4 ]]; then
    echo "Usage: run-test.sh <CONFIG> <SERVER_CONFIG> <BENCH_CONFIG> <LOG_DIR>"
    exit 0
fi

CONFIG=$1
SERVER_CONFIG=$2
BENCH_CONFIG=$3
LOG_DIR=$4

# CONFIG should define the following variables
#
# HOSTS - location of the file containing a list of remote machines to use
# BIN_DIR - full path to directory containing the test binaries
# SRC_DIR - full path to director containg the RooBench source and scripts
source $CONFIG

REMOTE_CONFIG_DIR="/shome/RooConfig"

function remote_setup {
    local REMOTE=$1
    local LOG_DIR_NAME=$2
    local SERVER_CONFIG=$3
    local BENCH_CONFIG=$4
    # Create directories
    ssh $REMOTE "
    mkdir -p ~/RooConfig
    mkdir -p ~/logs/$LOG_DIR_NAME
    ln -sFfn ~/logs/$LOG_DIR_NAME ~/logs/latest
    "
}

DATE_TIME=$(date +"%F-%H-%M-%S")

# Setup local log directories
mkdir -p $LOG_DIR
LOG_DIR=$(cd $LOG_DIR; pwd)
mkdir -p "$LOG_DIR/$DATE_TIME"
# ln -sFfh "$LOG_DIR/$DATE_TIME" "$LOG_DIR/latest"

# Save configs
cp $SERVER_CONFIG $LOG_DIR/$DATE_TIME/ServerConfig.json
cp $BENCH_CONFIG $LOG_DIR/$DATE_TIME/BenchConfig.json

# Upload configs
# REMOTE=${HOSTS[0]}
# ssh $REMOTE "mkdir -p $REMOTE_CONFIG_DIR"
# scp $SERVER_CONFIG $REMOTE:$REMOTE_CONFIG_DIR/ServerConfig.json
# scp $BENCH_CONFIG $REMOTE:$REMOTE_CONFIG_DIR/BenchConfig.json

# Remote setup
SERVER_ID=1
for HOST in ${HOSTS[@]}
do
    echo "Setup Server $SERVER_ID on $HOST"
    remote_setup $HOST $DATE_TIME $SERVER_CONFIG $BENCH_CONFIG
    let "SERVER_ID++"
done

##### Start Servers
SERVER_ID=1
for HOST in ${HOSTS[@]}
do
    echo "Start Server $SERVER_ID on $HOST"
    SERVER_NAME="server-$SERVER_ID"
    CMD="sudo nohup $SRC_DIR/scripts/roobench.py server launch $SERVER_NAME $REMOTE_CONFIG_DIR/ServerConfig.json"
    ssh $HOST $CMD
    let "SERVER_ID++"
done

sleep 0.5

##### Run Client
HOST=${HOSTS[0]}
echo "Start Client on $HOST"
CMD="sudo nohup $SRC_DIR/scripts/roobench.py server start $REMOTE_CONFIG_DIR/ServerConfig.json"
ssh $HOST $CMD

sleep 1

##### Dump stats
SERVER_ID=1
for HOST in ${HOSTS[@]}
do
    echo "Dump Server $SERVER_ID stats on $HOST"
    CMD="sudo nohup $SRC_DIR/scripts/roobench.py server stats $REMOTE_CONFIG_DIR/ServerConfig.json"
    ssh $HOST $CMD
    let "SERVER_ID++"
done

sleep 30

##### Dump stats
SERVER_ID=1
for HOST in ${HOSTS[@]}
do
    echo "Dump Server $SERVER_ID stats on $HOST"
    CMD="sudo nohup $SRC_DIR/scripts/roobench.py server stats $REMOTE_CONFIG_DIR/ServerConfig.json"
    ssh $HOST $CMD
    let "SERVER_ID++"
done

sleep 1

##### Stop Servers
SERVER_ID=1
for HOST in ${HOSTS[@]}
do
    echo "Stop Server $SERVER_ID on $HOST"
    ssh $HOST "
        sudo $SRC_DIR/scripts/roobench.py server stop $REMOTE_CONFIG_DIR/ServerConfig.json
    "
    let "SERVER_ID++"
done

##### Collect Logs
SERVER_ID=1
for HOST in ${HOSTS[@]}
do
    echo "Copying Server $SERVER_ID logs from $HOST"
    scp "$HOST:~/logs/$DATE_TIME/*" "$LOG_DIR/$DATE_TIME"
    let "SERVER_ID++"
done


