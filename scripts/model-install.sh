#!/bin/bash

# env variables NP2_MODULE_DIR, NP2_MODULE_PERMS must be defined and NP2_MODULE_OWNER, NP2_MODULE_GROUP will be used if
# defined when executing this script!
if [ -z "$NP2_MODULE_DIR" -o -z "$NP2_MODULE_PERMS" ]; then
    echo "Required environment variables not defined!"
    exit 1
fi

## optional env variable override
#if [ -n "$SYSREPOCTL_EXECUTABLE" ]; then
#    SYSREPOCTL="$SYSREPOCTL_EXECUTABLE"
## avoid problems with sudo PATH
#elif [ `id -u` -eq 0 ]; then
#    SYSREPOCTL=`su -c 'command -v sysrepoctl' -l $USER`
#else
#    SYSREPOCTL=`command -v sysrepoctl`
#fi
SYSREPOCTL="sysrepoctl"
echo "#### $SYSREPOCTL"
MODDIR=${DESTDIR}${NP2_MODULE_DIR}
PERMS=${NP2_MODULE_PERMS}
OWNER=${NP2_MODULE_OWNER}
GROUP=${NP2_MODULE_GROUP}

# array of modules to install
MODULES=(
"iana-if-type@2017-01-19.yang"
"ietf-interfaces@2018-02-20.yang -e if-mib"
"ietf-ip@2018-02-22.yang -e ipv4-non-contiguous-netmasks"
"ieee802-dot1q-bridge@2018-03-07.yang"
"ieee802-dot1q-sched@2018-09-10.yang -e scheduled-traffic"
"ieee802-ethernet-interface@2019-06-21.yang"
"ietf-system@2014-08-06.yang"
)

# functions
INSTALL_MODULE() {
	echo $(pwd)
	echo "moddir: $MODDIR"
    CMD="'$SYSREPOCTL' -i $MODDIR/$1 -s '$MODDIR' -p '$PERMS' -v2"
    if [ ! -z ${OWNER} ]; then
        CMD="$CMD -o '$OWNER'"
    fi
    if [ ! -z ${GROUP} ]; then
        CMD="$CMD -g '$GROUP'"
    fi
    eval $CMD
    local rc=$?
    if [ $rc -ne 0 ]; then
        exit $rc
    fi
}

UPDATE_MODULE() {
    CMD="'$SYSREPOCTL' -U $MODDIR/$1 -s '$MODDIR' -p '$PERMS' -v2"
    if [ ! -z ${OWNER} ]; then
        CMD="$CMD -o '$OWNER'"
    fi
    if [ ! -z ${GROUP} ]; then
        CMD="$CMD -g '$GROUP'"
    fi
    eval $CMD
    local rc=$?
    if [ $rc -ne 0 ]; then
        exit $rc
    fi
}

ENABLE_FEATURE() {
    "$SYSREPOCTL" -c $1 -e $2 -v2
    local rc=$?
    if [ $rc -ne 0 ]; then
        exit $rc
    fi
}

# get current modules
SCTL_MODULES=`$SYSREPOCTL -l`

for i in "${MODULES[@]}"; do
    name=`echo "$i" | sed 's/\([^@]*\).*/\1/'`

    SCTL_MODULE=`echo "$SCTL_MODULES" | grep "^$name \+|[^|]*| I"`
    if [ -z "$SCTL_MODULE" ]; then
        # install module with all its features
        INSTALL_MODULE "$i"
        continue
    fi

    sctl_revision=`echo "$SCTL_MODULE" | sed 's/[^|]*| \([^ ]*\).*/\1/'`
    revision=`echo "$i" | sed 's/[^@]*@\([^\.]*\).*/\1/'`
    if [ "$sctl_revision" \< "$revision" ]; then
        # update module without any features
        file=`echo "$i" | cut -d' ' -f 1`
        UPDATE_MODULE "$file"
    fi

    # parse sysrepoctl features and add extra space at the end for easier matching
    sctl_features="`echo "$SCTL_MODULE" | sed 's/\([^|]*|\)\{6\}\(.*\)/\2/'` "
    # parse features we want to enable
    features=`echo "$i" | sed 's/[^ ]* \(.*\)/\1/'`
    while [ "${features:0:3}" = "-e " ]; do
        # skip "-e "
        features=${features:3}
        # parse feature
        feature=`echo "$features" | sed 's/\([^[:space:]]*\).*/\1/'`

        # enable feature if not already
        sctl_feature=`echo "$sctl_features" | grep " ${feature} "`
        if [ -z "$sctl_feature" ]; then
            # enable feature
            ENABLE_FEATURE $name $feature
        fi

        # next iteration, skip this feature
        features=`echo "$features" | sed 's/[^[:space:]]* \(.*\)/\1/'`
    done
done
