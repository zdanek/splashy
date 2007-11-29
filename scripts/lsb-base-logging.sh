# This file is sourced by /lib/lsb/init-fuctions
# it redefines the log_end_message function to
# call splashy_update.
#
# KNOWN BUGS
# Splashy and console-screen.sh don't work together.

# int log_end_message (int exitstatus)
log_end_msg () {
    # If no arguments were passed, return
    [ -z "${1:-}" ] && return 1

    # Only do the fancy stuff if we have an appropriate terminal
    # and if /usr is already mounted
    if log_use_fancy_output; then
        RED=`$TPUT setaf 1`
        NORMAL=`$TPUT op`
        if [ $1 -eq 0 ]; then
            echo "."
        else
            /bin/echo -e " ${RED}failed!${NORMAL}"
        fi
    else
        if [ $1 -eq 0 ]; then
            echo "."
        else
            echo " failed!"
        fi
    fi

    ##############################################################
    # Start splashy code 

    # Bug #400598,#401999
    if [ -z "${RUNLEVEL:-}" ]; then
        # we need only the current level
        RUNLEVEL=`runlevel | sed 's/^. //'`
    fi

    STEPS_DIR=/lib/init/rw/splashy
    SPL_UPD=/sbin/splashy_update
    # load some default variables
    [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
    
    [ ! -d $STEPS_DIR ] && mkdir -p $STEPS_DIR

    SPL_PRG=$STEPS_DIR/$RUNLEVEL-progress

    [ -x $SPL_UPD ] || return $1;
    [ -f $SPL_PRG ] || return $1; 

    # It makes no sense for us to send this step if splashy is not running
    # Although then splashy_update would just return
    pidof splashy > /dev/null || return $1; 

    # Get progress percentage of this script
    # was calculated by update-progress-steps
    PER=`sed -n 's+'${0}' ++ p' $SPL_PRG`

    # This can not happen ...
    [ -n "$PER" ] || return $1;

    # in Sid 2006-10-08 05:57 EDT the scripts after S99rc.local
    # do not call lsb* functions. So we don't know when the boot process 
    # is done
    [ "${0##*/}" = "S99rc.local" ] && PER=100

    # Update progress bar 
    $SPL_UPD "progress $PER" || true

    if [ "x$PER" != "x100" ]; then
        # Set $PER to 100% if X is started or about to be started
        #
        # Actually we should have already been stopped by log_daemon_msg
        case ${0} in 
	    ?dm) PER=100;;
        esac
    fi

    if [ "x$PER" != "x100" ]; then
        pidof X > /dev/null && PER=100
    fi

    if [ "x$PER" != "x100" ]; then
        pidof Xgl > /dev/null && PER=100
    fi

    # Write to log (for testing)
    if [ "x$DEBUG" != "x0" ]; then
	cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
	echo "print ${0##*/} $PER" >> $STEPS_DIR/splashy.log
    fi

    # If we're at 100% stop splashy nicely
    [ "$PER" = "100" ] && stop_splashy

    return $1
}

# We have to stop _before_ ?dm starts
log_daemon_msg () {
    if [ -z "${1:-}" ]; then
        return 1
    fi

    if [ -z "${2:-}" ]; then
        echo -n "$1:"
        return
    fi

    echo -n "$1: $2"

    ##############################################################
    # Splashy code 
   
    # send log to splashy. it will show if F2 is pressed
    pidof splashy > /dev/null && splashy_update "scroll $1: $2"

    # Stop splashy on *dm
    case $2 in 
	?dm) stop_splashy ;;
    esac
}


stop_splashy () { 
    STEPS_DIR=/lib/init/rw/splashy
    SPL_UPD=/sbin/splashy_update
    # load some default variables
    [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"

    # just in case, make progressbar 100%
    $SPL_UPD "progress 100" 2> /dev/null || true

    # now we can exit Splashy:
    $SPL_UPD "exit" 2> /dev/null || true

    [ ! -d $STEPS_DIR ] && mkdir -p $STEPS_DIR

    # Write to log (for testing)
    if [ "x$DEBUG" != "x0" ]; then
	echo "exit was called" >> $STEPS_DIR/splashy.log
	cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
    fi

    # wait until splashy exits before changing tty's
    # this is because of the fade-out effect mostly
    # so 200 ms should do it
    sleep 0.2

    # FIXME what if splashy never exits? infinite loop!
    while `pidof splashy > /dev/null`; do
        if [ "x$DEBUG" != "x0" ]; then
	    echo "Splashy didn't die!" >> $STEPS_DIR/splashy.log
	fi

        killall -15 splashy > /dev/null 2>&1
	sleep 0.2

	#echo "calling killall -9 splashy" >> $STEPS_DIR/splashy.log
	killall -9 splashy > /dev/null 2>&1
    done

    # Do some magic with the TTYs
    [ -z "$CHVT_TTY" ] || splashy_chvt $CHVT_TTY || true

    # Bug #400598,#401999
    if [ -z "${RUNLEVEL:-}" ]; then
        # we need only the current level
        RUNLEVEL=`runlevel | sed 's/^. //'`
    fi
    # Bug #400598
    # if Splashy was running from initramfs,
    # and our runlevel is currently a number between 2 and 5,
    # we need to re-run keymap.sh and console-screen.sh
    if [ $RUNLEVEL -gt 1 -a $RUNLEVEL -lt 6 ]; then 
	if [ -x "/etc/init.d/keymap.sh" ]; then
	    if [ "x$DEBUG" != "x0" ]; then
		cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
		echo "calling keymap.sh" >> $STEPS_DIR/splashy.log
	    fi
	    /etc/init.d/keymap.sh start
	fi

        # console-screen.sh still stops Splashy at this point. Do we still need to run this?? - Luis
	if [ -x "/etc/init.d/console-screen.sh" ]; then
	    if [ "x$DEBUG" != "x0" ]; then
		cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
		echo "calling console-screen.sh" >> $STEPS_DIR/splashy.log
	    fi
	    #FIXME /etc/init.d/console-screen.sh start
	fi
    fi 

    # when not in debug mode, umount our tmpfs
    if [ "x$DEBUG" = "x0" ]; then
        umount $STEPS_DIR 2> /dev/null
    fi
}

