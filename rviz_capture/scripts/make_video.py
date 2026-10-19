#!/usr/bin/env python3
"""Encode one rviz_capture stream with a constant playback frame rate."""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stream', type=Path, help='view3d, window or images/<display> directory')
    parser.add_argument('output', type=Path, help='output .mp4 path (must not exist)')
    parser.add_argument('--fps', type=float, default=10, help='playback FPS, default: 10')
    args = parser.parse_args()
    if not 0 < args.fps <= 240:
        parser.error('--fps must be between 0 and 240')
    frames = sorted(args.stream.glob('*.png'))
    if not frames or any(path.name != f'{i:06d}.png' for i, path in enumerate(frames)):
        parser.error('stream must contain a contiguous PNG sequence starting at 000000.png')
    if args.output.exists():
        parser.error('output already exists')
    if shutil.which('ffmpeg') is None:
        parser.error('ffmpeg is required: sudo apt install ffmpeg')
    subprocess.run([
        'ffmpeg', '-n', '-framerate', str(args.fps), '-start_number', '0',
        '-i', str(args.stream.resolve() / '%06d.png'), '-frames:v', str(len(frames)),
        '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2',
        '-c:v', 'libx264', '-pix_fmt', 'yuv420p', str(args.output.resolve()),
    ], check=True)


if __name__ == '__main__':
    main()
