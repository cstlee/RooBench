#! /bin/bash

DIR=$1
SETUP=$2
REMOTE=$3

function generate-test-config {
    local REMOTE=$1
    local DIR=$2
    echo "#! /bin/bash" > "$DIR/roobench.config"
    echo "" >> "$DIR/roobench.config"

    SERVERS=$(ssh $REMOTE "/local/repository/scripts/get-rc-public-hostnames.sh /shome/rc-hosts.txt" | grep -v rcnfs | awk '{print $2}' | tr '\n' ' ')
    echo "HOSTS=($SERVERS)" >> "$DIR/roobench.config"
    echo "BIN_DIR=\"/shome/RooBench/build/Release\"" >> "$DIR/roobench.config"
    echo "SRC_DIR=\"/shome/RooBench\"" >> "$DIR/roobench.config"
}

scp ~/.tmux.conf $REMOTE:~/

generate-test-config $REMOTE $DIR

# PerfUtils
$SETUP $DIR/PerfUtils $REMOTE
source $DIR/PerfUtils/.crsync
ssh $REMOTE bash <<EOF
cd $DEST_DIR/build/Release
ninja
sudo ninja install
EOF

# Homa
$SETUP $DIR/Homa $REMOTE
source $DIR/Homa/.crsync
ssh $REMOTE bash <<EOF
cd $DEST_DIR/build/Release
ninja
sudo ninja install
EOF

# Roo
$SETUP $DIR/Roo $REMOTE
source $DIR/Roo/.crsync
ssh $REMOTE bash <<EOF
cd $DEST_DIR/build/Release
ninja
sudo ninja install
EOF

# RooBench
$SETUP $DIR/RooBench $REMOTE
source $DIR/RooBench/.crsync
ssh $REMOTE bash <<EOF
cd $DEST_DIR/build/Release
ninja
EOF

