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
	?dm) stop_splashy || return 0 ;;
    esac
}


stop_splashy () { 
    STEPS_DIR=/lib/init/rw/splashy
    SPL_UPD=/sbin/splashy_update
    # load some default variables
    [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"

    # it makes no sense to do this if splashy is not running
    #  - if running:
    #    * set progress to 100%
    #    * send exit
    pidof splashy > /dev/null && \
        $SPL_UPD "progress 100" 2> /dev/null && \
        $SPL_UPD "exit" 2> /dev/null 

    [ ! -d $STEPS_DIR ] && mkdir -p $STEPS_DIR

    # Write to log (for testing)
    if [ "x$DEBUG" != "x0" ]; then
	echo "passed 'exit' was call" >> $STEPS_DIR/splashy.log
        pidof splashy > /dev/null && echo 'Splashy is still running!' >> $STEPS_DIR/splashy.log
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

    # clear tty8 of console messages
    # (see splashy_video.c:splashy_start_splash().DirectFBSetOption("vt-num","8"))
    if [ "$(fgconsole 2>/dev/null)" = "8" ]; then
        clear >/dev/tty8 || true
    fi

    # Do some magic with the TTYs
    if [ -z "$CHVT_TTY" ] && [ "$(fgconsole 2>/dev/null)" != "$CHVT_TTY" ]; then 
        splashy_chvt $CHVT_TTY || true
    fi
    
    # we need to re-run keymap.sh and console-screen.sh only if Splashy scripts stopped it
    # see /etc/kbd/conf.d/splashy and /etc/console-tools/config.d/splashy
    if [ -x "/etc/init.d/keymap.sh" -a -f "/dev/shm/splashy-stopped-keymap" ]; then
        if [ "x$DEBUG" != "x0" ]; then
            cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
            echo "calling keymap.sh" >> $STEPS_DIR/splashy.log
        fi
        /etc/init.d/keymap.sh start || true
        rm -f /dev/shm/splashy-stopped-keymap
    fi

    # console-screen.sh still stops Splashy at this point. Do we still need to run this?? - Luis
    if [ -x "/etc/init.d/console-screen.sh" -a -f "/dev/shm/splashy-stopped-console-screen" ]; then
        if [ "x$DEBUG" != "x0" ]; then
            cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
            echo "calling console-screen.sh" >> $STEPS_DIR/splashy.log
        fi
        /etc/init.d/console-screen.sh start || true
        # whether it worked or not, we do not care
        rm -f /dev/shm/splashy-stopped-console-screen
    fi
    
    # splashy is now stopped. cleanup

    # Bug #455259
    # when not in debug mode, umount our tmpfs
    if [ "x$DEBUG" = "x0" ]; then
        mount | grep $STEPS_DIR > /dev/null 2>&1 \
            && umount $STEPS_DIR 2> /dev/null \
            || true
    fi
}

