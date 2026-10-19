# rviz_capture

여러 ROS 2 프로젝트에서 재사용하는 RViz 패널 플러그인입니다. ROS 2 Jazzy / Qt 5 기준이며, 프로젝트의 노드나 토픽 코드를 수정하지 않고 사용합니다.

## 빌드와 실행

```bash
source /opt/ros/jazzy/setup.bash
cd /home/hwe/projects/utils
colcon build --base-paths rviz_capture --packages-select rviz_capture
source install/setup.bash
rviz2
# 또는 기존 프로젝트 설정 사용
# rviz2 -d /home/hwe/projects/autoware-test3/rviz/fusion.rviz
```

다른 워크스페이스에서 사용해도 RViz 실행 터미널에 위 `install/setup.bash`를 source하면 됩니다. 빌드 의존성은 `rviz_common`, `rviz_rendering`, `pluginlib`, Qt 5 Widgets이며 ROS 패키지 설치는 `rosdep install --from-paths rviz_capture --ignore-src -r -y`를 사용할 수 있습니다.

RViz 메뉴 **Panels → Add New Panel → rviz_capture/CapturePanel**로 추가합니다. 이후 RViz 설정을 저장하면 선택한 대상, 출력 경로, FPS가 `.rviz`에 함께 저장됩니다. 설정을 다시 열어도 녹화는 자동 시작하지 않습니다.

## 캡처 대상

다음 항목을 복수 선택할 수 있습니다.

| 옵션 | 저장 위치 | 내용 |
| --- | --- | --- |
| `3D view` | `view3d/` | 포인트클라우드, 박스 등 메인 3D 렌더링 결과. 도구 모음과 다른 패널 제외 |
| `Image panels (each separately)` | `images/<display 인덱스>_<이름>/` | 활성화된 표준 `rviz_default_plugins/Image` 패널 각각의 렌더링 결과 |
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

같은 번호의 파일도 센서 타임스탬프가 같은 데이터라고 보장하지 않습니다. 입력 프레임을 모두 저장하거나 라이다·검출 박스·카메라의 동기화를 보장하려면 재생 측 데이터 준비 확인과 캡처 완료 응답을 연결하는 별도 인터페이스가 필요합니다. 현재 버전에는 ROS 캡처 서비스와 프레임별 재생 연동은 포함하지 않습니다.
