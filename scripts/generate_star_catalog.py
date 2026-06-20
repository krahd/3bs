#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

"""Create the compact HYG catalogue embedded by the renderer."""

import argparse
import csv
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--magnitude", type=float, default=6.5)
    args = parser.parse_args()

    rows: list[tuple[float, float, float, float]] = []
    with args.input.open(newline="", encoding="utf-8") as source:
        for star in csv.DictReader(source):
            try:
                magnitude = float(star["mag"])
                right_ascension = float(star["ra"])
                declination = float(star["dec"])
            except (KeyError, TypeError, ValueError):
                continue
            if magnitude > args.magnitude:
                continue
            try:
                colour_index = float(star.get("ci", ""))
            except (TypeError, ValueError):
                colour_index = 0.65
            rows.append((right_ascension, declination, magnitude, colour_index))

    rows.sort(key=lambda row: (row[2], row[0], row[1]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="ascii") as destination:
        destination.write("# HYG v4.1, magnitude <= 6.5\n")
        destination.write("# ra_hours,dec_degrees,magnitude,b_v_colour_index\n")
        writer = csv.writer(destination, lineterminator="\n")
        for row in rows:
            writer.writerow(f"{value:.6f}" for value in row)

    print(f"wrote {len(rows)} stars to {args.output}")


if __name__ == "__main__":
    main()
