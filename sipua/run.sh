#!/bin/bash
APPNAME="TIC_KRIPTO"
LOGDIR=.

ulimit -c unlimited

source /srv/bin/functions.sh

ensure_logging

log_only "Starting $APPNAME application"

./yketiccrypto.bin --config-file=crypto_call.ini 1>>$LOGFILE 2>&1

