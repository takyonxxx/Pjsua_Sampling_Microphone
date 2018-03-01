#!/bin/bash
APPNAME="TIC_KRIPTO"
LOGDIR=/var/log

source /srv/bin/functions.sh

reset_logging

copy_name=$(pwd|xargs basename|cut -d'.' -f2)
if [[ "$copy_name" == "" ]] ; then
    copy_name="yketiccrypto"
else
    link_line=$(grep "$copy_name" /srv/app/link.list|cut -d'#' -f1|tr -d ' ')
    IFS=':'
    set $link_line
    copy_type=$3
    unset IFS
fi

case $copy_type in
    "Cr")
        ini_name="radio"
        ;;
    "Cc")
        ini_name="crypto"
        ;;
    *)
        ini_name="tic"
        ;;
esac

# Copy INI files, if they don't exist in upper directory.
if [ ! -e /srv/app/$copy_name.ini ] ; then
    cp ./$ini_name.ini.master /srv/app/$copy_name.ini
    log_only "Copying $ini_name master INI file for $copy_name"
else
    log_only "$copy_name INI already exists"
fi

# Check If It is Anka then mv HF9661CommandName.js to HSTA_HF9661CommandName
# and mv ANKA_HF9661CommandName.js to HF9661CommandName.js
if [ -e /srv/.ANKA ] ; then
    mv HF9661CommandName.js HSTA_HF9661CommandName.js
    mv ANKA_HF9661CommandName.js HF9661CommandName.js
    log_only "Configurating HF9661CommandName.js for ANKA"
else
    log_only "Configurating HF9661CommandName.js for HSTA"
fi

#
ttyno="0"
case ${copy_name:3} in
    C1) ttyno="0"
    ;;
    C2) ttyno="2"
    ;;
    R1) ttyno="4"
    ;;
    R2) ttyno="5"
    ;;
    R3) ttyno="6"
esac
sed -i "/^PortName/s/=.*/= \/dev\/ttyS$ttyno/" /srv/app/$copy_name.ini


# Create ini file link to ini file under /srv/app.
ln -s /srv/app/$copy_name.ini ykeTIC.ini
log_only "Creating INI link"

# Following template copy block is needed to be run only once
# So get first of copy names ...
line=$(grep "ykeTIC" /srv/app/link.list|tr -d ' '|cut -d'#' -f1|grep ":C$"|head -1)
if [[ "$line" != "" ]] ; then
    IFS=":"
    set $line
    link_name=$2
    IFS="."
    set $link_name
    first_copy_name=$2
    unset IFS
    if [[ "$first_copy_name" == "" ]] ; then
        first_copy_name=$copy_name
    fi
else
    first_copy_name=$copy_name
fi

# ... then compare with current copy name before copying templates.
if [[ "$first_copy_name" == "$copy_name" ]] ; then
    # Template file is for management script. Copy them to reflect any changes.
    cp crypto.ini.template /srv/app
    log_only "Copying crypto template"

    cp radio.ini.template /srv/app
    log_only "Copying radio template"
fi











