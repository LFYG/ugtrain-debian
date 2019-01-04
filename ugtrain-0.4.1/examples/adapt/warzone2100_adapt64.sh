#!/bin/bash

# Tested with: Warzone 2100 2.3.8, 2.3.9, 3.1~rc2, 3.1.0, 3.1.1, 3.2.3

# We already know that warzone2100 is a 64-bit C++ application and that
# the Droid and Structure classes are allocated with _Znwm() or malloc().
# From previous discovery runs we already know the malloc sizes.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_VERS=`${APP_PATH} --version | grep -o "\([0-9]\+\\.\)\{2\}[0-9]\+"`
RC=0

# Droid size
MSIZE11="0x410"
MSIZE12="0x360"
MSIZE13="0x368"
MSIZE14="0x350"

# Structure size
MSIZE21="0x160"
MSIZE22="0x1a8"
MSIZE23="0x170"
MSIZE24="0x198"

. ./_common_adapt.sh

check_Znwm1()
{
    MSIZE=$1
    get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 4 4 4
    if [ $RC -ne 0 ]; then
        get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 3 3 3
    fi
    if [ $RC -ne 0 ]; then
      if [ -z "$FUNC_ADDR" ]; then
        get_func_addr "$APP_PATH" _Znwm
      fi
      get_malloc_code "$APP_PATH" "$FUNC_ADDR <" "$MSIZE," 4 4 4
    fi
    if [ $RC -ne 0 ]; then
      get_malloc_code "$APP_PATH" "$FUNC_ADDR <" "$MSIZE," 3 3 3
    fi
}

case "$APP_VERS" in
2.3.*)
    MSIZE1="$MSIZE11"
    get_malloc_code "$APP_PATH" "\<malloc@plt\>" "$MSIZE1," 3 3 3
    ;;
3.0*)
    MSIZE1="$MSIZE12"
    check_Znwm1 $MSIZE1
    ;;
3.1*)
    MSIZE1="$MSIZE12"
    check_Znwm1 $MSIZE1
    ;;
*)
    MSIZE1="$MSIZE13"
    check_Znwm1 $MSIZE1
    if [ $RC -ne 0 ]; then
        MSIZE1="$MSIZE14"
        check_Znwm1 $MSIZE1
    fi
    ;;
esac
if [ $RC -ne 0 ]; then exit 1; fi

CODE_ADDR1="$CODE_ADDR"

############################################

check_Znwm2()
{
    MSIZE=$1
    EXPL="$2"
    if [ -z "$EXPL" ]; then EXPL="8"; fi
    get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 4 $EXPL 4
    if [ $RC -ne 0 ]; then
        get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 3 7 7
    fi
    if [ $RC -ne 0 ]; then
      if [ -z "$FUNC_ADDR" ]; then
        get_func_addr "$APP_PATH" _Znwm
      fi
      get_malloc_code "$APP_PATH" "$FUNC_ADDR <" "$MSIZE," 4 $EXPL 4
    fi
    if [ $RC -ne 0 ]; then
      get_malloc_code "$APP_PATH" "$FUNC_ADDR <" "$MSIZE," 3 7 7
    fi
}

case "$APP_VERS" in
2.3.*)
    MSIZE2="$MSIZE21"
    get_malloc_code "$APP_PATH" "\<malloc@plt\>" "$MSIZE2," 3 7 7
    ;;
3.0*)
    MSIZE2="$MSIZE22"
    check_Znwm2 $MSIZE2
    ;;
3.1*)
    MSIZE2="$MSIZE22"
    check_Znwm2 $MSIZE2
    ;;
*)
    MSIZE2="$MSIZE23"
    check_Znwm2 $MSIZE2 9
    if [ $RC -ne 0 ]; then
        MSIZE2="$MSIZE24"
        check_Znwm2 $MSIZE2 9
    fi
    ;;
esac
if [ $RC -ne 0 ]; then exit 1; fi

CODE_ADDR2="$CODE_ADDR"

RESULT=`echo "2;Droid;$MSIZE1;0x$CODE_ADDR1;Structure;$MSIZE2;0x$CODE_ADDR2"`
echo "$RESULT"

# Should return something like this:
#  4a6c4d:       bf 60 03 00 00          mov    $0x360,%edi
#  4a6c52:       41 89 c6                mov    %eax,%r14d
#  4a6c55:       e8 b6 8a fb ff          callq  45f710 <_Znwm@plt>
#  4a6c5a:       89 ea                   mov    %ebp,%edx

# This shows us that 0x4a6c5a is the relevant Droid code address.
