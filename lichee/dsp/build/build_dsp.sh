#!/bin/bash

DSP_BUILD_SCRIPTS_DIR=$(cd $(dirname $0); pwd)
BUILD_CONFIG=$DSP_BUILD_SCRIPTS_DIR/../.buildconfig
DSP_TOP_DIR=$(cd ${DSP_BUILD_SCRIPTS_DIR}/..; pwd)
source ${DSP_BUILD_SCRIPTS_DIR}/mkcmd.sh

#load export value
if [ -f ${BUILD_CONFIG} ]; then
	. ${BUILD_CONFIG}
fi

function check_input_param()
{
	[ -z "$1" ] && echo "dsp_project_name is empty!" && return 1
	[ -z "$2" ] && echo "amp_dsp0_bin path is empty!" && return 1

	return 0
}

function get_from_config()
{
	local dsp_project_name=$1
	local config_file=${DSP_BUILD_SCRIPTS_DIR}/config

	get_board=$(echo $dsp_project_name | awk -F "_" '{print $NF}')
	get_DspCore=$(echo $dsp_project_name | awk -F "_" '{print $((NF-1))}')
	get_ic=$(echo $dsp_project_name | sed -e "s/_${get_DspCore}\|_${get_board}//g")

	get_data=$(grep ".*${get_ic}.*${get_DspCore}.*${get_board}" $config_file)
	[ $? -ne 0 ] && echo "DSP does not support compiling $dsp_project_name!"  && return 1

	get_XtensaTools=$(echo $get_data | cut -d " " -f1)
	get_kernel=$(echo $get_data | cut -d " " -f2)
	if [ -z "$get_XtensaTools" -o -z "$get_kernel" -o -z "$get_DspCore" -o -z "$get_board" ]; then
		echo "get data from config fail" && return 1
	fi

	# According to IC settings .buildconfig
	mk_autoconfig $get_XtensaTools $get_kernel $get_ic $get_DspCore $get_board
	return 0
}

function build_dsp()
{
	local dsp_project_name=$1
	local amp_dsp0_bin=$2
	local dsp_out_bin

	check_input_param $@
	[ $? -ne 0 ] && return 1

	cd ${DSP_TOP_DIR}
	get_from_config $dsp_project_name
	[ $? -ne 0 ] && return 1

	#update env for .buildconfig
	. ${BUILD_CONFIG}
	mk_defconfig
	mk_install_xcc

	# build/clean/config/menuconfig dsp
	cd ${DSP_TOP_DIR}
	./build.sh $3

	[ ! -z "$3" ] && return 0

	# copy dsp bin to Tina environment
	dsp_out_bin=${LICHEE_CHIP_OUT_DIR}/${LICHEE_IC}_${LICHEE_DSP_CORE}_${LICHEE_CHIP_BOARD}.bin
	if [ -f ${dsp_out_bin} ]; then
		cp -v ${dsp_out_bin} ${amp_dsp0_bin}
	else
		echo "ERROR: ${dsp_out_bin} not exit!"
		return 1
	fi

	return 0
}

build_dsp $@
