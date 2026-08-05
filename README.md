# GEC6818 Mail Box System

基于 GEC6818 嵌入式平台的智能快递柜系统，支持**快递员存件**、**用户取件**、**扫码取件**、**短信验证**等功能，配套云端验证服务。

## 功能概览

| 功能 | 说明 |
|------|------|
| 快递员存件 | 手机号登录 → 填柜号/箱型/时长 → 扫码支付 → 存件成功 |
| 用户取件 | 输入取件码 → 开箱取件 |
| 扫码取件 | 生成二维码 → 用户扫码验证 → 自动开箱 |
| 快件查询 | 手机号 + 短信验证码 → 查询包裹状态 |
| 广告轮播 | 待机时自动轮播广告图片 |
| 短信验证 | 通过互亿无线 API 发送短信验证码 |

## 目录结构

```
mail_box_pic/
├── include/            # 头文件
│   ├── ad_player.h         # 广告轮播
│   ├── keyboard_input.h    # 触摸键盘输入
│   ├── lcd_ui_display.h    # LCD 显示驱动
│   ├── login_page.h        # 登录页（快递员/用户）
│   ├── pickup_monitor.h    # 扫码监控线程
│   ├── query_page.h        # 快件查询
│   ├── ret_codes.h         # 统一返回码
│   ├── sms.h               # 短信发送
│   ├── store_page.h        # 存件流程
│   ├── takeout_page.h      # 取件流程
│   ├── ui_logic.h          # UI 主逻辑
│   ├── verify_gate.h       # 云端验证网关
│   └── ...
├── src/                # 源文件
│   ├── main.c              # 主入口
│   ├── ui_logic.c          # 主菜单、初始化
│   ├── login_page.c        # 登录界面
│   ├── query_page.c        # 查询界面
│   ├── takeout_page.c      # 取件界面
│   ├── store_page.c        # 存件界面
│   ├── sms.c               # 短信发送（从云端获取密钥）
│   ├── pickup_monitor.c    # 扫码监控
│   ├── verify_gate.c       # 验证网关
│   └── ...
├── ali-scf/        # 云端验证服务（部署到云服务器）
│   ├── index.js            # Node.js HTTP 服务
│   ├── sms_config.json     # 短信 API 密钥配置
│   └── package.json
├── resource/           # 图片资源
│   ├── menu_pic/           # 界面背景图
│   ├── num_pic/            # 数字/字母图片
│   └── AD_pic/             # 广告图片
├── lib/                # 静态库
│   ├── libjpeg.a
│   └── libqrencode.a
├── CMakeLists.txt
└── README.md
```

## 依赖

### 嵌入式端

| 依赖 | 说明 |
|------|------|
| GEC6818 开发板 | ARM Cortex-A9, LCD 触摸屏 |
| arm-linux-gcc | 交叉编译工具链 |
| libjpeg | JPEG 解码（jpegsrc.v9f） |
| libqrencode | 二维码生成 |
| cmake ≥ 3.10 | 构建系统 |
| pthread | 多线程支持 |

### 云端服务

| 依赖 | 说明 |
|------|------|
| Node.js ≥ 12 | 运行环境 |
| 互亿无线 API | 短信发送服务 |

## 构建

### 嵌入式端

```bash
# 1. 修改 CMakeLists.txt 中的交叉编译器和路径
#    CMAKE_C_COMPILER   → 你的 arm-linux-gcc 路径
#    JPEG_INCLUDE_PATH   → libjpeg 头文件路径
#    DEPLOY_HOST         → 开发板 IP

# 2. 构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. 部署到开发板
make deploy
```

### 云端服务
请先将ali-scf中的文件上传至服务器的/opt/mail-box-api路径下
```bash
cd tencent-scf
npm install

# 配置短信密钥
cp sms_config.example.json sms_config.json
# 编辑 sms_config.json，填入互亿无线 API 密码

# 启动
node index.js
```

## 云端服务 API

服务默认监听 `0.0.0.0:3000`。

### POST /sms_key

从云端获取短信 API 密钥，嵌入式设备发短信前调用。

```bash
curl -X POST http://服务器IP:3000/sms_key \
  -H "Content-Type: application/json" \
  -d '{"token":"mailbox_internal_token_2024"}'

# 响应
{"ok":true,"password":"[你的API密钥]"}
```

### POST /api/verify

用户扫码后提交手机号验证。

### GET /api/status

查询验证状态。

## 配置

### 短信密钥

短信 API 密码存储在云端 `sms_config.json`

```json
{
    "password": "你的互亿无线API密码",
    "auth_token": "设备端与服务端约定的认证token"
}
```

嵌入式端 `src/sms.c` 中配置密钥服务器地址：

```c
#define KEY_SERVER_HOST   "你的服务器IP"
#define KEY_SERVER_PORT   3000
#define KEY_SERVER_TOKEN  "与sms_config.json中auth_token一致"
```

### Demo 测试账号

开发阶段可使用内置测试账号，跳过真实短信发送：

| 手机号 | 验证码 |
|--------|--------|
| `13012345678` | `1234` |

可在 `src/login_page.c` 中修改 `DEMO_PHONE` 和 `DEMO_CODE` 宏。

## 触摸按键映射

界面采用 4×3 数字键盘布局，通过触摸坐标转换为按键值：

```
[1] [2] [3]
[4] [5] [6]
[7] [8] [9]
[返回] [0] [确认]
```

## 返回码

| 宏 | 值 | 含义 |
|----|----|------|
| `RET_TAKEOUT_OK` | 0 | 操作成功 |
| `RET_TAKEOUT_BACK` | 1 | 用户点击返回 |
| `RET_TAKEOUT_QUERY` | 2 | 进入查询 |
| `RET_TIMEOUT` | -2 | 页面超时（30秒无操作） |
| `RET_LOGIN_CANCEL` | -3 | 登录取消 |
| `RET_LOGIN_FAILED` | -4 | 验证码错误次数超限（3次） |
| `RET_SCAN_PICKUP` | 5 | 扫码取件验证完成 |

## License

MIT
