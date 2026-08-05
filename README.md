# Parcel Locker — 智能快递柜系统

基于 GEC6818 嵌入式平台的智能快递柜，支持快递员存件、用户取件、扫码取件、短信验证等功能。采用 **C 语言嵌入式端 + Node.js 云端服务** 的架构。

## 系统架构

```
┌──────────────────────────────────────────────────────────┐
│                  GEC6818 嵌入式设备                       │
│  ┌─────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │ 存件模块 │  │ 取件模块  │ │ 查询模块  │  │  扫码取件  │  │
│  └────┬────┘  └────┬─────┘  └────┬─────┘  └─────┬─────┘  │
│       └────────────┴─────────────┴──────────────┘        │
│                          │                               │
│                    ┌─────┴──────┐                        │
│                    │  UI 主逻辑  │                       │
│                    └─────┬──────┘                        │
│                          │                               │
│         ┌────────────────┼────────────────┐              │
│    ┌────┴────┐     ┌─────┴──────┐    ┌────┴────┐         │
│    │ 短信模块 │     │  验证网关   │    │ LCD显示 │         │
│    └────┬────┘     └─────┬──────┘    └─────────┘         │
└─────────┼────────────────┼───────────────────────────────┘
          │                │
          ▼                ▼
   ┌──────────────┐  ┌──────────────┐
   │ 云端短信密钥  │  │ 云端验证服务  │
   │ POST /sms_key│  │ GET/POST API │
   └──────┬───────┘  └──────┬───────┘
          │                 │
          ▼                 ▼
   ┌──────────────┐  ┌──────────────┐
   │ 互亿无线 SMS  │  │ 用户扫码验证  │
   │ 发送短信      │  │ H5 页面      │
   └──────────────┘  └──────────────┘
```

## 功能概览

| 功能 | 触发方式 | 流程 |
|------|---------|------|
| **快递员存件** | 主菜单选择"存件" | 手机号登录 → 输入柜号/选箱型/选时长 → 生成二维码 → 存件成功 |
| **用户取件** | 主菜单选择"取件" | 输入4位取件码 → 验证 → 开箱 |
| **扫码取件** | 取件界面自动生成二维码 | 用户扫码 → 手机验证 → 自动开箱 |
| **快件查询** | 取件界面点击"查询" | 输入手机号 → 短信验证 → 显示包裹状态 |
| **广告轮播** | 待机自动触发 | 循环播放广告图片 |
| **短信验证** | 登录/查询时触发 | 向云端获取密钥 → 调用互亿无线 → 发送短信 |

## 快速开始

### 1. 克隆仓库

```bash
git clone git@github.com:datpo-channel/Parcel-Locker.git
cd Parcel-Locker
```

### 2. 嵌入式端编译

需要先安装 **arm-linux-gcc 交叉编译工具链**。

```bash
# 修改 CMakeLists.txt 中的路径（改成你本机的路径）
#   CMAKE_C_COMPILER     → arm-linux-gcc 路径
#   JPEG_INCLUDE_PATH    → libjpeg 头文件路径
#   DEPLOY_HOST          → 开发板 IP 地址

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 部署到开发板
make deploy
```

### 3. 云端服务部署

将 `ali-scf/` 目录上传到你的云服务器：

```bash
scp -r ali-scf/* root@你的服务器IP:/opt/mail-box-api/
```

在服务器上：

```bash
cd /opt/mail-box-api

# 创建配置文件
cp sms_config.example.json sms_config.json

# 编辑配置，填入真实密钥
vim sms_config.json
```

`sms_config.json` 内容：

```json
{
    "password": "你的互亿无线API密码",
    "auth_token": "设备端与服务端约定的认证token"
}
```

启动服务：

```bash
node index.js
# 输出: 快递柜验证服务已启动: http://0.0.0.0:3000
```

### 4. 配置嵌入式端连接云端

编辑 `src/sms.c`，填入你的服务器 IP：

```c
#define KEY_SERVER_HOST   "你的服务器IP"
#define KEY_SERVER_PORT   3000
#define KEY_SERVER_TOKEN  "与sms_config.json中auth_token一致"
```

重新编译部署即可。

## 目录结构

```
├── include/               # 头文件
│   ├── ad_player.h        # 广告轮播模块
│   ├── keyboard_input.h   # 触摸键盘输入
│   ├── lcd_ui_display.h   # LCD 显示驱动（JPEG 解码、缓存）
│   ├── login_page.h       # 登录页（快递员/用户通用）
│   ├── pickup_monitor.h   # 扫码监控线程
│   ├── query_page.h       # 快件查询
│   ├── ret_codes.h        # 统一返回码定义
│   ├── sms.h              # 短信模块
│   ├── store_page.h       # 存件流程
│   ├── takeout_page.h     # 取件流程
│   ├── ui_logic.h         # 主菜单、初始化、工具函数
│   ├── verify_gate.h      # 云端验证网关（创建/查询/消费票据）
│   └── ...
├── src/                   # 源文件
│   ├── main.c             # 主入口，事件循环，功能跳转
│   ├── ui_logic.c         # 主菜单、取件码生成、二维码生成
│   ├── login_page.c       # 登录界面（手机号输入、验证码输入）
│   ├── query_page.c       # 查询界面（包裹状态展示）
│   ├── takeout_page.c     # 取件界面（取件码输入、成功页）
│   ├── store_page.c       # 存件界面（柜号/箱型/时长选择、支付）
│   ├── sms.c              # 短信发送（从云端获取密钥后调用互亿无线）
│   ├── pickup_monitor.c   # 扫码监控线程（轮询云端验证状态）
│   ├── verify_gate.c      # 验证网关（HTTP 请求云端 API）
│   └── ...
├── ali-scf/               # 云端服务（部署到服务器）
│   ├── index.js           # Node.js HTTP 服务（验证 API + 短信密钥 API）
│   ├── package.json
│   ├── sms_config.example.json  # 配置文件模板
│   └── sms_config.json    # 真实配置（不提交到仓库）
├── resource/              # 图片资源
│   ├── menu_pic/          # 界面背景图（登录、键盘、成功页等）
│   ├── num_pic/           # 数字/字母显示图片
│   └── AD_pic/            # 广告轮播图片
├── lib/                   # 第三方静态库
│   ├── libjpeg.a          # JPEG 解码库
│   └── libqrencode.a      # 二维码生成库
├── CMakeLists.txt         # CMake 构建配置
└── README.md
```

## 环境依赖

### 嵌入式端

| 依赖 | 版本要求 | 说明 |
|------|---------|------|
| GEC6818 开发板 | — | ARM Cortex-A9，1024×600 LCD 触摸屏 |
| arm-linux-gcc | — | ARM 交叉编译工具链 |
| libjpeg | v9f | JPEG 图片解码 |
| libqrencode | — | 二维码生成 |
| cmake | ≥ 3.10 | 构建系统 |
| pthread | — | 多线程（扫码监控、广告轮播） |

### 云端服务

| 依赖 | 版本要求 | 说明 |
|------|---------|------|
| Node.js | ≥ 12 | 原生 http 模块，无需额外依赖 |
| 互亿无线 | — | 短信发送 API，需注册账号 |

## 业务流程详解

### 快递员存件

```
主菜单 → 选择"存件"
  → 快递员手机号登录（短信验证）
  → 输入柜号（如 A01）
  → 选择箱型（大/中/小）
  → 选择存放时长（1/2/4/8小时）
  → 显示支付二维码
  → 存件成功，生成取件码
```

### 用户取件

```
主菜单 → 选择"取件"
  → 输入4位取件码
  → 验证通过 → 开箱 → 显示成功页
  → 验证失败 → 重试（最多3次）
```

### 扫码取件

```
取件界面 → 自动生成二维码（含取件token）
  → 用户手机扫码
  → 跳转 H5 验证页面
  → 输入手机号验证
  → 后台监控线程检测到验证完成
  → 自动开箱
```

### 快件查询

```
取件界面 → 点击"查询"
  → 输入手机号
  → 输入短信验证码
  → 显示包裹状态（已取件/未取件/无包裹）
```

## 云端 API 文档

服务监听 `0.0.0.0:3000`，所有接口支持 CORS。

### POST /sms_key

嵌入式设备获取短信 API 密钥。

**请求:**
```
POST /sms_key
Content-Type: application/json

{"token": "mailbox_internal_token_2024"}
```

**成功响应 (200):**
```json
{"ok": true, "password": "[你的密钥]"}
```

**失败响应 (403):**
```json
{"ok": false, "msg": "auth failed"}
```

### POST /api/verify

用户扫码后提交手机号验证。

**请求:**
```
POST /api/verify
Content-Type: application/json

{"token": "取件token", "phone": "13800001111"}
```

**成功响应 (200):**
```json
{"success": true, "message": "验证成功"}
```

**失败响应 (400/409):**
```json
{"error": "缺少必要参数"}
{"error": "已验证过", "alreadyVerified": true}
```

### GET /api/status

查询验证状态。

**请求:**
```
GET /api/status?token=取件token
```

**已验证响应 (200):**
```json
{"verified": true, "phone": "13800001111", "verifiedAt": "2024-01-01T00:00:00.000Z"}
```

**未验证响应 (200):**
```json
{"verified": false}
```

## 配置说明

### 短信密钥安全方案

短信 API 密码 存储在云端服务器。

```
设备每次发短信时的流程:
  设备 → POST /sms_key → 云端验证 token → 返回密码 → 设备使用密码调用互亿无线
```

| 配置项 | 位置 | 说明 |
|--------|------|------|
| 短信密码 | 服务器 `sms_config.json` | 互亿无线 API 密码 |
| 认证 token | 服务器 `sms_config.json` + 设备 `sms.c` | 两端必须一致 |
| 服务器地址 | 设备 `sms.c` `KEY_SERVER_HOST` | 云服务器 IP |
| 服务器端口 | 设备 `sms.c` `KEY_SERVER_PORT` | 默认 3000 |

### Demo 测试账号

开发阶段无需真实短信，使用内置测试账号：

| 角色 | 手机号 | 验证码 |
|------|--------|--------|
| 快递员/用户 | `13012345678` | `1234` |

修改位置：`src/login_page.c` 中的 `DEMO_PHONE` 和 `DEMO_CODE` 宏。

## 触摸按键说明

系统通过 4×3 矩阵键盘进行输入，触摸坐标自动映射到按键：

```
┌─────┬─────┬─────┐
│  1  │  2  │  3  │
├─────┼─────┼─────┤
│  4  │  5  │  6  │
├─────┼─────┼─────┤
│  7  │  8  │  9  │
├─────┼─────┼─────┤
│ 返回 │  0  │ 确认 │
└─────┴─────┴─────┘
```

按键映射逻辑在 `src/keyboard_input.c`，可根据实际触摸屏校准值调整坐标范围。

## 返回码参考

| 宏 | 值 | 含义 |
|----|----|------|
| `RET_TAKEOUT_OK` | 0 | 操作成功 |
| `RET_TAKEOUT_BACK` | 1 | 用户点击返回按钮 |
| `RET_TAKEOUT_QUERY` | 2 | 进入查询子页面 |
| `RET_TIMEOUT` | -2 | 页面超时（30秒无操作） |
| `RET_LOGIN_CANCEL` | -3 | 登录流程取消 |
| `RET_LOGIN_FAILED` | -4 | 验证码错误次数超限（3次） |
| `RET_SCAN_PICKUP` | 5 | 扫码取件验证完成 |

## 常见问题

### 编译与构建

<details>
<summary><b>Q: 编译报错找不到 arm-linux-gcc？</b></summary>

确认交叉编译工具链已安装：

```bash
which arm-linux-gcc
# 如果有输出路径则已安装，否则需要安装
```

安装后将 `CMakeLists.txt` 中的 `CMAKE_C_COMPILER` 改为完整路径：

```cmake
set(CMAKE_C_COMPILER /usr/local/arm/4.3.2/bin/arm-linux-gcc)  # 改成你的路径
```
</details>

<details>
<summary><b>Q: 编译报错找不到 jpeglib.h？</b></summary>

修改 `CMakeLists.txt` 中 `JPEG_INCLUDE_PATH` 为你的 libjpeg 头文件路径。例如：

```cmake
set(JPEG_INCLUDE_PATH "/home/gec/Workspace/jpegsrc.v9f/jpeg-9f/include")
```
</details>

<details>
<summary><b>Q: 编译报错找不到 libjpeg.a / libqrencode.a？</b></summary>

确认 `lib/` 目录下存在这两个静态库：

```bash
ls -la lib/libjpeg.a lib/libqrencode.a
```

如果缺失，需要先交叉编译 libjpeg 和 libqrencode，将 `.a` 文件放入 `lib/` 目录。
</details>

<details>
<summary><b>Q: 编译报错 undefined reference to pthread_create？</b></summary>

确认 `CMakeLists.txt` 中链接了 pthread：

```cmake
target_link_libraries(${PROJECT_NAME}
    ...
    pthread
    ...
)
```

CMake 中 `-lpthread` 必须放在链接库列表的最后。
</details>

### 运行与调试

<details>
<summary><b>Q: 程序启动后 LCD 黑屏？</b></summary>

1. 确认 LCD 设备节点存在：`ls -la /dev/fb0`
2. 确认图片资源路径正确，程序运行时 `resource/` 目录必须在可执行文件同级
3. 查看程序输出日志，检查是否有 `lcd_init failed` 或图片加载失败提示
4. 确认 `resource/menu_pic/jpg/main_menu.jpg` 等关键图片存在
</details>

<details>
<summary><b>Q: 触摸屏点击无响应？</b></summary>

1. **测试触摸设备**：`cat /dev/input/event0` 然后触摸屏幕，看是否有乱码输出（说明触摸驱动正常）
2. **检查设备路径**：确认 `src/config.h` 中 `TOUCHPAD_PATH` 与实际设备节点一致（通常是 `/dev/input/event0`）
3. **校准坐标**：`src/keyboard_input.c` 中的坐标范围需要匹配你的屏幕分辨率（1024×600），步骤：
   - 在 `get_key_from_touch()` 中添加调试打印 `printf("x=%d y=%d\n", ts_x, ts_y);`
   - 依次点击键盘四个角，记录坐标值
   - 根据实际坐标调整代码中的阈值
</details>

<details>
<summary><b>Q: 程序运行一段时间后卡死？</b></summary>

1. **内存泄漏**：用 `top` 查看内存是否持续增长，关注 `ui_logic.c` 中的 `malloc` 是否有对应的 `free`
2. **线程死锁**：检查 `pickup_monitor.c` 和 `ad_player.c` 的线程是否正确退出
3. **socket 阻塞**：`sms.c` 已设置超时（5秒），确认设备网络正常。如果网络很慢，可适当增大 `SMS_TIMEOUT_SEC`
4. **触摸事件堆积**：如果触摸响应延迟，检查 `touchpad.c` 是否正确读取和清空事件队列
</details>

### 短信功能

<details>
<summary><b>Q: 短信发送失败（返回 -5）？</b></summary>

**-5 表示无法从密钥服务器获取密码**，按以下顺序排查：

```bash
# 1. 确认服务器端口可达（在开发板上执行）
telnet 你的服务器IP 3000
# 或
curl -s -X POST http://你的服务器IP:3000/sms_key \
  -H "Content-Type: application/json" \
  -d '{"token":"mailbox_internal_token_2024"}'
```

2. 确认 `src/sms.c` 中 `KEY_SERVER_HOST` 和 `KEY_SERVER_PORT` 正确
3. 确认 `src/sms.c` 中 `KEY_SERVER_TOKEN` 与服务器 `sms_config.json` 中 `auth_token` 一致
4. 确认服务器防火墙已开放 3000 端口
</details>

<details>
<summary><b>Q: 短信发送失败（返回 -2 / -3 / -4）？</b></summary>

| 返回值 | 含义 | 排查方向 |
|--------|------|----------|
| -2 | 无法连接互亿无线 | 检查设备能否访问外网 `ping 118.31.68.22` |
| -3 | 发送数据失败 | 网络不稳定，重试 |
| -4 | 读取响应失败 | 互亿无线服务端问题，稍后重试 |

如果设备无外网访问能力，需要修改架构，让云端服务代理 SMS 发送。
</details>

<details>
<summary><b>Q: 如何切换短信服务商？</b></summary>

1. 修改 `src/sms.c` 中的 `SMS_HOST_IP`、`SMS_PORT` 和 HTTP 请求格式
2. 修改服务器 `sms_config.json` 中的 `password` 为新服务商的密钥
3. 如果只需要 demo 模式（不发送真实短信），使用测试账号 `13012345678` + `1234`
</details>

### 云端服务

<details>
<summary><b>Q: 服务器启动后外网无法访问？</b></summary>

```bash
# 1. 确认服务监听正确
ss -tlnp | grep 3000
# 应该显示 0.0.0.0:3000，如果是 127.0.0.1:3000 则只监听本地

# 2. 检查防火墙（阿里云/腾讯云需在安全组中放行 3000）
iptables -L -n | grep 3000

# 3. 测试本地接口
curl -X POST http://localhost:3000/sms_key \
  -H "Content-Type: application/json" \
  -d '{"token":"mailbox_internal_token_2024"}'
```
</details>

<details>
<summary><b>Q: Node.js 进程意外退出？</b></summary>

推荐使用 PM2 保持进程存活：

```bash
npm install -g pm2
pm2 start /opt/mail-box-api/index.js --name mail-box-api
pm2 save
pm2 startup    # 设置开机自启
```

查看日志：

```bash
pm2 logs mail-box-api
```
</details>

<details>
<summary><b>Q: 修改 sms_config.json 后如何生效？</b></summary>

```bash
# PM2 方式
pm2 restart mail-box-api

# 手动方式
kill $(lsof -t -i:3000)
cd /opt/mail-box-api && node index.js &
```
</details>

### 扫码取件

<details>
<summary><b>Q: 二维码不显示或显示异常？</b></summary>

1. 确认 `resource/qrcode/` 目录存在且有写入权限
2. 确认 `libqrencode.a` 已正确链接
3. 检查 `src/main.c` 中 `generate_qr()` 调用参数是否正确
4. 检查二维码尺寸：当前为 3 级容错、2 像素模块、85% 缩放，在 1024×600 屏幕上显示在 (188, 188) 位置
</details>

<details>
<summary><b>Q: 用户扫码后如何验证？</b></summary>

```
用户扫码 → 打开 H5 页面 → 输入手机号 → 点击验证
    → 云端 POST /api/verify → 数据写入 /tmp/verify_data.json
    → 设备轮询 GET /api/status?token=xxx → 检测到验证完成 → 自动开箱
```

轮询间隔：3 秒，超时：300 秒（5 分钟），可在 `src/main.c` 中修改。
</details>

<details>
<summary><b>Q: 扫码验证后没有自动开箱？</b></summary>

1. 确认 `pickup_monitor.c` 线程正常启动（查看日志是否有 `[SCAN]` 前缀）
2. 检查 `g_pickup_notify_flag` 是否被正确设置
3. 确认 `verify_gate.c` 中 `VG_STATUS_VERIFIED` 返回值正确
4. 检查柜号与手机号的绑定关系：`locker_info.c` 中 `small_phone` 字段必须与验证手机号一致
</details>

### 其他

<details>
<summary><b>Q: 如何替换界面图片？</b></summary>

所有界面图片为 JPEG 格式，存放在 `resource/menu_pic/jpg/` 目录。替换规则：

- 分辨率必须为 800x480，与 LCD 一致
- 文件名必须与代码中使用的名称一致（如 `main_menu.jpg`）
- 替换后无需重新编译，直接替换文件即可
- 键盘图片 `keyboard.jpg` 如需修改布局，需同步修改 `src/keyboard_input.c` 中的坐标映射
</details>

<details>
<summary><b>Q: 如何添加新的箱型或时长选项？</b></summary>

1. 添加对应的选中/未选中图片到 `resource/menu_pic/jpg/`
2. 修改 `src/store_page.c` 中的选项数组和显示逻辑
3. 修改 `src/ui_logic.c` 中的 `generate_pickup_code()` 函数（如果需要新的取件码格式）
</details>

<details>
<summary><b>Q: 如何修改页面超时时间？</b></summary>

各页面超时时间在对应源文件中定义，搜索 `TIMEOUT` 或 `timeout` 即可找到：

- 主菜单：`src/ui_logic.c` 中 `show_main_menu()` 函数
- 登录页：`src/login_page.c` 中 `show_login_common()` 函数
- 取件页：`src/takeout_page.c` 中 `show_takeout_code()` 函数
- 扫码轮询：`src/main.c` 中 `SCAN_PICKUP_POLL_TIMEOUT_SEC` 宏
</details>

## License

MIT
