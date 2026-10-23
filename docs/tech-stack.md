# ADR-0001：技术选型

- 状态：已采纳（2026-10）
- 替代：早先的「Tauri + Rust sidecar」提案

## 结论

| 层 | 决定 |
| --- | --- |
| UI 框架 | Qt 5.15 + qmake（Widgets，不用 QML） |
| 终端渲染 | 自研 `QWidget` + `QPainter`（Konsole 路线），抽象出 `Screen`/`VtParser` |
| SSH | 系统 `ssh` + `forkpty`，不自研协议 |
| Telnet | 自研 IAC 协商，`QTcpSocket` |
| 串口 | `QSerialPort` |
| 范围 | 只做 SSH / Telnet / 串口 |

## 背景

目标是用开源软件替代 MobaXterm。最初的方案是 Tauri + Rust sidecar（见下文对比），后来范围收敛到三个协议、并且明确只用 Qt 5，于是重新评估。

## 决策依据

**1. 终端渲染不是"画字"**
真正的难点是行内多属性分段、CJK 宽字符对齐、IME 候选窗定位、大流量下的帧合并。Qt 提供了 HarfBuzz 整形、字体回退、彩色 emoji、`inputMethodEvent`，前三点不用自己造。剩余工作量集中在 `Screen` 状态机和绘制循环，约 2000 行。

**2. SSH 的复杂度在"配置"而不是"协议"**
内嵌 libssh2 意味着：密钥格式、agent 转发、ProxyJump、known_hosts、键盘交互式认证、多因素，全都要自己实现一遍。直接跑系统 `ssh` 却是零成本——用户已有的配置原样生效。用 pty 包一层即可解决窗口尺寸和 SIGWINCH。

**3. 许可**
Qt 开源版是 LGPLv3：动态链接 + 允许替换库即可合规，本项目 MIT 开源天然满足。不能静态链接、不能用于 Mac App Store 的 DRM 场景；将来若闭源商业化需要商业许可。
qtermwidget 是 GPL-2.0，会传染整个项目，因此排除——这也是自研内核的直接原因。

**4. 为什么不用 Rust sidecar**
跨进程协议、打包、生命周期管理都是额外成本；在只做三个协议、单人开发的规模下，收益不足以抵消复杂度。这一点在团队/功能扩大（比如要 SFTP 并发传输、X11 转发）时可以重新评估——`Transport` 接口就是为此留的缝。

## 被否决的方案

| 方案 | 否决原因 |
| --- | --- |
| Tauri 2 + xterm.js | 依赖系统 WebView，三平台三个引擎；Linux 下 fcitx5/IME 有坑；终端选择/渲染要和 xterm.js 的内部模型搏斗 |
| QWebEngineView + xterm.js | 包体 +80MB，内存翻倍，等于把 Electron 装进 Qt |
| 内嵌 libssh2/libssh | 协议之外的配置成本极高，C 实现的攻击面也更大 |
| QSsh（Qt Creator 内部库） | 非公开模块，无 API 稳定性承诺，功能覆盖不足 |

## 后果

- 正面：单二进制、启动快、渲染可控、无第三方终端依赖、内核可无头测试
- 负面：`forkpty` 绑定 POSIX，Windows 需要单独实现（ConPTY）
- 负面：Qt 是单一供应商，遇到框架层 bug 只能等上游或绕路
- 待办：Windows 支持、SFTP 面板、标签内搜索、分屏
