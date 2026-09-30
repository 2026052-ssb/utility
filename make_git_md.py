import re
from pathlib import Path


def remove_chatgpt_content_references(text: str) -> str:
    """
    ChatGPT 내부 reference 제거.

    실제 형태:
    :chatgpt-content-reference{index="0"}

    혹시 다른 형태가 들어오는 경우도 함께 처리.
    """

    patterns = [
        # 실제 확인된 형태
        r':chatgpt-content-reference\s*\{[^}]*\}',

        # 혹시 존재할 수 있는 다른 형태
        r':chatgpt-content-reference\s*\[[^\]]*\]\s*(?:\{[^}]*\})?',
        r':contentReference\s*\[[^\]]*\]\s*(?:\{[^}]*\})?',
        r':contentReference\s*\{[^}]*\}',
    ]

    for pattern in patterns:
        text = re.sub(pattern, "", text)

    return text


def merge_split_math_operators(text: str) -> str:
    r"""
    단독 수식 연산자를 이전 줄에 붙임.

    입력:
        \hat{x}\_{k|k}
        \=
        \hat{x}\_{k|k-1}
        \+
        K(...)

    출력:
        \hat{x}\_{k|k} =
        \hat{x}\_{k|k-1} +
        K(...)
    """

    # ChatGPT 복사 시 escape된 형태까지 처리
    operator_map = {
        "=": "=",
        r"\=": "=",

        "+": "+",
        r"\+": "+",

        "-": "-",
        r"\-": "-",

        r"\pm": r"\pm",
        r"\approx": r"\approx",
        r"\simeq": r"\simeq",
        r"\sim": r"\sim",
        r"\neq": r"\neq",
        r"\le": r"\le",
        r"\ge": r"\ge",
        r"\leq": r"\leq",
        r"\geq": r"\geq",
        r"\to": r"\to",
        r"\rightarrow": r"\rightarrow",
        r"\Rightarrow": r"\Rightarrow",
        r"\Longrightarrow": r"\Longrightarrow",
    }

    lines = text.splitlines()
    result = []

    for line in lines:
        stripped = line.strip()

        if stripped in operator_map and result:
            operator = operator_map[stripped]

            # 빈 줄이 앞에 있다면 제거하면서
            # 실제 이전 내용 줄에 연산자를 붙임
            while result and not result[-1].strip():
                result.pop()

            if result:
                result[-1] = result[-1].rstrip() + " " + operator
            else:
                result.append(operator)

        else:
            result.append(line)

    return "\n".join(result)


def convert_math_blocks_for_github(text: str) -> str:
    r"""
    \[
        ...
    \]

    →
    
    $$
        ...
    $$
    """

    pattern = r"\\\[\s*(.*?)\s*\\\]"

    def replace(match):
        content = match.group(1).strip()
        return "$$\n" + content + "\n$$"

    return re.sub(
        pattern,
        replace,
        text,
        flags=re.DOTALL,
    )


def remove_markdown_escape(text: str) -> str:
    """
    ChatGPT 복사 과정에서 Markdown 문법 자체가 escape된 것을 복원.

    예:
        \*\*bold\*\*  → **bold**
        \# title      → # title
        \> quote      → > quote
        \- item       → - item
        \`code\`      → `code`

    단, LaTeX 명령은 건드리지 않음.
    """

    chars = r"*#>`"

    text = re.sub(
        rf"\\([{re.escape(chars)}])",
        r"\1",
        text,
    )

    # 리스트 시작의 \- 만 복원
    text = re.sub(
        r"(?m)^(\s*)\\-\s+",
        r"\1- ",
        text,
    )

    # horizontal rule \---
    text = re.sub(
        r"(?m)^\\---\s*$",
        "---",
        text,
    )

    return text


def cleanup_whitespace(text: str) -> str:
    # 줄 끝 공백 제거
    text = "\n".join(
        line.rstrip()
        for line in text.splitlines()
    )

    # 3줄 이상 빈 줄 → 2줄
    text = re.sub(r"\n{4,}", "\n\n\n", text)

    return text


def clean_for_github(text: str) -> str:

    # 1. ChatGPT 내부 reference 제거
    text = remove_chatgpt_content_references(text)

    # 2. 수식 연산자 합치기
    # 반드시 math block 변환보다 먼저
    text = merge_split_math_operators(text)

    # 3. \[...\] → $$...$$
    text = convert_math_blocks_for_github(text)

    # 4. Markdown escape 복원
    text = remove_markdown_escape(text)

    # 5. whitespace 정리
    text = cleanup_whitespace(text)

    return text.strip() + "\n"


def convert_file(
    input_path: str,
    output_path: str | None = None,
):

    input_path = Path(input_path)

    if output_path is None:
        output_path = input_path.with_name(
            input_path.stem
            + "_github"
            + input_path.suffix
        )
    else:
        output_path = Path(output_path)

    text = input_path.read_text(
        encoding="utf-8"
    )

    cleaned = clean_for_github(text)

    output_path.write_text(
        cleaned,
        encoding="utf-8",
    )

    print(f"Input : {input_path}")
    print(f"Output: {output_path}")


if __name__ == "__main__":
    convert_file(
        "input.md",
        "output.md",
    )