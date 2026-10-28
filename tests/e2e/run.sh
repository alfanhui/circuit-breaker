#!/bin/sh
# Runs inside the container. Boots /game/game.cue in Mednafen on a virtual screen (Xvfb),
# then follows a step script (one command per line, # for comments):
#   wait SECONDS        let the game run
#   press BUTTON [N]    tap a Saturn pad button N times (default 1), each held ~4 frames
#                       then released ~4 frames, so the game sees every press
#   hold BUTTON SECONDS hold a button down
#   keydown BUTTON      press a button and keep it down (shots can be taken meanwhile)...
#   keyup BUTTON        ...until this lets it go
#   shot NAME           save the screen to /out/NAME.png
# Buttons: up down left right a b c x y z l r start  (Mednafen's default keyboard mapping)
set -e
export DISPLAY=:99 MEDNAFEN_HOME=/tmp/mednafen PATH="$PATH:/usr/games"
mkdir -p "$MEDNAFEN_HOME" /out

key_for() {
	case "$1" in
	up) echo w ;; down) echo s ;; left) echo a ;; right) echo d ;;
	a) echo KP_1 ;; b) echo KP_2 ;; c) echo KP_3 ;;
	x) echo KP_4 ;; y) echo KP_5 ;; z) echo KP_6 ;;
	l) echo KP_7 ;; r) echo KP_9 ;;
	start) echo Return ;;
	*) echo "unknown button $1" >&2; exit 1 ;;
	esac
}

Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 &
sleep 1
mednafen -sound 0 -video.driver softfb -video.fs 0 \
	-ss.bios_jp /bios/bios.bin -ss.bios_na_eu /bios/bios.bin -ss.bios_sanity 0 \
	-ss.region_autodetect 0 -ss.region_default jp \
	/game/game.cue >/out/mednafen.log 2>&1 &

# SDL only reads the keyboard of the focused window
window=$(timeout 20 xdotool search --sync --onlyvisible --class mednafen | head -1) # the title is the disc name
[ -n "$window" ] || { echo "Mednafen window not found" >&2; exit 1; }
xdotool windowfocus "$window"

while read -r command arg1 arg2; do
	case "$command" in
	'' | '#'*) ;;
	wait) sleep "$arg1" ;;
	press)
		k=$(key_for "$arg1")
		i=0
		while [ "$i" -lt "${arg2:-1}" ]; do
			xdotool keydown "$k"; sleep 0.07; xdotool keyup "$k"; sleep 0.07
			i=$((i + 1))
		done
		;;
	hold) k=$(key_for "$arg1"); xdotool keydown "$k"; sleep "$arg2"; xdotool keyup "$k" ;;
	keydown) xdotool keydown "$(key_for "$arg1")" ;;
	keyup) xdotool keyup "$(key_for "$arg1")" ;;
	shot) import -window root -crop 906x720+0+0 "/out/$arg1.png"; echo "shot $arg1" ;;
	*) echo "unknown step: $command" >&2; exit 1 ;;
	esac
done <"$1"
