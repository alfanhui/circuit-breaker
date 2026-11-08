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
#   wait_up             wait_boot's, for a game booted on foot in the corridor (BOOT_ON_FOOT in
#                       src/input.c): its dark red-black, not the arena's navy
#   skip_prologue       jump to play, fresh at the start, from wherever the game boots
#                       (BOOT_SCENE in src/input.c): START (the debug camera), A for each part
#                       to go round (intro -> loop -> play -> intro), START. From the intro
#                       that's A twice; booted into play, three times (round to play afresh).
#                       The game must have booted (~15 s) or the presses are lost
#   start_intro         jump to the opening flight's start, for the scripts about the prologue
#                       (nothing to do when the game boots there)
# Buttons: up down left right a b c x y z l r start  (Mednafen's default keyboard mapping)
set -e
export DISPLAY=:99 MEDNAFEN_HOME=/tmp/mednafen PATH="$PATH:/usr/games"
mkdir -p "$MEDNAFEN_HOME" /out

# Which part of the game it boots into, read from the source the game was built from
boot_scene=$(sed -n 's/^#define BOOT_SCENE \(SCENE_[A-Z]*\).*/\1/p' /game/src/input.c)

# How many times A (the next part, in the debug camera) gets from the boot scene to $1, going
# round intro -> loop -> play -> intro. Play to play is 3: round again to start it afresh
scene_number() {
	case "$1" in SCENE_INTRO) echo 0 ;; SCENE_LOOP) echo 1 ;; SCENE_PLAY) echo 2 ;; *) echo 0 ;; esac
}
presses_to() {
	n=$(( ($(scene_number "$1") - $(scene_number "$boot_scene") + 3) % 3 ))
	[ "$n" -eq 0 ] && [ "$1" = SCENE_PLAY ] && n=3
	echo "$n"
}

# START, A n times, START: through the debug camera to another part of the game
jump_scenes() {
	[ "$1" -eq 0 ] && return
	for b in start $(i=0; while [ "$i" -lt "$1" ]; do echo a; i=$((i + 1)); done) start; do
		k=$(key_for "$b")
		xdotool keydown "$k"; sleep 0.07; xdotool keyup "$k"; sleep 0.07
	done
}

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
	wait_up)
		# Until the corridor's up: more red than green, and not dark. The BIOS's screens are
		# black, the SEGA logo ~(11, 12, 24), the globe grey ~(110, 119, 136): green as high as
		# red in all of them. The corridor's ~(38, 23, 40). At most 40 s, then carry on anyway
		t=0
		while [ "$t" -lt 80 ]; do
			import -window "$window" /tmp/boot.png
			rgb=$(convert /tmp/boot.png -resize '1x1!' -format '%[fx:int(255*r)] %[fx:int(255*g)]' info:)
			r=${rgb% *}
			g=${rgb#* }
			[ "$r" -gt 25 ] && [ $((g + 8)) -lt "$r" ] && break
			sleep 0.4
			t=$((t + 1))
		done
		echo "up after ~$((t * 55 / 100)) s of polling"
		sleep 0.3
		;;
	skip_prologue) jump_scenes "$(presses_to SCENE_PLAY)" ;;
	start_intro) jump_scenes "$(presses_to SCENE_INTRO)" ;;
	hold) k=$(key_for "$arg1"); xdotool keydown "$k"; sleep "$arg2"; xdotool keyup "$k" ;;
	keydown) xdotool keydown "$(key_for "$arg1")" ;;
	keyup) xdotool keyup "$(key_for "$arg1")" ;;
	shot) import -window "$window" "/out/$arg1.png"; echo "shot $arg1" ;;
	*) echo "unknown step: $command" >&2; exit 1 ;;
	esac
done <"$1"
