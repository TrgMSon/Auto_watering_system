# BLE USB provisioning diagnostics

The host never prints keys, binary frames, arbitrary serial replies, or exception
messages. It accepts the backend key as exact UTF-8 text (1–128 bytes); a
43-character ASCII URL-safe Base64 key is supported without decoding.

## Safely inspect an existing board

1. Disconnect pump load power. Close other serial applications.
2. Upload `ble_key_usb/ble_key_usb.ino` from this revision, with full flash erase
   **disabled** and the existing partition layout preserved. Do not erase NVS.
   Only the provisioning sketch needs updating; do not change production watering
   firmware or Policy B. If the updated sketch is already installed, skip upload.
3. From the repository root, run:

   ```powershell
   python firmware/provisioning/provision_ble_key.py --port COM5 --status
   ```

4. Reset the board if requested. Check the printed STA MAC against the backend.
   Status mode never asks for a key or sends a provisioning frame. It sends only
   `BKS1`; the sketch returns one of these fixed lines:

   | Result | Meaning / next step |
   | --- | --- |
   | `STATUS missing` | No record found. Arrange normal provisioning separately. |
   | `STATUS valid` | Structure, STA MAC, and CRC are valid. Do not replace it just to diagnose. |
   | `STATUS invalid` | Record was readable but failed validation. Coordinate recovery. |
   | `STATUS unreadable` | NVS read failed, or record was empty/oversized. Coordinate recovery. |

5. Stop after status inspection. To resume watering, restore the approved Demo
   image with full erase disabled and the same partition layout. Check its
   non-secret `BLE key loaded` / `missing` / `invalid` startup message.

The status branch uses `NvsReader` (`NVS_READONLY`) and never calls the writer,
commit, erase, or replacement code. It checks only the BLE record, not the
watering ledger. Uploading any sketch is a separate operation: preserve NVS and
partition layout throughout. Status does not prove the stored key equals the
backend key, and does not return the key or a fingerprint.

## Failure categories

| Diagnostic | Meaning |
| --- | --- |
| `existing-record` | Firmware rejected a non-missing record/read error with replacement disabled; no write attempted. |
| `invalid-record` | Invalid input length/identity confirmation, cancelled replacement confirmation, or firmware record rejection. |
| `storage-or-readback-failure` | NVS write/commit or subsequent verification failed; a write may have occurred. |
| `frame-error` | Firmware rejected framing; an old sketch also rejects `BKS1` this way. |
| `frame-timeout` | Firmware frame deadline expired (10 seconds from first byte). |
| `no-response` | Identity or reply was not received before the host deadline. |
| `unexpected-response` | Reply was not an allowlisted response; raw content is suppressed. |
| `hidden-input-error` | Input cancellation, EOF, encoding error, or unavailable secure terminal input. |
| `serial-error` | Missing pyserial, port/open/read/write/flush/close failure, short write, or other transport-path exception. |

Use a native interactive terminal for hidden input. Status mode requires Python
and pyserial but does not require a key. Do not use `--replace` for diagnosis;
`--status` and `--replace` are mutually exclusive. There are no automatic retries
or automatic replacements. The existing `BKP1` provisioning format and
`OK verified` success behavior are unchanged. Updated Python diagnostics work
with older provisioning sketches; the new status request requires the updated
sketch. An old sketch may reply `ERROR frame` and require reset afterward.

Each sketch boot accepts one request. Reset before another operation. A serial
error or absent acknowledgement does not prove that a provisioning write failed;
inspect status instead of retrying blindly. The host reply timeout remains one
second. Error reporting does not disclose which key is stored.

## Host-only verification

No board or serial port is used by these tests:

```powershell
python -B -m unittest discover -s firmware/tests -p '*helper_test.py'
g++ -std=c++17 firmware/tests/ble_provision_status_test.cpp -o "$env:TEMP/ble_provision_status_test.exe"
& "$env:TEMP/ble_provision_status_test.exe"
```
