<!--suppress HtmlDeprecatedAttribute, HtmlDeprecatedTag -->
<div align="center">

# <img src="resources/icons/app/full-icon-half.png" alt="仙交网络助手" />

---

### 基于Qt的西安交通大学校园网鉴权桌面客户端

[![GitHub License](https://img.shields.io/github/license/Hz6826/XJTUNetLogin)](LICENSE)
[![GitHub Issues](https://img.shields.io/github/issues/Hz6826/XJTUNetLogin)](https://github.com/Hz6826/XJTUNetLogin/issues)
[![GitHub PRs](https://img.shields.io/github/issues-pr/Hz6826/XJTUNetLogin)](https://github.com/Hz6826/XJTUNetLogin/pulls)
![GitHub Stars](https://img.shields.io/github/stars/Hz6826/XJTUNetLogin?style=social)

</div>

## 功能列表

- :white_check_mark: 已完成
- :memo: 进行中
- :x: 近期无计划实现

| 功能            | 实现情况               |
|---------------|--------------------|
| 登录/注销         | :white_check_mark: |
| 测试连接          | :white_check_mark: |
| 获取在线信息        | :white_check_mark: |
| 账户存储          | :white_check_mark: |
| 托盘最小化         | :white_check_mark: |
| 开机自启          | :white_check_mark: |
| 自动登录          | :white_check_mark: |
| 自动检查版本        | :white_check_mark: |
| MacOS/Linux支持 | :memo:             |
| Android 支持    | :memo:             |

## 运行

请在 [Releases](https://github.com/Hz6826/XJTUNetLogin/releases) 界面获取最新版本，其中可执行程序为 **Inno Setup** 打包的安装版，名称包含`portal`的压缩包为便携版。

**如果要使用开机自启功能，建议使用安装版！**

_注：两个版本文件并无差异，若主程序同目录下含有`config.json`会自动改为读取该文件，并转换为便携版，将日志存储在`./logs/`下_

## 原理

本仓库客户端`SrunClient`和加密库`SrunCrypto`参考了登录门户js的实现，基本流程如下：

### 登录

1. 第一次`requestChallenge`: 向服务器请求一个challenge token，附带`username`，此时`ip`为空。本次challenge丢弃，仅通过响应体获取服务器处的真实内网ip；
2. 第二次`requestChallenge`: 此时携带步骤1中获取的`ip`，重新获取challenge token，服务器返回`token`；
3. `requestAuth`: 登录主体，进行主要的加密、签名操作：
   1. `SrunCrypto::hmacMd5`: 使用 `HMAC-MD5` 加密，返回 32 位小写 hex 字符串，存入`password`；
   2. `SrunCrypto::encodeUserInfo`: 拼接 json，使用`xxteaEncrypt`将json用`token`加密，返回的`{SRBX1}`存入`info`；
   3. `SrunCrypto::buildChksum`: 将上述参数等拼接后计算`sha1`作为校验和
   4. 请求: 携带所有参数向服务器`/cgi-bin/srun_portal`发送`GET`请求，`action`为`login`，并读取响应；
4. `pollOnlineStatus`: 若登录成功，确认在线状态，默认3次；

### 注销

携带`username`、`ip`、`ac_id`等，向服务器`/cgi-bin/srun_portal`发送`GET`请求，`action`为`logout`。若注销成功，`pollOnlineStatus`确认在线状态，默认3次。

### 查询

携带`username`、`ip`、`ac_id`等，向服务器`/cgi-bin/rad_user_info`发送`GET`请求，服务器返回jsonp。

## 许可证

本项目使用了依据 [GNU Lesser General Public License v3](https://www.gnu.org/licenses/lgpl-3.0.en.html#license-text) 协议开源的 Qt 库，并遵循该协议的相关条款。

本项目使用 [Inno Setup](https://jrsoftware.org/isinfo.php) 打包，感谢Jordan Russell做出的贡献！

本项目基于 [MIT License](LICENSE) 开源。

## 致谢

- [@Ianzb](https://github.com/Ianzb/) : 提供了大量的宝贵意见

## Star History

_如果觉得这个项目对你有帮助的话，麻烦点一下右上角的Star，你的支持是我最大的动力，谢谢喵~_

[![Star History Chart](https://api.star-history.com/svg?repos=Hz6826/XJTUNetLogin&type=Date)](https://www.star-history.com/#Hz6826/XJTUNetLogin&Date)
