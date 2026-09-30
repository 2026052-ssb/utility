ImageMagick이라는 패키지를 활용한 gif 파일 만들기

### install
```bash
sudo apt install -y imagemagick-6.q16
```

### run
```bash
convert -delay 20 $(cat viz.txt) -loop 0 test.gif
```

### viz.txt 예시

```text
cropped/frame_00.png
cropped/frame_01.png
cropped/frame_02.png
cropped/frame_03.png
cropped/frame_04.png
cropped/frame_05.png
```
