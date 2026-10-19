# rviz_capture

여러 ROS 2 프로젝트에서 재사용하는 RViz 패널 플러그인입니다. ROS 2 Jazzy / Qt 5 기준이며, 프로젝트의 노드나 토픽 코드를 수정하지 않고 사용합니다.

## 빌드와 실행

```bash
source /opt/ros/jazzy/setup.bash
cd /home/hwe/projects/utils
colcon build --base-paths rviz_capture --packages-select rviz_capture
source install/setup.bash
GALLIUM_DRIVER=d3d12 \
MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA \
rviz2
# 또는 기존 프로젝트 설정 사용
# rviz2 -d /home/hwe/projects/autoware-test3/rviz/fusion.rviz
```

다른 워크스페이스에서 사용해도 RViz 실행 터미널에 위 `install/setup.bash`를 source하면 됩니다. 빌드 의존성은 `rviz_common`, `rviz_rendering`, `rviz_default_plugins`, `pluginlib`, `rclcpp`, ROS 인터페이스 생성기, Qt 5 Widgets이며 ROS 패키지 설치는 `rosdep install --from-paths rviz_capture --ignore-src -r -y`를 사용할 수 있습니다.

WSLg에서 D3D12 드라이버와 NVIDIA 어댑터를 지정해 RViz를 실행하려면 다음처럼 환경변수를 설정합니다.

```bash
GALLIUM_DRIVER=d3d12 \
MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA \
rviz2
# 기존 설정을 열려면 rviz2 뒤에 -d <설정 파일>을 추가합니다.
```

RViz 메뉴 **Panels → Add New Panel → rviz_capture/CapturePanel**로 추가합니다. 이후 RViz 설정을 저장하면 선택한 대상, 출력 경로, FPS가 `.rviz`에 함께 저장됩니다. 설정을 다시 열어도 녹화는 자동 시작하지 않습니다.

## 캡처 대상

다음 항목을 복수 선택할 수 있습니다.

| 옵션 | 저장 위치 | 내용 |
| --- | --- | --- |
| `3D view` | `view3d/` | 포인트클라우드, 박스 등 메인 3D 렌더링 결과. 도구 모음과 다른 패널 제외 |
| `Image panels (each separately)` | `images/<display 인덱스>_<이름>/` | 활성화된 `rviz_default_plugins/Image` 또는 `rviz_capture/SyncedImage` 패널 각각의 렌더링 결과 |
| `Whole RViz window` | `window/` | 도킹된 이미지와 설정 패널을 포함한 RViz 메인 창 클라이언트 영역 |

이미지 패널은 토픽 원본을 저장하는 대신 RViz에서 보이는 뷰를 저장합니다. 비율 유지 여백과 RViz 정규화 결과가 포함될 수 있습니다. 중첩 DisplayGroup도 탐색하며 같은 이름은 인덱스로 구분합니다. Camera 디스플레이와 제삼자 이미지 플러그인은 개별 이미지 캡처 대상이 아닙니다.

전체 창은 Qt UI 캡처에 보이는 RViz RenderPanel의 렌더 버퍼를 합성합니다. OS 화면 캡처에서 발생할 수 있는 검은 OpenGL 영역을 피하며, 다른 앱의 겹친 창을 포함하지 않습니다. 별도 창으로 분리한 패널과 운영체제 제목 표시줄은 포함되지 않습니다. 선택한 뷰를 보이게 유지하고 RViz를 최소화하지 마세요. ROS 2 Jazzy / Qt xcb(XWayland)에서 확인했으며 네이티브 Wayland는 별도 검증하지 않았습니다.

## 사용법과 저장 형식

- `Choose output directory`: 출력 상위 디렉터리 선택. 기본값 `~/rviz_captures`.
- `Capture frame`: 선택한 모든 대상을 한 번씩 PNG로 저장. 같은 설정에서 반복하면 같은 세션에 이어 저장.
- `Start recording` / `Stop recording`: 목표 FPS로 연속 저장. 시작할 때마다 UTC 시각과 임의 식별자로 새 세션 생성.
- 기본 FPS는 10, 설정 범위는 1–60. 녹화 중 대상·경로·FPS 변경과 단일 캡처는 잠깐 비활성화.

```text
~/rviz_captures/<UTC시각>_<식별자>/
├── frames.jsonl
├── view3d/000000.png
├── images/4_Camera_2__ROI_and_projected_3D/000000.png
├── images/5_Camera_3__ROI_and_projected_3D/000000.png
└── window/000000.png
```

각 스트림에 같은 번호를 사용합니다. `frames.jsonl`은 성공한 캡처 묶음마다 번호, 캡처 시작 UTC 시각, 목표 FPS, 상대 파일 경로를 기록합니다. 저장 실패 또는 선택한 뷰가 숨겨진 경우 연속 저장을 중지하고 패널에 원인을 표시합니다. 해당 묶음의 PNG는 지우고 이전 성공 프레임은 유지합니다. 활성 Image 디스플레이가 없으면 Image 옵션을 끄세요.

## 영상 변환

FFmpeg는 별도 설치합니다 (`sudo apt install ffmpeg`). 스트림 하나를 지정해서 MP4를 생성합니다.

```bash
source /home/hwe/projects/utils/install/setup.bash
ros2 run rviz_capture make_video.py \
  ~/rviz_captures/<세션>/view3d /tmp/lidar.mp4 --fps 10

# 이미지 또는 전체 창도 각각 변환 가능
# ros2 run rviz_capture make_video.py <세션>/images/<패널> camera.mp4 --fps 10
# ros2 run rviz_capture make_video.py <세션>/window rviz.mp4 --fps 10
```

기존 출력 파일은 덮어쓰지 않습니다. 000000부터 연속된 PNG 시퀀스를 검증하며, 홀수 크기는 오른쪽·아래 패딩으로 H.264에 맞춥니다. 녹화 중 창 크기나 활성 디스플레이 구성을 바꾸지 말고 녹화를 멈춘 뒤 변환하세요.

## 시간과 동기화 범위

이 버전은 **화면 캡처 도구**입니다. 각 뷰를 GUI 스레드에서 순서대로 렌더링하고 동기 저장합니다. 설정 FPS는 목표치이며 디스크·GPU 속도에 따라 실제 저장 간격이 늘어날 수 있고, 저장 중 RViz가 잠깐 멈출 수 있습니다. 영상 변환 FPS는 재생 속도이며 실제 캡처 시각 간격을 재현하지 않습니다.

수동·타이머 모드의 같은 번호 파일은 센서 timestamp 일치를 보장하지 않습니다. 데이터셋 프레임 단위로 저장하려면 아래 오프라인 동기화 서비스를 사용합니다.

## 오프라인 프레임 동기화

`/rviz_capture/capture_frame` (`rviz_capture/srv/CaptureFrame`) 서비스와 `rviz_capture_client.ReplayCapture`를 제공합니다. 한 재생 프로세스가 다음 입력을 멈추고, 같은 센서 timestamp를 가진 모든 시각화 데이터를 발행한 뒤 저장 성공 응답을 기다리는 방식입니다. 실시간 센서 버퍼링, 근사 시각 매칭, rosbag의 자유 실행은 지원하지 않습니다.

정확한 표시 완료를 확인하려면 `.rviz`에서 클래스만 다음과 같이 바꿉니다. 토픽·표시 이름·색상·시점 설정은 그대로 사용할 수 있습니다.

| 기존 클래스 | 동기화용 클래스 |
| --- | --- |
| `rviz_default_plugins/PointCloud2` | `rviz_capture/SyncedPointCloud2` |
| `rviz_default_plugins/MarkerArray` | `rviz_capture/SyncedMarkerArray` |
| `rviz_default_plugins/Image` | `rviz_capture/SyncedImage` |

동기화용 디스플레이는 별도 구독자의 수신 여부가 아니라 **실제 디스플레이의 메시지 처리 및 후속 update 완료**를 추적합니다. 서비스는 필요한 디스플레이의 timestamp가 모두 일치하고 Error 상태가 없는지 확인한 뒤 렌더링하고 저장합니다. 토픽만 수신하거나 고정 sleep이 지났다는 이유로 성공 처리하지 않습니다. Grid와 같은 정적 디스플레이는 그대로 사용합니다. 이 세 종류 외의 동적 디스플레이에 대해서는 동기화를 보장하지 않습니다.

`CaptureFrame` 요청:

- `source_frame`: 데이터셋 식별자. 예: `00/000123`.
- `sensor_stamp`: 모든 시각화 데이터에 공통으로 사용한 ROS header timestamp.
- `required_displays`: 필요한 디스플레이의 **정확하고 고유한 Name** 목록. 토픽 이름이 아닙니다. 활성 PointCloud2/MarkerArray/Image 디스플레이를 모두 포함하고 Synced 클래스로 바꿔야 합니다.
- `timeout_seconds`: 표시 완료 대기 제한, 유한한 0 초과 120 이하. PNG 저장은 동기 작업이므로 별도 응답 대기 여유가 필요합니다.
- `new_session`: 첫 요청에 true로 지정하여 새 출력 세션 생성.

성공 응답에는 `session_directory`, `capture_frame`, PNG 절대 경로 `files`를 반환합니다. 실패 시 다음 프레임을 발행하지 말고 재생을 중단하세요. 서비스 응답 자체가 끊기면 저장이 완료됐을 가능성이 있으므로 자동 재시도하지 않습니다. 한 RViz 프로세스에 Capture 패널 하나와 재생 클라이언트 하나를 사용하고, 서비스 동기화 중에는 타이머 녹화를 사용하지 않습니다. 수동 타이머 녹화 중 서비스 요청은 거절됩니다.

카메라 개수는 고정하지 않습니다. 카메라가 없는 프로젝트는 Image 옵션을 끄고 라이다·박스 디스플레이만 요청합니다. 카메라가 있으면 요청 목록에 필요한 Image 디스플레이를 추가합니다. MarkerArray는 빈 배열 대신 동일 header를 가진 DELETEALL 마커를 포함해야 하며 **모든 마커의 header stamp가 일치**해야 합니다. 검출이 0개인 프레임도 이 방법으로 이전 박스를 제거하고 동기화합니다. 포인트클라우드 `Decay Time: 0`을 유지하고, 프레임의 표시가 가능한 TF/고정 좌표계를 준비하세요.

```python
from rviz_capture_client import ReplayCapture

capture = ReplayCapture(node, ['LiDAR', 'Detection boxes'])  # 카메라 없는 구성
for frame in dataset:
    # 입력 발행 → 같은 timestamp의 추론 결과 대기
    # 같은 timestamp의 PointCloud2, MarkerArray, 선택적 Image 발행
    saved = capture.capture(frame.name, stamp)
    # 성공 응답 뒤에만 다음 입력 발행
```

공통 모듈은 `projects/utils/rviz_capture`에서 관리하며, autoware-test2/test3의 재생 스크립트와 RViz 설정에는 캡처 연동을 추가하지 않습니다. 기존 프로젝트를 수정하지 않고 사용하는 방식은 Capture 패널의 단일 캡처·타이머 녹화입니다. 위 동기화 서비스와 클라이언트는 별도 재생 도구를 위한 선택적 API로 유지합니다. 현재 공통 모듈에는 기존 재생기를 외부에서 정지·진행시키는 기능이 없으므로, 프로젝트를 수정하지 않는 사용 방식에서는 모든 입력 프레임의 누락 없는 동기화 저장을 보장하지 않습니다.

동기화 저장 시 `frames.jsonl`에 `synchronized: true`, `source_frame`, `sensor_stamp: {sec, nanosec}`, `required_displays`가 추가됩니다. `utc`는 캡처 시작 벽시계 시각이고 센서 timestamp와 구분합니다. 수동·타이머 캡처는 `synchronized: false`입니다. 동기화 모드에서는 FPS가 재생 제어에 사용되지 않으며 영상 변환 시 원하는 재생 FPS를 지정합니다. 녹화 대상 설정은 패널 체크박스를 따릅니다.


## 검증

ROS 2 Jazzy / Qt xcb 환경에서 실제 RViz를 띄우는 통합 검사를 제공합니다. 기본 ROS domain은 187이며 환경변수 `ROS_DOMAIN_ID`로 바꿀 수 있습니다. PyYAML과 Pillow가 필요합니다.

```bash
source /opt/ros/jazzy/setup.bash
source /home/hwe/projects/utils/install/local_setup.bash
python3 /home/hwe/projects/utils/rviz_capture/tests/smoke_offline.py
```

검사는 카메라 2개/0개 구성, 늦게 도착한 카메라를 기다리는 동작, 실제 이미지 중심 픽셀의 프레임 일치, 검출 0개인 DELETEALL, 시간 초과 시 파일·로그를 추가하지 않는 동작을 확인합니다. 임시 디렉터리와 자체 실행한 RViz만 사용합니다.

2026-10-19 검증: Jazzy 빌드와 위 GUI 통합 검사(카메라 2개/0개)를 통과했습니다. 프로젝트별 연동을 제거하기 전 autoware-test3/test2에서도 KITTI 시퀀스 00 실제 추론+RViz 재생 각 2프레임의 sensor timestamp 일치 및 PNG 내용을 확인했습니다. 해당 프로젝트별 연동은 현재 되돌린 상태이며 일반 재생에서 동기화 캡처를 자동 수행하지 않습니다.
