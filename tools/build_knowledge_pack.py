"""Build a Qwen-friendly Markdown knowledge pack from the exercise repository."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "embedded"

TOPICS = [
    ("01_C_Fundamentals", "01_C语言基础.md", "C 语言基础", ["C", "位操作", "指针", "结构体", "volatile", "字节序"]),
    ("02_Memory", "02_内存管理.md", "内存管理", ["栈", "堆", "内存池", "链接器", "MMIO"]),
    ("03_Interrupts_and_ISR", "03_中断与ISR.md", "中断与 ISR", ["ISR", "环形缓冲区", "临界区", "DMA"]),
    ("04_Bare_Metal_Peripherals", "04_STM32裸机外设.md", "STM32 裸机外设", ["GPIO", "UART", "SPI", "I2C", "定时器", "PWM"]),
    ("05_RTOS", "05_FreeRTOS.md", "FreeRTOS", ["任务调度", "队列", "信号量", "互斥锁", "死锁"]),
    ("06_Communication_Protocols", "06_通信协议.md", "通信协议", ["UART", "CRC", "Modbus", "CAN"]),
    ("07_Linux_Embedded", "07_嵌入式Linux.md", "嵌入式 Linux", ["sysfs", "GPIO", "hwmon", "procfs"]),
    ("08_IoT_Protocols", "08_IoT协议.md", "IoT 协议", ["MQTT", "CoAP", "JSON", "TLS"]),
]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").lstrip("\ufeff").rstrip()


CN_NUMBERS = {"一": "1", "二": "2", "三": "3", "四": "4", "五": "5", "六": "6", "七": "7", "八": "8", "九": "9", "十": "10"}

# A few upstream answer files group tasks or use a different task order.
# Map question task -> answer task explicitly so the knowledge pack does not
# create a plausible-looking but technically wrong pairing.
ANSWER_TASK_MAPS = {
    "04_Bare_Metal_Peripherals/03_spi_i2c.c": {str(i): "1" for i in range(1, 7)},
    "05_RTOS/01_tasks_and_scheduling.c": {"1": "1", "2": "3", "3": "4", "5": "2", "6": "5"},
    "05_RTOS/02_semaphores_mutexes.c": {"1": "1", "2": "2", "3": "3", "5": "4"},
    "04_Bare_Metal_Peripherals/02_uart_bare_metal.c": {"1": "1", "2": "2", "3": "3", "4": "4", "5": "5", "6": "5"},
}

MANUAL_ANSWERS = {
    ("05_RTOS/01_tasks_and_scheduling.c", "4"): (
        "选择任务栈大小时，先统计局部变量、函数调用深度、中断嵌套、库函数以及上下文保存开销，"
        "再留出安全余量。运行阶段使用 `uxTaskGetStackHighWaterMark()` 或栈染色观察最小剩余空间，"
        "覆盖最坏输入和最长调用路径后逐步收敛。高水位只反映测试覆盖到的历史最小值，不能替代最坏情况分析。"
    ),
    ("05_RTOS/02_semaphores_mutexes.c", "4"): (
        "死锁需要同时满足互斥、持有并等待、不可抢占和循环等待。两个任务分别持有 Mutex1 与 Mutex2，"
        "又互相等待对方释放时就会永久阻塞。工程上应规定全局一致的加锁顺序，尽量缩短临界区，"
        "避免持锁调用未知代码；必要时使用超时、`try-lock` 和失败回退。"
    ),
}


def task_sections(text: str) -> tuple[str, dict[str, str]]:
    """Split C teaching files at their TASK/任务 headings, keeping every byte."""
    pattern = re.compile(r"(?im)^.*?(?:TASK|任务)\s*([0-9]+|[一二三四五六七八九十]+)[^\n]*$")
    matches = []
    seen = set()
    for match in pattern.finditer(text):
        raw = match.group(1)
        key = CN_NUMBERS.get(raw, raw)
        if key not in seen:
            matches.append((key, match.start()))
            seen.add(key)
    if not matches:
        return text, {}
    preamble = text[:matches[0][1]].rstrip()
    sections = {}
    for index, (key, start) in enumerate(matches):
        end = matches[index + 1][1] if index + 1 < len(matches) else len(text)
        sections[key] = text[start:end].strip()
    return preamble, sections


def c_units(topic_dir: Path, problem: Path) -> list[str]:
    answer = topic_dir / "answers" / f"{problem.stem}_answers.c"
    title = problem.stem.split("_", 1)[-1].replace("_", " ")
    source = problem.relative_to(ROOT).as_posix()
    problem_text = read(problem)
    answer_text = read(answer) if answer.exists() else ""
    problem_preamble, problem_tasks = task_sections(problem_text)
    answer_preamble, answer_tasks = task_sections(answer_text)
    task_map = ANSWER_TASK_MAPS.get(source, {})
    parts = [[
        f"## {title}：核心理论与上下文",
        "",
        f"来源：`{source}`",
        "",
        "### 面试助手回答要求",
        "",
        "先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明，不要只复述代码。",
        "",
        "### 理论、公共定义与使用说明",
        "",
        "```c",
        problem_preamble,
        "```",
    ]]
    if answer_preamble:
        parts[0] += [
            "",
            "### 答案文件公共定义",
            "",
            f"答案来源：`{answer.relative_to(ROOT).as_posix()}`",
            "",
            "```c",
            answer_preamble,
            "```",
        ]
    for key, question in problem_tasks.items():
        unit = [
            f"## {title}：任务 {key}",
            "",
            f"来源：`{source}`；任务编号：`{key}`",
            "",
            "### 面试助手回答要求",
            "",
            "先给出 30～60 秒可口述结论，再解释原理、实现步骤、边界条件和常见错误；代码题需要结合参考实现说明。",
            "",
            "### 题目与待实现代码",
            "",
            "```c",
            question,
            "```",
            "",
            "### 对应参考答案",
            "",
        ]
        answer_key = task_map.get(key) if source in ANSWER_TASK_MAPS else key
        manual_answer = MANUAL_ANSWERS.get((source, key))
        if manual_answer:
            unit += [manual_answer]
        elif answer_key in answer_tasks:
            if answer_key != key:
                unit += [f"答案文件中的对应内容位于任务 `{answer_key}`（原文件任务顺序不同或合并了多个任务）。", ""]
            unit += ["```c", answer_tasks[answer_key], "```"]
        else:
            unit.append("原答案文件没有与本题严格对应的独立段落。请依据题目理论作答，不要把其他编号的实现冒充为本题答案。")
        parts.append(unit)
    if not problem_tasks:
        parts[0] += ["", "### 对应参考答案", "", "```c", answer_text, "```"]
    return ["\n".join(part) for part in parts]


def build_topic(dirname: str, output: str, title: str, tags: list[str]) -> None:
    topic_dir = ROOT / dirname
    problems = sorted(p for p in topic_dir.glob("*.c") if p.is_file())
    body = [
        "---",
        f'title: "{title}"',
        f'source_directory: "{dirname}"',
        "knowledge_type: embedded_interview_qa",
        "language: zh-CN",
        "tags: [" + ", ".join(tags) + "]",
        "---",
        "",
        f"# {title}",
        "",
        "本文件将题目与对应答案放在同一个二级标题下。检索命中一个知识单元后，应同时参考题目、理论和答案生成中文口述回答。",
        "",
    ]
    units = []
    for problem in problems:
        units.extend(c_units(topic_dir, problem))
    body.append("\n\n---\n\n".join(units))
    (OUT / output).write_text("\n".join(body).rstrip() + "\n", encoding="utf-8", newline="\n")


def markdown_source_unit(title: str, paths: list[Path], tags: list[str]) -> str:
    body = [
        "---",
        f'title: "{title}"',
        "knowledge_type: embedded_interview_qa",
        "language: zh-CN",
        "tags: [" + ", ".join(tags) + "]",
        "---",
        "",
        f"# {title}",
        "",
        "使用要求：回答时先输出可直接口述的短答案，再按原理、工程实践、常见错误和追问展开。",
    ]
    for path in paths:
        body += [
            "",
            "---",
            "",
            f"## 来源：{path.name}",
            "",
            f"原始路径：`{path.relative_to(ROOT).as_posix()}`",
            "",
            read(path),
        ]
    return "\n".join(body).rstrip() + "\n"


def build_special_topics() -> None:
    system = ROOT / "12_System_Design" / "01_bootloader_design.md"
    (OUT / "09_Bootloader与OTA.md").write_text(
        markdown_source_unit("Bootloader 与 OTA", [system], ["Bootloader", "OTA", "A/B分区", "看门狗"]),
        encoding="utf-8", newline="\n"
    )

    qa = ROOT / "13_Interview_QA"
    build_rapid_fire(qa / "01_rapid_fire_100.md", qa / "answers" / "01_rapid_fire_100_answers.md")
    (OUT / "11_代码排错与白板题.md").write_text(
        markdown_source_unit(
            "代码排错与白板题",
            [qa / "02_bug_hunt_collection.md", qa / "03_whiteboard_exercises.md"],
            ["代码排错", "白板题", "竞态", "内存安全"],
        ), encoding="utf-8", newline="\n"
    )
    (OUT / "12_项目深挖.md").write_text(
        markdown_source_unit(
            "项目深挖问答",
            [qa / "04_personal_project_answers.md"],
            ["项目面试", "UART", "DMA", "状态机", "调试"],
        ), encoding="utf-8", newline="\n"
    )


def build_rapid_fire(question_path: Path, answer_path: Path) -> None:
    questions = {}
    for match in re.finditer(r"(?m)^(\d+)\.\s+(.+)$", read(question_path)):
        questions[match.group(1)] = match.group(2).strip()
    answer_text = read(answer_path)
    answer_matches = list(re.finditer(r"(?m)^\*\*(\d+)\.\s+(.+?)\*\*\s*$", answer_text))
    answers = {}
    answer_titles = {}
    for index, match in enumerate(answer_matches):
        end = answer_matches[index + 1].start() if index + 1 < len(answer_matches) else len(answer_text)
        answers[match.group(1)] = answer_text[match.end():end].strip()
        answer_titles[match.group(1)] = match.group(2).strip()
    lines = [
        "---",
        'title: "嵌入式综合八股文"',
        "knowledge_type: embedded_interview_qa",
        "language: zh-CN",
        "tags: [八股文, 快速问答, C, RTOS, 协议]",
        "---",
        "",
        "# 嵌入式综合八股文",
        "",
        "每道题与对应答案位于同一个二级标题中。回答时应将英文参考答案组织为自然、准确的中文口述，不要逐字硬译。",
        "",
        f"问题来源：`{question_path.relative_to(ROOT).as_posix()}`",
        "",
        f"答案来源：`{answer_path.relative_to(ROOT).as_posix()}`",
    ]
    for number in sorted(questions, key=int):
        lines += [
            "", "---", "", f"## 第 {number} 题：{questions[number]}", "",
            "### 30～60 秒口述任务", "",
            "根据下方参考答案，用中文依次回答：结论、原因、工程场景和易错点。", "",
            "### 参考答案", "",
            answers.get(number, "当前答案文件未提供对应编号。"),
        ]
        if number in answer_titles and answer_titles[number] != questions[number]:
            lines += ["", f"英文原题：{answer_titles[number]}"]
    (OUT / "10_综合八股文.md").write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8", newline="\n")


def build_index() -> None:
    files = [output for _, output, _, _ in TOPICS] + [
        "09_Bootloader与OTA.md",
        "10_综合八股文.md",
        "11_代码排错与白板题.md",
        "12_项目深挖.md",
    ]
    lines = [
        "# Embedded 中文面试知识包",
        "",
        "本目录专门用于导入 Qwen、RAG 知识库或面试助手。原仓库练习文件保持不变；这里把题目、理论、答案和代码整理到同一主题文档中。",
        "",
        "## 导入文件",
        "",
    ]
    lines += [f"- [{name}](./{name})" for name in files]
    lines += [
        "",
        "## 检索与切块规则",
        "",
        "- 优先按二级标题切块；同一二级标题内的题目与答案应视为一个知识单元。",
        "- 若平台必须按长度切块，请保留标题路径，并设置 15%～20% 重叠。",
        "- 代码块不可从函数中间截断。题目块命中后，应继续检索同标题下的参考答案。",
        "- `10_综合八股文.md` 中问题列表与答案列表来自同一套编号，应按编号关联。",
        "",
        "## 推荐系统提示词",
        "",
        "```text",
        "你是嵌入式软件工程师面试教练。请基于知识库同时检索题目和对应答案。",
        "回答结构固定为：①30～60秒口述版；②原理展开；③工程中的使用或实现；④易错点；⑤可能追问。",
        "代码题必须解释关键语句、边界条件、并发/中断安全和时间复杂度，不能只粘贴代码。",
        "用户回答后再评分和纠错；除非用户要求，不要一开始就泄漏完整答案。",
        "项目经历只使用用户确认的信息，不得把题库示例冒充为用户亲历。",
        "若知识库内容与芯片手册、协议标准或官方 RTOS 文档冲突，以官方资料为准。",
        "```",
    ]
    (OUT / "00_知识包说明.md").write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8", newline="\n")


def main() -> None:
    OUT.mkdir(exist_ok=True)
    for dirname, output, title, tags in TOPICS:
        build_topic(dirname, output, title, tags)
    build_special_topics()
    build_index()
    print(f"Generated {len(list(OUT.glob('*.md')))} Markdown files in {OUT}")


if __name__ == "__main__":
    main()
