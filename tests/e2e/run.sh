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
#   record_sound        (anywhere in the script) record the game's sound to /out/sound.wav: the
#                       container has no speakers, so it plays to SDL's silent "dummy" output
#   wait_boot           wait until the game is up (its floor fills the screen), however long
#                       the BIOS took: put it before the first press
#   skip_prologue       jump from the opening flight (where the game boots) straight to play,
#                       fresh at the start: START (the debug camera), A twice (intro -> loop ->
#                       play), START. The game must have booted (~15 s) or the presses are lost
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
sound="-sound 0"
if grep -q '^record_sound' "$1"; then
	export SDL_AUDIODRIVER=dummy
	rm -f /out/sound.wav # Mednafen won't record over an old one
	sound="-sound 1 -sound.driver sdl -soundrecord /out/sound.wav"
fi
# $sound is several options, so it's left unquoted to split into them
mednafen $sound -video.driver softfb -video.fs 0 \
	-ss.bios_jp /bios/bios.bin -ss.bios_na_eu /bios/bios.bin -ss.bios_sanity 0 \
	-ss.region_autodetect 0 -ss.region_default jp \
	/game/game.cue >/out/mednafen.log 2>&1 &

# SDL only reads the keyboard of the focused window
window=$(timeout 20 xdotool search --sync --onlyvisible --class mednafen | head -1) # the title is the disc name
[ -n "$window" ] || { echo "Mednafen window not found" >&2; exit 1; }
xdotool windowfocus "$window"

while read -r command arg1 arg2; do
	case "$command" in
	'' | '#'* | record_sound) ;;
	wait) sleep "$arg1" ;;
	press)
		k=$(key_for "$arg1")
		i=0
		while [ "$i" -lt "${arg2:-1}" ]; do
			xdotool keydown "$k"; sleep 0.07; xdotool keyup "$k"; sleep 0.07
			i=$((i + 1))
		done
		;;
	wait_boot)
		# Until the game is up: the floor's dark navy filling the screen. Averaged, the game's
		# shots are ~(26-43, 30-61, 53-91) RGB; the BIOS's are black, its SEGA logo ~(11, 12, 24)
		# and its Saturn globe grey ~(110, 119, 136). Boot time varies (~14-18 s), and a press
		# sent before it's up is lost. At most 40 s, then carry on anyway
		t=0
		while [ "$t" -lt 80 ]; do
			import -window "$window" /tmp/boot.png
			rgb=$(convert /tmp/boot.png -resize '1x1!' -format '%[fx:int(255*r)] %[fx:int(255*b)]' info:)
			r=${rgb% *}
			b=${rgb#* }
			[ "$r" -lt 70 ] && [ "$b" -gt 40 ] && [ "$b" -gt $((r + 20)) ] && break
			sleep 0.4
			t=$((t + 1))
		done
		echo "booted after ~$((t * 55 / 100)) s of polling"
		sleep 0.3 # a few frames more, so the game is reading the pad
		;;
	skip_prologue)
		for b in start a a start; do
			k=$(key_for "$b")
			xdotool keydown "$k"; sleep 0.07; xdotool keyup "$k"; sleep 0.07
		done
		;;
	hold) k=$(key_for "$arg1"); xdotool keydown "$k"; sleep "$arg2"; xdotool keyup "$k" ;;
	keydown) xdotool keydown "$(key_for "$arg1")" ;;
	keyup) xdotool keyup "$(key_for "$arg1")" ;;
	shot) import -window "$window" "/out/$arg1.png"; echo "shot $arg1" ;;
	*) echo "unknown step: $command" >&2; exit 1 ;;
	esac
done <"$1"
