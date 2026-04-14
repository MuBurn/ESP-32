## VSCode+ESP-IDF 开发环境配置（2026.3）

1.打开vscode拓展搜索ESP-IDF

![image-20260326212132357](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212132357.png)

2.在commands中找到advanced -> 打开ESP-IDF  安装管理器

![image-20260326212335456](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212335456.png)

![image-20260326212356003](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212356003.png)

右上角有中文

3.安装配置

![image-20260326212453342](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212453342.png)

（python环境配置略过）芯片全选

![image-20260326212546212](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212546212.png)

版本可选最新的稳定版

![image-20260326212619989](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212619989.png)

镜像选延迟最小的

![image-20260326212656405](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212656405.png)

| 组件名称          | 功能说明                                                     | 推荐场景                                            |
| ----------------- | ------------------------------------------------------------ | --------------------------------------------------- |
| **core**          | ESP-IDF 核心包，是编译、构建和运行 ESP32 系列芯片程序的基础依赖，**必须勾选**。 | 所有 ESP-IDF 开发场景                               |
| **test-specific** | 用于运行 ESP-IDF 内置的特定测试脚本、单元测试和集成测试的工具包。 | 需要运行官方测试用例、进行固件稳定性测试的开发者    |
| **ci**            | 为持续集成（CI）流程提供的脚本和工具，支持在 GitLab CI、GitHub Actions 等平台自动化构建与测试。 | 参与团队协作、需要自动化部署 / 测试的项目           |
| **docs**          | 构建 ESP-IDF 官方文档所需的工具链（如 Sphinx、Doxygen），可本地生成 HTML/PDF 版开发手册。 | 需要离线查阅完整文档、参与文档贡献的开发者          |
| **ide**           | 提供对 VS Code、Eclipse 等 IDE 的支持，包括调试配置、代码补全、工程模板等功能。 | 使用图形化 IDE 进行开发、希望获得便捷调试体验的用户 |
| **mcp**           | 提供 MCP（Model Context Protocol）服务器功能，用于与 AI 模型交互，辅助代码生成和上下文理解。 | 结合 AI 助手进行开发、需要智能代码辅助的场景        |

这些组件我都选了

![image-20260326212833324](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260326212833324.png)

| 工具名称         | 功能说明                                                     | 推荐场景                                                 |
| ---------------- | ------------------------------------------------------------ | -------------------------------------------------------- |
| **qemu-xtensa**  | Xtensa 架构的 QEMU 模拟器，可在电脑上模拟运行 ESP32 固件，无需真实硬件即可进行功能测试、调试与性能分析。 | 无硬件时提前验证代码、自动化测试、复现硬件相关问题。     |
| **qemu-riscv32** | RISC-V 架构的 QEMU 模拟器，用于模拟运行 ESP32-C3 等 RISC-V 芯片的固件，支持指令级仿真与调试。 | 针对 RISC-V 系列芯片的开发与测试，无硬件时验证固件行为。 |

可选工具主要用于模拟和仿真，非必须不需要用

安装路径不含中文就行

可能会出现安装失败的情况，检查日志 若出现这个保错

```
error: Microsoft Visual C++ 14.0 or greater is required.
ERROR: Failed building wheel for netifaces
```

前往微软官网下载[Microsoft C++ 生成工具 - Visual Studio](https://visualstudio.microsoft.com/zh-hans/visual-cpp-build-tools/)

点击 **下载生成工具** 并运行安装程序

在安装界面中，**勾选这一个选项**：

- **使用 C++ 的桌面开发**（Desktop development with C++）

 **适用于 x64/x86 的 MSVC 生成工具 (最新版)**

**Windows 11 SDK（或对应版本的 Windows SDK**）

在其中勾选有关sdk俩个选项

下载安装完重启即可

