#!/bin/bash
# set -x

FILES=(
    yketiccrypto.bin
	crypto_call.ini.master
	deploy.sh
	run.sh
)

PARAM=$1

main $PARAM
