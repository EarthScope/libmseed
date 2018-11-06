#!/bin/bash

# Documentation build directory, relative to script directory
DOCDIR=gh-pages

# Extract version from header
if [[ $(grep LIBMSEED_VERSION ../libmseed.h) =~ LIBMSEED_VERSION[[:space:]]+\"(.*)\"[[:space:]]* ]]; then
    PROJECT_VERSION=${BASH_REMATCH[1]}
else
    echo "WARNING: Cannot extract version"
fi

# Documentation build command, injecting current project version
DOCBUILD="(cat Doxyfile; echo 'PROJECT_NUMBER=${PROJECT_VERSION}') | doxygen -"

# Check for command line options
while getopts ":YN" o; do
    case "${o}" in
        Y)
            PUSHYES=Y
            ;;
        N)
            PUSHYES=N
            ;;
        *)
            echo "Usage: $0 [-Y] [-N]" 1>&2;
            exit 1;
            ;;
    esac
done
shift $((OPTIND-1))

# Change to working directory, aka base directory of script
BASEDIR=$(dirname $0)
cd $BASEDIR || { echo "Cannot change-directory to $BASEDIR, exiting"; exit 1; }

# Make clean gh-pages directory
rm -rf $DOCDIR
mkdir $DOCDIR || { echo "Cannot create directory '$DOCDIR', exiting"; exit 1; }

# Build docs
eval $DOCBUILD || { echo "Error running '$DOCBUILD', exiting"; exit 1; }

GITREMOTE=$(git config --get remote.origin.url)

# Exit if already instructed to not push
if [[ "$PUSHYES" == "N" ]]; then
    echo "Exiting without pushing to remote"
    exit 0
fi

# Unless pushing already acknowledged ask if docs should be pushed
if [[ "$PUSHYES" != "Y" ]]; then
    read -p "Push documentation to gh-pages branch of $GITREMOTE (y/N)? " PUSHYES

    if [[ "$PUSHYES" != "Y" && "$PUSHYES" != "y" ]]; then
        exit 0
    fi
fi

cd $DOCDIR || { echo "Cannot change-directory to $DOCDIR, exiting"; exit 1; }

# Init repo, add & commit files, add remote origin
git init
touch .nojekyll
git add .
git commit -m 'committing documentation'

git remote add origin $GITREMOTE

echo "Pushing contents of $BASEDIR/$DOCDIR to $GITREMOTE branch gh-pages"
git push --quiet --force origin master:gh-pages
