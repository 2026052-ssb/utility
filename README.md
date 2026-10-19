# utility
작업에 편리한 도구 모음집

Ubuntu 24.04 환경에서 작동

1. [make_gif](make_gif.md)  
ImageMagick 패키지를 활용한 gif 파일 생성기  

2. [make_git_md](make_git_md.py)  
ChatGPT에서 생성된 답변을 사용하기 전 LaTex 수식 변환과 필요없는 링크 필터링  
`input.md` -> `output.md`

3. [rviz_capture](rviz_capture/README.md)
ROS 2 Jazzy RViz 공통 캡처 패널. 라이다 3D 뷰·Image 패널별 PNG 저장과 RViz 창 전체 저장을 복수 선택하며, 단일 캡처·지정 FPS 연속 저장·MP4 변환과 오프라인 재생 프레임 동기화 서비스를 지원합니다. 카메라 유무에 따라 동기화 디스플레이를 지정할 수 있습니다.

- VSCode > Source Control에서 Git 인식이 안될 경우  
~/.vscode-server/data/Machine/settings.json
```json
{
  "git.scanRepositories": [
    "projects/BUFFER-X",
  ]
}
```
**Ctrl+Shift+P > Developer: Reload Window**를 실행
