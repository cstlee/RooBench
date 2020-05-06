#! /bin/bash

PROJECT_DIR=$1
REMOTE=$2

CRSYNC_FILE=$PROJECT_DIR/.crsync

sed -i '.bak' -e "s/DEST=.*/DEST=$REMOTE/g" $CRSYNC_FILE

source $CRSYNC_FILE

pushd $PROJECT_DIR
crsync
popd

ssh $REMOTE bash << EOF
cd $DEST_DIR
mkdir build
cd build

mkdir Debug
pushd Debug
cmake ../../ -GNinja -DCMAKE_BUILD_TYPE=Debug
popd

mkdir Release
pushd Release
cmake ../../ -GNinja -DCMAKE_BUILD_TYPE=Release
popd

EOF

