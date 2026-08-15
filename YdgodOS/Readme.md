# YdgodOS  v2.0

- 调度策略：抢占式优先调度
- 更新日期：2026 年 8 月 15 日

## 版本特性 / Version Features

- 更换了内核，使用 FreeRTOS 内核（不想造轮子了）
- Replace kernel, adopt FreeRTOS kernel (avoid reinventing the wheel)
- 新增 STM32F103C8T6 板级支持包 (常规 STM32 核心板)
- Add BSP support for STM32F103C8T6 (general STM32 core board)
- 新的系统架构，可以直接访问外设
- New system architecture, supports direct peripheral access
- 新增操作系统 CLI 应用，方便用户交互
- Add OS‑level CLI application for user interaction

STM32F103C8T6 的可运行示例位于 `bsp/YdgodOS_STMF103_demo`，默认 CLI 为 USART1（PA9/PA10，115200 8N1），PC13 为运行心跳指示灯。

## 架构说明 / Architecture

YdgodOS 定义一套统一的操作系统抽象接口。
YdgodOS defines a set of unified OS‑abstraction interfaces.

RTOS 内核以可插拔后端方式实现，当前默认后端：FreeRTOS V10.x。
RTOS kernel is implemented as pluggable backend. Current default backend: FreeRTOS V10.x.

上层组件均为自主实现 / All upper‑layer components are self‑developed：

- OS 封装抽象层 / OS wrapper abstraction layer
- 外设驱动框架 /peripheral driver framework
- CLI 命令式子系统 / CLI command‑line subsystem
- 协作式任务调度器 /cooperative task scheduler
- Wake‑Lock 电源管理模块 / Wake‑Lock power‑management module
- 通用工具与基础数据结构 /common utilities & basic data structures

应用程序运行于 YdgodOS 之上。
Applications run on top of YdgodOS.

框架与具体硬件、终端业务应用解耦，便于跨平台复用。
The framework is decoupled from hardware and end‑user applications for cross‑platform reuse.

## 第三方许可说明 / Third‑party License

本项目使用 FreeRTOS 作为内核后端实现。
This project uses FreeRTOS as kernel backend implementation.

FreeRTOS Copyright (C) [Amazon.com](https://link.wtturl.cn/?target=https%3A%2F%2FAmazon.com&scene=im&aid=497858&lang=zh), Inc. or its affiliates.
采用 MIT 许可证分发。Distributed under the MIT License.

完整版权信息请查阅 FreeRTOS 原始源码文件。
Refer to original FreeRTOS source files for full copyright notice.
