# scripts/mkcmd.sh
#
# (c) Copyright 2013
# Allwinner Technology Co., Ltd. <www.allwinnertech.com>
# James Deng <csjamesdeng@allwinnertech.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# export importance variable
XtensaTools_version=(
"RI-2020.4-linux"
"RI-2020.5-linux"
)

XtensaTools_core=(
"aw_axi_cfg0"
"aw_h5_cfg0_prod"
)

XtensaTools_package=(
"aw_axi_cfg0_linux.tgz"
"aw_h5_cfg0_prod_linux.tgz"
)

allconfig=(
LICHEE_XTENSATOOLS
LICHEE_KERNEL_DIR
)

function mk_error()
{
	echo -e "\033[47;31mERROR: $*\033[0m"
}

function mk_warn()
{
	echo -e "\033[47;34mWARN: $*\033[0m"
}

function mk_info()
{
	echo -e "\033[47;30mINFO: $*\033[0m"
}

# define importance variable
export LICHEE_BUILD_DIR=$(cd $(dirname $0) && pwd)
export LICHEE_TOP_DIR=$(cd $LICHEE_BUILD_DIR/.. && pwd)
export LICHEE_KERNEL_DIR=${LICHEE_TOP_DIR}/kernel
export LICHEE_CHIP_DIR=${LICHEE_TOP_DIR}/device/chips
export LICHEE_CHIP_IC_DIR
export LICHEE_CHIP_CORE_DIR
export LICHEE_CHIP_BOARD_DIR
export LICHEE_CHIP_OUT_DIR
export LICHEE_CHIP_ARCH_DIR
export LICHEE_CUR_PATH
export LICHEE_HOST_PATH
export LICHEE_XTENSA_TOOLS_PACKAGE
export LICHEE_PACK_DIR=${LICHEE_TOP_DIR}/pack

# XTENSA config
export LM_LICENSE_FILE="27000@192.168.204.44"
export XTENSA_TOOLS_DIR
export XTENSA_SYSTEM
export XTENSA_PATH

#eg. save_config "LICHEE_PLATFORM" "$LICHEE_PLATFORM" $BUILD_CONFIG
function save_config()
{
	local cfgkey=$1
	local cfgval=$2
	local cfgfile=$3
	local dir=$(dirname $cfgfile)
	[ ! -d $dir ] && mkdir -p $dir
	cfgval=$(echo -e "$cfgval" | sed -e 's/^\s\+//g' -e 's/\s\+$//g')
	if [ -f $cfgfile ] && [ -n "$(sed -n "/^\s*export\s\+$cfgkey\s*=/p" $cfgfile)" ]; then
		sed -i "s|^\s*export\s\+$cfgkey\s*=\s*.*$|export $cfgkey=$cfgval|g" $cfgfile
	else
		echo "export $cfgkey=$cfgval" >> $cfgfile
	fi
}

function load_config()
{
	local cfgkey=$1
	local cfgfile=$2
	local defval=$3
	local val=""

	[ -f "$cfgfile" ] && val="$(sed -n "/^\s*export\s\+$cfgkey\s*=/h;\${x;p}" $cfgfile | sed -e 's/^[^=]\+=//g' -e 's/^\s\+//g' -e 's/\s\+$//g')"
	eval echo "${val:-"$defval"}"
}


function list_subdir()
{
	echo "$(eval "$(echo "$(ls -d $1/*/)" | sed  "s/^/basename /g")")"
}

function mk_select()
{
	local val_list=$1
	local cfg_key=$2
	local cnt=0
	local cfg_val=$(load_config $cfg_key $BUILD_CONFIG)
	local cfg_idx=0
	local banner=$(echo ${cfg_key:7} | tr '[:upper:]' '[:lower:]')
	local input_ok=0

	printf "All available $banner:\n"
	for val in $val_list; do
		if [ ${val} != "config" ] && [ ${val} != "default" ] ; then
			array[$cnt]=$val
			if [ "X_$cfg_val" == "X_${array[$cnt]}" ]; then
				cfg_idx=$cnt
			fi
			printf "%4d. %s\n" $cnt $val
			let "cnt++"
		fi
	done
	while true; do
		#read default value
		read -p "Choice [${array[$cfg_idx]}]: " choice
		if [ -z "${choice}" ]; then
			choice=$cfg_idx
		fi

		#check choice num
		if [ -z "${choice//[0-9]/}" ] ; then
			if [ $choice -ge 0 -a $choice -lt $cnt ] ; then
				cfg_val="${array[$choice]}"
				input_ok=1
			fi
		fi

		#check choice string
		for val in ${array[@]}; do
			if [ "X_$choice" == "X_$val" ]; then
				cfg_val="$choice"
				input_ok=1
				break;
			fi
		done

		if [ $input_ok -eq 1 ]; then
			break;
		else
			mk_error "invalid value ..."
			exit 0;
		fi
	done

	export $cfg_key=$cfg_val
	save_config "$cfg_key" "$cfg_val" $BUILD_CONFIG
}

function select_XtensaTools()
{
	local val_list="${XtensaTools_version[@]}"
	local cfg_key="LICHEE_XTENSATOOLS"
	mk_select "$val_list" "$cfg_key"

	#set xtensa tool env
	XTENSA_TOOLS_DIR=/opt/${LICHEE_XTENSATOOLS}/XtensaTools
	XTENSA_PATH=${XTENSA_TOOLS_DIR}/bin:${XTENSA_TOOLS_DIR}/lib/iss
	XTENSA_SYSTEM=${LICHEE_TOP_DIR}/XtDevTools/${LICHEE_XTENSATOOLS}/config
	LICHEE_HOST_PATH=${PATH}
	LICHEE_CUR_PATH=${PATH}:${XTENSA_PATH}

	save_config "XTENSA_TOOLS_DIR" "$XTENSA_TOOLS_DIR" $BUILD_CONFIG
	save_config "XTENSA_PATH" "$XTENSA_PATH" $BUILD_CONFIG
	save_config "XTENSA_SYSTEM" "$XTENSA_SYSTEM" $BUILD_CONFIG
	save_config "LICHEE_CUR_PATH" "$LICHEE_CUR_PATH" $BUILD_CONFIG
	save_config "LICHEE_HOST_PATH" "$LICHEE_HOST_PATH" $BUILD_CONFIG
}

function select_XtensaCore()
{
	local cnt=0
	local val_list="${XtensaTools_version[@]}"

	local core_key
	local tgz_package

	for val in $val_list; do
		if [ "x$LICHEE_XTENSATOOLS" == "x${val}" ]; then
			core_key="${XtensaTools_core[cnt]}"
			tgz_package="${XtensaTools_package[cnt]}"
			save_config "XTENSA_CORE" "$core_key" $BUILD_CONFIG
			save_config "LICHEE_XTENSA_TOOLS_PACKAGE" "$tgz_package" $BUILD_CONFIG
			return
		fi
		let "cnt++"
	done

	exit 1;
}

function select_kernel()
{
	local val_list=$(list_subdir $LICHEE_KERNEL_DIR)
	local cfg_key="LICHEE_KERNEL"
	mk_select "$val_list" "$cfg_key"
}

function select_ic()
{
	local val_list=$(list_subdir $LICHEE_CHIP_DIR)
	local cfg_key="LICHEE_IC"
	local arch_key
	mk_select "$val_list" "$cfg_key"
	LICHEE_CHIP_IC_DIR=${LICHEE_CHIP_DIR}/${LICHEE_IC}
	save_config "LICHEE_CHIP_IC_DIR" "$LICHEE_CHIP_IC_DIR" $BUILD_CONFIG

	if [ "x$LICHEE_IC" == "xr528" -o "x$LICHEE_IC" == "xd1" \
		-o "x$LICHEE_IC" == "xt113" -o "x$LICHEE_IC" == "xt113s2" \
		-o "x$LICHEE_IC" == "xt113_i" -o "x$LICHEE_IC" == "xt113_s3p" \
		-o "x$LICHEE_IC" == "xt113_s4" -o "x$LICHEE_IC" == "xt113_s4p" ]; then
		arch_key="sun8iw20"
	elif [ "x$LICHEE_IC" == "xr128s1" -o "x$LICHEE_IC" == "xr128s2" -o "x$LICHEE_IC" == "xr128s3" ]; then
		arch_key="sun20iw2"
	elif [ "x$LICHEE_IC" == "xr329" ]; then
		arch_key="sun50iw11"
	elif [ "x$LICHEE_IC" == "xa523" -o "x$LICHEE_IC" == "xmr527" -o "x$LICHEE_IC" == "xt527" ]; then
		arch_key="sun55iw3"
	elif [ "x$LICHEE_IC" == "xt736" ]; then
		arch_key="sun60iw1"
	else
		echo "ERROR: unkwon arch ..."
		exit 0;
	fi
	save_config "LICHEE_CHIP_ARCH" "$arch_key" $BUILD_CONFIG
}

function select_DspCore()
{
	local val_list=$(list_subdir $LICHEE_CHIP_IC_DIR)
	local cfg_key="LICHEE_DSP_CORE"
	mk_select "$val_list" "$cfg_key"
	LICHEE_CHIP_CORE_DIR=${LICHEE_CHIP_IC_DIR}/${LICHEE_DSP_CORE};
	save_config "LICHEE_CHIP_CORE_DIR" "$LICHEE_CHIP_CORE_DIR" $BUILD_CONFIG
}

function select_board()
{
	local val_list=$(list_subdir $LICHEE_CHIP_CORE_DIR)
	local cfg_key="LICHEE_CHIP_BOARD"
	mk_select "$val_list" "$cfg_key"
	LICHEE_CHIP_BOARD_DIR=${LICHEE_CHIP_CORE_DIR}/${LICHEE_CHIP_BOARD}
	save_config "LICHEE_CHIP_BOARD" "$LICHEE_CHIP_BOARD" $BUILD_CONFIG
	save_config "LICHEE_CHIP_BOARD_DIR" "$LICHEE_CHIP_BOARD_DIR" $BUILD_CONFIG
}

function mk_defconfig()
{
	cd ${LICHEE_TOP_DIR}
	./scripts/conf --defconfig=arch/common/configs/${LICHEE_KERN_DEFCONF} Kconfig
	mk_info "use arch/common/configs/${LICHEE_KERN_DEFCONF} ..."
}

function mk_savedefconfig()
{
	cd ${LICHEE_TOP_DIR}
	./scripts/conf --savedefconfig=arch/common/configs/${LICHEE_KERN_DEFCONF} Kconfig
	mk_info "save .config to arch/common/configs/${LICHEE_KERN_DEFCONF} ..."
}

function mk_config()
{
	save_config "LICHEE_TOP_DIR" "$LICHEE_TOP_DIR" $BUILD_CONFIG
	select_XtensaTools
	select_XtensaCore
	select_kernel
	select_ic
	select_DspCore
	select_board
	mk_tools
	parse_boardconfig
	#update .buildconfig
	. $LICHEE_TOP_DIR/.buildconfig

	LICHEE_CHIP_OUT_DIR=${LICHEE_TOP_DIR}/out/${LICHEE_IC}/${LICHEE_CHIP_BOARD}
	save_config "LICHEE_CHIP_OUT_DIR" "$LICHEE_CHIP_OUT_DIR" $BUILD_CONFIG

	LICHEE_CHIP_ARCH_DIR=${LICHEE_TOP_DIR}/arch/${LICHEE_CHIP_ARCH}
	save_config "LICHEE_CHIP_ARCH_DIR" "$LICHEE_CHIP_ARCH_DIR" $BUILD_CONFIG

	LICHEE_CHIP_LSP_DIR=${LICHEE_TOP_DIR}/arch/${LICHEE_CHIP_ARCH}/lsp/${LICHEE_DSP_CORE}/${LICHEE_LSP}
	save_config "LICHEE_CHIP_LSP_DIR" "$LICHEE_CHIP_LSP_DIR" $BUILD_CONFIG
}

function mk_tools()
{
	mk_info "Prepare executive of tools ..."
	bash -c ${LICHEE_PACK_DIR}/build.sh
}

function mk_lichee()
{
	# get BoardConfig.mk config
	parse_boardconfig

	#set
	XTENSA_PATH=${XTENSA_TOOLS_DIR}/bin:${XTENSA_TOOLS_DIR}/lib/iss
	XTENSA_SYSTEM=${LICHEE_TOP_DIR}/XtDevTools/${LICHEE_XTENSATOOLS}/config
	mk_info "----------------------------------------"
	mk_info "LICHEE_XTENSATOOLS=${LICHEE_XTENSATOOLS}"
	mk_info "XTENSA_CORE=${XTENSA_CORE}"
	mk_info "LICHEE_KERNEL=${LICHEE_KERNEL}"
	mk_info "LICHEE_IC=${LICHEE_IC}"
	mk_info "LICHEE_CHIP_ARCH=${LICHEE_CHIP_ARCH}"
	mk_info "LICHEE_DSP_CORE=${LICHEE_DSP_CORE}"
	mk_info "LICHEE_CHIP_BOARD=${LICHEE_CHIP_BOARD}"
	mk_info "LICHEE_KERN_DEFCONF=${LICHEE_KERN_DEFCONF}"
	mk_info "LICHEE_LSP=${LICHEE_LSP}"
	mk_info "----------------------------------------"

	export PATH=${LICHEE_CUR_PATH}
	cd ${LICHEE_TOP_DIR} && make clean
	make -j32 CONFIG_LSP=${LICHEE_LSP} CONFIG_HEAD_DIR=${LICHEE_RTOS_HEAD}
	make pack
}

function mk_clean()
{
	#set
	XTENSA_PATH=${XTENSA_TOOLS_DIR}/bin:${XTENSA_TOOLS_DIR}/lib/iss
	XTENSA_SYSTEM=${LICHEE_TOP_DIR}/XtDevTools/${LICHEE_XTENSATOOLS}/config

	export PATH=${LICHEE_CUR_PATH}
	cd ${LICHEE_TOP_DIR} && make clean
}

function mk_install_xcc()
{
	local BUILD_CONFIG=$LICHEE_TOP_DIR/.buildconfig
	local xcc_version=$(load_config LICHEE_XTENSATOOLS $BUILD_CONFIG)
	local xcc_core=$(load_config XTENSA_CORE $BUILD_CONFIG)
	local tgz_package=$(load_config LICHEE_XTENSA_TOOLS_PACKAGE $BUILD_CONFIG)

	mk_info "Prepare XtDevTools of aw package ..."
	mk_info "Version:${xcc_version}"
	mk_info "Core:${xcc_core}"
	mk_info "Software package:${tgz_package}"

	cp ${LICHEE_TOP_DIR}/XtDevTools/downloads/*/${tgz_package} ${LICHEE_TOP_DIR}/XtDevTools/downloads/${tgz_package}
	if [ -f ${LICHEE_TOP_DIR}/XtDevTools/downloads/${tgz_package} ] ; then
		if [ ! -d "${LICHEE_TOP_DIR}/XtDevTools/${xcc_version}/${xcc_core}" ]; then
			mk_info "install XtDevTools ${tgz_package} ..."
			cd ${LICHEE_TOP_DIR}/XtDevTools
			tar zxvf downloads/${tgz_package}
			cd ${LICHEE_TOP_DIR}/XtDevTools/${xcc_version}
			mkdir -p config
			$(echo "$(find -name aw*cfg0 -o -name aw*cfg0_prod )" | sed  "s/^/cd /g")
			./install --xtensa-tools "/opt/${xcc_version}/XtensaTools" --no-default --no-replace --registry "../config"
		else
			mk_info "XtDevTools ${tgz_package} already installed ..."
		fi
	else
		mk_error "XtDevTools ${tgz_package} can not find ..."
	fi
	rm ${LICHEE_TOP_DIR}/XtDevTools/downloads/${tgz_package}
	cd ${LICHEE_TOP_DIR}
}

function mk_delete_xcc()
{
	local val_list="${XtensaTools_version[@]}"
	for val in $val_list; do
		rm -rf ${LICHEE_TOP_DIR}/XtDevTools/${val}
		mk_info "delete XtDevTools ${val} ..."
	done
}

function parse_boardconfig()
{
	local default_config="${LICHEE_CHIP_CORE_DIR}/default/BoardConfig.mk"
	local special_config="${LICHEE_CHIP_CORE_DIR}/${LICHEE_CHIP_BOARD}/BoardConfig.mk"

	local default_freertos_config="${LICHEE_CHIP_CORE_DIR}/default"
	local special_freertos_config="${LICHEE_CHIP_CORE_DIR}/${LICHEE_CHIP_BOARD}"

	local config_list=""
	config_list=($default_config $special_config)

	local fetch_list=""
	local fpare_list=""
	for f in ${config_list[@]}; do
		if [ -f $f ]; then
			fetch_list=(${fetch_list[@]} $f)
			fpare_list="$fpare_list -f $f"
		else
			mk_info "${f} can not find ..."
		fi
	done

	local v_cfg=""
	for f in ${config_list[@]}; do
		v_cfg=$(echo $f | sed "s|\.mk$|-${LICHEE_KERN_VER#*-}.mk|g")
		if [ -f $v_cfg ]; then
			fetch_list=(${fetch_list[@]} $v_cfg)
			fpare_list="$fpare_list -f $v_cfg"
		fi
	done

	if [ -z "${fetch_list[0]}" ]; then
		mk_error "BoardConfig not found!"
		exit 1
	fi

	export LICHEE_BOARDCONFIG_PATH="\"${fetch_list[@]}\""

	local cfgkeylist=(
		LICHEE_KERN_DEFCONF  LICHEE_LSP
		)

	local cfgkey=""
	local cfgval=""
	for cfgkey in ${cfgkeylist[@]}; do
		cfgval="$(echo '__unique:;@echo ${'"$cfgkey"'}' | make -f - $fpare_list __unique)"
		eval "$cfgkey='$cfgval'"
	done

	save_config "LICHEE_KERN_DEFCONF" "$LICHEE_KERN_DEFCONF" $BUILD_CONFIG
	save_config "LICHEE_LSP" "$LICHEE_LSP" $BUILD_CONFIG

	if [ -f $special_freertos_config/FreeRTOSConfig.h ]; then
		save_config "LICHEE_RTOS_HEAD" "$special_freertos_config" $BUILD_CONFIG
	else
		if [ -f $default_freertos_config/FreeRTOSConfig.h ]; then
			save_config "LICHEE_RTOS_HEAD" "$default_freertos_config" $BUILD_CONFIG
		else
			mk_error "FreeRTOSConfig.h can not find ..."
		fi
	fi
}

function save_XtensaTools()
{
	export LICHEE_XTENSATOOLS=$1
	save_config "LICHEE_XTENSATOOLS" "$1" $BUILD_CONFIG

	#set xtensa tool env
	XTENSA_TOOLS_DIR=/opt/${LICHEE_XTENSATOOLS}/XtensaTools
	XTENSA_PATH=${XTENSA_TOOLS_DIR}/bin:${XTENSA_TOOLS_DIR}/lib/iss
	XTENSA_SYSTEM=${LICHEE_TOP_DIR}/XtDevTools/${LICHEE_XTENSATOOLS}/config
	LICHEE_HOST_PATH=${PATH}
	LICHEE_CUR_PATH=${PATH}:${XTENSA_PATH}

	save_config "XTENSA_TOOLS_DIR" "$XTENSA_TOOLS_DIR" $BUILD_CONFIG
	save_config "XTENSA_PATH" "$XTENSA_PATH" $BUILD_CONFIG
	save_config "XTENSA_SYSTEM" "$XTENSA_SYSTEM" $BUILD_CONFIG
	save_config "LICHEE_CUR_PATH" "$LICHEE_CUR_PATH" $BUILD_CONFIG
	save_config "LICHEE_HOST_PATH" "$LICHEE_HOST_PATH" $BUILD_CONFIG
}

function save_kernel()
{
	export LICHEE_KERNEL=$1
	save_config "LICHEE_KERNEL" "$1" $BUILD_CONFIG
}

function save_ic()
{
	export LICHEE_IC=$1
	LICHEE_CHIP_IC_DIR=${LICHEE_CHIP_DIR}/${LICHEE_IC}
	save_config "LICHEE_IC" "$1" $BUILD_CONFIG

	if [ "x$LICHEE_IC" == "xr528" -o "x$LICHEE_IC" == "xd1" \
		-o "x$LICHEE_IC" == "xt113" -o "x$LICHEE_IC" == "xt113s2" \
		-o "x$LICHEE_IC" == "xt113_i" -o "x$LICHEE_IC" == "xt113_s3p" ]; then
		arch_key="sun8iw20"
	elif [ "x$LICHEE_IC" == "xr128s1" -o "x$LICHEE_IC" == "xr128s2" -o "x$LICHEE_IC" == "xr128s3" ]; then
		arch_key="sun20iw2"
	elif [ "x$LICHEE_IC" == "xr329" ]; then
		arch_key="sun50iw11"
	elif [ "x$LICHEE_IC" == "xa523" -o "x$LICHEE_IC" == "xmr527" -o "x$LICHEE_IC" == "xt527" ]; then
		arch_key="sun55iw3"
	elif [ "x$LICHEE_IC" == "xt736" ]; then
		arch_key="sun60iw1"
	else
		echo "ERROR: unkwon arch ..."
		exit 0;
	fi
	save_config "LICHEE_CHIP_ARCH" "$arch_key" $BUILD_CONFIG
}

function save_DspCore()
{
	export LICHEE_DSP_CORE=$1
	save_config "LICHEE_DSP_CORE" "$1" $BUILD_CONFIG
	LICHEE_CHIP_CORE_DIR=${LICHEE_CHIP_IC_DIR}/${LICHEE_DSP_CORE};
	save_config "LICHEE_CHIP_CORE_DIR" "$LICHEE_CHIP_CORE_DIR" $BUILD_CONFIG
}

function save_board()
{
	export LICHEE_CHIP_BOARD=$1
	save_config "LICHEE_CHIP_BOARD" "$1" $BUILD_CONFIG
	LICHEE_CHIP_BOARD_DIR=${LICHEE_CHIP_CORE_DIR}/${LICHEE_CHIP_BOARD}
	save_config "LICHEE_CHIP_BOARD_DIR" "$LICHEE_CHIP_BOARD_DIR" $BUILD_CONFIG
}

function mk_autoconfig()
{
	save_config "LICHEE_TOP_DIR" "$LICHEE_TOP_DIR" $BUILD_CONFIG
	save_XtensaTools $1
	select_XtensaCore
	save_kernel $2
	save_ic $3
	save_DspCore $4
	save_board $5
	mk_tools
	parse_boardconfig
	#update .buildconfig
	. $LICHEE_TOP_DIR/.buildconfig

	LICHEE_CHIP_OUT_DIR=${LICHEE_TOP_DIR}/out/${LICHEE_IC}/${LICHEE_CHIP_BOARD}
	save_config "LICHEE_CHIP_OUT_DIR" "$LICHEE_CHIP_OUT_DIR" $BUILD_CONFIG

	LICHEE_CHIP_ARCH_DIR=${LICHEE_TOP_DIR}/arch/${LICHEE_CHIP_ARCH}
	save_config "LICHEE_CHIP_ARCH_DIR" "$LICHEE_CHIP_ARCH_DIR" $BUILD_CONFIG

	LICHEE_CHIP_LSP_DIR=${LICHEE_TOP_DIR}/arch/${LICHEE_CHIP_ARCH}/lsp/${LICHEE_DSP_CORE}/${LICHEE_LSP}
	save_config "LICHEE_CHIP_LSP_DIR" "$LICHEE_CHIP_LSP_DIR" $BUILD_CONFIG
}
