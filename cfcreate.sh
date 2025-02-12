#!/bin/sh
# Copyright (c) YAPP contributors
# SPDX-License-Identifier: MIT
# $Id: cfcreate,v 1.1 1996/08/10 17:21:45 thaler Exp $
# Locate yapp.conf
ok=0
for etcdir in /etc /usr/local/etc /usr/bbs ~cfadm
do
   if [ -f $etcdir/yapp.conf ]; then
      echo "Found yapp.conf in $etcdir"
      ok=1
      break
   fi
done
if [ $ok -ne 1 ]; then
   echo "Couldn't find yapp.conf"
   exit 1
fi

# Locate bbsdir
bbsdir=`grep "^bbsdir:" $etcdir/yapp.conf`
if [ "$bbsdir" = "" ]; then
   bbsdir="/usr/bbs"
fi

#
# Get conference name with (name) & without (plain) underlines
#
echo -n "Short name (including underlines): "
read name
plain=`echo $name | sed s/_//`

#
# Get longer description (still 1 line though)
#
echo "Enter one-line description"
echo -n "> "
read desc

#
# Get directory
#
echo -n "Subdirectory [$plain]: "
read subdir
if [ "$subdir" = "" ]; then
   subdir=$plain
fi
dir=$bbsdir/confs/$subdir

#
# Get list of fairwitnesses
#
echo -n "Fairwitnesses: "
read fws

#
# Get conference type, and email address
#
cat << EOM
You must now select a conference type.  If you require PicoSpan compatibility,
you must enter one of: 0, 4, 5, 6, or 8.  Otherwise, you may use one or more
of the following (in numeric or text form), separated by spaces or commas:

   0    public         : Anyone may participate
   4    ulist          : Access restricted to user list (in 'ulist' file)
   5    password       : Access requires password (in 'secret' file)
                         (currently only supported in text mode, not via WWW)
   6    password ulist : Use both a user list and a password together
   8    protected      : Anyone may participate, but the item files are
                         not world readable.
  16    readonly       : Anyone may observe, even if can't participate
                         (conference becomes readable in WWW without 
                         authentication)
  64    mail           : Mailing list conference
 128    registered     : Posts only accepted from registered users
                         (only applies if 'mail' flag is also present)
 256    noenter        : Only hosts can enter new items

One of the first five MUST be selected.  The latter four may optionally be
added.
EOM
echo -n "Security type: "
read type
mailaddr=""
echo -n "Email address (only used for mail type conferences): "
read mailaddr

#
# Create the conference
#
echo "$name:$dir" >> $bbsdir/conflist
echo "$plain:$desc" >> $bbsdir/desclist
if [ "$mailaddr" != "" ]; then
   echo "$mailaddr:$plain" >> $bbsdir/maillist
fi
mkdir $dir
echo "!<pc02>" > $dir/config
echo ".$subdir.cf" >> $dir/config
echo "0" >> $dir/config
echo "$fws" >> $dir/config
echo "$type" >> $dir/config
echo "$mailaddr" >> $dir/config
echo "Welcome to the $plain conference.  This file may be edited by a fairwitness." > $dir/login
echo "You are now leaving the $plain conference.  This file may be edited by a fairwitness." > $dir/logout
chmod 644 $dir/*
