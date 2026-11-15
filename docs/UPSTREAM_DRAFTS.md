# jo-engine upstream: what we sent

Three drafts for `johannes-fetz/joengine`, from docs/BACKLOG.md "Stabilise graphics" (U-3, U-5,
U-6). **Posted 2026-11-15** with the user's go-ahead: U-6 as [johannes-fetz/joengine#110](https://github.com/johannes-fetz/joengine/issues/110), U-5 as [a comment on #107](https://github.com/johannes-fetz/joengine/issues/107#issuecomment-6892310486) (its "the issue we're opening" became "#110"), U-3 as [johannes-fetz/joengine#111](https://github.com/johannes-fetz/joengine/issues/111). Follow the replies there; the texts below are as sent.

**Replies (2026-11-15, the author):**
- **#110, closed: we were wrong.** Measured field by field (VDP1 drawing the frame number, NBG0
  scrolled by it, a Mednafen recording read back): in the 1.2 / 0.5 phase VDP1 shows every image
  for exactly one field, and `jo_core_get_late_frame_count()` matches the repeated fields. Our
  proposed fix would have reported 300 late frames where the screen showed 600 images. The
  judder was VDP2: 299 repeats and 299 skips. The counters stay, now documented.
- **#111, closed, fixed in `5b1dbac`** ("VDP2: the scroll and the rotation planes of a frame shown
  with its VDP1 image (issues #110, #111)"), option 1: scroll registers and rotation parameters
  set in `jo_core_run()` callbacks are kept with the frame's number and applied when VDP1 shows
  its image; before `jo_core_run()` or in a VBLANK callback, at the next VBLANK as before. Zoom
  stays at the next VBLANK. Not covered: tables a program writes to VRAM itself (our fog's
  per-line tables).
- **#107:** no reply to our comment yet.
- **Taken in (2026-11-15):** upstream `47720e7` merged into the fork; the game's planes now use
  this frame's camera and its late count is jo's again (BACKLOG SG-3, SG-4).

Checked before writing (2026-11-15): no open or closed issue on the late count, the frame tags or
the background planes' timing (`gh search issues` for "late frame", "jo_core_get_fps", "frame tag",
"background_3d_plane", "manual change"). #107 is closed: its fix (`d5e1abc`) is merged, and
the author asked for exactly the test U-5 reports.

Tested on: jo-engine `master` at `caf0656` (v2026.3), in Mednafen 1.29.0 (the version jo's own
tests use), on the PC. **Not on a Saturn.**

| # | Kind | Title | Order to send |
|---|---|---|---|
| U-6 | New issue | `jo_core_get_fps()` and the late count miss a frame of 1 to 2 fields | First: a bug, a small fix, a repro |
| U-5 | Comment on #107 | The test you asked for: `JO_FRAMERATE_DYNAMIC` on our busiest views | Any time: a reply |
| U-3 | New issue | The VDP2 planes and the VDP1 frame they belong to | Last: a design question |

---

## U-6: new issue

**Title:** `jo_core_get_fps()` and `jo_core_get_late_frame_count()` don't see a frame of 1 to 2 fields

Hello, and thank you for the VBLANK erase, #107.

**What we saw.** In our 3D game, it judders as if at 30 fps, while
`jo_core_get_fps()` says 60 and `jo_core_get_late_frame_count()` doesn't move. Our own counter,
which compared the frame's time with a field rounded to the nearest, missed it too.

**Why, as we read `__jo_core_synch()`.** It waits until `__jo_core_last_synch_vblank + JO_FRAMERATE`.
A frame whose callbacks end at 1.2 fields finds that VBLANK already past, so it doesn't wait. The
next frame starts mid-field, ends at 1.7, and waits only to the VBLANK at 2.0. Two frames in two
fields: `fps` counts 60, and the late test (`vblank_count - last_synch_vblank > JO_FRAMERATE`) sees
a difference of 1 both times. But both frames end in the same field and none in the field before,
so a picture is shown twice: the motion is at 30.

**Reproduced** (jo `master` `caf0656`, Mednafen 1.29.0; 3D module off, otherwise the default makefile).
A VBLANK callback counts the fields in which no frame ended:

```c
#include <jo/jo.h>

#define FIELD_FRC 13982 /* FRC counts in a field (CKS 1/32, NTSC) */
#define PHASE_FRAMES 600

static volatile unsigned int frames_ended = 0, repeats = 0, fields = 0;
static unsigned int frames_seen_at_vblank = 0, frame = 0;
static unsigned int phase_fields_start, phase_repeats_start, phase_late_start;

/* A field in which no frame ended: the picture before is shown again */
static void count_fields(void)
{
    ++fields;
    if (frames_ended == frames_seen_at_vblank)
        ++repeats;
    frames_seen_at_vblank = frames_ended;
}

static void burn(const int counts)
{
    const unsigned short start = (unsigned short)jo_time_get_frc();

    while ((unsigned short)((unsigned short)jo_time_get_frc() - start) < counts)
        ;
}

static void my_draw(void)
{
    const int phase = frame < PHASE_FRAMES ? 1 : 2;

    if (frame == 0 || frame == PHASE_FRAMES)
    {
        phase_fields_start = fields;
        phase_repeats_start = repeats;
        phase_late_start = jo_core_get_late_frame_count();
    }
    /* Phase 1: 0.5 field a frame. Phase 2: 1.2 and 0.5 fields in turn */
    burn(phase == 2 && (frame & 1) ? FIELD_FRC * 12 / 10 : FIELD_FRC / 2);
    if (frame < 2 * PHASE_FRAMES)
    {
        jo_printf(1, 4 + phase * 3, "phase %d", phase);
        jo_printf(1, 5 + phase * 3, "frames %d fields %d late %d rep %d ", (int)(frame % PHASE_FRAMES) + 1,
                  (int)(fields - phase_fields_start), (int)(jo_core_get_late_frame_count() - phase_late_start),
                  (int)(repeats - phase_repeats_start));
        jo_printf(1, 6 + phase * 3, "jo_core_get_fps %d   ", jo_core_get_fps());
        ++frame;
    }
    ++frames_ended;
}

void jo_main(void)
{
    jo_core_init(JO_COLOR_Black);
    jo_core_add_vblank_callback(count_fields);
    jo_core_add_callback(my_draw);
    jo_core_run();
}
```

Result, after 600 frames of each phase:

| | frames | fields | `jo_core_get_late_frame_count()` | fields with no frame | `jo_core_get_fps()` |
|---|---|---|---|---|---|
| 0.5 field a frame | 600 | 598 | 0 | 0 | 60 |
| 1.2 and 0.5 fields in turn | 600 | 600 | 0 | **300** | 60 |

**A possible fix** (we haven't built it into jo; happy to send it as a pull request, tested with
the repro and the test ROM, if you'd like it in this form). A frame is late when its VBLANK has
already come by the time `__jo_core_synch()` starts waiting for it:

```c
    target = __jo_core_last_synch_vblank + (JO_FRAMERATE < 1 ? 1 : JO_FRAMERATE);
    late = (int)(__jo_core_vblank_count - target) >= 0;   /* its VBLANK is already past */
    while (...)
    ...
        if (late)
            ++__jo_core_late_frames;
```

`jo_core_get_fps()` could then count the pictures that really changed (a frame not late, or the
first frame after a late one), but that's a choice of meaning, so we'd rather ask than send it.

**What we did meanwhile:** our game times each frame with the FRC from one frame's start to the
next. It counts a frame late for every whole field it runs past its own, plus 1 ms of allowance.
In the view where the judder shows, it now finds 141 late frames of 192, where it found 0.

Tell us if anything here is wrong!

🤖 Generated with [Claude Code](https://claude.com/claude-code)

---

## U-5: comment on #107

Thank you for the VBLANK erase, and for the clear explanation. You asked us to try
`JO_FRAMERATE_DYNAMIC = 1` again on our busiest views, with `jo_profile_vdp1_last()`. Here it is
(jo `master` `caf0656`, Mednafen 1.29.0, `JO_COMPILE_WITH_PROFILER_MODULE = 1`).

Our game now has a benchmark build: it goes to 12 fixed views with no input, settles 90 frames
at each, then counts 192. The eight below are the ones with the most VDP1 work (first person, a
few dozen polygons, many of them large and near the camera). **V** is the longest `jo_profile_vdp1_last()` at the view, as a % of a
field (13982 FRC counts). **Late** counts frames that ran past their field. (We count those
ourselves: the issue we're opening on `jo_core_get_late_frame_count()` says why.)

| View | 1-cycle (`JO_FRAMERATE = 1`): V | late | dynamic: V | late |
|---|---|---|---|---|
| 1 | 70 | 0 | 81 | 0 |
| 2 | 83 | 0 | 94 | 0 |
| 3 (with a mirror, drawn twice) | 95 | 0 | 106 | 192 |
| 4 | 78 | 0 | 89 | 0 |
| 5 | 97 | 0 | 108 | 192 |
| 6 | 91 | 0 | 102 | 192 |
| 7 | 98 | 0 | 109 | 192 |
| 8 | 55 | 0 | 66 | 0 |

**So the erase works:** the picture's clean in the dynamic mode, with no trails in our films. But
every view's VDP1 time is about 11% of a field (~1.8 ms) longer in the dynamic mode.
That's what we'd expect if the drawing starts at the end of the VBLANK, after the VBE erase,
where in 1-cycle mode it starts at the change. Views that were at 91-98% of a field go over, and
then every frame waits a second field: 30 fps where 1-cycle mode held 60 (view 5 has ~25
polygons, most of them large faces near the camera).

So for a 60 fps game that's near VDP1's limit, 1-cycle mode stays faster for us, and we've kept
`JO_FRAMERATE = 1`. We're not asking for a change; this is the measurement you asked for. If
there's a mode that starts the drawing at VBLANK-IN when the previous frame finished in time,
we'd be glad to test it.

Two notes in fairness:
- Mednafen only. We haven't run either mode on a Saturn yet.
- Our own mistake, found while doing this: our game reset the FRC to 0 each frame for its own
  timing, which broke `jo_profile_vdp1_last()` and `jo_get_ticks()`. It now only reads the FRC;
  the figures above are after that fix.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

---

## U-3: new issue

**Title:** The VDP2 planes and the VDP1 frame they belong to: keep them together, as glows do?

Hello, a design question. We'd like to know how you'd want it before offering anything.

**What we saw.** Our floor and sky are jo's VDP2 planes (`jo_background_3d_plane_a_draw()` and
`_b_draw()`); the world is VDP1. A plane's rotation parameters take effect at the next VBLANK, but
the VDP1 picture of the same frame is shown later. With the two CPUs, the frame's table is sorted
and copied by the slave in the next frame, then drawn and shown. So when the camera pitches
quickly, the VDP2 sky plane moves a frame or two before the VDP1 polygons standing on the
horizon: for those frames a band of sky shows under polygons that should meet the floor. We
found the delay was 2 frames and now draw the planes with the camera of 2 frames ago. That's right only while every frame takes one field: after
a late frame, the delay changes for a frame or two.

**What jo already has.** The glows do exactly this (`glow.c`). Their state is kept with the
frame's number (`__jo_vdp1_frame_tag()`) and written by a VBLANK callback when that frame is shown
(`__jo_vdp1_displayed_frame_changed()`). Both are internal
(`jo_engine/internal/vdp1_command_pipeline.h`).

**Why we can't do it from outside:**
- the planes write their parameters when they're called, from the 3D matrix stack, which can't
  run in a VBLANK callback;
- the number we'd need is that of the frame VDP1 shows from the *next* field (the table taken at
  this VBLANK, `__jo_vdp1_taken_tag`), and it's a static.

**Two ways we could see:**
1. The planes keep their rotation parameters with the frame's number and write them when it's
   shown, as the glows do. Nothing would change for programs, and every game with a VDP2 floor
   would line up.
2. Public calls: the frame number being built, and the one VDP1 shows from the next field. A
   program could then pick the camera itself.

We'd be glad to write either as a pull request, with a check in the test ROM, if you tell us which
you'd prefer (or neither, if you see it differently).

**What we do meanwhile:** the planes are drawn with the camera of 2 frames ago (a ring of 2), right
while frames are on time.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
