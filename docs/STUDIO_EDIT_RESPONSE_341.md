# Studio clip editing response (#341)

Paused ordinary audio clip edits publish a prepared voice list through the engine transition mailbox. The callback only exchanges vectors; outgoing PCM and ReadAhead ownership stays in the transition until control-thread acknowledgement. No plugin state capture, DSP/PDC/worker preparation or device restart is required without Process Buffer. A queued Play consumes the accepted voice list before transport controls.

Process Buffer requires producer/device quiescence, then swaps only voices while retaining prepared DSP and buffers. Failed restart restores voices and the ProjectStore transaction. ARA, performance transitions and recording retain their existing guarded paths. Structural channel changes still prepare routing, retaining unchanged insert chains.

NativeInsert opaque byte states share immutable storage between model snapshots. Mutation replaces storage and cannot alter older Undo snapshots. Persistence schema and encoded bytes remain unchanged. Strip painting and insert labels retain a project snapshot and read references instead of copying Track/insert vectors.

Ready stretch results retain an LRU of up to 64 entries / 512 MiB including current requests, so Delete/Undo can reuse prepared audio. Private cache directories retire on a background worker; current voices retain their storage until safe acknowledgement.

`desktop_clip_response` verifies immutable snapshot isolation, serialization, real callback PCM adoption, queued Play, concurrent cancellation, zero callback allocation/free, plugin identity through clip edits/Undo, rapid Delete/Undo and buffered restart rollback. Existing stretch model/cache/desktop tests cover cache eviction/cancellation and prepared playback. Windows profile includes these tests and GUI/ARA regressions.

Synthetic Linux measurements against 4fd0e445 (8 channels, native inserts, ordinary clips): Delete/move/trim/split/fades p50 0.03–0.08 ms with zero device stops and unchanged insert generation. With 8 × 8 MiB opaque plugin states, history classification p50 fell from 621 ms to 0.004 ms and disconnected Application Undo from 738 ms to 0.012 ms. These isolate code costs; they do not certify physical ASIO or vendor plugin latency. User-machine acceptance is still required.
