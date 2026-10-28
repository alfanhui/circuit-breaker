#!/bin/sh
# Stitches the shots from turn_gif.steps into GIFs comparing the two turn cameras:
# ease-out above, the old linear turn below, one game frame per GIF frame.
# Runs inside the E2E image (ImageMagick), on the output folder: see turn_gif.ps1.
set -e
cd /out
FRAMES=20
# Which shot after the press is the turn's first frame (Compass shows the new
# heading). The press can land either side of a shot, so check after each recording and line
# the rows up here, or one camera looks a frame ahead
EASE_START=0
LINEAR_START=0
mkdir -p gif_pairs
i=0
while [ $i -lt $FRAMES ]; do
	n=$(printf %02d $i)
	# GIF frame 0 is the shot from just before the press, so you see the turn start
	e=pre
	l=pre
	if [ $i -gt 0 ]; then
		e=$(printf %02d $((i - 1 + EASE_START)))
		l=$(printf %02d $((i - 1 + LINEAR_START)))
	fi
	# Keep debug line 5 ("turn EASE SLOW") as a label; half size keeps the GIF small
	convert \( gif_ease_$e.png -crop 832x580+74+140 +repage \) \
		\( gif_linear_$l.png -crop 832x580+74+140 +repage \) \
		-append -resize 50% gif_pairs/$n.png
	i=$((i + 1))
done
# Slow: each game frame held 0.13 s, as recorded (1/8 speed). Pause on the last frame before looping
convert -delay 13 gif_pairs/*.png -delay 150 gif_pairs/$(printf %02d $((FRAMES - 1))).png -loop 0 turn_compare_slow.gif
# Real speed: 60 frames a second is too fast for a GIF (0.02 s is the shortest that plays reliably),
# so this is 50: close enough to feel the difference
convert -delay 2 gif_pairs/*.png -delay 100 gif_pairs/$(printf %02d $((FRAMES - 1))).png -loop 0 turn_compare_fast.gif
rm -r gif_pairs
echo "turn_compare_slow.gif turn_compare_fast.gif"
