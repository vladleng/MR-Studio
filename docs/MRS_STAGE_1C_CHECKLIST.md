# 0.1d — MRS Stage 1c: disk read-ahead
Download MR-Studio-0.1d-ASIO-Windows, extract and run MoonRiverStudio.exe.
Use WAV files matching the project/device sample rate. Small files preload;
sources above 8 MiB decoded audio use bounded background disk buffers.

1. Open a long WAV (for example 10–30 minutes at 48 kHz), connect vendor ASIO,
   then Play. Also try a WAV that previously exceeded the 256 MiB preload limit.
   Playback starts without decoding the whole source into RAM.
2. New at the matching rate; Import WAVs with several long stems. Play together:
   stems stay aligned. Waveforms build in the background and retain stereo channels.
3. Pause away from zero; seek near the middle/end, then Play. Audio matches the
   selected position. Test seek while playing too; the control thread primes disk pages.
4. Move, trim, split, delete and Undo/Redo while paused. Source offsets and hidden
   source recovery remain correct; ASIO stays connected and the paused cursor stays.
5. Save .mrsproject and reopen, reconnect audio if needed. Long external WAV
   references and edited clip bounds survive. Test the same file in two split clips.
6. Leave playback running for several minutes; resize, zoom and scroll while the
   waveform builds. Bottom status: disk underruns 0 / errors 0. ASIO underruns
   are separate. Report audible gaps or growing counters with rate/buffer/stem count.
7. During a disposable test, move/delete a streamed WAV, then seek to an uncached
   region. Expect a clear media/read-ahead error; unavailable disk samples are silence,
   with counters, and the audio callback does not wait or crash.
8. New/Open another project during background waveform generation, then exit:
   pending work cancels and shutdown completes.

Backend automated checks also cover loop wrap, page boundaries, short EOF,
independent source offsets, bounded decode and zero callback allocations.
Loop UI is a later task; this stage uses the existing shared transport loop API.

Limits: RIFF PCM16/24/32 and float32 (including supported extensible WAV), no RF64,
compressed formats or resampling. 128 total clip voices, up to 32 streamed voices
and 256 MiB of disk page storage. Eight 8192-frame pages per streamed voice;
stereo uses 512 KiB per voice. Preloaded assets retain the existing 512 MiB cap.
Waveform peak storage is capped per source; coarser bins are used for very long files.
Media remains external; changing a source file during a session is unsupported.
A disk stall may cause diagnosed silence; there is no realtime file fallback.

All six CI jobs passed (53/53 Linux/ASIO; 54/54 Windows offline). User acceptance passed on 2026-10-03. Whole #21 stays open; next is Stage 1d recording.

At most 128 retained source assets including Undo media; New/Open releases the cache.

Tested ASIO artifact: https://github.com/vladleng/MR-Studio/actions/runs/37127216892/artifacts/11274837845


## Acceptance — 2026-10-03
User confirmed all base 0.1d and UI/mono follow-up functions work. Stage 1c /
0.1d upd1 fix1 accepted; PR #45 merged into main. Validated code:
2177219103682c08bc011b9c5e9baee8f8f00aa5 (all six CI jobs passed; Linux/ASIO 54/54,
Windows offline 55/55). Whole #21 remains open. Next: Stage 1d / 0.1e recording,
monitor foundation and integrated save/load acceptance; development not started.
