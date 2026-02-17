#!/usr/bin/env bash

ulimit -Sd `ulimit -Hd`

target=riscv64
topdir=/home/hchauhan/workspace/riscv-openbsd
srcdir=${topdir}/openbsd-src
destdir=${topdir}/dest.${target}
objdir=${topdir}/obj.${target}
toolsdir=${topdir}/tools.${target}

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root. Try using sudo." 1>&2
   exit 1
fi

# The rest of your script goes here
echo "Root privileges confirmed. Continuing with the script."

BUILD_TOOLS=0
BUILD_DISTRIB=0
BUILD_KERNEL=0
NPROC=1

while getopts "tdk:c:h" arg; do
    case $arg in
	c)
	    NPROC=${OPTARG}
	    ;;
	h)
	    echo "usage: build.sh -t -d -k <kernel config> -h"
	    echo "                -t -> To build cross tools"
	    echo "                -d -> To build distrib"
	    echo "                -k -> To build kernel with given config"
	    echo "                -c -> Number of CPUs to use for the build process"
	    exit 0
	    ;;
	t)
	    BUILD_TOOLS=1
	    ;;
	d)
	    BUILD_DISTRIB=1
	    ;;
	k)
	    BUILD_KERNEL=1
	    CONFIG_NAME=${OPTARG}
	    ;;
	*)
	    echo "Uknown option $?"
	    exit 1
	    ;;
    esac
done

cd ${srcdir}

CONFIG_PATH=${srcdir}/sys/arch/${target}/conf

if [ ${BUILD_TOOLS} -eq 1 ]; then
    echo "Using ${NPROC} CPUs to build toolchain..."
    make -f Makefile.cross TARGET=${target} CROSSDIR=${destdir} -j${NPROC} cross-dirs
    make -f Makefile.cross TARGET=${target} CROSSDIR=${destdir} -j${NPROC} cross-tools
    ln -sf ${destdir}/usr/obj ${objdir}
    ln -sf ${destdir}/usr/${target}-* ${toolsdir}
fi

export CROSS_TOOL_PATH=${topdir}/dest.riscv64/usr/bin/

if [ ! -e ${CROSS_TOOL_PATH}/riscv64-unknown-openbsd7.8-clang ]; then
    echo "Cannot find compiler at location ${CROSS_TOOL_PATH}"
    exit 255
fi

export PATH=$PATH:${CROSS_TOOL_PATH}

if [ ${BUILD_DISTRIB} -eq 1 ]; then
    make -f Makefile.cross TARGET=${target} CROSSDIR=${destdir} -j1 cross-distrib
fi

if [ ${BUILD_KERNEL} -eq 1 ]; then
    if [ ! -e "${CONFIG_PATH}/${CONFIG_NAME}" ]; then
	echo "Configuration file: ${CONFIG_NAME} doesn't exist at path ${CONFIG_PATH}"
	exit 1
    fi

    KOBJDIR=${objdir}/sys/arch/${target}/compile/
    ls ${KOBJDIR}
    echo "Configuring kernel..."
    config -b ${KOBJDIR} -s ${srcdir}/sys sys/arch/${target}/conf/${CONFIG_NAME}
    cd ${KOBJDIR} && make CC=riscv64-unknown-openbsd7.8-clang STRIP=riscv64-unknown-openbsd7.8-strip
fi
