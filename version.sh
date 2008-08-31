#!/bin/sh
TAG=`git tag`
MAJOR=`echo $TAG|cut -d. -f1`
MINOR=`echo $TAG|cut -d. -f2`
REVISION=`git rev-list $TAG..HEAD | wc -l`
FILE=spugl-client/daemon.h

sed -e "s/VERSION_MAJOR.*$/VERSION_MAJOR $MAJOR/;s/VERSION_MINOR.*$/VERSION_MINOR $MINOR/;s/VERSION_REVISION.*$/VERSION_REVISION $REVISION/;s/VERSION_STRING.*$/VERSION_STRING \"$MAJOR.$MINOR.$REVISION\"/;" <$FILE >.version
mv .version $FILE

echo $MAJOR.$MINOR.$REVISION >version.txt
