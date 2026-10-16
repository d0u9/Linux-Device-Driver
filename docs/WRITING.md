# 文档写作规约

本文件记录教程维护过程中确认的约定，供后续修改参考。

## 修改范围

- 讨论方案、解释原理或 review 时，先给结论，不直接修改文件。
- 修改文档时，先修正不正确的内容，不顺带扩写背景、增加功能或重写整章。
- 指定只改某篇时，其他文件中的问题单独说明，不自行修改。
- 全局重命名时，同步更新文件名、路径、链接、工具和测试中的引用。
- 保留作者已有的有效修改；不因为统一风格而改写无关内容。

## 语言与术语

- 教程正文保持英文，使用 Linux 内核社区常用的术语和表达。
- 用词准确，例如 kernel module、init/exit function、userspace、kbuild、
  in-tree/out-of-tree、built-in/module，以及 I/O readiness。
- 明确区分宿主机（host）与虚拟机（guest）、源码树与构建产物、
  配置符号与命令行选项，不用含糊的“这里”“运行它”代替执行位置。
- 解释当前步骤需要的内容，避免重复前文、无关背景和未经验证的泛化结论。

## 命令与文件编辑标记

每个操作代码块前标明用途，尤其不能把写入 guest `/init` 的内容写得像
让读者在宿主机终端直接执行。使用以下标记：

| 标记 | 含义 |
| --- | --- |
| **RUN ON HOST** | 在宿主机执行。 |
| **RUN IN GUEST** | 在 QEMU guest 中执行。 |
| **EDIT FILE** | 编辑文件，注明文件路径、执行时机和插入位置。 |
| **HOST QEMU ARGUMENT** | QEMU 启动参数片段，不是独立命令。 |

写入 initramfs 启动脚本的示例：

> **EDIT FILE: `$LDD_ROOT/initramfs/init` on the host → `/init` in the guest.**
> This block runs at guest boot, after mounting devtmpfs and before starting telnetd.

```sh
# Guest /init: after mounting devtmpfs, before starting telnetd.
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
```

- 宿主机 Bash 命令用 `bash` 代码块，guest shell 命令用 `sh`，
  参数片段和菜单用 `text`。不要把菜单示意图当作 shell 命令检查。
- 明确步骤顺序；新增启动命令应注明位于哪个挂载之后、哪个循环之前。
- 修改 initramfs 文件后，说明需要重新打包并重启 guest。
- QEMU 参数应说明是追加还是替换，避免重复创建网卡或后端。
- shell 路径变量加引号。写入 guest 脚本的 heredoc 默认使用带引号的
  分隔符，如 `<< 'EOF'`，避免宿主机提前展开变量。

## 教程仓库与运行环境

- 仓库保存 `docs/`、`examples/` 和 `tools/`；独立的 `$LDD_ROOT`
  保存运行环境。不要默认把教程仓库根目录当作 runtime。
- 工具源文件与安装后的文件分开说明，给出安装或更新命令。
- 构建依赖写在对应构建章节；内核配置集中在内核构建章节，其他章节引用。
- 路径以当前目录约定为准；明确宿主机导出路径与 guest 挂载路径的对应关系。
- 配置尽量限制在项目运行环境内。需要 shell 初始化配置等持久改动时，
  明确说明；安装脚本当前只打印提示，不自动修改用户的 shell 配置。

## 版本、验证与待办

- 指明实际使用的内核系列、软件版本和架构，不笼统写“最新版本”。
- 保留已经确定的获取方式，不擅自把 `wget` 发布包流程改成 Git checkout。
- 区分目标版本、已构建版本和已验证的示例，不能把更新说明写成兼容性保证。
- 优先引用上游官方文档；修改后检查本地链接和命令语法。
- 明确验证边界：语法检查、模拟测试和真实 guest 测试不能混为一谈。
- 未完成工作统一用 `TODO:`，写清待办和完成条件，不使用特殊的私有标记。
- 提交信息保持一行，说明最终修改的意图；没有要求时不自动提交或推送。
