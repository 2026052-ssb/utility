"""Blocking offline replay handshake; the caller owns the rclpy node/executor."""
import math
import time


def add_capture_arguments(parser):
    parser.add_argument('--capture-rviz', action='store_true',
                        help='wait for RViz synchronized PNG capture before the next frame')
    parser.add_argument('--capture-timeout', type=float, default=30.0,
                        help='RViz display/capture timeout in seconds (0, 120]')
    parser.add_argument('--capture-service', default='/rviz_capture/capture_frame')


class ReplayCapture:
    def __init__(self, node, required_displays, timeout=30.0,
                 service='/rviz_capture/capture_frame'):
        import rclpy
        from rviz_capture.srv import CaptureFrame
        if not math.isfinite(timeout) or not 0 < timeout <= 120:
            raise ValueError('capture timeout must be finite and in (0, 120]')
        if not required_displays or len(set(required_displays)) != len(required_displays):
            raise ValueError('required_displays must contain unique RViz display names')
        self.node = node
        self.rclpy = rclpy
        self.service_type = CaptureFrame
        self.required = list(required_displays)
        self.timeout = float(timeout)
        self.first = True
        self.client = node.create_client(CaptureFrame, service)
        if not self.client.wait_for_service(timeout_sec=timeout):
            raise RuntimeError(f'RViz capture service unavailable: {service}; load the capture RViz config')

    def capture(self, source_frame, stamp):
        request = self.service_type.Request()
        request.source_frame = str(source_frame)
        request.sensor_stamp = stamp
        request.required_displays = self.required
        request.timeout_seconds = self.timeout
        request.new_session = self.first
        future = self.client.call_async(request)
        # Extra response margin for synchronous PNG writes. On failure, never retry
        # automatically: a timed-out server may still finish saving this frame.
        deadline = time.monotonic() + self.timeout + 30.0
        while self.rclpy.ok() and not future.done():
            if time.monotonic() >= deadline:
                future.cancel()
                raise RuntimeError('RViz capture response timeout; replay stopped without advancing; inspect output before retrying')
            self.rclpy.spin_once(self.node, timeout_sec=0.02)
        if not self.rclpy.ok():
            raise KeyboardInterrupt
        response = future.result()
        if response is None or not response.success:
            raise RuntimeError(f'RViz capture failed: {response.message if response else "no response"}; replay stopped')
        self.first = False
        return {'session_directory': response.session_directory,
                'capture_frame': response.capture_frame, 'files': list(response.files)}
