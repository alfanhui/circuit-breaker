JO_COMPILE_WITH_VIDEO_MODULE = 0
JO_COMPILE_WITH_BACKUP_MODULE = 0
JO_COMPILE_WITH_TGA_MODULE = 1
JO_COMPILE_WITH_AUDIO_MODULE = 0
JO_COMPILE_WITH_3D_MODULE = 1
JO_COMPILE_WITH_PSEUDO_MODE7_MODULE = 0
JO_COMPILE_WITH_EFFECTS_MODULE = 0
JO_PSEUDO_SATURN_KAI_SUPPORT = 1
JO_COMPILE_WITH_DUAL_CPU_MODULE = 0
JO_DEBUG = 1
JO_FRAMERATE = 1
# 60Hz NTSC output: the target console is a Japanese Saturn (0 = 50Hz PAL)
JO_NTSC = 1
JO_COMPILE_USING_SGL=1
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
	src/floor_fade_vdp2.c \
	src/object_fog_vdp1.c \
	src/view.c \
	src/input.c \
	src/level.c \
	src/trail_render.c \
	src/hud.c
JO_ENGINE_SRC_DIR=../../jo_engine
COMPILER_DIR=../../Compiler
include $(COMPILER_DIR)/COMMON/jo_engine_makefile
