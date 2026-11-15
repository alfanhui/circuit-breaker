# The disc's header (IP.BIN), which a Saturn's CD player and emulators show: jo-engine 2026 writes it
# from these (Compiler/COMMON/jo_engine_makefile has the defaults and what each field holds). The
# maker ID stays jo's own, JO-ENGINE: the game is built on it. Peripherals J: the control pad only
JO_GAME_TITLE = CIRCUITBREAKER
JO_GAME_VERSION = V0.100
JO_GAME_RELEASE_DATE = 20261109
JO_GAME_PERIPHERALS = J
JO_COMPILE_WITH_VIDEO_MODULE = 0
JO_COMPILE_WITH_BACKUP_MODULE = 0
JO_COMPILE_WITH_TGA_MODULE = 1
JO_COMPILE_WITH_AUDIO_MODULE = 1
JO_COMPILE_WITH_3D_MODULE = 1
JO_COMPILE_WITH_PSEUDO_MODE7_MODULE = 0
JO_COMPILE_WITH_EFFECTS_MODULE = 0
JO_PSEUDO_SATURN_KAI_SUPPORT = 1
# Both SH-2s: the slave sorts and sends jo's VDP1 commands, and draws the corridor's floor and
# boxes (src/slave_draw.h) while the master builds the rest. jo-engine 2026 does all of it on the
# master otherwise: on foot that was ~40% of a frame more than under SGL, which shared it out
JO_COMPILE_WITH_DUAL_CPU_MODULE = 1
# 1 for jo's profiler: the debug line's VDP1 pixels by part (row 9, utils/perf.h) and the benchmark's
# VDP1 drawing time. It costs ~2 us a polygon, so 0 to play (and to commit)
JO_COMPILE_WITH_PROFILER_MODULE = 0
JO_DEBUG = 1
# 60 fps, VDP1's frame buffers swapped at every VBlank, and VDP1 erases the one it draws into
# itself, free. A frame VDP1 can't finish in 1/60 s is shown with its nearest commands missing (it
# draws far to near): the debug line's "vdp1 over" counts them, 0 in every film of play
# (2026-11-08). JO_FRAMERATE_DYNAMIC = 1 swaps only finished frames (slowing down instead, as
# SGL did), but jo then clears each frame with a whole-screen polygon: ~4 ms of VDP1's 16.7, and
# on foot that made VDP1 the limit (48-52 fps in the yard, 60 now). BACKLOG: both, in jo
# Tried again 2026-11-15 with jo v2026.3 (docs/BACKLOG.md, SG-2), which erases in the VBlank in
# place of most of that polygon (johannes-fetz/joengine#107): still slower. With the manual change
# VDP1 erases during the VBlank and starts drawing only after it, ~2.4 ms of its 16.7 lost a field,
# so where it's near full it misses every change: the benchmark (tests/e2e/bench.steps) went from 2
# views failing to 6, the CD block's hub 60 fps to 30 with 25 polygons (hub.steps). A view that's
# late with this on has under ~2.4 ms of VDP1 to spare: set it to 1 to find those
JO_FRAMERATE_DYNAMIC = 0
JO_FRAMERATE = 1
# 60Hz NTSC output: the target console is a Japanese Saturn (0 = 50Hz PAL)
JO_NTSC = 1
# jo's malloc heap starts as a static array in our BSS (352 KB). jo-engine 2026 (no SGL) also
# gives jo_malloc whatever high Work RAM the program and jo's own work area leave free, and main.c
# adds low Work RAM behind that. (Under SGL this size was a limit: the array had to end before
# SGL's sort list at 0x060C0000)
JO_GLOBAL_MEMORY_SIZE_FOR_MALLOC = 360448
SRCS=main.c \
	src/utils/3d.c \
	src/utils/low_level_input.c \
	src/utils/perf.c \
	src/utils/static_mesh.c \
	src/game/cycle.c \
	src/game/trail.c \
	src/game/press_latch.c \
	src/game/arena.c \
	src/game/trail_pieces.c \
	src/game/floor_fade.c \
	src/game/view_cull.c \
	src/game/wall_clip.c \
	src/game/turn_ease.c \
	src/game/depth_fog.c \
	src/game/turn_gate.c \
	src/game/collision.c \
	src/game/minimap.c \
	src/game/rider.c \
	src/game/bot.c \
	src/game/trail_occlusion.c \
	src/game/game_state.c \
	src/game/camera_shake.c \
	src/game/status_message.c \
	src/game/attract_route.c \
	src/game/attract_text.c \
	src/game/loop_break.c \
	src/game/intro_flight.c \
	src/game/arrival_flight.c \
	src/game/surround.c \
	src/game/rounds.c \
	src/game/wall_flicker.c \
	src/game/breach.c \
	src/game/corridor.c \
	src/game/city.c \
	src/game/frustum.c \
	src/game/vdp1_gauge.c \
	src/game/occluder.c \
	src/game/skyline.c \
	src/game/lift.c \
	src/game/landmarks.c \
	src/game/city_seen.c \
	src/game/board.c \
	src/game/walker.c \
	src/game/target.c \
	src/game/body.c \
	src/game/settings.c \
	src/game/drone.c \
	src/game/bench.c \
	src/game/draw_budget.c \
	src/game/floor_hides.c \
	src/floor_fade_vdp2.c \
	src/object_fog_vdp1.c \
	src/view.c \
	src/input.c \
	src/level.c \
	src/trail_render.c \
	src/slave_draw.c \
	src/hud.c \
	src/drone_draw.c \
	src/sound.c
JO_ENGINE_SRC_DIR=../../jo_engine
COMPILER_DIR=../../Compiler
include $(COMPILER_DIR)/COMMON/jo_engine_makefile
