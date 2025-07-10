#!/bin/bash

RTOS_TOPDIR=${1}
cd ${RTOS_TOPDIR}

source ${RTOS_TOPDIR}/tools/scripts/envsetup.sh

function print_help()
{
	echo "build_rtos.sh - this script is used to build rots when terminal have not run lunch_rtos and copy rots bin to tina environment."
	echo "Usage examples: ./build_rtos.sh rtos_path rtos_project_name Tina_bin_path"
}

function build_rtos()
{
	if [ ! -d "$1" ]; then
		echo -e "ERROR: rtos path ${1} not exit"
		print_help
		return 1
	fi

	local PROJECT
	local TINA_RTOS_BIN
	local T=$(gettop)

	if [ -n "$2" ]; then
		PROJECT=$2
	else
		echo -e "ERROR: rtos project name is null"
		print_help
		return 1
	fi

	lunch_rtos $PROJECT
	mrtos $4

	build_results=$?

	if [ $build_results != 0 ]; then
		return $build_results
	fi

	if [ "x$4" = "xclean" ]; then
		return 0
	fi

	if [ -n "$3" ]; then
		TINA_RTOS_BIN=$3
	else
		echo -e "INFO: TINA_RTOS_BIN is null"
		print_help
		return 1
	fi

	#copy rtos bin to Tina environment
	local RISV64_STRIP=${RTOS_BUILD_TOOLCHAIN}strip
	local IMG_DIR=${T}/lichee/rtos/build/${PROJECT}/img
	if [ -f ${IMG_DIR}/rt_system.elf ]; then
		cp -v ${IMG_DIR}/rt_system.elf ${TINA_RTOS_BIN}
		${RISV64_STRIP} ${TINA_RTOS_BIN}
	else
		echo -e "ERROR: rt_system.elf not exit"
		return 1
	fi

	return 0
}

build_rtos $@
