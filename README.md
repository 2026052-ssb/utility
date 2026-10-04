# utility
작업에 편리한 도구 모음집

Ubuntu 24.04 환경에서 작동

1. [make_gif](make_gif.md)  
ImageMagick 패키지를 활용한 gif 파일 생성기  

2. [make_git_md](make_git_md.py)  
ChatGPT에서 생성된 답변을 사용하기 전 LaTex 수식 변환과 필요없는 링크 필터링  
`input.md` -> `output.md`

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
