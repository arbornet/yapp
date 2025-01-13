#!/bin/sh
# $Id: cfdelete,v 1.2 1996/09/07 03:23:42 thaler Exp $
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
bbsdir=`grep "^bbsdir:" $etcdir/yapp.conf | cut -d: -f2`
if [ "$bbsdir" = "" ]; then
   bbsdir="/usr/bbs"
fi

# Locate partdir
partdir=`grep "^partdir:" $etcdir/yapp.conf | cut -d: -f2`
if [ "$partdir" = "" ]; then
   partdir="work"
fi

echo -n "Conference name (with underlines) to delete: "
read name
plain=`echo $name | sed s/_//`
confline=`grep "^$name:" $bbsdir/conflist`
if [ "$confline" = "" ]; then
   echo "Conference not found."
   exit
fi
IFS=:
set $confline
IFS=
dir=$2
echo "Directory=$dir"

#
# Verify that it's empty
#
files=`ls $dir/_* 2> /dev/null`
if [ "$files" != "" ]; then
   echo "You must first kill all items in the conference."
   exit
fi

# Get the participation filename from the config file
partname=`tail +2 $dir/config | head -1`

#
# If partdir != "work", reap .cf files
#
if [ "$partdir" != "work" ]; then
   for login in `cat $dir/ulist`
   do
      c1=`echo $login | cut -c1`
      c2=`echo $login | cut -c2`
      echo "Deleting $partdir/$c1/$c2/$login/$partname..."
      /bin/rm -f $partdir/$c1/$c2/$login/$partname
   done
fi

# Delete the conference directory
echo "Deleting $dir..."
/bin/rm -rf $dir

#
# Remove line from conflist
#
echo "Removing entries from $bbsdir/conflist..."
grep -v ":$dir"'$' $bbsdir/conflist > $bbsdir/conflist.tmp
mv $bbsdir/conflist $bbsdir/conflist.bak
mv $bbsdir/conflist.tmp $bbsdir/conflist

#
# Remove line from desclist
#
echo "Removing entry from $bbsdir/desclist..."
grep -v "^$plain:" $bbsdir/desclist > $bbsdir/desclist.tmp
mv $bbsdir/desclist $bbsdir/desclist.bak
mv $bbsdir/desclist.tmp $bbsdir/desclist

#
# Remove line from maillist
#
echo "Removing entries from $bbsdir/maillist..."
grep -v ":$plain"'$' $bbsdir/maillist > $bbsdir/maillist.tmp
mv $bbsdir/maillist $bbsdir/maillist.bak
mv $bbsdir/maillist.tmp $bbsdir/maillist
