# Headless Saturn emulator for end-to-end checks: Mednafen on a virtual X screen (Xvfb),
# driven with xdotool. The BIOS is copyrighted, so it's mounted at run time, never baked in.
FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends mednafen xvfb xdotool x11-apps imagemagick \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
