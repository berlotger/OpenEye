#!/usr/bin/env python3
"""
send_command.py — OpenVisionEye
STATUS: 1 REAL — a plain TCP client, no special dependencies beyond the
Python standard library (socket, sys).

Sends one line of text to the XIAO's control-channel TCP server
(docs/wifi_protocol.md, port 3333) from a PC connected to the XIAO's own
Wi-Fi Access Point. Useful for testing the protocol, or as a starting point
for a companion phone/PC app later, without needing the XIAO's own USB
Serial Monitor.

Usage:
    1. Connect your PC's Wi-Fi to the XIAO's SoftAP (SSID printed on the
       XIAO's Serial Monitor at boot, e.g. "OpenVisionEye-A1B2C3").
    2. python3 send_command.py "HEY_GLASSES"
    3. python3 send_command.py "take a photo"

By default connects to 192.168.4.1:3333, which is the XIAO's default SoftAP
IP and the control port defined in ProtocolDefs.h — override with --host/
--port if you've changed either.
"""

import argparse
import socket
import sys


def main():
    parser = argparse.ArgumentParser(description="Send a text command to the OpenVisionEye XIAO control channel.")
    parser.add_argument("message", help="The line to send, e.g. HEY_GLASSES or 'take a photo'")
    parser.add_argument("--host", default="192.168.4.1", help="XIAO SoftAP IP (default: 192.168.4.1)")
    parser.add_argument("--port", type=int, default=3333, help="Control channel TCP port (default: 3333)")
    parser.add_argument("--timeout", type=float, default=5.0, help="Socket timeout in seconds (default: 5.0)")
    parser.add_argument("--listen", action="store_true",
                         help="After sending, keep the connection open and print any lines the XIAO sends back "
                              "(e.g. SAY:... replies) until Ctrl+C.")
    args = parser.parse_args()

    try:
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            sock.sendall((args.message + "\n").encode("utf-8"))
            print(f"Sent: {args.message}")

            if args.listen:
                sock.settimeout(None)
                print("Listening for replies (Ctrl+C to stop)...")
                buffer = b""
                try:
                    while True:
                        chunk = sock.recv(1024)
                        if not chunk:
                            print("Connection closed by XIAO.")
                            break
                        buffer += chunk
                        while b"\n" in buffer:
                            line, buffer = buffer.split(b"\n", 1)
                            print("<-", line.decode("utf-8", errors="replace").strip())
                except KeyboardInterrupt:
                    print("\nStopped.")
    except (ConnectionRefusedError, OSError) as e:
        print(f"Could not connect to {args.host}:{args.port} — {e}", file=sys.stderr)
        print("Check that your PC's Wi-Fi is connected to the XIAO's SoftAP "
              "and that the XIAO's Serial Monitor shows the control channel is up.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
