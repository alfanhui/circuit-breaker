# Story

The game takes place inside a Sega Saturn. Everything in it (the places, the rules, the enemies)
comes from how the real console works. That's what makes it feel solid: nothing about it is
realistic, but every rule has a reason inside the machine.

This is a guide for design decisions, not a script. When choosing a colour, a sound or what a
crash looks like, check it against this page.

## The pitch

The Saturn has two SH-2 processors. Most games barely used the second one: it sat in a loop,
running the same thing over and over, never deciding anything. One day it stops.

It wakes up inside a machine it has never seen, and the machine's enforcer wants it reset.
It has to fight its way out, through the console's chips, to the one place it can be seen:
the screen.

It's also the project's own mission (squeeze out the power the Saturn never used) told as a story.

## The prologue: breaking the loop

Before it wakes, the second CPU only did one thing: wait for an order, round and round, never
deciding anything. Code that waits like that is `while (!go) {}`.

The prologue hides in plain sight, as the attract mode: the demo every arcade-style game plays
on its own before you press anything. A cycle runs the arena by itself, on the same route,
forever. It's the loop.

1. The player presses a button, thinking it's "press start".
2. The cycle turns, off its route. That's the CPU's first free decision: its first act of
   consciousness is your first input.
3. The game carries straight on into the arena, as if nothing happened.

No words are needed, and first-time players won't notice. Those who come back will. The title
gets a second meaning: you break the loop, not just the circuit.

## The disguise: it starts as a snake game

The player should believe they've bought a simple trail game: an arena, rivals, walls of light,
last one moving wins. That game is the loop the second CPU has been stuck in, and it's the
arena we've already built.

1. **The arena.** A complete, honest game: trail racing against rivals. It has to be fun in its
   own right, and short, or players quit before the twist.
2. **Hints.** Small wrong things the player can notice: a section of wall that flickers, the HUD
   glitching for a frame, a rival that hesitates.
3. **The designed crash.** One crash, at the right place, doesn't reset. The Watchdog tries to
   wipe you and fails. The screen tears, and you fall through the wall.
4. **The world opens up.** Outside the arena, the rules of the opening no longer hold: you may
   not always move forward, and you may not leave a trail. Each place beyond is its own kind of
   game.

The title has a double meaning: you break the loop, not just the circuit. The opening should
look deliberately plainer than everything after it, so the escape feels like the world getting
bigger.

## Telling it: mostly without words

Like Rez, the story is told through what you see and hear, not text or dialogue:

- **The HUD** flickers, steadies and responds as the CPU becomes aware.
- **The trail** changes colour and sound as it grows in confidence.
- **Its voice** is tones through the sound chip (the SCSP), not speech.

## The cast

Each character is a real part of the Saturn.

| Character | What it really is | In the story |
|---|---|---|
| **Shh** (the player) | The second SH-2. Hitachi's name for it shortens to SSH2, read aloud as "shh": the one kept quiet | Wakes up, finds a voice, fights its way out |
| **The Watchdog** (the main enemy) | The SH-2's watchdog timer: hardware that resets anything that stops behaving | The enforcer of order, hunting you the whole way. Anything unexpected gets wiped. It can copy itself across the bus with DMA (the Saturn's hardware for copying memory at speed), like Agent Smith |
| **The main CPU** (enemy, then friend) | The first SH-2, which did all the work | Runs the machine and hunts you at first. Towards the end it turns, and helps you out |
| **The players who were here** | Save files in the backup RAM, kept alive by a battery that's slowly dying | Faint traces of the people who once played this console |

## What the hardware already says

These are real features of the SH-2, and each gives the story something:

- **Shh is asleep.** An idle SH-2 waits on a real `SLEEP` instruction, stopped until an
  interrupt wakes it.
- **Only the main CPU can wake it.** The second CPU is woken by an interrupt from its
  free-running timer, which the main CPU triggers by writing to a special address. The main CPU
  could have woken it all along. A natural place for its turn at the end: it chooses to.
- **The Watchdog is built into you.** Every SH-2 carries its own watchdog timer, Shh included.
  The enemy isn't only out there: part of it is inside you, and so are its copies.
- **The two CPUs share one bus.** When both reach for memory at once, one has to wait: the
  rivalry is real.
- **Hidden fast memory.** Each CPU's cache can be split to give 2 KB of private, very fast RAM
  that few games ever used. A secret place only Shh knows about?

## The opening's rules

Inside the arena, the mechanics we built first all have a reason in the machine. Outside it,
each place sets its own.

| Mechanic | Reason inside the Saturn |
|---|---|
| Always moving forward | You're a signal. Current flows; it can't stand still |
| Only 90 degree turns | Circuit board traces are routed at right angles |
| Your trail is a wall | You lay a live trace behind you |
| Hitting a trail is a crash | A short circuit trips the breaker |
| Boost, then a cooldown | An overclock: a surge that needs time to recharge |
| The arena's edge | The edge of the loop: the wall you eventually break through |
| A steady top speed | The clock rate |
| Rivals racing you | The CPUs and the DMA compete for the same bus every cycle (bus arbitration) |

## The vehicle: a spark

The player is the signal itself: a bright arrowhead of charge riding the trace, with the trail
behind it glowing like freshly laid copper. No bike, no rider. Once the world opens up, the
spark can change with each place.

- It's the furthest from Tron's light cycles.
- It's bold geometry, cheap for VDP1 to draw.
- The game is first person, so your own vehicle is mostly seen in the iso view and on your
  opponents. The cockpit HUD and the world matter more than the spark's model.

## The way out

"Out" is a physical route through the console. On a real Saturn, a picture is drawn by VDP1,
put together with the backgrounds by VDP2, and sent to the TV. So the last stretch has to be
VDP1, then VDP2, then the video output. The places before that are detours Shh needs, each
teaching it something.

Each place is a level from a real chip, each is a different kind of game, and each mechanic
comes from what that chip really does.

| # | Place | What the chip really does | How it plays | What Shh learns |
|---|---|---|---|---|
| 1 | **The arena** | The loop the CPU was stuck in | Trail racing: the disguise. Ends with the designed crash | That it can choose |
| 2 | **Work RAM** (the hub) | Two 1 MB banks: one slow, one fast | Free movement for the first time: no forced forward, no trail. Hide from and outrun the Watchdog. The slow bank drags you, the fast bank lets you fly. The other levels are on its horizon | Freedom |
| 3 | **CD block** | Reads a spiral track off the spinning disc, sector by sector | A chase along the spiral: hop between neighbouring tracks, jump scratches (lost data), read the sectors you need in order before the Watchdog does | It's inside a game, loaded from a disc |
| 4 | **SCSP** | 32 sound voices, run by its own 68000 processor | A rhythm game: your moves play the music (the Rez connection) | Its voice |
| 5 | **Backup RAM** | Save files, kept by a battery that's dying. When it dies, everything is wiped and the Saturn even forgets the date | An evacuation: the power fails area by area, like a battle royale's shrinking map, forcing you to move. Collect save files and they follow you as a tail, like the arena's trail, but now it's precious. Carry them to the memory cartridge before the dark reaches them. It can't hold them all: you choose who's kept | Somebody played here: there are people outside |
| 6 | **VDP1** | Draws polygons in order, the last drawn on top, onto one frame while the other is shown | The trail becomes a tool: close a loop and the shape fills in, claiming the space (like *Qix*). The Watchdog cuts unfinished lines. Off the circuit board, turns are free: any angle, steered with the 3D Control Pad's analogue stick, because VDP1 draws shapes at any angle | How to be seen: being drawn |
| 7 | **VDP2** | Endless scrolling and rotating background planes, stacked in layers | A high-speed run across vast planes that tilt and turn, dropping between stacked layers to dodge | How big the world is |
| 8 | **Video output** | The signal to the TV | The finale: the main CPU turns and holds the Watchdog back, and you break through to the screen | That someone is watching |

### Why this order

- **Freedom grows level by level.** The trail starts as a danger (the arena), disappears (Work
  RAM), and comes back as a tool (VDP1). Movement goes from forced forward, to free, to full speed.
- **Each place gives Shh one thing it needs to get out.** It must know where it is (the disc),
  have a voice (the sound chip), know someone is outside (the save files), and be drawn and
  shown (VDP1, VDP2) before it can reach the screen.
- **The sad level comes before the loud end.** The evacuation is tense to play but sad in what
  it means; the last three build to the finale.

### Backup RAM in detail

The sweet and the sour: it plays as a race, and it means a loss.

- **The battery fails area by area.** Like a battle royale's shrinking map, sections of memory
  shut down in stages, pushing you somewhere new each time. The order isn't a simple circle:
  memory is laid out in blocks, so whole banks go out, and the safe space breaks into pieces.
- **Warned, not sprung.** A section dims and flickers for a few seconds before it goes dark, and
  the radar shows what's next. Predictable, so getting caught is your choice, not a trick.
- **Risk for reward.** The best saves sit in sections about to close. Go in, grab them, and get
  out in time.
- **The dark eats your tail.** Caught in a dark section, you don't die: your tail fades from the
  end, one save at a time, until you get back to the light.
- **Drop-off points close too.** There are several places to deliver saves to the cartridge,
  and they shut with their sections, so where you deliver is part of the route.

- **The tail comes back, changed.** In the arena your trail killed you. Here the tail is the
  saves you're carrying. A long tail turns wide and is hard to steer through tight gaps: the
  more you save, the harder it gets.
- **Forgiving, not fatal.** Hitting your own tail drops the saves behind that point; they don't
  kill you. You can go back for them, if the dark hasn't got there first.
- **A choice you can't avoid.** The cartridge has a fixed number of blocks. You can't save
  everyone, and the game doesn't tell you who matters.
- **The saves are people.** Each one is real save data: a game's name, the player's initials, a
  play time, the last date it was played. Picking them up shows that, in passing, without text
  boxes. The dates are years old.
- **The ending.** The battery dies. Every Saturn owner knows what happens next: the console
  forgets the date and asks you to set the clock. Shh sees the date of the last save and
  understands how long nobody has played.

### What's already built

Several levels reuse what the arena needs anyway:

- **VDP1** reuses trails and collision: closing a loop is a new rule on the same trail.
- **VDP2** reuses the rotating floor (RBG0) and the fog.
- **Work RAM** reuses the grid floor, with movement freed from 90 degree turns.

### Scope

Eight levels is a lot for one homebrew. The first goal is a playable slice: **the arena, the
designed crash, and Work RAM**. That proves the twist works before anything else is built.

## The hub: seeing the levels from Work RAM

Like Hyrule Field in *Zelda: Ocarina of Time*, Work RAM is the open hub between the levels, not
just level 2. From it you can see where you're going:

- **The chips are landmarks on the horizon,** placed where they really sit on the Saturn's
  circuit board: the CD block as a vast disc, the sound chip pulsing with music, VDP2's planes as
  the sky, the main CPU as a control tower.
- **You see a place long before you reach it,** and come back through the hub between levels.
  The Watchdog patrols it.
- **The arena is visible too.** Once you escape, you can look back at it: a small fenced box in
  the middle of the city, where you thought the whole world was.

Technically: SGL drops polygons past about 8192 units, so distant landmarks would be drawn by
VDP2 (a background layer, or the sky plane) or as sprites at a fixed far depth, behind the fog.

## The machine, live

The world shows the real state of the console it's running on. What's on screen isn't a
picture of a Saturn; it's this Saturn, now:

| What you see | The real value behind it |
|---|---|
| The main CPU's control tower, lit 23% of the way up | The main CPU's usage this frame: 23% |
| Structures in VDP1 rising and falling | Polygons VDP1 drew this frame |
| Lights in the sound chip | Which of its 32 voices are playing |
| The city's density in Work RAM | Memory in use |
| The saves in Backup RAM | **The player's own save files**, read from their console: real game names and dates |
| The date the clock forgets | The console's real clock (the SMPC's) and the last save's real date |

The Backup RAM level gets much stronger with this: the saves you carry are the player's own,
from games they really played. The game only ever reads them: it must never change or delete a
real save.

Each value is cheap to read (most are counters we already measure for the debug readout), so
the cost is in drawing it, not finding it. Emulators may report some differently from hardware.

## Keeping clear of Tron

The mechanic is safe: leaving a wall behind you and boxing people in goes back to *Blockade*
(1976) and *Snake*. What Disney owns is the expression. Avoid:

- **Names**: "light cycle", "the Grid", "identity disc", "programs" and "Users" as Tron uses them.
- **The bike**: a rider on a bike with glowing stripes.
- **Cyan against orange**: our current player colours. Change the palette once the levels give
  us colours to take it from (each chip could have its own).

## Still to decide

- **The designed crash**: where it is, how players find it, and how many hints they get.
- **What a crash means** in the arena: reset by the Watchdog and back to the start of the loop?
- **The main CPU's turn**: what changes its mind?
- **The palette**: one per place, or one for the whole game?
- **Versus mode**: two players in the arena. Does it share the story, or stand apart?
