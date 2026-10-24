# 主题设计指南

本文说明如何为仙交网络助手制作、调试和分发自定义主题。示例主题见仓库根目录的 `themes/`（`blue`、`sakura`、`cyber`、`matrix`）。

## 主题放在哪

- 安装版：`%APPDATA%\Hz6826\XJTUNetLogin\themes\<主题ID>`
- 便携版：主程序同目录 `themes\<主题ID>`（便携模式下 `Config::appDataDir()` 就是 exe 目录）
- 程序启动时会自动创建 `themes` 目录；设置 → 外观 → 主题旁有「打开主题目录」按钮
- 一个子文件夹 = 一套主题，**文件夹名就是主题 ID**（`config.json` 的 `ui.themeId` 保存的就是它）
- `default` 是保留 ID：不使用任何自定义样式，恢复 Qt 原生样式（Windows 11 上为 `windows11`）

## 目录结构

```
themes/<主题ID>/
  manifest.json            # 可选：元信息与界面配色
  styles.qss               # 通用样式
  styles-light.qss         # 浅色样式
  styles-dark.qss          # 深色样式
  log_textedit.qss         # 可选：日志区样式
  log_textedit-light.qss
  log_textedit-dark.qss
  images/...               # 图片资产（png / svg）
```

- `styles.qss` 与 `styles-light.qss` / `styles-dark.qss` **至少提供一个**，否则该目录不会被识别为主题。
- 查找顺序：`styles-<当前明暗>.qss` → `styles.qss` → `styles-<另一明暗>.qss`；`log_textedit` 同理。
- 系统明暗切换时会自动重新应用当前主题，不需要重启程序。
- 目录里放 README、LICENSE 等其它文件不影响识别（扫描只看是否存在 `styles*.qss`）。

## manifest.json

```json
{
  "name": "蓝调",
  "version": "1.0.0",
  "author": "Hz6826",
  "updatedAt": "2026-10-24",
  "link": "https://github.com/Hz6826/XJTUNetLogin",
  "iconColor": { "light": "#2f6fb3", "dark": "#9ecbff" },
  "titleBarColor": { "light": "#e3ebf5", "dark": "#1e2430" },
  "borderColor": { "light": "#b9c6d6", "dark": "#3a4a63" },
  "linkColor": { "light": "#1a5fa8", "dark": "#7ec8ff" }
}
```

| 字段                                          | 说明                                  |
|---------------------------------------------|-------------------------------------|
| `name`                                      | 设置页下拉框里的显示名；缺省时用文件夹名                |
| `version` / `author` / `updatedAt` / `link` | 悬停提示中展示的元信息，都可以省略                   |
| `iconColor`                                 | 界面线框图标（折叠箭头、清空、打开目录）的线条颜色           |
| `titleBarColor`                             | Windows 11 原生标题栏背景色，文字颜色按底色亮度自动黑/白  |
| `borderColor`                               | Windows 11 原生窗口边框色                  |
| `linkColor`                                 | “关于”页里富文本链接的颜色（QSS 的 `color` 管不到链接） |

- 四个颜色字段都支持**单个颜色字符串**或 **`{ "light": ..., "dark": ... }` 按明暗分开**；没写或值无效时保持系统默认。
- 标题栏/边框配色依赖 DWM，仅 Windows 11 支持；Windows 10 及更早版本会静默忽略，不影响其它样式。
- 富文本链接（“关于”页的版本检查、开源许可、仓库/作者链接）的颜色由 HTML 里的 `<a>` 标签决定，QSS 的 `color` 和 `QPalette::Link` 都管不到；主题用 `linkColor` 指定，程序会写进 `<a style="color:...">`。不写时链接沿用系统默认蓝色，深色主题下可能看不清。

## styles.qss 写法

普通 Qt Style Sheet 即可。示例主题里的常用选择器：

```css
QWidget { background-color: #f5f7fa; color: #202733; }
QLineEdit, QComboBox { border: 1px solid #b9c6d6; border-radius: 4px; padding: 2px 6px; }
QPushButton { background-color: #e3ebf5; border-radius: 4px; padding: 4px 10px; }
QPushButton:hover { background-color: #d3e0f0; }
QGroupBox { border: 1px solid #b9c6d6; border-radius: 6px; }
QGroupBox::title { color: #2f6fb3; }
QTabBar::tab:selected { background: #ffffff; color: #1a4f8a; }
QScrollBar:vertical { background: #eef2f7; width: 10px; }
QScrollBar::handle:vertical { background: #b9c6d6; border-radius: 5px; min-height: 24px; }
```

### 图片资产

用相对路径引用主题目录里的图片：

```css
QPushButton { border-image: url(images/btn.png); }
```

程序会在应用前把相对 `url(...)` 改写成绝对路径（Qt 的 QSS 默认按进程工作目录解析 url，直接用相对路径会找不到文件）。支持 png/jpg/svg，SVG 由 Qt 的 `qsvg` 图像格式插件加载，releases 里已随包携带。

### 背景图

窗口尺寸约束（`MainWindow.h`）：宽 360–500 px，高 495–695 px（日志区展开时；收起后高度会再减少约 300 px）。展开状态大致是 `480 × 690`，接近 **5:7** 的竖图，所以：

- **推荐画布：1000 × 1400 px**（5:7，2x 资源），另提供 1x 版本 `bg.png`（500 × 700）和 2x 版本 `bg@2x.png`（1000 × 1400）。Qt 样式表会按屏幕缩放比例自动挑 `@2x` 文件（`QStyleSheetStyle` 的 `@Nx` 机制），不用自己判断 DPI。
- Qt 样式表**没有 `background-size`**，所以图片不会自动等比缩放，按需求二选一：
  - 跟随窗口拉伸：`QMainWindow { border-image: url(images/bg.png) 0 0 0 0; }`（`border-image` 九宫格缩放，后四个 `0` 表示不切边、整体拉伸）
  - 原尺寸居中、超出裁掉：`background-image: url(images/bg.png); background-position: center; background-repeat: no-repeat;`（小图默认会平铺）
- 窗口宽高比在 0.52–0.72 之间变化（日志展开/收起、窗口拖动缩放），拉伸会有轻微变形：把视觉重点放在中间区域，边缘留白，别让关键元素贴边。
- 网格/渐变类背景适合可拉伸设计；写实图片建议用居中裁剪方案。
- 背景图上还要盖控件，记得让面板半透明，否则会被背景色规则完全盖住：

```css
QMainWindow { border-image: url(images/bg.png) 0 0 0 0; }
QWidget { background: transparent; }
QGroupBox, QTabWidget::pane { background-color: rgba(255, 255, 255, 0.72); }
```

- 资源建议用 png/jpg、单张 < 500 KB，避免主题包过大。

### 下拉箭头

默认会显示系统箭头；要跟主题配色一致，需要自带箭头图片并写 `down-arrow` 规则，同时给文字留出右侧空间：

```css
QComboBox {
    padding-right: 18px;
}

QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 18px;
    border: none;
    background: transparent;
}

QComboBox::down-arrow {
    image: url(images/arrow-down-light.svg);
    width: 10px;
    height: 6px;
}
```

深色变体里把 `arrow-down-light.svg` 换成深色配色版本即可。

### 日志区

日志文本框带内联样式（默认深色配色），全局 QSS 覆盖不到，需要单独的 `log_textedit*.qss`：

```css
QPlainTextEdit {
    background-color: #0d1117;
    color: #c9d1d9;
    border: 1px solid #3a4a63;
    border-radius: 4px;
    font-family: Consolas, 'Courier New', monospace;
    font-size: 12px;
}
```

日志正文按 `ERROR/WARN/SUCCESS/STEP/HTTP/INFO` 分级着色，程序会根据日志区实际底色的亮度在「深底/浅底」两套前景色中自动选择，**不需要也不应该在 QSS 里设置**。

### 其它注意事项

- 登录/注销按钮的角色色（登录蓝、注销红）由程序写在控件级样式里，主题的 `QPushButton { color: ... }` 不会覆盖它。
- 主题只作用于应用内控件；系统托盘图标、窗口原生标题栏文字等由系统/程序控制（标题栏背景和边框可用上面 manifest 的字段配置）。
- 颜色支持 `rgba()`，可以做半透明/描边效果。

## 调试

1. 把主题文件夹放进主题目录，在 设置 → 外观 里点「刷新」，列表会重新扫描并立即重新应用当前主题。
2. 修改 `styles*.qss` 后再点「刷新」即可看到效果，不用重启。
3. 读取失败时日志面板会输出 `无法读取主题 xxx 的 styles.qss，已恢复默认样式`，并回落到默认配色。

## 分发

- 仓库根目录 `themes/` 下的主题会随**便携版** zip 一起发布（放在 exe 同目录的 `themes/` 下）。
- **安装版**由 Inno Setup 把主题装到 `%APPDATA%\Hz6826\XJTUNetLogin\themes`，卸载时保留（不删除用户修改过的主题）。
- 第三方主题可以打包成文件夹直接放进主题目录，也可以参考仓库里的示例主题结构提交 PR。
