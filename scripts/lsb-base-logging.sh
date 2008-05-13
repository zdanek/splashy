# This file is sourced by /lib/lsb/init-fuctions
# it redefines the log_end_message function to
# call splashy_update.
# See /lib/lsb/init-functions for usage help.
#
# KNOWN BUGS
# * Splashy and console-screen.sh don't work together
# * This script assumes binary paths do not contain spaces

stop_splashy () { 
    # 
    # when we exit Splashy there are a few things that need to be done:
    # - set progressbar to 100%
    # - Splashy always runs on tty8, if the user wants us to switch TTYs, 
    #   we will do it with splashy_chvt
    # - if Splashy stopped console-screen.sh and keyboard.sh, this is the time to re-run those
    #   console-screen.sh sets the fonts for the console and it cannot be done while in 
    #   graphics mode
    # - umount our STEPS_DIR
    STEPS_DIR=/lib/init/rw/splashy
    SPL_UPD=/sbin/splashy_update
    # load some default variables
    [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"

    # it makes no sense to do this if splashy is not running
    #  - if running:
    #    * set progress to 100%
    #    * wait a bit for it to actually show
    #    * send exit
    pidof splashy > /dev/null && \
        $SPL_UPD "progress 100" 2> /dev/null && \
        sleep 0.3 && \
        $SPL_UPD "exit" 2> /dev/null 

    # wait until splashy exits before changing tty's
    # this is because of the fade-out effect mostly
    # so 200 ms should do it
    pidof splashy > /dev/null && sleep 0.2

    [ ! -d $STEPS_DIR ] && mkdir -p $STEPS_DIR

    # Write to log (for testing)
    if [ "x$DEBUG" != "x0" ]; then
	echo "passed 'exit' was call" >> $STEPS_DIR/splashy.log
        pidof splashy > /dev/null && echo 'Splashy is still running!' >> $STEPS_DIR/splashy.log
	cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
    fi

    # if splashy does not exit. we kill -9 it...
    while pidof splashy > /dev/null; do
        if [ "x$DEBUG" != "x0" ]; then
	    echo "Splashy didn't die!" >> $STEPS_DIR/splashy.log
	fi

        kill -15 `pidof splashy`
	sleep 0.2

	#echo "calling killall -9 splashy" >> $STEPS_DIR/splashy.log
	killall -9 `pidof splashy` > /dev/null 2>&1
    done

    # clear tty8 of console messages
    # (see splashy_video.c:splashy_start_splash().DirectFBSetOption("vt-num","8"))
    if [ "$(fgconsole 2>/dev/null)" = "8" ]; then
        clear >/dev/tty8 || true
    fi
 
    # we need to re-run keymap.sh and console-screen.sh only if Splashy scripts stopped it
    # see /etc/kbd/conf.d/splashy and /etc/console-tools/config.d/splashy
    if [ -x "/etc/init.d/keymap.sh" -a -f "$STEPS_DIR/splashy-stopped-keymap" ]; then
        if [ "x$DEBUG" != "x0" ]; then
            cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
            echo "calling keymap.sh" >> $STEPS_DIR/splashy.log
        fi
        /etc/init.d/keymap.sh start || true
        rm -f $STEPS_DIR/splashy-stopped-keymap
        # clear tty8 of console messages
        # (see splashy_video.c:splashy_start_splash().DirectFBSetOption("vt-num","8"))
        if [ "$(fgconsole 2>/dev/null)" = "8" ]; then
            clear >/dev/tty8 || true
        fi
    fi

    # console-screen.sh still stops Splashy at this point. Do we still need to run this?? - Luis
    if [ -x "/etc/init.d/console-screen.sh" -a -f "$STEPS_DIR/splashy-stopped-console-screen" ]; then
        if [ "x$DEBUG" != "x0" ]; then
            cat /proc/loadavg >> $STEPS_DIR/splashy.log 2>&1
            echo "calling console-screen.sh" >> $STEPS_DIR/splashy.log
        fi
        /etc/init.d/console-screen.sh start || true
        # whether it worked or not, we do not care
        rm -f $STEPS_DIR/splashy-stopped-console-screen
        # clear tty8 of console messages
        # (see splashy_video.c:splashy_start_splash().DirectFBSetOption("vt-num","8"))
        if [ "$(fgconsole 2>/dev/null)" = "8" ]; then
            clear >/dev/tty8 || true
        fi
    fi

    # Do some magic with the TTYs
    if [ -n "$CHVT_TTY" ]; then
        if [ "x$CHVT_TTY" = "xauto" ]; then
            # find the number of the TTY where X is running and switch to this console
            XTTY=`ps -ef|grep X|grep tty| awk '{print $6}'|sort|head -1|sed 's,[^[:digit:]],,g'`
            splashy_chvt ${XTTY:-1} || true
        elif [ "$(fgconsole 2>/dev/null)" != "$CHVT_TTY" ]; then 
            splashy_chvt $CHVT_TTY || true
        else
            # fall back to tty1
            splashy_chvt 1 || true
        fi
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

log_use_splashy () {
    if [ "${loop:-n}" = y ]; then
        return 1
    fi
    pidof splashy > /dev/null && return 0
}

log_to_console () {
    [ "${loop:-n}" != y ] || return 0
    [ "${QUIET:-no}" != yes ] || return 0

    # Only output to the console when we're given /dev/null
    stdin=`readlink /proc/self/fd/0`
    [ "${stdin#/dev/null}" != "$stdin" ] || return 0

    func=$1
    shift

    loop=y $func "$@" </dev/console >/dev/console 2>&1 || true
}

log_success_msg () {
    if log_use_splashy; then
        SPL_UPD=/sbin/splashy_update
        # load some default variables
        [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
        [ -x $SPL_UPD ] || return $1;

        $SPL_UPD "scroll $*" || true # STATUS
    fi

    log_to_console log_success_msg "$@"

    echo " * $@"
}

log_failure_msg () {
    if log_use_splashy; then
        SPL_UPD=/sbin/splashy_update
        # load some default variables
        [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
        [ -x $SPL_UPD ] || return $1;

        $SPL_UPD "scroll $*" || true # STATUS
    fi

    log_to_console log_failure_msg "$@"

    if log_use_fancy_output; then
        RED=`$TPUT setaf 1`
        NORMAL=`$TPUT op`
        echo " $RED*$NORMAL $@"
    else
        echo " * $@"
    fi
}

log_warning_msg () {
    if log_use_splashy; then
        SPL_UPD=/sbin/splashy_update
        # load some default variables
        [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
        [ -x $SPL_UPD ] || return $1;

        $SPL_UPD "scroll $*" || true # STATUS
    fi

    log_to_console log_warning_msg "$@"

    if log_use_fancy_output; then
        YELLOW=`$TPUT setaf 3`
        NORMAL=`$TPUT op`
        echo " $YELLOW*$NORMAL $@"
    else
        echo " * $@"
    fi
}

log_begin_msg () {
    log_daemon_msg "$1"
}

# We have to stop _before_ ?dm starts
log_daemon_msg () {
    if [ -z "$1" ]; then
        return 1
    fi

    if log_use_splashy; then
        SPL_UPD=/sbin/splashy_update
        # load some default variables
        [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
        [ -x $SPL_UPD ] || return $1;

        $SPL_UPD "scroll $*" || true
    fi

    log_to_console log_daemon_msg "$@"

    if log_use_fancy_output && $TPUT xenl >/dev/null 2>&1; then
        COLS=`$TPUT cols`
        if [ "$COLS" ] && [ "$COLS" -gt 6 ]; then
            COL=`$EXPR $COLS - 7`
        else
	    COLS=80
            COL=73
        fi
        # We leave the cursor `hanging' about-to-wrap (see terminfo(5)
        # xenl, which is approximately right). That way if the script
        # prints anything then we will be on the next line and not
        # overwrite part of the message.

        # Previous versions of this code attempted to colour-code the
        # asterisk but this can't be done reliably because in practice
        # init scripts sometimes print messages even when they succeed
        # and we won't be able to reliably know where the colourful
        # asterisk ought to go.

        printf " * $*       "
        # Enough trailing spaces for ` [fail]' to fit in; if the message
        # is too long it wraps here rather than later, which is what we
        # want.
        $TPUT hpa `$EXPR $COLS - 1`
        printf ' '
    else
        echo " * $@"
        COL=
    fi

    ##############################################################
    # Splashy code 
    # Stop splashy on *dm
    case $2 in 
	?dm) stop_splashy || true ;;
    esac
}

log_progress_msg () {
    :
}

log_end_msg () {
    if [ -z "$1" ]; then
        return 1
    fi

    if log_use_splashy; then
        SPL_UPD=/sbin/splashy_update
        # load some default variables
        [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
        [ -x $SPL_UPD ] || return $1;

        if [ "$1" -eq 0 ]; then
            $SPL_UPD "scroll OK" || true # SUCCESS
        else
            $SPL_UPD "scroll failed" || true # FAILURE
        fi
    fi

    log_to_console log_end_msg "$@"

    if [ "$COL" ] && [ -x "$TPUT" ]; then
        printf "\r"
        $TPUT hpa $COL
        if [ "$1" -eq 0 ]; then
            echo "[ OK ]"
        else
            printf '['
            $TPUT setaf 1 # red
            printf fail
            $TPUT op # normal
            echo ']'
        fi
    else
        if [ "$1" -eq 0 ]; then
            echo "   ...done."
        else
            echo "   ...fail!"
        fi
    fi
    ##############################################################
    # Start splashy code 

    # Bug #400598,#401999
    if [ -z "${RUNLEVEL:-}" ]; then
        # we need only the current level
        RUNLEVEL=`runlevel | sed 's/^. //'`
        # Bug # 470816
        if [ -z "$RUNLEVEL" ]; then
            # if we can't figure out the runlevel (such as when run
            # from a cron job) then don't do anything with Splashy
            exit $1
        fi
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

    # if we are shutting down or rebooting, there is no need to go further
    if [ "x$RUNLEVEL" = "x6" ] || [ "x$RUNLEVEL" = "x0" ]; then
	return $1
    fi

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

log_action_msg () {
    if log_use_splashy; then
        SPL_UPD=/sbin/splashy_update
        # load some default variables
        [ -r "/etc/default/splashy" ] && . "/etc/default/splashy"
        [ -x $SPL_UPD ] || return $1;
 
        $SPL_UPD "scroll $*" || true
    fi

    log_to_console log_action_msg "$@"

    echo " * $@"
}

log_action_begin_msg () {
    log_daemon_msg "$@..."
}

log_action_cont_msg () {
    log_daemon_msg "$@..."
}

log_action_end_msg () {
    # In the future this may do something with $2 as well.
    log_end_msg "$1" || true
}
