#!/usr/bin/env python3
"""GUI integration check: offline capture with two cameras and without cameras.

Run after sourcing ROS and rviz_capture; requires a working DISPLAY and PyYAML/Pillow.
Uses ROS_DOMAIN_ID=187 unless overridden by the caller. Starts/stops only its own RViz.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time

os.environ.setdefault('ROS_DOMAIN_ID', '187')
import rclpy
from rclpy.qos import QoSProfile, DurabilityPolicy
from sensor_msgs.msg import PointCloud2, PointField, Image
from visualization_msgs.msg import Marker, MarkerArray
from builtin_interfaces.msg import Time
from rviz_capture.srv import CaptureFrame
from rviz_capture_client import ReplayCapture
import yaml
from PIL import Image as PILImage


def wait(node, predicate, timeout=15):
    end = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() > end:
            raise AssertionError('integration check timed out')
        rclpy.spin_once(node, timeout_sec=0.02)


def run_case(root, cameras):
    required = ['cloud', 'boxes'] + (['camera2', 'camera3'] if cameras else [])
    displays = [
        {'Class': 'rviz_capture/SyncedPointCloud2', 'Name': 'cloud', 'Enabled': True,
         'Topic': {'Value': '/smoke/cloud', 'Reliability Policy': 'Reliable'},
         'Position Transformer': 'XYZ', 'Color Transformer': 'FlatColor', 'Decay Time': 0},
        {'Class': 'rviz_capture/SyncedMarkerArray', 'Name': 'boxes', 'Enabled': True,
         'Topic': {'Value': '/smoke/boxes', 'Reliability Policy': 'Reliable',
                   'Durability Policy': 'Transient Local'}},
    ]
    for c in ([2, 3] if cameras else []):
        displays.append({'Class': 'rviz_capture/SyncedImage', 'Name': f'camera{c}',
                         'Enabled': True, 'Topic': {'Value': f'/smoke/camera{c}',
                                                  'Reliability Policy': 'Reliable'}})
    config = {'Panels': [{'Class': 'rviz_capture/CapturePanel', 'Name': 'Capture',
                         'Output': str(root/'output'), 'View3D': True,
                         'Images': cameras, 'Window': True}],
              'Visualization Manager': {'Class': '', 'Enabled': True,
                  'Global Options': {'Fixed Frame': 'map', 'Frame Rate': 30},
                  'Displays': displays}, 'Window Geometry': {'Width': 1200, 'Height': 900}}
    path = root/'test.rviz'
    path.write_text(yaml.safe_dump(config))
    with (root/'rviz.log').open('w') as log:
        process = subprocess.Popen(['rviz2', '-d', str(path)], stdout=log, stderr=log)
        node = rclpy.create_node('capture_integration_check')
        try:
            capture = ReplayCapture(node, required, timeout=20)
            cloud_pub = node.create_publisher(PointCloud2, '/smoke/cloud', 1)
            marker_pub = node.create_publisher(MarkerArray, '/smoke/boxes',
                QoSProfile(depth=1, durability=DurabilityPolicy.TRANSIENT_LOCAL))
            image_pubs = {c: node.create_publisher(Image, f'/smoke/camera{c}', 1)
                          for c in ([2, 3] if cameras else [])}
            wait(node, lambda: all(p.get_subscription_count() > 0
                 for p in [cloud_pub, marker_pub, *image_pubs.values()]))

            def publish(sec, omit=None):
                stamp = Time(sec=sec)
                cloud = PointCloud2()
                cloud.header.frame_id = 'map'; cloud.header.stamp = stamp
                cloud.height = 1; cloud.width = 2; cloud.point_step = 12; cloud.row_step = 24
                cloud.fields = [PointField(name=name, offset=i*4, datatype=PointField.FLOAT32, count=1)
                                for i, name in enumerate(['x', 'y', 'z'])]
                cloud.data = struct.pack('<ffffff', 1, 0, 0, 2, 1, 0)
                cloud_pub.publish(cloud)
                # Zero detections still carry a stamped clear marker.
                clear = Marker(); clear.header = cloud.header; clear.action = Marker.DELETEALL
                marker_pub.publish(MarkerArray(markers=[clear]))
                for camera, pub in image_pubs.items():
                    if camera == omit: continue
                    image = Image(); image.header = cloud.header
                    image.height = 32; image.width = 48; image.encoding = 'rgb8'; image.step = 144
                    image.data = bytes([sec, 30, 70])*32*48
                    pub.publish(image)
                return stamp

            first = capture.capture('dataset/first', publish(50))
            assert first['capture_frame'] == 0
            # Ask for a frame while one view is still on the previous frame.
            stamp = publish(100, omit=3 if cameras else None)
            request = CaptureFrame.Request(source_frame='dataset/second', sensor_stamp=stamp,
                required_displays=required, timeout_seconds=5.0)
            future = capture.client.call_async(request)
            if cameras:
                end = time.monotonic() + 0.5
                while time.monotonic() < end: rclpy.spin_once(node, timeout_sec=0.02)
                assert not future.done(), 'saved while camera3 was still on the previous frame'
                publish(100)
            wait(node, future.done)
            assert future.result().success, future.result().message
            session = Path(first['session_directory'])
            rows = [json.loads(line) for line in (session/'frames.jsonl').read_text().splitlines()]
            assert len(rows) == 2 and rows[1]['sensor_stamp'] == {'sec': 100, 'nanosec': 0}
            assert all(row['synchronized'] for row in rows)
            assert len(rows[1]['files']) == (4 if cameras else 2)
            for file in rows[1]['files']:
                with PILImage.open(session/file) as image:
                    image.load()
                    if file.startswith('images/'):
                        pixel = image.convert('RGB').getpixel((image.width//2, image.height//2))
                        assert pixel == (100, 30, 70), (file, pixel)
            # Missing timestamps must fail without appending any frame or advancing playback.
            request.sensor_stamp = Time(sec=200); request.timeout_seconds = 0.3
            future = capture.client.call_async(request)
            wait(node, future.done)
            assert not future.result().success and not future.result().files
            assert len((session/'frames.jsonl').read_text().splitlines()) == 2
            print('PASS:', 'two cameras' if cameras else 'no cameras',
                  '- render pixels, delayed view, empty detections, timeout, manifest')
        finally:
            node.destroy_node()
            process.terminate()
            try: process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()


def main():
    rclpy.init()
    try:
        with tempfile.TemporaryDirectory(prefix='rviz_capture_sync_') as directory:
            for cameras in [True, False]:
                root = Path(directory)/('cameras' if cameras else 'lidar')
                root.mkdir()
                run_case(root, cameras)
    finally:
        rclpy.shutdown()


if __name__ == '__main__':
    main()
