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
#   record              (anywhere in the script) record every frame Mednafen shows to
#                       /out/film.mov, for the glitch checks below (tests/e2e/glitch.py)
#   flashes_from        the flash check (glitch.py) starts at the frame being shown now...
#   flashes_to NAME     ...and ends here: a frame that differs from both its neighbours far more
#                       than they differ from each other is a one-frame fault. Each is saved with
#                       its neighbours as NAME_flash_N_a/b/c.png. The film carries on, and exits 4
#                       at its end if any check found a fault
#   holes_from          the holes check: in a check build (GLITCH_CHECK in src/features.h) the
#   holes_to NAME       VDP2 back colour is magenta, which nothing else uses, so any magenta below
#                       the screen's middle is a hole in the level. Saved as NAME_hole_N.png
#   record_sound        (anywhere in the script) record the game's sound to /out/sound.wav: the
#                       container has no speakers, so it plays to SDL's silent "dummy" output
#   wait_boot           wait until the game is up (its floor fills the screen), however long
#                       the BIOS took: put it before the first press
#   wait_up             wait_boot's, for a game booted on foot in the corridor (BOOT_ON_FOOT in
#                       src/features.h): its dark red-black, not the arena's navy
#   skip_prologue       jump to play, fresh at the start, from wherever the game boots
#                       (BOOT_SCENE in src/features.h): START (the debug camera), A for each part
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
boot_scene=$(sed -n 's/^#define BOOT_SCENE \(SCENE_[A-Z]*\).*/\1/p' /game/src/features.h)

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
record=""
if grep -q '^record\s*$' "$1"; then
	rm -f /out/film.mov
	# Every frame as it's shown: no frame skipping, no doubled pixels, the PNG codec (each frame a
	# whole PNG file, which glitch.py finds by its signature while the movie is still written), no
	# aspect correction (with it, each frame was 10560 pixels wide and Mednafen fell to ~12 fps).
	# The same settings as jo-engine's own test harness (tests/harness/emulator.py)
	record="-video.frameskip 0 -ss.correct_aspect 0 -qtrecord /out/film.mov -qtrecord.vcodec png -qtrecord.w_double_threshold 0 -qtrecord.h_double_threshold 0"
fi
# How many frames are in the recording so far. Mednafen writes as it goes, a few frames behind
frames_now() {
	sleep 0.2
	python3 /game/tests/e2e/glitch.py count /out/film.mov
}
glitch_failed=0
if grep -q '^record_sound' "$1"; then
	export SDL_AUDIODRIVER=dummy
	rm -f /out/sound.wav # Mednafen won't record over an old one
	sound="-sound 1 -sound.driver sdl -soundrecord /out/sound.wav"
fi
# $sound and $record are several options each, so they're left unquoted to split into them
mednafen $sound $record -video.driver softfb -video.fs 0 \
	-ss.bios_jp /bios/bios.bin -ss.bios_na_eu /bios/bios.bin -ss.bios_sanity 0 \
	-ss.region_autodetect 0 -ss.region_default jp \
	/game/game.cue >/out/mednafen.log 2>&1 &

# SDL only reads the keyboard of the focused window
window=$(timeout 20 xdotool search --sync --onlyvisible --class mednafen | head -1) # the title is the disc name
[ -n "$window" ] || { echo "Mednafen window not found" >&2; exit 1; }
xdotool windowfocus "$window"

while read -r command arg1 arg2; do
	case "$command" in
	'' | '#'* | record_sound | record) ;;
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
		# red in all of them. The corridor's ~(38, 23, 40). Not bright: a glitch-check build's
		# loading screen is the magenta back colour (255, 0, 255), red ahead of green too. At most
		# 40 s, then carry on anyway
		t=0
		while [ "$t" -lt 80 ]; do
			import -window "$window" /tmp/boot.png
			rgb=$(convert /tmp/boot.png -resize '1x1!' -format '%[fx:int(255*r)] %[fx:int(255*g)]' info:)
			r=${rgb% *}
			g=${rgb#* }
			[ "$r" -gt 25 ] && [ "$r" -lt 120 ] && [ $((g + 8)) -lt "$r" ] && break
			sleep 0.4
			t=$((t + 1))
		done
		echo "up after ~$((t * 55 / 100)) s of polling"
		sleep 0.3
		;;
	boot_at) ;; # run_e2e.ps1 builds the game standing on that tower (BOOT_AT_TOWER): nothing to do here
	bench) ;; # run_e2e.ps1 builds the game as the benchmark (BENCH, src/game/bench.h): nothing to do here
	wait_verdict)
		# The benchmark's verdict (src/game/bench.h): the whole screen turns green (pass) or red
		# (fail) once every view's run. Until then the game's own colours: never that green or red
		# over the whole picture. Polls for at most $arg1 seconds (each view takes ~5 s at 60 fps,
		# longer when it's slow), then shoots it as bench_verdict and exits 0 (pass), 2 (fail) or
		# 3 (no verdict in time)
		verdict=none
		start=$(date +%s)
		while [ $(($(date +%s) - start)) -lt "${arg1:-300}" ]; do
			import -window "$window" /tmp/verdict.png
			rgb=$(convert /tmp/verdict.png -resize '1x1!' -format '%[fx:int(255*r)] %[fx:int(255*g)] %[fx:int(255*b)]' info:)
			set -- $rgb
			# Green or red well ahead of the other two (the text and the black border pull the
			# average down: the green read ~(20, 85, 35)). The game's cyan floor has as much blue as
			# green, its purples as much blue as red: neither passes for these
			[ "$2" -gt $(($1 + 40)) ] && [ "$2" -gt $(($3 + 30)) ] && { verdict=pass; break; }
			[ "$1" -gt $(($2 + 50)) ] && [ "$1" -gt $(($3 + 40)) ] && { verdict=fail; break; }
			sleep 2
		done
		sleep 1 # the lines are printed with the colour: a frame or two to be sure
		import -window "$window" /out/bench_verdict.png
		echo "verdict $verdict after $(($(date +%s) - start)) s (shot bench_verdict)"
		case "$verdict" in pass) exit 0 ;; fail) exit 2 ;; *) exit 3 ;; esac
		;;
	skip_prologue) jump_scenes "$(presses_to SCENE_PLAY)" ;;
	start_intro) jump_scenes "$(presses_to SCENE_INTRO)" ;;
	hold) k=$(key_for "$arg1"); xdotool keydown "$k"; sleep "$arg2"; xdotool keyup "$k" ;;
	keydown) xdotool keydown "$(key_for "$arg1")" ;;
	keyup) xdotool keyup "$(key_for "$arg1")" ;;
	flashes_from) flashes_start=$(frames_now) ;;
	flashes_to)
		python3 /game/tests/e2e/glitch.py flashes /out/film.mov "$flashes_start" "$(frames_now)" /out "${arg1:-film}" || glitch_failed=1
		;;
	holes_from) holes_start=$(frames_now) ;;
	holes_to)
		python3 /game/tests/e2e/glitch.py holes /out/film.mov "$holes_start" "$(frames_now)" /out "${arg1:-film}" || glitch_failed=1
		;;
	shot) import -window "$window" "/out/$arg1.png"; echo "shot $arg1" ;;
	# Mednafen's save state (F5, slot 0) copied to /out/NAME.mc0: the whole machine at that moment,
	# Work RAM included. For jo's PC sampling profiler (JO_TEST_PCPROF_FRAME: its histograms are
	# at the top of Low Work RAM; tools/pcprof_report.py reads them back from the state)
	savestate)
		find / -xdev -name '*.mc0' -delete 2>/dev/null
		xdotool windowfocus --sync "$window"
		xdotool key F5 # (SDL reads only the focused window: no --window)
		sleep 3
		state=$(find / -xdev -name '*.mc0' 2>/dev/null | head -1)
		[ -n "$state" ] && cp "$state" "/out/$arg1.mc0" && echo "savestate $arg1 ($state)" ||
			{ echo "savestate: no state written (mednafen.log:)" >&2; tail -5 /out/mednafen.log >&2; }
		;;
	*) echo "unknown step: $command" >&2; exit 1 ;;
	esac
done <"$1"
# A glitch check found a fault (above): the film fails, after every step has run
[ "$glitch_failed" -eq 0 ] || { echo "glitch checks: FAILED" >&2; exit 4; }
