#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved.
#
# The file initially was copied from sockapit-ts/scripts/run.sh.
#

set -e
pushd "$(dirname "$(which "$0")")" >/dev/null
RUNDIR="$(pwd -P)"
popd >/dev/null

# fix path to RUNDIR in case run.sh is launched from scripts directory
test "$(basename $RUNDIR)" = "scripts" && RUNDIR="${RUNDIR}/.."
test -e "${RUNDIR}/scripts/guess.sh" && source "${RUNDIR}/scripts/guess.sh"

. ${TE_TS_RIGSDIR}/scripts/lib.run

if test -z "${TE_BASE}" ; then
    if test -e dispatcher.sh ; then
        export TE_BASE="$(pwd -P)"
    elif test -e "${RUNDIR}/dispatcher.sh" ; then
        export TE_BASE="${RUNDIR}"
    else
        echo "TE_BASE environment variable must be set" >&2
        exit 1
    fi
fi

if test -z "${TE_TS_ZETAFERNO}" -a -d "${RUNDIR}/zetaferno-ts" ; then
    export TE_TS_ZETAFERNO="${RUNDIR}/zetaferno-ts"
fi

if test -z "${ZETAFERNO_TS_LIBDIR}" ; then
    echo RUNDIR=${RUNDIR}
    if test -d ${RUNDIR}/talib_zetaferno_ts ; then
        export ZETAFERNO_TS_LIBDIR="${RUNDIR}/talib_zetaferno_ts"
        echo Exporting ZETAFERNO_TS_LIBDIR=${ZETAFERNO_TS_LIBDIR}
    fi
fi

export SCRIPTS_DIR="${RUNDIR}"

SUITE_SCRIPTS=
SUITE_SCRIPTS="${SUITE_SCRIPTS} ${RUNDIR}/guess.sh"
SUITE_SCRIPTS="${SUITE_SCRIPTS} ${RUNDIR}/run.sh"
export SUITE_SCRIPTS

usage() {
cat <<EOF
USAGE: run.sh [run.sh options] [dispatcher.sh options]
Options:
  --cfg=<CFG>               Configuration to be used
  --ool=<OOL CFG>           OOL product configuration file
  --ignore-nm               To suppress NetworkManager checking

EOF
${TE_BASE}/dispatcher.sh --help
exit 1
}

RUN_OPTS="${RUN_OPTS} --trc-comparison=normalised --build-meson --sniff-not-feed-conf"
RUN_OPTS="${RUN_OPTS} --tester-only-req-logues"

is_cmod=false
while test -n "$1" ; do
    case $1 in
        --help) usage ;;
        --ignore-nm) ignore_network_manager="true" ;;
        --ignore-zeroconf) ignore_zeroconf="true" ;;
        --ool-profile=*)
        # OOL specific profiles
        ool_profile=${1#--ool-profile=}
        RUN_OPTS="${RUN_OPTS} --script=ool/profile:$ool_profile"
        ;;
        --ool=m32|--ool=m64)
        # Export these variables must occur before the call 'scripts/iut_os'
        export TE_OOL_UL=${1#--ool=}
        export TE_IUT_CFLAGS_VAR="${TE_IUT_CFLAGS_VAR} -${TE_OOL_UL}"
        export TE_IUT_CPPFLAGS_VAR="${TE_IUT_CPPFLAGS_VAR} -${TE_OOL_UL}"
        ;;&
        --ool=*)
        # OOL specific configurations
        ool_config=${1#--ool=}
        if [[ "$ool_config" != "${ool_config/bond/}" ]] || \
           [[ "$ool_config" != "${ool_config/team/}" ]] ; then
            export SOCKAPI_TS_BOND=$ool_config
            ool_config=aggregation
        fi
        OOL_SET="$OOL_SET $ool_config"
        ;;
        --cfg=cmod-x3sim-*)
            is_cmod=true
            ;;&
        --cfg=*)

        cfg=${1#--cfg=}
        RUN_OPTS="${RUN_OPTS} --opts=run/$cfg"
        ${is_cmod} || take_items "$cfg"
            ;;
        --tester-*=[^\"]*)
            RUN_OPTS="${RUN_OPTS} ${1%%=*}=\"${1#*=}\""
            ;;
        *)  RUN_OPTS="${RUN_OPTS} $1" ;;
    esac
    shift 1
done

check_sf_zetaferno_dir $OOL_SET

if test -n "${TE_TS_ZETAFERNO}" ; then
    RUN_OPTS="${RUN_OPTS} --opts=opts.ts"
fi

if test -z "${TE_BUILD}" ; then
    if test "${RUNDIR}" = "$(pwd -P)" ; then
        TE_BUILD="$(pwd -P)/build"
        mkdir -p build
    else
        TE_BUILD="$(pwd -P)"
    fi
    export TE_BUILD
fi

MY_OPTS=
MY_OPTS="${MY_OPTS} --conf-dirs=\"${RUNDIR}/conf:${SF_TS_CONFDIR}:${TE_TS_RIGSDIR}\""
test -e "${RUNDIR}/trc/top.xml" &&
    MY_OPTS="${MY_OPTS} --trc-db=${RUNDIR}/trc/top.xml"

host="$cfg"
if ! $is_cmod ; then
    hosts=$(cat ${TE_TS_RIGSDIR}/env/$cfg | egrep "(TE_IUT=|TE_TST[0-9]*=)" | sed "s/.*=//")
    export_te_workspace_make_dirs "${TE_TS_RIGSDIR}/env/$host"
fi

if test -z "$ignore_network_manager" ; then
    for curr_host in ${hosts}; do
        [ -n "`ssh $curr_host ps aux 2>/dev/null | egrep NetworkManager.*/var/run/NetworkManager | grep -v grep`" ] || continue
        echo "NetworkManager is running on $curr_host. Use --ignore-nm to suppress warning."
        exit 1
    done
fi

if test -z "$ignore_zeroconf" ; then
    for curr_host in ${hosts}; do
        [ -n "`ssh $curr_host /sbin/route 2>/dev/null | grep ^link-local`" ] || continue
        echo "ZEROCONF is enabled on $curr_host. Use --ignore-zeroconf to suppress warning."
        echo "Add 'NOZEROCONF=yes' line to /etc/sysconfig/network to disable ZEROCONF."
        exit 1
    done
fi

if ! $is_cmod ; then
    export_cmdclient $host

    # Note: firmware variants (full/low) applicable for sfc only
    iut_ifs=( $(get_sfx_ifs $host sfc "") )
    export_iut_fw_version $host ${iut_ifs[0]}
fi
OOL_SET=$(fw_var_consistency $OOL_SET)

if test $? -eq 1 ; then
    exit 1
fi
if test "x${OOL_SET/nopio/}" == "x${OOL_SET}" -a \
        "x${OOL_SET/zf_pio2/}" == "x${OOL_SET}" ; then
    OOL_SET="${OOL_SET} zf_pio2"
fi

# ool/config/branch_* should be moved at the end.
# This file takes into account all changes made by other options to
# handle branch-depended requirements
# This is a copy from sapi-ts/scripts/ool_fix_consistency.sh
for i in ${OOL_SET} ; do
    if test "x${i/branch_}" != "x${i}" ; then
        OOL_SET="${OOL_SET/${i}/} $i"
        break
    fi
done

for i in $OOL_SET ; do
    RUN_OPTS="$RUN_OPTS --script=ool/config/$i"
done

eval "${TE_BASE}/dispatcher.sh ${MY_OPTS} ${RUN_OPTS}"
RESULT=$?

if test ${RESULT} -ne 0 ; then
    echo FAIL
    echo ""
fi

echo -ne "\a"
exit ${RESULT}
