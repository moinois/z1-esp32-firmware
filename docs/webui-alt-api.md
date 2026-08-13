# `webui-alt` expected API

This document records the API expected by the ignored `webui-alt` frontend
extracted from its bundled JavaScript. It describes client expectations only;
it is not a normative addition to the public firmware specification.

## Origin and ports

The frontend uses the page hostname and preserves HTTP/HTTPS for the WebSocket
scheme:

| Purpose | URL constructed by the frontend | Observed firmware endpoint |
|---|---|---|
| Main page and static assets | `http(s)://HOST/` | Port 80, available |
| Machine/control WebSocket | `ws(s)://HOST:81/ws` | Port 81, currently unavailable |
| Stream endpoint | `http(s)://HOST:82/stream` | Port 82, `/stream` currently returns 404 |
| Video WebSocket | `ws(s)://HOST:82/ws_video` | Port 82, available and returns `101 Switching Protocols` |

The port-81 URL is not a fallback: it is constructed explicitly and is used by
the main control connection. A client therefore cannot operate fully against a
firmware image that exposes only port 80 and port 82.

## Port 81 control WebSocket

The frontend opens `GET /ws` with a WebSocket upgrade and reconnects
automatically up to five times, with a three-second interval. It enables a
heartbeat and sends the JSON text message:

```json
{"ReqData":true}
```

The bundled code also uses the connection for machine/control messages and
expects text messages that it parses as JSON. The exact command vocabulary is
runtime-driven by the UI and should be captured from browser network logs before
implementing a compatibility adapter. The current firmware does not expose
this listener, so those control requests cannot be tested against the target.

## Port 82 stream endpoint

The frontend constructs a normal HTTP URL to `/stream`. It expects a successful
HTTP response containing stream data. The current firmware returns `404 Not
Found` for this path; therefore this part of the alternative UI is currently
incompatible.

## Port 82 video WebSocket

The frontend opens `GET /ws_video` with a WebSocket upgrade. On connection it
sends the text command:

```text
start_stream
```

It expects binary WebSocket messages, treats each binary payload as a video
image/blob, and creates an object URL for display. The frontend enables
automatic reconnect (five attempts, three-second interval) and does not enable
heartbeat on this video connection.

The current firmware accepts the handshake on port 82 and therefore satisfies
the transport-level part of this expectation. Whether the returned binary
frames match the Maker frontend's image format requires a camera/recording
fixture and is not established by the handshake alone.

## Static resources

The alternative image contains an index page, JavaScript bundle, CSS, favicon,
manifest, fonts, audio files, and PNG assets. Relative resource URLs are
resolved from port 80. The firmware's static-file server must preserve the
resource paths and suitable MIME types.

## Compatibility status

The `--alt_webui` image was built and installed through `/update` and
`/updateffs`. Direct endpoint checks showed:

- port 80 main page: HTTP 200;
- port 80 firmware API: HTTP 200;
- port 81: connection refused;
- port 82 `/stream`: HTTP 404;
- port 82 `/ws_video`: WebSocket 101.

Consequently, the remaining compatibility gaps are the absent port-81 control
WebSocket and the missing port-82 `/stream` route. These findings do not alter
the normative specification or the default `webui` build.
