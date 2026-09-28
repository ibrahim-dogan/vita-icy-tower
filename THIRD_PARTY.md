# Third-party code and references

## icytower-ng (MIT)

The gameplay presentation (animation frames and timings, eye candy, the custom
random generator, the background generation) was written following icytower-ng.

BSD 3-Clause License

Copyright (c) 2025, Roy Eldar

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## replay_checker by RaMMicHaeL

The engine in src/core.c follows the reverse engineered Icy Tower 1.3.1 engine
published as open source in replay_checker:
https://ramensoftware.com/revealing-the-secrets-of-icy-tower-v1-3-1

## Allegro 4

src/datafile.c (datafile encryption and LZSS) and pal_make_light() in src/gfx.c
re-implement algorithms of the Allegro 4 game library (giftware license).

## LZMA SDK decoder by Igor Pavlov (public domain), third_party/lzma/

## stb_vorbis (public domain / MIT), third_party/stb/

## font8x8 (public domain), third_party/font8x8_basic.h
