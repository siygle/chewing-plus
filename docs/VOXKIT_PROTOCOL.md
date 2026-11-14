# voxkit socket protocol, version 1

Normative spec for the Unix-socket protocol between an input method and the
voxkit dictation daemon. It is the protocol already implemented by
[fcitx5-voxkit](https://github.com/siygle/fcitx5-voxkit) `main`:

- daemon: `daemon/voxkit/src/protocol.rs`, `server.rs`
- fcitx5 client: `module/VoiceClient.cpp`

This tree does not change that daemon or the fcitx5 chewing addon. The addon
still talks to the fcitx5 voxkit **module** (`voxkit_public.h`). The module
is the socket client. IBus and the macOS app will speak this spec directly
so they can share one daemon with fcitx5.

`include/voxkit_protocol.h` is a dependency-free C view of the same version.
`VOXKIT_PROTO_VERSION` is 1.

## Transport

- `AF_UNIX` `SOCK_STREAM`.
- One UTF-8 JSON object per line, terminated by `\n`. No length prefix.
- Unknown object fields are ignored.
- `v` greater than 1 is rejected by the daemon (`unsupported protocol version`).
  Clients that follow this spec send `"v":1`. The current daemon also accepts
  a missing `v` (it decodes as 0). Do not send any other number.
- The daemon binds the socket mode `0600`. If a stale socket file is not
  connectable it is removed and rebound. A live listener is left alone.
- The client does not send audio. The daemon opens the microphone.

Socket path, first match:

1. `VOXKIT_SOCKET`
2. `$XDG_RUNTIME_DIR/voxkit.sock`
3. `/tmp/voxkit-<uid>.sock`

One connection has one worker. A second `start` on that connection drops the
recording in progress and begins another. `req` is chosen by the client and
copied onto every event for that turn.

## Client to daemon

`language` defaults to `zh` when omitted or empty. `convert` defaults to
`none`. Any value other than `s2twp` is treated as no conversion. `s2twp` asks
the daemon to run OpenCC (Taiwan phrasing) on the final text; if OpenCC is
missing the original text is returned and the daemon logs a warning.

`model` is optional. Empty or omitted keeps the daemon's current model
(`VOXKIT_MODEL`, otherwise the daemon default). A non-empty unknown id fails
the turn with `bad-model`.

`start`'s `vocabulary` replaces the connection's list when it is non-empty.
An empty array keeps whatever `set_vocabulary` or a previous `start` stored.

```json
{"v":1,"type":"start","req":1,"language":"zh","convert":"s2twp","model":"qwen3-asr-0.6b","vocabulary":["小麥注音"]}
{"v":1,"type":"stop","req":1}
{"v":1,"type":"cancel","req":1}
{"v":1,"type":"set_vocabulary","req":2,"vocabulary":["小麥注音"]}
{"v":1,"type":"ping","req":4}
```

## Daemon to client

```json
{"v":1,"type":"ready","req":1}
{"v":1,"type":"partial","req":1,"stable":"","tentative":"聆聽中"}
{"v":1,"type":"level","req":1,"rms_dbfs":-22.0}
{"v":1,"type":"partial","req":1,"stable":"","tentative":"辨識中"}
{"v":1,"type":"final","req":1,"text":"你好"}
{"v":1,"type":"discarded","req":1,"reason":"no_speech"}
{"v":1,"type":"error","req":1,"code":"model-not-ready","message":"run: voxkit model install"}
{"v":1,"type":"pong","req":4}
```

`partial` is a status string today, not a streaming transcript. `stable` is
reserved and currently empty. `level` is the microphone RMS in dBFS while
recording.

`error.code` values the daemon sends:

| code | when |
|---|---|
| `bad-request` | the line is not a version-1 command |
| `bad-model` | `model` is not an ASR id the daemon knows |
| `model-not-ready` | that model's file is not installed |
| `no-input` | the microphone could not be opened |
| `not-recording` | `stop` with no matching `start` |
| `transcribe-failed` | the recognizer returned an error |

The fcitx5 client also synthesizes `disconnected` locally when the socket
closes. That code is not sent by the daemon.

## Turn

1. Client sends `start`.
2. Daemon checks the model file and opens the microphone. On success it sends
   `ready`, then `partial` with `tentative` `聆聽中`, then `level` events
   (about every 200 ms, over the last 1600 samples). On failure it sends
   `error` and records nothing.
3. Client sends `stop` with the same `req`. The daemon keeps capturing for
   **120 ms** (`TAIL` in `server.rs`), then recognizes. Success is `final`.
   No speech after the VAD is `discarded` / `no_speech`. A recognizer failure
   is `error` / `transcribe-failed` (or `model-not-ready` when the message
   says the model is not installed).
4. `cancel` with the same `req` drops the capture. An in-flight recognizer
   run is asked to abort. The daemon does not send `final` for that `req`.

`ping` is answered with `pong` and does not touch the microphone.

## What this version does not promise

- No binary audio framing. A future version would bump `v`.
- No streaming word-level `partial`. Frontends must tolerate the two status
  strings above.
- The daemon README in fcitx5-voxkit still mentions a 200 ms tail and a 60 s
  cap in places. `server.rs` uses 120 ms and does not enforce that cap.
  This document follows the code.
- Model weights, Silero, and OpenCC are daemon concerns. See
  [CROSS_PLATFORM.md](CROSS_PLATFORM.md). Default model and
  `VOXKIT_MODEL` belong to the daemon's service file (the shipped user unit
  sets `qwen3-asr-0.6b`), not to this protocol.

## Compatibility

A version-1 client works with the current fcitx5-voxkit daemon without a
daemon release. The chewing addon is unchanged: push-to-talk stays inside
the fcitx5 module, which already emits these messages. IBus and macOS must
send `v` 1 and the field names above; they must not require fields the
daemon does not send.
